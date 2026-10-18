#include "DemoCommand.h"
#include <string.h>

void DemoCommand_Init(DemoCommand_State *State)
{
	memset(State, 0, sizeof(*State));
}

/** 函数：接收丢失/错误后丢弃到行尾，避免把残缺命令拼起来执行 */
void DemoCommand_Invalidate(DemoCommand_State *State)
{
	State->Length = 0;
	State->Discard = 1;
}

/** 函数：按CR/LF解析HELP、STATUS、LED 100~2000，IDLE不是命令结束符 */
DemoCommand_Result DemoCommand_Push(DemoCommand_State *State, uint8_t Byte, uint16_t *Period)
{
	uint8_t Index;
	uint32_t Value = 0;
	if (Byte != '\r' && Byte != '\n')
	{
		if (State->Discard) return DEMO_COMMAND_NONE;
		if (Byte < 32 || Byte > 126 || State->Length >= sizeof(State->Line) - 1)
			DemoCommand_Invalidate(State);
		else State->Line[State->Length ++] = (char)Byte;
		return DEMO_COMMAND_NONE;
	}
	if (State->Discard)
	{
		DemoCommand_Init(State);
		return DEMO_COMMAND_INVALID;
	}
	if (State->Length == 0) return DEMO_COMMAND_NONE;
	State->Line[State->Length] = '\0';
	State->Length = 0;
	if (strcmp(State->Line, "HELP") == 0) return DEMO_COMMAND_HELP;
	if (strcmp(State->Line, "STATUS") == 0) return DEMO_COMMAND_STATUS;
	if (strncmp(State->Line, "LED ", 4) != 0 || State->Line[4] == '\0') return DEMO_COMMAND_INVALID;
	for (Index = 4; State->Line[Index]; Index ++)
	{
		uint8_t Digit = (uint8_t)State->Line[Index];
		if (Digit < '0' || Digit > '9') return DEMO_COMMAND_INVALID;
		Value = Value * 10 + Digit - '0';
		if (Value > 2000) return DEMO_COMMAND_INVALID; // 及时限幅，乘法不会溢出
	}
	if (Value < 100) return DEMO_COMMAND_INVALID;
	*Period = (uint16_t)Value;
	return DEMO_COMMAND_LED;
}
