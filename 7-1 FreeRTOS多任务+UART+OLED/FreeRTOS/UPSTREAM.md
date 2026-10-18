# FreeRTOS 内核来源

- 官方仓库：https://github.com/FreeRTOS/FreeRTOS-Kernel
- 固定版本：V11.2.0（2025-03-04 发布），并非运行时下载依赖。
- 版本说明：https://github.com/FreeRTOS/FreeRTOS-Kernel/releases/tag/V11.2.0
- 官方归档：https://github.com/FreeRTOS/FreeRTOS-Kernel/releases/download/V11.2.0/FreeRTOS-KernelV11.2.0.zip
- 官方公布且已校验的归档 SHA256：`f7899b278a1cdf1f972f7bcc1767fb4ae144f7e7e5917a11076467ce071aafc7`。
- 许可证：同目录 `LICENSE.md`，MIT；原文件的版权和许可证头保留。

仅提取本例程需要的 `tasks.c / queue.c / list.c`、`include/*.h`、`portable/RVDS/ARM_CM3/port.c / portmacro.h` 和 `portable/MemMang/heap_4.c`。这些官方源码未修改，包括原始换行；`upstream-sha256.json` 记录从已校验归档提取的逐文件哈希，`tools/check_projects.py` 会复核。

应用配置在 `User/FreeRTOSConfig.h`。内核使用原生 FreeRTOS API，没有 HAL、CMSIS-RTOS 或 CubeMX 依赖。工程内已包含全部构建所需源码，可单独复制、离线编译。
