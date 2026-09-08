/* ble_report.c：USART1 每秒上报一条 JSON（protocol.md 格式）
 * 为避开 Keil 浮点 printf 依赖，所有数值手工格式化为整数+3 位小数。
 */
#include "app.h"
#include "ble_report.h"
#include "dose.h"

#define BLE_BAUD 115200u

static void uart1_send_char(char c)
{
    USART_SendData(USART1, (uint8_t)c);
    while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET)
    {
    }
}

static void uart1_send_str(const char *s)
{
    while (*s) uart1_send_char(*s++);
}

void ble_init(void)
{
    GPIO_InitTypeDef  gpio;
    USART_InitTypeDef usart;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_USART1, ENABLE);

    /* PA9 = TX：复用推挽 */
    gpio.GPIO_Pin   = GPIO_Pin_9;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    gpio.GPIO_Mode  = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOA, &gpio);

    /* PA10 = RX：浮空输入 */
    gpio.GPIO_Pin   = GPIO_Pin_10;
    gpio.GPIO_Mode  = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &gpio);

    usart.USART_BaudRate            = BLE_BAUD;
    usart.USART_WordLength          = USART_WordLength_8b;
    usart.USART_StopBits            = USART_StopBits_1;
    usart.USART_Parity              = USART_Parity_No;
    usart.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    usart.USART_Mode                = USART_Mode_RX | USART_Mode_TX;
    USART_Init(USART1, &usart);
    USART_Cmd(USART1, ENABLE);
}

/* ---- 数字格式化（无浮点 printf） ---- */

/* 将无符号整数写入 out（十进制，NUL 结尾），返回长度 */
static uint8_t u32_to_str(uint32_t v, char *out)
{
    char tmp[12];
    uint8_t i = 0u, j = 0u;

    if (v == 0u) { out[0] = '0'; out[1] = '\0'; return 1u; }
    while (v) { tmp[i++] = (char)('0' + (v % 10u)); v /= 10u; }
    while (i)  { out[j++] = tmp[--i]; }
    out[j] = '\0';
    return j;
}

/* 将 0~999 写为 3 位（不足补零） */
static void u32_pad3(uint32_t v, char *out)
{
    out[0] = (char)('0' + (v / 100u) % 10u);
    out[1] = (char)('0' + (v / 10u) % 10u);
    out[2] = (char)('0' + (v % 10u));
    out[3] = '\0';
}

/* 把 float(>=0) 按 "整数.3位小数" 追加到 buf */
static void append_fixed3(char *buf, uint16_t *pos, float v)
{
    uint32_t milli = (uint32_t)(v * 1000.0f + 0.5f);
    char tmp[24];

    u32_to_str(milli / 1000u, tmp);
    while (*tmp) buf[(*pos)++] = *tmp++;

    buf[(*pos)++] = '.';
    u32_pad3(milli % 1000u, tmp);
    while (*tmp) buf[(*pos)++] = *tmp++;
}

static void append_str(char *buf, uint16_t *pos, const char *s)
{
    while (*s) buf[(*pos)++] = *s++;
}

void ble_report_1s(void)
{
    char    buf[192];
    uint16_t p = 0u;
    char    tmp[24];

    append_str(buf, &p, "{\"t\":\"d\",\"cps\":");
    u32_to_str(dose_cps(), tmp);
    append_str(buf, &p, tmp);

    append_str(buf, &p, ",\"rate\":");
    append_fixed3(buf, &p, dose_rate());

    append_str(buf, &p, ",\"acc\":");
    append_fixed3(buf, &p, dose_acc());

    append_str(buf, &p, ",\"thr\":");
    append_fixed3(buf, &p, dose_thr());

    append_str(buf, &p, ",\"al\":");
    buf[p++] = (char)('0' + dose_alarm());

    append_str(buf, &p, "}\n");
    buf[p] = '\0';

    uart1_send_str(buf);
}
