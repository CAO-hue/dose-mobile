/* keys.c：四按键扫描与消抖
 * 按键低有效（按下时引脚被拉低）。
 * 功能：PA6 阈值+；PA7 阈值-；PB8 累计时间+；PB9 累计时间-。
 */
#include "app.h"
#include "keys.h"
#include "dose.h"

#define KEY_DEBOUNCE_MS   20u   /* 消抖时间 */

#define KEY_THR_UP_PIN    GPIO_Pin_6   /* PA6 */
#define KEY_THR_DN_PIN    GPIO_Pin_7   /* PA7 */
#define KEY_TIME_UP_PIN   GPIO_Pin_8   /* PB8 */
#define KEY_TIME_DN_PIN   GPIO_Pin_9   /* PB9 */

typedef struct {
    GPIO_TypeDef *port;
    uint16_t      pin;
    uint32_t      down_since;  /* 开始按下的时刻 */
    uint8_t       reported;    /* 该次按下是否已触发 */
} key_t;

static key_t keys[4];

void keys_init(void)
{
    GPIO_InitTypeDef gpio;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB, ENABLE);

    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    gpio.GPIO_Mode  = GPIO_Mode_IPU;   /* 上拉输入，按下接 GND -> 读到低电平 */

    gpio.GPIO_Pin = KEY_THR_UP_PIN;
    GPIO_Init(GPIOA, &gpio);
    gpio.GPIO_Pin = KEY_THR_DN_PIN;
    GPIO_Init(GPIOA, &gpio);

    gpio.GPIO_Pin = KEY_TIME_UP_PIN;
    GPIO_Init(GPIOB, &gpio);
    gpio.GPIO_Pin = KEY_TIME_DN_PIN;
    GPIO_Init(GPIOB, &gpio);

    keys[0].port = GPIOA; keys[0].pin = KEY_THR_UP_PIN;
    keys[1].port = GPIOA; keys[1].pin = KEY_THR_DN_PIN;
    keys[2].port = GPIOB; keys[2].pin = KEY_TIME_UP_PIN;
    keys[3].port = GPIOB; keys[3].pin = KEY_TIME_DN_PIN;
}

static uint8_t key_is_down(const key_t *k)
{
    return GPIO_ReadInputDataBit(k->port, k->pin) == Bit_RESET;
}

void keys_scan(void)
{
    uint32_t now = g_sys_ms;
    uint32_t i;

    for (i = 0; i < 4; i++)
    {
        key_t *k = &keys[i];

        if (key_is_down(k))
        {
            if (k->down_since == 0u)
            {
                k->down_since = now;   /* 开始按下计时 */
            }
            if ((!k->reported) && ((now - k->down_since) >= KEY_DEBOUNCE_MS))
            {
                k->reported = 1;
                switch (i)
                {
                    case 0: dose_thr_inc();       break;   /* 阈值 + */
                    case 1: dose_thr_dec();       break;   /* 阈值 - */
                    case 2: dose_acc_time_inc();  break;   /* 累计时间 + */
                    default: dose_acc_time_dec(); break;   /* 累计时间 - */
                }
            }
        }
        else
        {
            k->down_since = 0u;
            k->reported   = 0u;
        }
    }
}
