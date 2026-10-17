#ifndef __IIC_H
#define __IIC_H
#include "stm32f10x.h"
typedef enum
{
	IIC_OK = 0, IIC_BUSY, IIC_NACK, IIC_BUS_ERROR, IIC_DMA_ERROR, IIC_TIMEOUT, IIC_ARGUMENT
} IIC_ResultTypeDef;
void IIC_Init(void);
IIC_ResultTypeDef IIC_WriteRegsDMA(uint8_t Address, uint8_t Reg, const uint8_t *Data, uint16_t Length);
IIC_ResultTypeDef IIC_ReadRegsDMA(uint8_t Address, uint8_t Reg, uint8_t *Data, uint16_t Length);
void IIC_Service(void);
uint8_t IIC_GetResult(IIC_ResultTypeDef *Result);
#endif
