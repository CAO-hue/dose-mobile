/* alarm.c：声光报警 -> v2 只有光报警（PA7 LED，低电平点亮）
 */
#include "app.h"
#include "alarm.h"
#include "dose.h"

#define LED_PIN        GPIO_Pin_7     /* PA7 -> R5 -> LED D2 -> +3.3V，低电平点亮 */
#define BLINK_MAX_MS   500u
#define BLINK_MIN_MS   150u

static uint8_t  alarming;
static uint16_t blink_ms;

void alarm_init(void)
{
    GPIO_InitTypeDef gpio;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    gpio.GPIO_Pin   = LED_PIN;
    gpio.GPIO_Mode  = GPIO_Mode_Out_PP;
    gpio.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(GPIOA, &gpio);
    GPIO_SetBits(GPIOA, LED_PIN);      /* 灭 */

    alarming = 0u;
    blink_ms = BLINK_MAX_MS;
}

void alarm_refresh(void)
{
    float rate  = dose_rate();
    float thr   = dose_thr();
    float ratio = 0.0f;

    if (dose_alarm())
    {
        if (thr > 0.0f)
        {
            ratio = (rate - thr) / thr;
            if (ratio < 0.0f) ratio = 0.0f;
            if (ratio > 1.0f) ratio = 1.0f;
        }
        blink_ms = (uint16_t)(BLINK_MAX_MS -
                    (uint32_t)((BLINK_MAX_MS - BLINK_MIN_MS) * ratio));
        alarming = 1u;
    }
    else
    {
        GPIO_SetBits(GPIOA, LED_PIN);  /* 灭 */
        alarming = 0u;
        blink_ms = BLINK_MAX_MS;
    }
}

void alarm_led_tick(uint32_t now_ms)
{
    static uint32_t last = 0u;
    static uint8_t  on   = 0u;

    if (!alarming)
    {
        GPIO_SetBits(GPIOA, LED_PIN);
        last = 0u;
        on   = 0u;
        return;
    }
    if ((now_ms - last) >= blink_ms)
    {
        last = now_ms;
        on ^= 1u;
        if (on) GPIO_ResetBits(GPIOA, LED_PIN);   /* 亮 */
        else    GPIO_SetBits(GPIOA, LED_PIN);     /* 灭 */
    }
}
