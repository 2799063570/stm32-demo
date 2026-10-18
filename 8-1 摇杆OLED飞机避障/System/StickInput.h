#ifndef __STICK_INPUT_H
#define __STICK_INPUT_H
#include <stdint.h>
typedef struct
{
	uint32_t SumX, SumY;
	uint16_t CenterX, CenterY, MinX, MaxX, MinY, MaxY;
	uint8_t Samples, Ready, StableButton, ButtonCount, Armed, PressEvent;
	int8_t DX, DY;
} StickInput_TypeDef;
void StickInput_Init(StickInput_TypeDef *Stick);
void StickInput_Feed(StickInput_TypeDef *Stick, uint16_t X, uint16_t Y, uint8_t Pressed);
uint8_t StickInput_TakePress(StickInput_TypeDef *Stick);
#endif
