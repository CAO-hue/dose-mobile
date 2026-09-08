#ifndef __KEYS_H
#define __KEYS_H

/* 四按键：低有效（按下接 GND）。默认映射（可改这里）：
 *   PA6 = 阈值+   PA7 = 阈值-   PB8 = 累计时间+   PB9 = 累计时间-
 */
void keys_init(void);   /* 初始化按键 GPIO */
void keys_scan(void);   /* 主循环周期性调用：消抖并触发调节动作 */

#endif /* __KEYS_H */
