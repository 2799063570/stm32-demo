#ifndef __IIC_H
#define __IIC_H
#include "stm32f10x.h"

typedef enum
{
	IIC_OK = 0,    /* 启动成功 / 事务完成成功，取决于调用的 API */
	IIC_BUSY,      /* 驱动忙、总线忙或旧结果尚未消费 */
	IIC_NACK,      /* 从机未应答 */
	IIC_BUS_ERROR, /* 总线错误或仲裁丢失等 */
	IIC_DMA_ERROR, /* DMA 传输错误 */
	IIC_TIMEOUT,   /* 事务或 STOP 收尾超时 */
	IIC_ARGUMENT   /* 地址、长度或缓冲区无效 */
} IIC_ResultTypeDef;

/** PB8/PB9 重映射到 I2C1，100kHz；DMA1 CH6/CH7 用于 TX/RX。 */
void IIC_Init(void);
/** 7 位 Address 无需左移；Length=1..32，启动时复制 Data；IIC_OK 仅表示启动成功。 */
IIC_ResultTypeDef IIC_WriteRegsDMA(uint8_t Address, uint8_t Reg, const uint8_t *Data, uint16_t Length);
/** Length=2..32；Data 在结果完成前必须有效；同一时刻只允许一笔事务。 */
IIC_ResultTypeDef IIC_ReadRegsDMA(uint8_t Address, uint8_t Reg, uint8_t *Data, uint16_t Length);
/** main 持续调用：检查 50ms 事务超时和 STOP 释放，不阻塞等待。 */
void IIC_Service(void);
/** main 消费一次结果：返回 1=取得完成结果，0=尚无结果；未消费时不能启动新事务。 */
uint8_t IIC_GetResult(IIC_ResultTypeDef *Result);
#endif
