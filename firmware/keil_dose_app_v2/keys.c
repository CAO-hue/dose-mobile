/* keys.c：四按键扫描与消抖（v2 板：PA3/PA4/PA5/PA6，低有效）
 */
#include "app.h"
#include "keys.h"
#include "dose.h"

#define KEY_DEBOUNCE_MS   20u

#define KEY_THR_UP_PIN    GPIO_Pin_4   /* PA4 阈值+ */
#define KEY_THR_DN_PIN    GPIO_Pin_3   /* PA3 阈值- */
#define KEY_TIME_UP_PIN   GPIO_Pin_5   /* PA5 累计时间+ */
#define KEY_TIME_DN_PIN   GPIO_Pin_6   /* PA6 累计时间- */

typedef struct {
    uint16_t pin;
    uint32_t down_since;
    uint8_t  reported;
} key_t;

static key_t keys[4];

void keys_init(void)
{
    GPIO_InitTypeDef gpio;
    uint8_t i;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);

    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    gpio.GPIO_Mode  = GPIO_Mode_IPU;      /* 上拉输入，按下为低 */

    keys[0].pin = KEY_THR_UP_PIN;
    keys[1].pin = KEY_THR_DN_PIN;
    keys[2].pin = KEY_TIME_UP_PIN;
    keys[3].pin = KEY_TIME_DN_PIN;

    for (i = 0; i < 4u; i++)
    {
        gpio.GPIO_Pin = keys[i].pin;
        GPIO_Init(GPIOA, &gpio);
    }
}

static uint8_t key_down(const key_t *k)
{
    return GPIO_ReadInputDataBit(GPIOA, k->pin) == Bit_RESET;
}

void keys_scan(void)
{
    uint32_t now = g_sys_ms;
    uint8_t i;

    for (i = 0; i < 4u; i++)
    {
        key_t *k = &keys[i];

        if (key_down(k))
        {
            if (k->down_since == 0u) k->down_since = now;
            if ((!k->reported) && ((now - k->down_since) >= KEY_DEBOUNCE_MS))
            {
                k->reported = 1u;
                switch (i)
                {
                    case 0: dose_thr_inc();      break;   /* PA4 阈值+ */
                    case 1: dose_thr_dec();      break;   /* PA3 阈值- */
                    case 2: dose_acc_time_inc(); break;   /* PA5 时间+ */
                    default:dose_acc_time_dec(); break;   /* PA6 时间- */
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
