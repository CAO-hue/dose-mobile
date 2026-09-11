#ifndef __DOSE_H
#define __DOSE_H

/* 剂量核心：脉冲计数(TIM2/PA0) -> CPS(平滑) -> 剂量率/累计剂量/报警判定
 * 注意：剂量率 = CPS × K，K 尚未标定（默认 0.0018），请用放射源标定后修改。 */

void    dose_init(void);            /* 初始化 TIM1 外部计数与默认参数 */
void    dose_tick_1s(void);         /* 每秒调用一次：读数、平滑、算剂量、判报警 */

uint32_t dose_cps(void);            /* 平滑后计数率 */
float   dose_rate(void);            /* 剂量率 µSv/h */
float   dose_acc(void);             /* 累计剂量 µSv（持续累计） */
float   dose_thr(void);             /* 报警阈值 µSv/h */
uint16_t dose_acc_time(void);       /* 累计时间（显示参考，分钟） */
uint8_t dose_alarm(void);           /* 0/1 是否超阈值 */

void    dose_thr_inc(void);         /* 阈值 + */
void    dose_thr_dec(void);         /* 阈值 - */
void    dose_acc_time_inc(void);    /* 累计时间 + */
void    dose_acc_time_dec(void);    /* 累计时间 - */

#endif /* __DOSE_H */
