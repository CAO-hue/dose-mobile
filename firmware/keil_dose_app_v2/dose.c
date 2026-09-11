/* dose.c：脉冲计数 / 平滑 / 剂量计算 / 阈值与累计时间调节 / 报警判定
 *
 * 硬件：TIM2 外部时钟模式2，ETR=PA0 输入辐射脉冲（上升沿计数，自由运行 16 位）。
 *       dose_tick_1s() 每秒读一次计数差值。
 *
 * 剂量：rate(uSv/h) = CPS * K；acc(uSv) += rate/3600，每秒累加，持续累计不清零。
 *       累计时间仅作显示参考（默认 60 分钟，可调 1~999 分钟）。
 */
#include "app.h"
#include "dose.h"

/* ---- 可调参数（标定/改默认值只需改这里） ---- */
#define DOSE_K           0.0018f    /* 换算系数：uSv/h per CPS —— 待标定！ */
#define SMOOTH_N         8u         /* 平滑窗口（秒） */
#define THR_DEFAULT      0.50f      /* 默认报警阈值 uSv/h */
#define THR_STEP         0.05f      /* 阈值步进 */
#define THR_MIN          0.05f
#define THR_MAX          10.0f
#define ACC_TIME_DEFAULT 60u        /* 默认累计时间（分钟，显示参考） */
#define ACC_TIME_MIN     1u
#define ACC_TIME_MAX     999u

static volatile uint32_t g_cps_raw;      /* 上一秒原始计数 */
static uint32_t ring[SMOOTH_N];
static uint8_t  ring_idx;
static uint8_t  ring_cnt;

static float    g_rate;          /* uSv/h */
static float    g_acc;           /* uSv */
static float    g_thr = THR_DEFAULT;
static uint16_t g_acc_time = ACC_TIME_DEFAULT;
static uint8_t  g_alarm;

/* TIM2 外部脉冲计数：ETR(PA0) 上升沿计数 */
void dose_init(void)
{
    TIM_TimeBaseInitTypeDef tb;
    GPIO_InitTypeDef        gpio;
    uint32_t i;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);

    /* PA0 输入（上拉） */
    gpio.GPIO_Pin   = GPIO_Pin_0;
    gpio.GPIO_Mode  = GPIO_Mode_IPU;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &gpio);

    /* TIM2 时基：ARR=0xFFFF 自由计数，PSC=0 */
    tb.TIM_Period        = 0xFFFF;
    tb.TIM_Prescaler     = 0;
    tb.TIM_ClockDivision = TIM_CKD_DIV1;
    tb.TIM_CounterMode   = TIM_CounterMode_Up;
    tb.TIM_RepetitionCounter = 0;
    TIM_TimeBaseInit(TIM2, &tb);

    /* ETR 外部时钟模式2：每个上升沿 CNT+1 */
    TIM_ETRClockMode2Config(TIM2, TIM_ExtTRGPSC_OFF,
                            TIM_ExtTRGPolarity_NonInverted, 0x0F);
    TIM_SetCounter(TIM2, 0);
    TIM_Cmd(TIM2, ENABLE);

    /* 初始状态 */
    for (i = 0; i < SMOOTH_N; i++) ring[i] = 0u;
    ring_idx = 0u;
    ring_cnt = 0u;
    g_cps_raw = 0u;
    g_rate = 0.0f;
    g_acc  = 0.0f;
    g_alarm = 0u;
}

/* 每秒：读计数 -> 平滑 -> 剂量 -> 报警 */
void dose_tick_1s(void)
{
    uint32_t now = TIM_GetCounter(TIM2);
    uint32_t delta = (uint16_t)(now - g_cps_raw); /* 16 位回绕安全 */
    uint32_t sum = 0u;
    uint8_t  i;

    g_cps_raw = now;

    ring[ring_idx] = delta;
    ring_idx = (uint8_t)((ring_idx + 1u) % SMOOTH_N);
    if (ring_cnt < SMOOTH_N) ring_cnt++;

    for (i = 0; i < ring_cnt; i++) sum += ring[i];
    /* 平滑后 CPS（窗口内平均，四舍五入） */
    g_rate = ((float)sum / (float)ring_cnt) * DOSE_K;
    g_acc += g_rate / 3600.0f;         /* uSv */
    g_alarm = (g_rate >= g_thr) ? 1u : 0u;
}

uint32_t dose_cps(void)
{
    /* 由 g_rate 反推平滑 CPS 的整数显示值 */
    return (uint32_t)(g_rate / DOSE_K + 0.5f);
}

float dose_rate(void)   { return g_rate; }
float dose_acc(void)    { return g_acc; }
float dose_thr(void)    { return g_thr; }
uint16_t dose_acc_time(void){ return g_acc_time; }
uint8_t dose_alarm(void){ return g_alarm; }

void dose_thr_inc(void)
{
    g_thr += THR_STEP;
    if (g_thr > THR_MAX) g_thr = THR_MAX;
}

void dose_thr_dec(void)
{
    g_thr -= THR_STEP;
    if (g_thr < THR_MIN) g_thr = THR_MIN;
}

void dose_acc_time_inc(void)
{
    if (g_acc_time < ACC_TIME_MAX) g_acc_time++;
}

void dose_acc_time_dec(void)
{
    if (g_acc_time > ACC_TIME_MIN) g_acc_time--;
}
