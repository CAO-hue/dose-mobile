#ifndef __BLE_SOFTUART_H
#define __BLE_SOFTUART_H

/* 软件串口（VG6328A 蓝牙透传模块）：
 *   PB9 = MCU TX  -> 模块 RXD
 *   PB8 = MCU RX  <- 模块 TXD
 *   115200 8N1（模块默认波特率）
 * 每秒发送一行 JSON（协议见 protocol.md）。
 * 注意：PB8/PB9 不是 STM32 硬件串口，这里用 GPIO 位翻转 + DWT 周期计时实现。
 */
void ble_softuart_init(void);
void ble_softuart_report_1s(void);

#endif /* __BLE_SOFTUART_H */
