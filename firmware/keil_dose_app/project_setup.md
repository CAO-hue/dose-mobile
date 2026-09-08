# Keil5 建工程说明（keil_dose_app 源码包）

> 适用：Keil MDK5 + STM32F103C8T6 + ST 标准外设库 V3.5（STM32F10x_StdPeriph）
> 本包**不含 ST 库与启动文件**，请按下述步骤添加。

## 1. 准备
- 安装 Keil MDK5，并在 Pack Installer 安装 **Keil.STM32F1xx_DFP**
- 解压 **STM32F10x 标准外设库 V3.5**（如 `STM32F10x_StdPeriph_Lib_V3.5.0`），记下其路径，下面用 `$(LIB)` 表示

## 2. 新建工程
1. Project → New uVision Project → 选一个目录命名（如 `dose.uvprojx`）
2. 器件选择：**STMicroelectronics → STM32F1xx → STM32F103C8**
3. 提示添加启动文件：选 **否**（我们手动加）

## 3. 添加源文件到工程（按分组）
```
keil_dose_app/          本包源码（main.c、oled_ssd1306.c、keys.c、dose.c、alarm.c、ble_report.c）
$(LIB)/Libraries/CMSIS/Device/ST/STM32F10x/Startup/arm/startup_stm32f10x_md.s   (若无此路径，从库内 Project 模板里复制 startup_stm32f10x_md.s)
$(LIB)/Libraries/CMSIS/Device/ST/STM32F10x/Source/system_stm32f10x.c
$(LIB)/Libraries/STM32F10x_StdPeriph_Driver/src/stm32f10x_gpio.c
$(LIB)/Libraries/STM32F10x_StdPeriph_Driver/src/stm32f10x_rcc.c
$(LIB)/Libraries/STM32F10x_StdPeriph_Driver/src/stm32f10x_tim.c
$(LIB)/Libraries/STM32F10x_StdPeriph_Driver/src/stm32f10x_usart.c
```
> stm32f10x_conf.h 本包已自带（只启用 gpio/rcc/tim/usart，assert 置空）。

## 4. 工程选项（Options for Target）
- **Target**：Xtal(MHz) = `8`；勾选 Use MicroLIB（可选，本代码未用 printf 浮点）
- **C/C++**：
  - Define：`STM32F10X_MD,USE_STDPERIPH_DRIVER`
  - Include Paths 至少包含：
    - 本包目录 `...\keil_dose_app`
    - `$(LIB)\Libraries\CMSIS\CM3\CoreSupport`
    - `$(LIB)\Libraries\CMSIS\DeviceSupport\ST\STM32F10x`（若存在）
    - `$(LIB)\Libraries\STM32F10x_StdPeriph_Driver\inc`
  - 语言：C99（可选）
- **Debug**：选择 ST-Link Debugger → Settings 确认能识别目标 → Flash Download 勾选 Reset and Run
- 若用 AC6 编译 StdPeriph 有告警，可切到 AC5（若你的 MDK 带）或忽略警告

## 5. 编译烧录
- 编译应 **0 Error**；用 ST-Link 下载后复位运行。
- 预期：OLED 上电显示 RATE/CPS/ACC/THR/TIME/ALM；无脉冲时 CPS≈0。

## 6. 联调顺序
1. **无蓝牙先看串口**：USB-TTL 接 PA9(TX)→模块RX/或直接接 USB-TTL RX，115200，应每秒收到：
   `{"t":"d","cps":0,"rate":0.000,"acc":0.000,"thr":0.500,"al":0}`
2. 接外挂 BLE 模块（接线见 `../hardware_ble_wiring.md`）：nRF Connect 应看到 Notify；
3. 手机打开 https://cao-hue.github.io/dose-mobile/ → 连接 → 实时显示。

## 7. 常见问题
- **OLED 上下颠倒/镜像**：改 `oled_ssd1306.c` 初始化中 `0xA1↔0xA0`、`0xC8↔0xC0`
- **剂量率不准**：改 `dose.c` 的 `DOSE_K`（用放射源标定）
- **按键方向反**：改 `keys.c` 顶部 PIN 宏
- **暂时不接 BLE**：无需改动，TX 空发不影响；若想关掉，删 main 里 `ble_init()/ble_report_1s()` 调用并移除 usart 库文件
