# STM32F103C8T6 进阶例程（中文注释版）

已按你提供的 `STM32Project-有注释版` 重写。每个例程都是**独立 Keil 工程**，沿用 `Start / Library / User / System / Hardware / Objects / Listings` 目录、`Serial_Init()` / `Encoder_Get()` 这类命名、中文函数说明、花括号另起一行和分段初始化注释。

## 五个工程

| 目录 | 实现内容 | 打开方式 |
|---|---|---|
| [1-1 UART+DMA+空闲中断](<1-1 UART+DMA+空闲中断/说明.md>) | USART1 循环 DMA 接收、HT/TC/IDLE 中断、软件 FIFO、DMA 发送回显 | 目录内 `Project.uvprojx` |
| [2-1 IIC+DMA+中断](<2-1 IIC+DMA+中断/说明.md>) | 硬件 I2C2、收发 DMA、EV/ER 中断状态机、MPU6050 连续读取 | 同上 |
| [3-1 CAN+中断](<3-1 CAN+中断/说明.md>) | CAN1 异步发送、TX/RX 中断、接收队列、内部回环/正常总线 | 同上 |
| [4-1 TIM输入捕获+编码器](<4-1 TIM输入捕获+编码器/说明.md>) | TIM2 输入捕获测周期、溢出扩展、TIM3 AB 相编码器读取 | 同上 |
| [5-1 定时器PID前馈速度闭环](<5-1 定时器PID前馈速度闭环/说明.md>) | 10ms 控制周期、编码器测速、PID+前馈、TB6612 PWM、串口目标 RPM | 同上 |

原先的九例程切换工程与 `App/BSP/Examples` 目录已移除。参考资料所在目录仅用于读取，未作修改。

## 统一环境

- **STM32F103C8T6**，8 MHz 外部晶振，系统 72 MHz；按官方 64 KiB Flash / 20 KiB SRAM 链接。
- **标准外设库 SPL V3.5.0**，没有使用 HAL；`Start`、`Library` 从你的工程模板复制，保留原版权说明。
- **Keil µVision + ARM Compiler 5**；本机使用 µVision 5.24、ARM Compiler 5.06 update 5。工程指定 `Keil.STM32F1xx_DFP.2.2.0`，其他设备包版本可在 Keil 中重新选择。
- 日志串口统一为 **USART1，PA9 TX、PA10 RX，115200 / 8N1 / 无流控**。第 2～4 项仅发送日志，不使用 PA10。
- 代码采用 UTF-8（新业务源码带 BOM，便于 Keil 显示中文注释）。

## 目录对应关系

```text
每个例程/
├─ Project.uvprojx             双击打开，直接编译本例程
├─ 说明.md                    接线、流程、验证、边界条件
├─ Start/                     芯片头文件、启动汇编、系统时钟、CMSIS
├─ Library/                   STM32标准外设库
├─ User/                      main.c、配置、中断公共入口、链接文件
├─ System/                    Delay、Timer、PID、Control等系统模块
├─ Hardware/                  Serial、IIC、CAN、IC、Encoder、PWM、Motor
├─ Objects/                   HEX/BIN/AXF和构建日志
└─ Listings/                  MAP和调用关系
```

外设中断处理函数放在对应模块 `.c` 文件末尾；`User/stm32f10x_it.c` 放 SysTick 和公共异常。多个中断优先级统一在 `Delay_Init()` 设置分组，避免各模块反复改分组。

## 使用步骤

1. 进入所需例程目录，先读 `说明.md`。
2. 打开 `Project.uvprojx`，确认编译器为 ARM Compiler 5，按 F7 编译。
3. 用 ST-Link/SWD 下载 `Objects/Project.hex`；BOOT0=0，复位运行。BIN 文件的烧录起始地址为 **0x08000000**。
4. 接 USB-TTL：PA9→RXD、PA10→TXD、GND 共地，串口助手设 115200 8N1。使用兼容 3.3 V 逻辑的接口。

每个工程的库和启动文件都在自身目录内，可单独复制到其他位置使用。完整构建应保留原目录里的 `说明.md` 与 `User/Config.h`（如有）。

## 第五项：先改参数，再做电机实验

按你确认的默认方案使用 **TB6612 单通道接口**。修改第五项 `User/Config.h` 中的编码器 PPR、减速比、反馈方向、最大目标 RPM 和控制增益。11 PPR、30:1 等数值只是示例，未经实际电机标定。

串口命令以 `\r` 或 `\n` 结束：

```text
CLEAR
SPEED 60
SPEED -60
SPEED 0
STOP
STATUS
```

速度单位为**输出轴 RPM**，只接受整数。运行时建议每 **500 ms** 重发目标；连续 **1000 ms** 没有有效 SPEED 命令会停机并锁定故障。启动不转，CLEAR 不会自动启动，SPEED 0 / STOP 立即使 TB6612 进入待机。调参流程和故障码见第五项说明。

## 构建与软件检查

```powershell
# 批量重建五个工程（可通过 -UV4 指定自己的Keil路径）
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\build_all.ps1

# 检查HEX校验和、Flash/RAM范围，以及ISR是否实际绑定到模块中的强符号
python .\tools\check_projects.py

# 电脑端回归：编译真实PID/Command/Control源文件，硬件接口采用显式桩
# 需要Visual Studio C工具链，可用 -VCVars 指定vcvars64.bat
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\test_logic.ps1
```

`tools/make_projects.py` 用于业务文件有增删时重建工程分组，不会生成或覆盖业务 C 源码。

五个工程均已完成编译和固件结构检查；PID、命令与控制逻辑已通过电脑端回归。**尚未进行实板烧录、通信波形验证或实际电机调参。** 具体结果见 [验证记录](docs/验证记录.md)。

## 资料与来源

- 本机风格参考：`D:\files\嵌入式\STM32入门教程资料\程序源码\STM32Project-有注释版`。业务驱动为本次重写，未将原教程等待超时后仍继续事务的处理方式照搬。
- [RM0008 参考手册](https://www.st.com/resource/en/reference_manual/rm0008-stm32f101xx-stm32f102xx-stm32f103xx-stm32f105xx-and-stm32f107xx-advanced-armbased-32bit-mcus-stmicroelectronics.pdf)：DMA映射、I²C DMA末字节、CAN、定时器。
- [ES096 芯片勘误](https://www.st.com/resource/en/errata_sheet/es096-stm32f101x8b-stm32f102x8b-and-stm32f103x8b-mediumdensity-device-limitations-stmicroelectronics.pdf)：I²C BUSY锁定等限制。
- [MPU6050 寄存器说明](https://invensense.tdk.com/wp-content/uploads/2015/02/MPU-6000-Register-Map1.pdf)：电源管理和连续数据寄存器。
- [TB6612FNG 数据手册](https://toshiba.semicon-storage.com/info/datasheet_en_20141001.pdf?did=10660)：方向、PWM和STBY控制。
- `Start`、`Library` 保留所复制厂商文件的版权/使用说明。`tools/TargetOption.xml` 仅复用已验证的 Keil 工程选项结构，最终路径全部指向各例程自身目录。
