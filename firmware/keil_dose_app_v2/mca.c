/* mca.c：能谱直方图 + 实测死时间（双计数法）
 *
 * 事件时序（NEWSTAR2 规格书）：
 *   TRIG 上升沿 -> 等 >=5us(峰保稳定) -> 读 ADC(PDHOUT) -> 落道
 *   -> RSTPDH=1(复位) -> 延时 -> RSTPDH=0(工作)
 *
 * 死时间（全部实测，不用估算值）：
 *   trig_total：TIM2/PA0 对 TRIG 的硬件计数（每 1 秒累加差值）
 *   processed ：本模块成功读出并复位的事件数
 *   D_counts  = 1 - processed/trig_total
 *   D_busy    = busy_cycles / (total_ms * 72000)   （交叉校验）
 */
#include "app.h"
#include "mca.h"
#include "ble_softuart.h"

#define MCA_ADC_CH        ADC_Channel_8     /* PB0 = ADC1_IN8 */
#define MCA_RST_PIN       GPIO_Pin_10       /* RSTPDH */
#define MCA_TRIG_PIN      GPIO_Pin_11       /* TRIG (EXTI) */

#define MCA_SETTLE_US     5u                /* 规格书：TRIG 后 >=5us 读取 */
#define MCA_RESET_US      1u                /* RSTPDH 复位脉宽（实测可调） */
#define CPU_MHZ           72u
#define CYCLES_PER_US     (CPU_MHZ)

/* 能谱能量区间：30 ~ 1600 keV（覆盖 241Am 59.5 / 137Cs 662 / 60Co 1332 / 40K 1461）
 * 换算：1fC≈28.7keV，增益 25mV/fC，PDHOUT 基线 1.258V，ADC=3.3V/4096
 *   30 keV  -> 1.284V -> ADC 1594
 *   1600 keV-> 2.652V -> ADC 3292
 * （标称值；实际能量刻度用标准源标定后可再修正） */
#define MCA_ADC_BASE      1594u   /* 30 keV */
#define MCA_ADC_FULL      3292u   /* 1600 keV */

/* DWT 周期计数器（旧版 CMSIS 无定义，直接访问寄存器） */
#define DWT_CTRL_REG      (*(volatile uint32_t *)0xE0001000u)
#define DWT_CYCCNT_REG    (*(volatile uint32_t *)0xE0001004u)
#define DEMCR_REG         (*(volatile uint32_t *)0xE000EDFCu)
#define TRCENA_BIT        (1u << 24)
#define CYCCNTENA_BIT     (1u << 0)

static uint32_t hist[MCA_NCH];              /* 4KB RAM */

static volatile uint8_t  busy;
static volatile uint32_t trig_total;        /* N_TRIG：入射(触发)事件数 */
static volatile uint32_t processed;         /* N_processed：成功读出并复位的事件 */
static volatile uint32_t binned;            /* 落入有效道的事件 */
static volatile uint32_t outrange;          /* 幅度超出映射范围的事件 */
static volatile uint32_t pileup;            /* 忙期间到达、被丢弃的事件 */
static volatile unsigned long long busy_cycles;  /* 忙时间（CPU 周期） */

static uint16_t last_cnt;
static uint32_t t0_ms;
static uint32_t last_upload_ms;

static void dwt_init(void)
{
    DEMCR_REG |= TRCENA_BIT;
    DWT_CYCCNT_REG = 0u;
    DWT_CTRL_REG |= CYCCNTENA_BIT;
}
static void delay_us(uint32_t us)
{
    uint32_t start = DWT_CYCCNT_REG;
    uint32_t need  = us * CYCLES_PER_US;
    while ((DWT_CYCCNT_REG - start) < need) { }
}
static uint16_t adc_read(void)
{
    ADC_SoftwareStartConvCmd(ADC1, ENABLE);
    while (ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC) == RESET) { }
    return ADC_GetConversionValue(ADC1);
}

void mca_init(void)
{
    GPIO_InitTypeDef gpio;
    ADC_InitTypeDef  adc;
    EXTI_InitTypeDef exti;
    NVIC_InitTypeDef nvic;
    uint32_t i;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB | RCC_APB2Periph_AFIO | RCC_APB2Periph_ADC1, ENABLE);
    RCC_ADCCLKConfig(RCC_PCLK2_Div6);            /* ADCCLK = 72/6 = 12MHz */

    /* PB0 = ADC 输入 */
    gpio.GPIO_Pin  = GPIO_Pin_0;
    gpio.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_Init(GPIOB, &gpio);

    /* PB10 = RSTPDH，默认 0 = PDH 工作 */
    gpio.GPIO_Pin   = MCA_RST_PIN;
    gpio.GPIO_Mode  = GPIO_Mode_Out_PP;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &gpio);
    GPIO_ResetBits(GPIOB, MCA_RST_PIN);

    /* PB11 = TRIG 输入（下拉，上升沿） */
    gpio.GPIO_Pin  = MCA_TRIG_PIN;
    gpio.GPIO_Mode = GPIO_Mode_IPD;
    GPIO_Init(GPIOB, &gpio);

    GPIO_EXTILineConfig(GPIO_PortSourceGPIOB, GPIO_PinSource11);
    exti.EXTI_Line    = EXTI_Line11;
    exti.EXTI_Mode    = EXTI_Mode_Interrupt;
    exti.EXTI_Trigger = EXTI_Trigger_Rising;
    exti.EXTI_LineCmd = ENABLE;
    EXTI_Init(&exti);

    nvic.NVIC_IRQChannel = EXTI15_10_IRQn;
    nvic.NVIC_IRQChannelPreemptionPriority = 0;
    nvic.NVIC_IRQChannelSubPriority = 0;
    nvic.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&nvic);

    /* ADC1 配置（单次转换） */
    ADC_DeInit(ADC1);
    adc.ADC_Mode               = ADC_Mode_Independent;
    adc.ADC_ScanConvMode       = DISABLE;
    adc.ADC_ContinuousConvMode = DISABLE;
    adc.ADC_ExternalTrigConv   = ADC_ExternalTrigConv_None;
    adc.ADC_DataAlign          = ADC_DataAlign_Right;
    adc.ADC_NbrOfChannel       = 1;
    ADC_Init(ADC1, &adc);
    ADC_RegularChannelConfig(ADC1, MCA_ADC_CH, 1, ADC_SampleTime_55Cycles5);
    ADC_Cmd(ADC1, ENABLE);
    ADC_ResetCalibration(ADC1);
    while (ADC_GetResetCalibrationStatus(ADC1)) { }
    ADC_StartCalibration(ADC1);
    while (ADC_GetCalibrationStatus(ADC1)) { }

    for (i = 0; i < MCA_NCH; i++) hist[i] = 0u;
    busy = 0u;
    trig_total = processed = binned = outrange = pileup = 0u;
    busy_cycles = 0ull;
    dwt_init();
    last_cnt = TIM_GetCounter(TIM2);
    t0_ms = g_sys_ms;
    last_upload_ms = g_sys_ms;
}

/* TRIG 中断：读一个事件的峰值并落道 */
void EXTI15_10_IRQHandler(void)
{
    if (EXTI_GetITStatus(EXTI_Line11) != RESET)
    {
        EXTI_ClearITPendingBit(EXTI_Line11);

        if (busy)
        {
            pileup++;                 /* 忙期间到达：计入堆积，丢失该事件 */
            return;
        }
        busy = 1u;
        {
            uint32_t c0 = DWT_CYCCNT_REG;
            uint16_t v;
            uint32_t ch;

            delay_us(MCA_SETTLE_US);
            v = adc_read();
            processed++;

            if (v > MCA_ADC_BASE)
            {
                ch = ((uint32_t)(v - MCA_ADC_BASE) * MCA_NCH) / (MCA_ADC_FULL - MCA_ADC_BASE);
                if (ch < MCA_NCH) { hist[ch]++; binned++; }
                else              { outrange++; }   /* > 1600 keV */
            }
            else
            {
                outrange++;                          /* < 30 keV，低于区间下限，不落道 */
            }

            GPIO_SetBits(GPIOB, MCA_RST_PIN);      /* RSTPDH = 1 复位 */
            delay_us(MCA_RESET_US);
            GPIO_ResetBits(GPIOB, MCA_RST_PIN);    /* RSTPDH = 0 工作 */

            busy = 0u;
            busy_cycles += (unsigned long long)(DWT_CYCCNT_REG - c0);
        }
    }
}

/* 每秒累加 N_TRIG（TIM2/PA0 硬件计数，不受软件忙闲影响） */
void mca_tick_1s(void)
{
    uint16_t now = TIM_GetCounter(TIM2);
    trig_total += (uint16_t)(now - last_cnt);
    last_cnt = now;
}

uint8_t mca_upload_due(void)
{
    if ((g_sys_ms - last_upload_ms) >= MCA_UPLOAD_MS)
    {
        last_upload_ms = g_sys_ms;
        return 1u;
    }
    return 0u;
}

uint8_t mca_is_busy(void)
{
    return busy;
}

static char *ap_s(char *p, const char *s) { while (*s) *p++ = *s++; return p; }
static char *ap_u32(char *p, uint32_t v)
{
    char t[12];
    uint8_t i = 0u;
    if (v == 0u) { *p++ = '0'; return p; }
    while (v) { t[i++] = (char)('0' + (v % 10u)); v /= 10u; }
    while (i) *p++ = t[--i];
    return p;
}

/* 发送 256 道（4 道合并 1 道）+ 实测死时间指标 */
void mca_upload(void)
{
    char buf[512];
    uint8_t k, j;
    uint32_t total_ms = g_sys_ms - t0_ms;
    uint32_t busy_us  = (uint32_t)(busy_cycles / (unsigned long long)CYCLES_PER_US);

    for (k = 0u; k < (MCA_SEND_BINS / MCA_CHUNK_BINS); k++)
    {
        char *p = buf;
        p = ap_s(p, "{\"t\":\"s\",\"n\":256,\"i\":");
        p = ap_u32(p, k);
        p = ap_s(p, ",\"ch\":[");
        for (j = 0u; j < MCA_CHUNK_BINS; j++)
        {
            uint32_t idx = (uint32_t)k * MCA_CHUNK_BINS + j;
            uint32_t sum = hist[idx * 4u] + hist[idx * 4u + 1u]
                         + hist[idx * 4u + 2u] + hist[idx * 4u + 3u];
            p = ap_u32(p, sum);
            if (j + 1u < MCA_CHUNK_BINS) *p++ = ',';
        }
        p = ap_s(p, "]}\n");
        *p = '\0';
        ble_softuart_send(buf);
    }

    {
        char *p = buf;
        p = ap_s(p, "{\"t\":\"m\",\"trig\":");   p = ap_u32(p, trig_total);
        p = ap_s(p, ",\"proc\":");                  p = ap_u32(p, processed);
        p = ap_s(p, ",\"bin\":");                   p = ap_u32(p, binned);
        p = ap_s(p, ",\"oor\":");                   p = ap_u32(p, outrange);
        p = ap_s(p, ",\"pile\":");                  p = ap_u32(p, pileup);
        p = ap_s(p, ",\"busy_us\":");               p = ap_u32(p, busy_us);
        p = ap_s(p, ",\"total_ms\":");              p = ap_u32(p, total_ms);
        p = ap_s(p, "}\n");
        *p = '\0';
        ble_softuart_send(buf);
    }
}
