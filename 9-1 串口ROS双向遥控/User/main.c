#include "stm32f10x.h"
#include "Delay.h"
#include "Joystick.h"
#include "Serial.h"
#include "OLED.h"
#include "RemoteConfig.h"
#include "RemoteView.h"

int main(void)
{
	static Remote_InputTypeDef Input;
	static Remote_LinkTypeDef Link;
	static Remote_ParserTypeDef Parser;
	Remote_FrameTypeDef Frame, Reply;
	uint8_t ADCReady, Pressed, Byte, Buffer[REMOTE_MAX_FRAME];
	uint16_t RawX, RawY, Budget;
	uint32_t LastSample, LastDraw, Errors, Drops;
	Delay_Init(); OLED_Init(); Serial_Init();
	ADCReady = Joystick_Init();
	Remote_InputInit(&Input); Remote_LinkInit(&Link); Remote_ParserInit(&Parser);
	Errors = Serial_GetErrorCount(); Drops = Serial_GetDropCount();
	LastSample = LastDraw = Delay_GetTick();
	Remote_View(&Input, &Link, LastDraw);
	while (1)
	{
		uint32_t Now = Delay_GetTick();
		if ((uint32_t)(Now - LastSample) >= REMOTE_SAMPLE_MS)
		{
			LastSample = Now;
			if (ADCReady && Joystick_Read(&RawX, &RawY, &Pressed)) Remote_InputFeed(&Input, RawX, RawY, Pressed);
			else Remote_InputInit(&Input);             // ADC失效立即清除旧方向，恢复后重新校准
		}
		if (Errors != Serial_GetErrorCount() || Drops != Serial_GetDropCount())
		{
			Errors = Serial_GetErrorCount(); Drops = Serial_GetDropCount();
			Remote_ParserInit(&Parser); Remote_LinkInit(&Link);
			for (Budget = 0; Budget < 512 && Serial_ReadByte(&Byte); Budget ++); // 丢弃受影响字节
		}
		for (Budget = 0; Budget < 128 && Serial_ReadByte(&Byte); Budget ++)
		{
			if (Remote_ParseByte(&Parser, Byte, &Frame) && Remote_LinkFrame(&Link, &Frame, &Input, Delay_GetTick(), &Reply))
			{
				/*忙则丢弃本次回复，不积压旧输入；电脑会超时并重新轮询。*/
				Serial_SendArrayDMA(Buffer, Remote_Encode(&Reply, Buffer));
			}
		}
		if ((uint32_t)(Now - LastDraw) >= REMOTE_DRAW_MS)
		{
			LastDraw = Now; Remote_View(&Input, &Link, Now);
		}
		OLED_Service();
	}
}
