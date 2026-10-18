#ifndef __SERIAL_H
#define __SERIAL_H
#include "stm32f10x.h"
void led_init(void);
void Serial_Init(void);
void Serial_SendByte(uint8_t Byte);
void Serial_SendString(const char *String);
void Serial_Printf(const char *Format, ...);
uint8_t Serial_ReadByte(uint8_t *Byte);
uint8_t Serial_SendArrayDMA(const uint8_t *Array, uint16_t Length);
uint8_t Serial_GetTxBusy(void);
uint32_t Serial_GetIdleCount(void);
uint32_t Serial_GetDropCount(void);
uint32_t Serial_GetErrorCount(void);
#endif
