#include "stm32f10x.h"
#include "Delay.h"
#include "Joystick.h"
#include "JogKeys.h"
#include "Serial.h"
#include "OLED.h"
#include "JogConfig.h"
#include "JogRemote.h"
#include "JogView.h"

int main(void)
{
	static Jog_Remote Remote;
	static Remote_ParserTypeDef Parser;
	Remote_FrameTypeDef Frame, Reply;
	uint8_t ADCReady, Pressed, Byte, Buffer[REMOTE_MAX_FRAME];
	uint16_t RawX = 2048, RawY = 2048, Budget;
	uint32_t LastSample, LastDraw, LastRetry, Errors, Drops;
	Delay_Init(); OLED_Init(); Serial_Init();
	ADCReady = Joystick_Init(); JogKeys_Init();
	Jog_Init(&Remote); Remote_ParserInit(&Parser);
	Errors = Serial_GetErrorCount(); Drops = Serial_GetDropCount();
	LastSample = LastDraw = LastRetry = Delay_GetTick();
	Jog_View(&Remote, LastDraw);
	while (1)
	{
		uint32_t Now = Delay_GetTick();
		Jog_Tick(&Remote, Now);
		if (!ADCReady && (uint32_t)(Now - LastRetry) >= 1000)
		{
			LastRetry = Now; ADCReady = Joystick_Init();
		}
		if ((uint32_t)(Now - LastSample) >= JOG_SAMPLE_MS)
		{
			uint8_t Valid;
			LastSample = Now;
			Valid = ADCReady && Joystick_Read(&RawX, &RawY, &Pressed);
			if (!Valid) ADCReady = 0;
			Jog_Feed(&Remote, RawX, RawY, JogKeys_Read(), Valid, Delay_GetTick());
		}
		if (Errors != Serial_GetErrorCount() || Drops != Serial_GetDropCount())
		{
			Errors = Serial_GetErrorCount(); Drops = Serial_GetDropCount();
			Remote_ParserInit(&Parser); Jog_Disconnect(&Remote);
			for (Budget = 0; Budget < 512 && Serial_ReadByte(&Byte); Budget ++);
		}
		for (Budget = 0; Budget < 128 && Serial_ReadByte(&Byte); Budget ++)
		{
			if (Remote_ParseByte(&Parser, Byte, &Frame) && Jog_OnFrame(&Remote, &Frame, Delay_GetTick(), &Reply))
			{
				/* DMA内部复制。不等待、不积压旧INPUT；忙/错误时撤销本地授权，交由PC超时换会话。 */
				uint8_t Length = Remote_Encode(&Reply, Buffer);
				Jog_TxResult(&Remote, Length && Serial_SendArrayDMA(Buffer, Length));
			}
		}
		Now = Delay_GetTick();
		if ((uint32_t)(Now - LastDraw) >= JOG_DRAW_MS)
		{
			LastDraw = Now; Jog_View(&Remote, Now);
		}
		OLED_Service();
	}
}
