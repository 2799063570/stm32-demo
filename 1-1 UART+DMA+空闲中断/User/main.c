#include "stm32f10x.h"                  // Device header
#include "Delay.h"
#include "Serial.h"

int main(void)
{
	uint8_t RxData[128];
	uint16_t Length;
	uint32_t LastLog = 0;
	/*模块初始化*/
	Delay_Init();
	Serial_Init();
	Serial_SendString("USART1: DMA circular RX + HT/TC + IDLE, DMA TX echo\r\n");
	while (1)
	{
		/*DMA发送空闲时，取出已收到的数据并通过DMA回显*/
		if (!Serial_GetTxBusy())
		{
			Length = 0;
			while (Length < sizeof(RxData) && Serial_ReadByte(&RxData[Length])) Length ++;
			if (Length) Serial_SendArrayDMA(RxData, Length);
		}
		/*只在主循环打印，空闲事件用于及时收尾，不作为业务协议唯一分包依据*/
		if ((uint32_t)(Delay_GetTick() - LastLog) >= 1000)
		{
			LastLog = Delay_GetTick();
			Serial_Printf("\r\nIDLE=%lu DROP=%lu ERROR=%lu\r\n",
				(unsigned long)Serial_GetIdleCount(), (unsigned long)Serial_GetDropCount(),
				(unsigned long)Serial_GetErrorCount());
		}
	}
}
