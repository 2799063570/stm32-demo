#ifndef __REMOTE_INPUT_H
#define __REMOTE_INPUT_H
#include <stdint.h>
typedef struct
{
	uint32_t SumX, SumY;
	uint16_t CenterX, CenterY, MinX, MaxX, MinY, MaxY, RawX, RawY;
	int16_t X, Y;
	uint8_t Samples, Ready, ADCValid, StableButton, ButtonCount, Armed, Page;
	uint8_t Selection, Command;                   // 菜单选择与最近一次已生成请求；两者不混用
	uint8_t NavLatched, LongSent;                 // 回中后允许再次导航；长按只发送一次取消
	uint16_t RequestId, HoldSamples;              // 请求编号持久保留在INPUT中；按住时按5ms计数
} Remote_InputTypeDef;
void Remote_InputInit(Remote_InputTypeDef *Input);
void Remote_InputFeed(Remote_InputTypeDef *Input, uint16_t X, uint16_t Y, uint8_t Pressed);
uint8_t Remote_InputButton(const Remote_InputTypeDef *Input);
#endif
