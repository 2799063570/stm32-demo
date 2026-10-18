#include "stm32f10x.h"                  // Device header
#include "Delay.h"
#include "Serial.h"
#include "IC.h"
#include "Encoder.h"
#include "OLED.h"

int main(void)
{
	uint32_t LastSample = 0, PeriodUs;
	/*模块初始化：TIM2负责PA0输入捕获，TIM3负责PA6/PA7编码器，二者同时工作*/
	Delay_Init();
	Serial_Init();
	IC_Init();
	Encoder_Init();
	OLED_Init();
	Serial_SendString("TIM2 CH1 PA0 capture + TIM3 PA6/PA7 encoder\r\n");
	while (1)
	{
		if ((uint32_t)(Delay_GetTick() - LastSample) >= 100)
		{
			int16_t Delta = Encoder_Get();
			LastSample = Delay_GetTick();
			if (IC_GetPeriod(&PeriodUs))
			{
				uint32_t MilliHz = 1000000000U / PeriodUs;
				Serial_Printf("PERIOD=%lu us FREQ=%lu Hz ", (unsigned long)PeriodUs, (unsigned long)(1000000 / PeriodUs));
				OLED_PrintLine(1, "F:%lu.%03luHz", (unsigned long)(MilliHz / 1000), (unsigned long)(MilliHz % 1000));
				OLED_PrintLine(2, "P:%luus", (unsigned long)PeriodUs);
			}
			else
			{
				Serial_SendString("CAPTURE=NO_SIGNAL ");
				OLED_PrintLine(1, "F:NO SIGNAL");
				OLED_PrintLine(2, "P:---");
			}
			Serial_Printf("ENC_CNT=%u DELTA=%d IC_OVERRUN=%lu\r\n", Encoder_GetCounter(), Delta, (unsigned long)IC_GetOverrunCount());
			OLED_PrintLine(3, "CNT:%5u", Encoder_GetCounter());
			OLED_PrintLine(4, "D:%6d O:%lu", Delta, (unsigned long)(IC_GetOverrunCount() % 1000));
		}
		OLED_Service();
	}
}
