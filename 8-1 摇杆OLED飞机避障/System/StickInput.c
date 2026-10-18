#include "StickInput.h"
#include "GameConfig.h"
#include <string.h>

void StickInput_Init(StickInput_TypeDef *Stick)
{
	memset(Stick, 0, sizeof(*Stick));
	Stick->MinX = Stick->MinY = 4095;
	Stick->StableButton = 1;                      // 上电必须先松开，禁止长按自动开始
}

static int8_t StickInput_Axis(uint16_t Raw, uint16_t Center)
{
	int32_t Delta = (int32_t)Raw - Center;
	if (Delta > STICK_FAST_ZONE) return 2;
	if (Delta > STICK_DEAD_ZONE) return 1;
	if (Delta < -STICK_FAST_ZONE) return -2;
	if (Delta < -STICK_DEAD_ZONE) return -1;
	return 0;
}

/** 函数：每5ms喂入一组有效采样；滤掉中心抖动并生成单次按键事件 */
void StickInput_Feed(StickInput_TypeDef *Stick, uint16_t X, uint16_t Y, uint8_t Pressed)
{
	Pressed = Pressed != 0;
	if (Pressed == Stick->StableButton) Stick->ButtonCount = 0;
	else if (++ Stick->ButtonCount >= STICK_DEBOUNCE_SAMPLES)
	{
		Stick->ButtonCount = 0;
		Stick->StableButton = Pressed;
		if (!Pressed) Stick->Armed = 1;
		else if (Stick->Armed && Stick->Ready) Stick->PressEvent = 1;
	}
	if (!Stick->Ready)
	{
		/*中心必须稳定且位于中部；误推到端点时一直提示松手，不采用错误中心。*/
		if (X < 1024 || X > 3071 || Y < 1024 || Y > 3071)
		{
			Stick->Samples = 0;
			Stick->SumX = Stick->SumY = 0;
			Stick->MinX = Stick->MinY = 4095;
			Stick->MaxX = Stick->MaxY = 0;
			return;
		}
		if (X < Stick->MinX) Stick->MinX = X;
		if (X > Stick->MaxX) Stick->MaxX = X;
		if (Y < Stick->MinY) Stick->MinY = Y;
		if (Y > Stick->MaxY) Stick->MaxY = Y;
		Stick->SumX += X;
		Stick->SumY += Y;
		if (++ Stick->Samples == STICK_CAL_SAMPLES)
		{
			if (Stick->MaxX - Stick->MinX <= STICK_CAL_SPAN && Stick->MaxY - Stick->MinY <= STICK_CAL_SPAN)
			{
				Stick->CenterX = (uint16_t)(Stick->SumX / STICK_CAL_SAMPLES);
				Stick->CenterY = (uint16_t)(Stick->SumY / STICK_CAL_SAMPLES);
				Stick->Ready = 1;
			}
			Stick->Samples = 0;
			Stick->SumX = Stick->SumY = 0;
			Stick->MinX = Stick->MinY = 4095;
			Stick->MaxX = Stick->MaxY = 0;
		}
		return;
	}
	Stick->DX = StickInput_Axis(X, Stick->CenterX);
	Stick->DY = StickInput_Axis(Y, Stick->CenterY);
#if STICK_SWAP_XY
	{
		int8_t Temp = Stick->DX;
		Stick->DX = Stick->DY;
		Stick->DY = Temp;
	}
#endif
#if STICK_INVERT_X
	Stick->DX = -Stick->DX;
#endif
#if STICK_INVERT_Y
	Stick->DY = -Stick->DY;
#endif
}

uint8_t StickInput_TakePress(StickInput_TypeDef *Stick)
{
	uint8_t Event = Stick->PressEvent;
	Stick->PressEvent = 0;
	return Event;
}
