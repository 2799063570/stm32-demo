#ifndef __OLED_CONFIG_H
#define __OLED_CONFIG_H

/*0.96寸128x64 SSD1306兼容SPI屏；LTP03A JY丝印尚未确认控制器型号*/
#define OLED_COLUMN_OFFSET  0      // 正常SSD1306为0；改变偏移不等于支持其他控制器
#define OLED_DATA_CHUNK     8      // 每次Service最多发送8字节显存
#define OLED_RESET_LOW_MS   5      // 非阻塞低电平复位
#define OLED_POWER_ON_MS    100    // 释放RES后非阻塞等待
#define OLED_RETRY_MS       1000   // SPI外设超时后重新初始化的间隔

#if OLED_DATA_CHUNK < 1 || OLED_DATA_CHUNK > 8
#error OLED_DATA_CHUNK must be 1..8
#endif
#if OLED_COLUMN_OFFSET < 0 || OLED_COLUMN_OFFSET > 4
#error OLED_COLUMN_OFFSET must be 0..4
#endif
#endif
