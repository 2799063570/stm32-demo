#include "stm32f10x.h"                  // Device header
#include "Delay.h"
#include "Serial.h"
#include "OLED.h"

int main(void)
{
	uint8_t RxData[128];
	uint16_t Length;
	uint32_t LastDisplay = 0, RxBytes = 0, TxBytes = 0;
	/*模块初始化*/
	Delay_Init();
	Serial_Init();
	OLED_Init();
	led_init();
	GPIO_WriteBit(GPIOC, GPIO_Pin_13, 1);
	Serial_SendString("USART1: DMA circular RX + HT/TC + IDLE, DMA TX echo\r\n");
	while (1)
	{
		/*DMA发送空闲时，取出已收到的数据并通过DMA回显*/
		if (!Serial_GetTxBusy())
		{
			Length = 0;
			while (Length < sizeof(RxData) && Serial_ReadByte(&RxData[Length])) Length ++;
			if (Length)
			{
				RxBytes += Length;
				if (Serial_SendArrayDMA(RxData, Length)) TxBytes += Length;
			}
		}
		/*状态改由OLED显示，避免周期日志占满回显串口的发送带宽*/
		if ((uint32_t)(Delay_GetTick() - LastDisplay) >= 200)
		{
			LastDisplay = Delay_GetTick();
			if (Serial_GetErrorCount()) OLED_PrintLine(1, "ERR:%lu", (unsigned long)Serial_GetErrorCount());
			else OLED_PrintLine(1, "UART DMA+IDLE");
			OLED_PrintLine(2, "RX:%lu", (unsigned long)RxBytes);
			OLED_PrintLine(3, "TX:%lu", (unsigned long)TxBytes);
			OLED_PrintLine(4, "I%lu D%lu", (unsigned long)(Serial_GetIdleCount() % 100000),
				(unsigned long)(Serial_GetDropCount() % 100000));
		}
		OLED_Service();                             // 一次最多8字节SPI，不在ISR中刷新
	}
}
