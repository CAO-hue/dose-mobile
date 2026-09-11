#ifndef __KEYS_H
#define __KEYS_H

/* 四按键（v2，低有效，按下接 GND）：
 *   PA4 = 阈值+   PA3 = 阈值-   PA5 = 累计时间+   PA6 = 累计时间-
 * 如需改按键对应，只改 keys.c 顶部的 PIN 宏。
 */
void keys_init(void);
void keys_scan(void);

#endif /* __KEYS_H */
