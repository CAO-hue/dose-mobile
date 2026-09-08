#ifndef __ALARM_H
#define __ALARM_H

/* 声光报警：
 *  PB5 = 报警 LED（低电平亮），超阈值时按与超出程度成反比的周期闪烁
 *  PB0 = 无源蜂鸣器（TIM3_CH3 PWM），超阈值时鸣响，超出越多频率越高越急促 */

void alarm_init(void);                 /* 初始化 LED GPIO 与 TIM3 PWM */
void alarm_refresh(void);              /* 每秒(剂量更新后)调用：按剂量率刷新报警状态 */
void alarm_led_tick(uint32_t now_ms);  /* 主循环周期性调用：驱动 LED 闪烁 */

#endif /* __ALARM_H */
