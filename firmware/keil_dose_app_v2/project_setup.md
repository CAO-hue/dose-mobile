# Keil5 建工程说明（v2 · 改进版剂量仪）

> 适用：Keil MDK5 + STM32F103C8T6 + ST 标准外设库 V3.5
> 对应硬件：改进版（含 VG6328A 蓝牙模块、USB、无蜂鸣器）

## 1. 本版引脚映射（务必对照）
| 功能 | 引脚 | 说明 |
|---|---|---|
| 脉冲计数输入 | **PA0** | ASIC TRIG → R22 → SN74LV1T34 → PA0；用 TIM2_ETR 外部计数 |
| OLED SCL / SDA | **PA1 / PA2** | SSD1306，软件 IIC |
| 按键 ×4 | **PA4=阈值+ / PA3=阈值− / PA5=累计时间+ / PA6=累计时间−** | 低有效（按下接 GND） |
| 报警 LED | **PA7** | 低电平点亮（本版无蜂鸣器） |
| 蓝牙 VG6328A | **PB9=MCU TX→模块RXD；PB8=MCU RX←模块TXD** | 软件串口，115200 8N1 |
| SWD | PA13(PA13)/PA14 | 下载调试 |

## 2. 新建 Keil 工程
1. Project → New uVision Project → 选 **STM32F103C8**；
2. 添加启动文件 `startup_stm32f10x_md.s`（从标准库 Project 模板或
   `Libraries/CMSIS/Device/ST/STM32F10x/Startup/arm/` 复制）；
3. 添加库文件：
   ```
   system_stm32f10x.c
   stm32f10x_gpio.c
   stm32f10x_rcc.c
   stm32f10x_tim.c
   ```
   （本版**不用** USART 库，因为蓝牙走软件串口）
4. 添加本文件夹的 .c：`main.c / dose.c / keys.c / alarm.c / oled_ssd1306.c / ble_softuart.c`；
5. Options for Target：
   - Target：Xtal = 8 MHz
   - C/C++ → Define：`STM32F10X_MD,USE_STDPERIPH_DRIVER`
   - Include Paths：本文件夹 + 标准库的 `CMSIS\CM3\CoreSupport`、
     `CMSIS\DeviceSupport\ST\STM32F10x`、`STM32F10x_StdPeriph_Driver\inc`
   - Debug：ST-Link，Flash Download 勾选 Reset and Run

## 3. VG6328A 蓝牙模块配置（重要）
模块出厂默认：**115200 8N1**，BLE 名 `XLBLE`，服务 `0xFFE0`，写 `0xFFE1`，通知 `0xFFE2`。
建议用 USB-TTL 先配置（发 AT 指令，注意每条后面加回车换行 0x0D 0x0A）：
```
AT+ENAT       进入命令模式（返回 OK）
AT+SPOF       关闭经典蓝牙 SPP（只留 BLE，省电少干扰）
AT+LEON       确保 BLE 广播开启
AT+LENA DOSE  改 BLE 名称为 DOSE（可选）
AT+REST       复位生效
AT+EXAT       回到数据模式
```
- 若软件串口 115200 不稳定：发 `AT+BAUD1` 改为 9600，复位后把
  `ble_softuart.c` 里的 `BLE_BAUD` 改成 `9600u`，重新编译。
- 默认参数可用 `AT+RDEF` 恢复出厂。

## 4. 手机网页
网页已改为 VG6328A 的 UUID（服务 0xFFE0、通知 0xFFE2）：
https://cao-hue.github.io/dose-mobile/
手机 Chrome/Edge 打开 → 连接仪器 → 选择名为 `XLBLE`（或你改的名字）的设备。

## 5. 调试顺序
1. 先不接蓝牙：示波器确认 PA0 有脉冲（来自 ASIC TRIG）；
2. OLED 应显示 RATE/CPS/ACC/THR/TIME/ALM；按键可调阈值与累计时间；
3. 超阈值时 PA7 LED 闪烁（本版无声音报警）；
4. 接蓝牙：手机网页应实时收到每秒一行 JSON；
5. 若手机收不到数据：确认模块在数据模式（AT+EXAT）、BLE 广播开启、UUID 正确。

## 6. 常见问题
- **OLED 不亮/花屏**：确认 SCL=PA1、SDA=PA2，模块地址 0x3C；上下颠倒时改
  `oled_ssd1306.c` 初始化里 `0xA1↔0xA0`、`0xC8↔0xC0`；
- **蓝牙收不到**：先用 USB-TTL 直接接模块 RXD/TXD 验证 115200 能发 AT 并返回 OK；
- **剂量率不准**：改 `dose.c` 的 `DOSE_K`（待标定）。
