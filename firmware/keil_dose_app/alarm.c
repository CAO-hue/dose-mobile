/* alarm.c：报警执行（LED 闪烁 + 蜂鸣器 PWM）
 * 报警强度随超出程度变化：LED 闪得越快、蜂鸣器音调越高越急促。
 */
#include "app.h"
#include "alarm.h"
#include "dose.h"

#define LED_PIN   GPIO_Pin_5    /* PB5，低电平点亮 */
#define BZ_PIN    GPIO_Pin_0    /* PB0，TIM3_CH3 */

#define BZ_FREQ_BASE   2700u
#define BZ_FREQ_MAX    6500u
#define BLINK_MAX_MS   500u
#define BLINK_MIN_MS   150u

static uint8_t alarming;
static uint16_t blink_ms = BLINK_MAX_MS;

static void buzzer_pin_af(void)
{
    GPIO_InitTypeDef gpio;
    gpio.GPIO_Pin   = BZ_PIN;
    gpio.GPIO_Mode  = GPIO_Mode_AF_PP;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &gpio);
}

static void buzzer_pin_low(void)
{
    GPIO_InitTypeDef gpio;
    gpio.GPIO_Pin   = BZ_PIN;
    gpio.GPIO_Mode  = GPIO_Mode_Out_PP;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &gpio);
    GPIO_ResetBits(GPIOB, BZ_PIN);   /* 强制低电平，蜂鸣器静音 */
}

static void buzzer_set_freq(uint16_t freq_hz)
{
    uint32_t arr;
    if (freq_hz < 1100u) freq_hz = 1100u;   /* ARR 需 ≤ 65535 */
    arr = (SystemCoreClock / freq_hz) - 1u;
    if (arr > 0xFFFFu) arr = 0xFFFFu;
    TIM_SetAutoreload(TIM3, (uint16_t)arr);
    TIM_SetCompare3(TIM3, (uint16_t)(arr / 2u));  /* 50% 占空比 */
}

void alarm_init(void)
{
    GPIO_InitTypeDef        gpio;
    TIM_TimeBaseInitTypeDef tb;
    TIM_OCInitTypeDef       oc;

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB | RCC_APB2Periph_AFIO, ENABLE);

    /* PB5 LED：推挽输出，初始高电平(灭) */
    gpio.GPIO_Pin   = LED_PIN;
    gpio.GPIO_Mode  = GPIO_Mode_Out_PP;
    gpio.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(GPIOB, &gpio);
    GPIO_SetBits(GPIOB, LED_PIN);

    /* TIM3 时基（PWM 频率在报警时再设置） */
    tb.TIM_Period        = 0xFFFF;
    tb.TIM_Prescaler     = 0;
    tb.TIM_ClockDivision = TIM_CKD_DIV1;
    tb.TIM_CounterMode   = TIM_CounterMode_Up;
    tb.TIM_RepetitionCounter = 0;
    TIM_TimeBaseInit(TIM3, &tb);

    /* CH3 PWM1 */
    oc.TIM_OCMode      = TIM_OCMode_PWM1;
    oc.TIM_OutputState = TIM_OutputState_Enable;
    oc.TIM_Pulse       = 0;
    oc.TIM_OCPolarity  = TIM_OCPolarity_High;
    TIM_OC3Init(TIM3, &oc);
    TIM_OC3PreloadConfig(TIM3, TIM_OCPreload_Enable);
    TIM_ARRPreloadConfig(TIM3, ENABLE);

    TIM_Cmd(TIM3, DISABLE);
    buzzer_pin_low();

    alarming = 0u;
    blink_ms = BLINK_MAX_MS;
}

void alarm_refresh(void)
{
    float rate = dose_rate();
    float thr  = dose_thr();
    float ratio = 0.0f;
    uint32_t freq;

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
        freq = BZ_FREQ_BASE + (uint32_t)((BZ_FREQ_MAX - BZ_FREQ_BASE) * ratio);

        buzzer_pin_af();
        buzzer_set_freq((uint16_t)freq);
        TIM_SetCounter(TIM3, 0);
        TIM_Cmd(TIM3, ENABLE);
        alarming = 1u;
    }
    else
    {
        TIM_Cmd(TIM3, DISABLE);
        TIM_SetCompare3(TIM3, 0);
        buzzer_pin_low();
        GPIO_SetBits(GPIOB, LED_PIN);   /* LED 灭 */
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
        GPIO_SetBits(GPIOB, LED_PIN);
        last = 0u;
        on   = 0u;
        return;
    }

    if ((now_ms - last) >= blink_ms)
    {
        last = now_ms;
        on ^= 1u;
        if (on) GPIO_ResetBits(GPIOB, LED_PIN);   /* 亮（低电平有效） */
        else    GPIO_SetBits(GPIOB, LED_PIN);     /* 灭 */
    }
}
