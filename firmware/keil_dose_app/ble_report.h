#ifndef __BLE_REPORT_H
#define __BLE_REPORT_H

/* BLE 上报：USART1(PA9=TX, PA10=RX, 115200 8N1)
 * 每秒输出一行 JSON（格式见 protocol.md），由外挂 NUS 透传模块转发到手机。
 * 无模块时不影响运行（只是 TX 空发）。 */

void ble_init(void);        /* 初始化 USART1 */
void ble_report_1s(void);   /* 每秒调用：发送一条剂量 JSON */

#endif /* __BLE_REPORT_H */
