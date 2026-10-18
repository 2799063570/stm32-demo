#include "RemoteInput.h"
#include "RemoteConfig.h"
#include "RemoteProtocol.h"
#include <string.h>

void Remote_InputInit(Remote_InputTypeDef *Input)
{
	memset(Input, 0, sizeof(*Input));
	Input->MinX = Input->MinY = 4095;
	Input->RawX = Input->RawY = 2048;
	Input->StableButton = 1;                       // 先松开再按下，避免上电长按触发翻页
	Input->Selection = REMOTE_HOME;
}

static void Remote_Request(Remote_InputTypeDef *Input, uint8_t Command)
{
	/*请求保持在后续INPUT帧中直到下次操作；电脑按会话/编号去重，不按按键电平重复执行。*/
	Input->RequestId ++;
	if (Input->RequestId == 0) Input->RequestId = 1;
	Input->Command = Command;
}

static int16_t Remote_Axis(uint16_t Raw, uint16_t Center)
{
	int32_t Delta = (int32_t)Raw - Center;
	int32_t Magnitude = Delta >= 0 ? Delta : -Delta;
	int32_t Range = Delta >= 0 ? 4095 - Center : Center;
	if (Magnitude <= REMOTE_DEAD_ZONE || Range <= REMOTE_DEAD_ZONE) return 0;
	Magnitude = (Magnitude - REMOTE_DEAD_ZONE) * 1000 / (Range - REMOTE_DEAD_ZONE);
	if (Magnitude > 1000) Magnitude = 1000;
	return (int16_t)(Delta >= 0 ? Magnitude : -Magnitude);
}

uint8_t Remote_InputButton(const Remote_InputTypeDef *Input)
{
	return Input->Ready && Input->ADCValid && Input->Armed && Input->StableButton;
}

/** 函数：5ms采样，32个稳定中部样本校准；四次稳定采样确认按键 */
void Remote_InputFeed(Remote_InputTypeDef *Input, uint16_t X, uint16_t Y, uint8_t Pressed)
{
	Input->ADCValid = 1;
	Input->RawX = X; Input->RawY = Y;
	Pressed = Pressed != 0;
	if (Pressed == Input->StableButton) Input->ButtonCount = 0;
	else if (++ Input->ButtonCount >= 4)
	{
		Input->StableButton = Pressed; Input->ButtonCount = 0;
		if (!Pressed)
		{
			/*短按在松开时确认，长按取消后不再追加短按请求；反馈页短按不触发运动。*/
			if (Input->Armed && Input->Ready && Input->HoldSamples && !Input->LongSent && Input->Page == 0)
				Remote_Request(Input, Input->Selection);
			Input->Armed = 1; Input->HoldSamples = 0; Input->LongSent = 0;
		}
	}
	if (!Input->Ready)
	{
		if (X < 1024 || X > 3071 || Y < 1024 || Y > 3071)
		{
			Input->Samples = 0; Input->SumX = Input->SumY = 0;
			Input->MinX = Input->MinY = 4095; Input->MaxX = Input->MaxY = 0;
			return;
		}
		if (X < Input->MinX) Input->MinX = X;
		if (X > Input->MaxX) Input->MaxX = X;
		if (Y < Input->MinY) Input->MinY = Y;
		if (Y > Input->MaxY) Input->MaxY = Y;
		Input->SumX += X; Input->SumY += Y;
		if (++ Input->Samples == 32)
		{
			if (Input->MaxX - Input->MinX <= 200 && Input->MaxY - Input->MinY <= 200)
			{
				Input->CenterX = (uint16_t)(Input->SumX / 32);
				Input->CenterY = (uint16_t)(Input->SumY / 32);
				Input->Ready = 1;
			}
			Input->Samples = 0; Input->SumX = Input->SumY = 0;
			Input->MinX = Input->MinY = 4095; Input->MaxX = Input->MaxY = 0;
		}
		return;
	}
	Input->X = Remote_Axis(X, Input->CenterX);
	Input->Y = Remote_Axis(Y, Input->CenterY);
#if REMOTE_SWAP_XY
	{ int16_t Temp = Input->X; Input->X = Input->Y; Input->Y = Temp; }
#endif
#if REMOTE_INVERT_X
	Input->X = -Input->X;
#endif
#if REMOTE_INVERT_Y
	Input->Y = -Input->Y;
#endif
	if (Input->StableButton && Input->Armed)
	{
		if (Input->HoldSamples < 160) Input->HoldSamples ++;
		if (Input->HoldSamples >= 160 && !Input->LongSent)
		{
			Remote_Request(Input, REMOTE_CANCEL); Input->LongSent = 1;
		}
	}
	if (Input->X > -300 && Input->X < 300 && Input->Y > -300 && Input->Y < 300) Input->NavLatched = 0;
	/*只在松开按键时导航；摇杆回中后才允许下一次，避免持续偏转导致菜单飞快翻动。*/
	if (!Input->StableButton && !Input->NavLatched)
	{
		if (Input->X > 600) { Input->Page = (Input->Page + 1) % 5; Input->NavLatched = 1; }
		else if (Input->X < -600) { Input->Page = (Input->Page + 4) % 5; Input->NavLatched = 1; }
		else if (Input->Page == 0 && Input->Y > 600) { Input->Selection = REMOTE_HOME; Input->NavLatched = 1; }
		else if (Input->Page == 0 && Input->Y < -600) { Input->Selection = REMOTE_ZERO; Input->NavLatched = 1; }
	}
}
