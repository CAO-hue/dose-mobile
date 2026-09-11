#ifndef __ALARM_H
#define __ALARM_H

/* 报警（v2 板）：仅 LED，PA7，低电平点亮（本版没有蜂鸣器）
 * 超阈值时 LED 闪烁，超出越多闪得越快。 */
void alarm_init(void);
void alarm_refresh(void);
void alarm_led_tick(uint32_t now_ms);

#endif /* __ALARM_H */
