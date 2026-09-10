# 便携式个人辐射剂量仪 · 安卓手机实时剂量显示（BLE 外挂模块）

> 保存时间：2026-09-07 ｜ 论文：毕业论文_杨清波.pdf（小尺寸便携式个人辐射剂量仪，STM32F103C8T6 + CdZnTe）

## 一、目标（已确认）
把论文剂量仪（计数 → CPS / 剂量率 / 累计剂量 / 报警）的实时数据，通过**外挂 BLE 透传模块**发送到**安卓手机网页**显示。
v1 范围：**剂量功能实时显示**；不做能谱、不做远程控制、不做历史记录。

## 二、架构
```
STM32 空闲USART1 ──▶ BLE 透传模块(NUS) ──无线──▶ 安卓 Chrome/Edge 网页(Web Bluetooth)
  每秒发一行 JSON                               解析并实时显示
```

## 三、当前进度（网站部分已完成 ✅）
- [x] 手机网页 `index.html`（单文件、无依赖、中文界面）
  - 大数字剂量率（µSv/h，标注“未标定仅供参考”）
  - 计数率 CPS / 累计剂量 / 报警阈值卡片
  - 超阈值报警强化：整页红闪 + 大数字发光 + 横幅放大 + 手机震动
  - 计数率趋势图（近 3 分钟，带纵坐标刻度 CPS）
  - 演示模式（无硬件可预览）；连接/断开按钮；错误提示
- [x] 部署：GitHub Pages
  - 仓库：https://github.com/CAO-hue/dose-mobile
  - 网页：https://cao-hue.github.io/dose-mobile/
- [x] 数据协议文档（`protocol.md`，网页已按其实现）
- [x] STM32 固件上报示例（`firmware/stm32_uart_report.c`）
- [x] 外挂 BLE 接线与配置说明（`hardware_ble_wiring.md`，依据实际原理图）
- [x] 外挂 BLE 飞线接线记录（`外挂BLE接线记录.md`，主原理图不改动）
- [x] **整机完整固件 v1**（`firmware/keil_dose_app/`：计数/剂量/OLED/按键/报警/BLE 上报 + 建工程说明；待你在 Keil 编译与真机验证）

## 四、网页数据协议（与未来固件约定）
- GATT：NUS（Nordic UART Service）
  - 服务 `6e400001-b5a3-f393-e0a9-e50e24dcca9e`
  - Notify 特征 `6e400003-b5a3-f393-e0a9-e50e24dcca9e`（设备 → 手机）
- 每秒 1 行 JSON，`\n` 结尾：
  `{"t":"d","cps":123,"rate":0.12,"acc":0.34,"thr":0.50,"al":0}`
  - `cps` 每秒计数率（必填）；`rate` 剂量率 µSv/h；`acc` 累计剂量 µSv；`thr` 报警阈值 µSv/h；`al` 0/1 报警
- 网页按 `\n` 重组整行再解析，坏行跳过；单位常量在 `index.html` 顶部可改。

## 五、本地文件
- `D:\工程文件\dose-mobile\index.html` —— 网页（git 仓库，remote: GitHub）
- `D:\工程文件\dose-mobile\README.md` —— 项目进度说明
- `D:\工程文件\dose-mobile\protocol.md` —— 数据协议文档（固件↔网页约定）
- `D:\工程文件\dose-mobile\hardware_ble_wiring.md` —— 外挂 BLE 接线/配置说明
- `D:\工程文件\dose-mobile\外挂BLE接线记录.md` —— 飞线样机接线记录（主原理图不改动）
- `D:\工程文件\dose-mobileirmware\stm32_uart_report.c` —— STM32 每秒上报示例
- `D:\工程文件\dose-mobileirmware\keil_dose_app\` —— 整机完整固件 v1（main + oled/keys/dose/alarm/ble_report + project_setup.md 建工程说明）
- `D:\工程文件\work\` —— 过程文件（论文提取文本 paper_text.txt、gh 工具与登录配置 gh-config、调试脚本等，非交付物）

## 六、如何更新网页
改完 `index.html` 后提交并推送，GitHub Pages 约 1 分钟自动生效：
```powershell
cd D:\工程文件\dose-mobile
git add index.html
git commit -m "说明"
git push origin main
```
（推送凭据：本机 gh 已授权，配置在 `D:\工程文件\work\gh-config`；网络受限时可用本机代理 127.0.0.1:7890，或直连。）

## 七、待办（按已确认计划）
1. 安装 Keil MDK5 + STM32F1 DFP + ST 标准外设库 V3.5（见 `firmware/keil_dose_app/project_setup.md`）
2. 按 `project_setup.md` 新建工程并编译 `keil_dose_app`（0 error），ST-Link 烧录真机验证
3. 模块到手：按 `hardware_ble_wiring.md` 接线 → 手机网页端到端联调（网页已支持）
4. 硬件小型化（下一版 PCB 集成，后续阶段）

## 八、安全提醒
GitHub 登录密码曾在对话中出现，已视为泄露，**请尽快改密并开启两步验证**。当前部署用的是设备码授权（gh），密码未被使用。
