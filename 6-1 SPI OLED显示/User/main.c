#include "stm32f10x.h"                  // Device header
#include "Delay.h"
#include "Serial.h"
#include "OLED.h"

int main(void)
{
	uint32_t LastDraw = 0, LastLog = 0;
	Delay_Init();
	Serial_Init();
	OLED_Init();
	Serial_SendString("0.96 SPI OLED: SCK=PB3 MOSI=PB5 CS=PB0 DC=PB1 RES=PB12\r\n");
	while (1)
	{
		uint32_t Now = Delay_GetTick();
		if ((uint32_t)(Now - LastDraw) >= 200)
		{
			int32_t Demo = (int32_t)((Now / 40) % 200) - 100;
			LastDraw = Now;
			if ((Now / 4000) % 2 == 0)
			{
				OLED_PrintLine(1, "SPI OLED 128x64");
				OLED_PrintLine(2, "SPI1 2.25MHz");
				OLED_PrintLine(3, "UP:%lus E:%lu", (unsigned long)(Now / 1000), (unsigned long)OLED_GetErrorCount());
				OLED_PrintLine(4, "DEMO");
				OLED_DrawBar(40, 52, 88, 11, Demo, 100);
			}
			else
			{
				uint8_t X;
				OLED_Clear();
				OLED_PrintLine(1, "GRAPH DEMO (sim)");
				OLED_DrawLine(0, 40, 127, 40, 1);
				for (X = 1; X < 128; X ++)
				{
					int16_t A = ((X - 1 + Now / 100) % 40), B = ((X + Now / 100) % 40);
					if (A > 20) A = 40 - A;
					if (B > 20) B = 40 - B;
					OLED_DrawLine(X - 1, 60 - A * 2, X, 60 - B * 2, 1);
				}
			}
		}
		OLED_Service();
		if ((uint32_t)(Now - LastLog) >= 1000)
		{
			LastLog = Now;
			Serial_Printf("OLED ready=%u SPI_error=%lu (no SPI ACK detection)\r\n", OLED_GetReady(), (unsigned long)OLED_GetErrorCount());
		}
	}
}
