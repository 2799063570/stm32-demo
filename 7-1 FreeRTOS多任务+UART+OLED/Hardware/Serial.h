#ifndef __SERIAL_H
#define __SERIAL_H
#include "stm32f10x.h"
#include "FreeRTOS.h"
#include "task.h"
/*单接收者、单发送者：这些任务API仅由App_SerialTask调用*/
void Serial_SetTask(TaskHandle_t Task);
void Serial_Init(void);
uint8_t Serial_ReadByte(uint8_t *Byte);
uint8_t Serial_SendArrayDMA(const uint8_t *Array, uint16_t Length);
void Serial_AbortTx(void);
uint8_t Serial_GetTxBusy(void);
uint32_t Serial_GetIdleCount(void);
uint32_t Serial_GetDropCount(void);
uint32_t Serial_GetErrorCount(void);
#endif
