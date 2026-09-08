/**
 * stm32_uart_report.c
 * 外挂 BLE 透传模块 · STM32F103C8T6 数据上报示例（配合 protocol.md）
 *
 * 作用：初始化 USART1(PA9=TX, PA10=RX, 115200 8N1)，每秒把测量数据
 *       按 protocol.md 的 JSON 行格式发给蓝牙透传模块。
 *
 * 接入方法（不改动现有计数/OLED/按键/报警逻辑）：
 *   1) 在系统初始化处调用  USART1_Init();
 *   2) 在你工程每秒产生一次新数据的地方（例如 1s 定时/主循环里每秒一次）
 *      调用  dose_report_1s();
 *
 * 变量映射：本文件用到 g_cps/g_rate/g_acc/g_thr/g_alarm 五个示例变量，
 *           请改成你工程里实际用于 OLED 显示和报警判定的变量；
 *           若某变量不存在（例如只完成了计数），删掉对应字段即可，
 *           网页会显示 "--"（cps 必须保留）。
 *
 * 注意：JSON 里浮点 %.3f 需要开启浮点 printf（Keil 勾选 Use MicroLIB，
 *       或采用完整 printf 库），否则串口输出会不正确。
 */
#include "stm32f10x.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_usart.h"
#include <stdio.h>

/* ========== 示例变量（TODO：改为你工程中的实际变量） ========== */
extern volatile uint32_t g_cps;   /* 每秒计数率（个/秒） */
extern float           g_rate;    /* 剂量率 µSv/h（当前显示值，未标定） */
extern float           g_acc;     /* 累计剂量 µSv */
extern float           g_thr;     /* 报警阈值 µSv/h */
extern uint8_t         g_alarm;   /* 报警状态：0 正常 / 1 超阈值 */

/* ========== USART1 初始化 ========== */
void USART1_Init(void)
{
    GPIO_InitTypeDef  GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_USART1, ENABLE);

    /* PA9  = USART1_TX：复用推挽输出 */
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_9;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    /* PA10 = USART1_RX：浮空输入 */
    GPIO_InitStructure.GPIO_Pin  = GPIO_Pin_10;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    USART_InitStructure.USART_BaudRate            = 115200;
    USART_InitStructure.USART_WordLength          = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits            = USART_StopBits_1;
    USART_InitStructure.USART_Parity              = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode                = USART_Mode_RX | USART_Mode_TX;
    USART_Init(USART1, &USART_InitStructure);

    USART_Cmd(USART1, ENABLE);
}

/* ========== 串口发送一个字符串（阻塞方式） ========== */
static void uart1_send_str(const char *s)
{
    while (*s)
    {
        USART_SendData(USART1, (uint8_t)(*s++));
        while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET)
        {
        }
    }
}

/* ========== 每秒上报一条 JSON（与 protocol.md 一致） ========== */
void dose_report_1s(void)
{
    char buf[192];
    int  n;

    n = snprintf(buf, sizeof(buf),
                 "{\"t\":\"d\",\"cps\":%lu,\"rate\":%.3f,\"acc\":%.3f,\"thr\":%.3f,\"al\":%u}\r\n",
                 (unsigned long)g_cps,
                 (double)g_rate,
                 (double)g_acc,
                 (double)g_thr,
                 (unsigned int)g_alarm);

    if (n > 0)
    {
        if (n >= (int)sizeof(buf))
        {
            n = (int)sizeof(buf) - 1; /* 防止越界，截断后仍以 \n 结尾即可 */
        }
        uart1_send_str(buf);
    }
}

/* 说明：
 * - 行尾用 \r\n 或 \n 均可，网页按 \n 分行；
 * - 若暂时只有计数率（论文实测阶段），可只发：
 *     snprintf(buf, sizeof(buf), "{\"t\":\"d\",\"cps\":%lu}\r\n", (unsigned long)g_cps);
 */
