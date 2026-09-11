/* main.c - 小尺寸便携式个人辐射剂量仪 整机固件 v1
 *
 * 功能（按已确认计划）：
 *   - PA0 脉冲计数(TIM2/ETR) -> 每秒 CPS(8s 平滑)
 *   - 剂量率 = CPS x K(默认 0.0018, 待标定) ；累计剂量每秒累加(不清零)
 *   - OLED(SSD1306, PA1/PA2 IIC) 显示 RATE/CPS/ACC/THR/TIME/ALM
 *   - 按键(PA3~PA6)调阈值与累计时间
 *   - 超阈值声光报警(PA7 LED 闪烁 + （本版无蜂鸣器），越超越急促)
 *   - 每秒 USART1(PA9) 输出一行 JSON 给外挂 BLE 模块(protocol.md)
 *
 * 说明：剂量率换算系数 K 未标定，请用放射源标定后修改 dose.c 的 DOSE_K。
 */
#include "app.h"
#include "oled_ssd1306.h"
#include "keys.h"
#include "dose.h"
#include "alarm.h"
#include "ble_softuart.h"

volatile uint32_t g_sys_ms = 0u;

void SysTick_Handler(void)
{
    g_sys_ms++;
}

/* ---------- 简单格式化（无 printf） ---------- */
static void ap_char(char *b, uint16_t *p, char c) { b[(*p)++] = c; }

static void ap_str(char *b, uint16_t *p, const char *s)
{
    while (*s) b[(*p)++] = *s++;
}

static void ap_u32(char *b, uint16_t *p, uint32_t v)
{
    char t[12];
    uint8_t i = 0u, j = 0u;
    if (v == 0u) { ap_char(b, p, '0'); return; }
    while (v) { t[i++] = (char)('0' + (v % 10u)); v /= 10u; }
    while (i) ap_char(b, p, t[--i]);
}

static void ap_fixed3(char *b, uint16_t *p, float v)
{
    uint32_t m = (uint32_t)(v * 1000.0f + 0.5f);
    ap_u32(b, p, m / 1000u);
    ap_char(b, p, '.');
    ap_char(b, p, (char)('0' + (m / 100u) % 10u));
    ap_char(b, p, (char)('0' + (m / 10u) % 10u));
    ap_char(b, p, (char)('0' + (m % 10u)));
}

/* ---------- OLED 整屏绘制 ---------- */
static void oled_draw_all(void)
{
    char    line[24];
    uint16_t p;

    oled_clear_buf();

    p = 0u; ap_str(line, &p, "RATE "); ap_fixed3(line, &p, dose_rate());
            ap_str(line, &p, " uSv/h"); line[p] = 0; oled_text(0u, 0u, line);

    p = 0u; ap_str(line, &p, "CPS  "); ap_u32(line, &p, dose_cps());
            line[p] = 0; oled_text(1u, 0u, line);

    p = 0u; ap_str(line, &p, "ACC  "); ap_fixed3(line, &p, dose_acc());
            ap_str(line, &p, " uSv"); line[p] = 0; oled_text(2u, 0u, line);

    p = 0u; ap_str(line, &p, "THR  "); ap_fixed3(line, &p, dose_thr());
            ap_str(line, &p, " uSv/h"); line[p] = 0; oled_text(3u, 0u, line);

    p = 0u; ap_str(line, &p, "TIME "); ap_u32(line, &p, dose_acc_time());
            ap_str(line, &p, " MIN"); line[p] = 0; oled_text(4u, 0u, line);

    if (dose_alarm())
    {
        oled_text(6u, 0u, "!! ALARM !!");
    }
    else
    {
        oled_text(6u, 0u, "ALM OK");
    }

    oled_update();
}

/* ---------- 主函数 ---------- */
int main(void)
{
    uint32_t last_sec;
    float    thr_shown;
    uint16_t time_shown;

    SystemInit();   /* 启动文件已调用；此处再保险，确保 72MHz */

    if (SysTick_Config(SystemCoreClock / 1000u))   /* 1ms 中断 */
    {
        while (1) { }
    }

    oled_init();
    keys_init();
    alarm_init();
    dose_init();
    ble_softuart_init();

    last_sec  = g_sys_ms / 1000u;
    thr_shown = dose_thr();
    time_shown = dose_acc_time();

    oled_draw_all();

    for (;;)
    {
        uint32_t sec;

        keys_scan();

        sec = g_sys_ms / 1000u;
        if (sec != last_sec)
        {
            last_sec = sec;
            dose_tick_1s();      /* 读计数/平滑/剂量/报警判定 */
            alarm_refresh();     /* 刷新声光报警 */
            ble_softuart_report_1s();     /* 向手机发一行 JSON */
            oled_draw_all();
        }

        alarm_led_tick(g_sys_ms);   /* LED 闪烁（按报警强度） */

        /* 按键改变阈值/累计时间后立即刷新显示 */
        if ((dose_thr() != thr_shown) || (dose_acc_time() != time_shown))
        {
            thr_shown  = dose_thr();
            time_shown = dose_acc_time();
            oled_draw_all();
        }
    }
}
