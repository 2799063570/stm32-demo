#include "stm32f10x.h"                  // Device header
#include "Delay.h"
#include "Serial.h"
#include "IC.h"
#include "Encoder.h"

int main(void)
{
	uint32_t LastSample = 0, PeriodUs;
	/*模块初始化：TIM2负责PA0输入捕获，TIM3负责PA6/PA7编码器，二者同时工作*/
	Delay_Init();
	Serial_Init();
	IC_Init();
	Encoder_Init();
	Serial_SendString("TIM2 CH1 PA0 capture + TIM3 PA6/PA7 encoder\r\n");
	while (1)
	{
		if ((uint32_t)(Delay_GetTick() - LastSample) >= 100)
		{
			int16_t Delta = Encoder_Get();
			LastSample = Delay_GetTick();
			if (IC_GetPeriod(&PeriodUs))
				Serial_Printf("PERIOD=%lu us FREQ=%lu Hz ", (unsigned long)PeriodUs, (unsigned long)(1000000 / PeriodUs));
			else Serial_SendString("CAPTURE=NO_SIGNAL ");
			Serial_Printf("ENC_CNT=%u DELTA=%d IC_OVERRUN=%lu\r\n", Encoder_GetCounter(), Delta, (unsigned long)IC_GetOverrunCount());
		}
	}
}
