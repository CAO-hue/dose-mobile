#ifndef __APP_H
#define __APP_H

/* 公共头：包含 ST 库主头，并提供毫秒时基（SysTick）全局变量 */
#include "stm32f10x.h"

extern volatile uint32_t g_sys_ms;   /* 1ms 递增，由 main.c 的 SysTick_Handler 维护 */

#endif /* __APP_H */
