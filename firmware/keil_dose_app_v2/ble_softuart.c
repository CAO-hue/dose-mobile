/* ble_softuart.c：软件串口 TX（PB9）+ 每秒 JSON 上报
 *
 * VG6328A 默认：115200 8N1，BLE 服务 0xFFE0 / 写 0xFFE1 / 通知 0xFFE2
 * 时序：用 DWT->CYCCNT（72MHz）做精确位延时，1 bit = 72e6/115200 = 625 周期。
 * 每个字节发送期间关中断（约 87us），避免 SysTick 抖动导致误码。
 */
#include "app.h"
#include "ble_softuart.h"
#include "dose.h"

#define BLE_TX_PIN   GPIO_Pin_9     /* PB9 -> 模块 RXD */
#define BLE_RX_PIN   GPIO_Pin_8     /* PB8 <- 模块 TXD（本版只发不收） */
#define BLE_BAUD     115200u
#define CPU_HZ       72000000u
#define BIT_CYCLES   (CPU_HZ / BLE_BAUD)    /* 625 */

static void dwt_init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0u;
    DWT->CTRL  |= DWT_CTRL_CYCCNTENA_Msk;
}

static void delay_cycles(uint32_t c)
{
    uint32_t start = DWT->CYCCNT;
    while ((DWT->CYCCNT - start) < c) { }
}

void ble_softuart_init(void)
{
    GPIO_InitTypeDef gpio;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

    gpio.GPIO_Pin   = BLE_TX_PIN;
    gpio.GPIO_Mode  = GPIO_Mode_Out_PP;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &gpio);

    gpio.GPIO_Pin   = BLE_RX_PIN;
    gpio.GPIO_Mode  = GPIO_Mode_IPU;
    GPIO_Init(GPIOB, &gpio);

    GPIO_SetBits(GPIOB, BLE_TX_PIN);   /* 空闲高电平 */
    dwt_init();
}

static void tx_byte(uint8_t b)
{
    uint8_t i;

    __disable_irq();
    GPIO_ResetBits(GPIOB, BLE_TX_PIN);          /* 起始位，低 */
    delay_cycles(BIT_CYCLES);

    for (i = 0u; i < 8u; i++)
    {
        if (b & (1u << i)) GPIO_SetBits(GPIOB, BLE_TX_PIN);
        else               GPIO_ResetBits(GPIOB, BLE_TX_PIN);
        delay_cycles(BIT_CYCLES);
    }

    GPIO_SetBits(GPIOB, BLE_TX_PIN);            /* 停止位，高 */
    delay_cycles(BIT_CYCLES);
    __enable_irq();
}

static void tx_str(const char *s)
{
    while (*s) tx_byte((uint8_t)(*s++));
}

/* ---- 数字格式化（避免浮点 printf） ---- */
static uint8_t u32_str(uint32_t v, char *out)
{
    char t[12]; uint8_t i = 0u, j = 0u;
    if (v == 0u) { out[0]='0'; out[1]='\0'; return 1u; }
    while (v) { t[i++] = (char)('0' + (v % 10u)); v /= 10u; }
    while (i) out[j++] = t[--i];
    out[j] = '\0';
    return j;
}
static void pad3(uint32_t v, char *out)
{
    out[0] = (char)('0' + (v / 100u) % 10u);
    out[1] = (char)('0' + (v / 10u) % 10u);
    out[2] = (char)('0' + (v % 10u));
    out[3] = '\0';
}
static void app_f3(char *buf, uint16_t *pos, float v)
{
    uint32_t m = (uint32_t)(v * 1000.0f + 0.5f);
    char t[24];
    u32_str(m / 1000u, t); while (*t) buf[(*pos)++] = *t++;
    buf[(*pos)++] = '.';
    pad3(m % 1000u, t); while (*t) buf[(*pos)++] = *t++;
}
static void app_s(char *buf, uint16_t *pos, const char *s)
{ while (*s) buf[(*pos)++] = *s++; }

void ble_softuart_report_1s(void)
{
    char buf[192]; uint16_t p = 0u; char t[24];

    app_s(buf, &p, "{\"t\":\"d\",\"cps\":");
    u32_str(dose_cps(), t); app_s(buf, &p, t);

    app_s(buf, &p, ",\"rate\":"); app_f3(buf, &p, dose_rate());
    app_s(buf, &p, ",\"acc\":");  app_f3(buf, &p, dose_acc());
    app_s(buf, &p, ",\"thr\":");  app_f3(buf, &p, dose_thr());

    app_s(buf, &p, ",\"al\":");
    buf[p++] = (char)('0' + dose_alarm());
    app_s(buf, &p, "}\n");
    buf[p] = '\0';

    tx_str(buf);
}
