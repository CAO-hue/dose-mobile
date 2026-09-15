#ifndef __MCA_H
#define __MCA_H

/* 能谱(MCA)：NEWSTER2 峰保输出 PDHOUT -> PB0(ADC1_IN8)
 *   RSTPDH -> PB10 (1=复位, 0=PDH工作)
 *   TRIG   -> PB11 (EXTI 上升沿)
 * 死时间用实测双计数法：D = 1 - processed / trig_total
 *   trig_total 来自 TIM2/PA0 硬件计数（每个 TRIG 都会计数，不受软件忙闲影响）
 *   processed  为本模块实际读出并复位的事件数
 * 同时用忙时间法交叉校验：D_busy = busy_us / total_us
 */

#define MCA_NCH          1024u   /* 内部道数 */
#define MCA_SEND_BINS     256u   /* 上传道数（4 道合并 1 道） */
#define MCA_CHUNK_BINS     32u   /* 每帧发送道数（8 帧发完 256 道） */
#define MCA_UPLOAD_MS    5000u   /* 上传周期 ms */

void    mca_init(void);
void    mca_tick_1s(void);        /* 每秒累计 N_TRIG（读 TIM2 计数差值） */
uint8_t mca_upload_due(void);     /* 到上传周期返回 1 */
void    mca_upload(void);         /* 发送 256 道谱 + 实测死时间指标 */
uint8_t mca_is_busy(void);

#endif /* __MCA_H */
