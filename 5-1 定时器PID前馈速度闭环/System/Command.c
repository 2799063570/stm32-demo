#include "Command.h"
#include <string.h>

/** 函数：跳过空格，不把任意非数字字符悄悄当成0 */
static const char *Command_SkipSpace(const char *String)
{
	while (*String == ' ' || *String == '\t') String ++;
	return String;
}

/**
  * 函    数：完整解析一行命令，与串口分包无关，可在电脑上单独测试
  * 参    数：Line不包含换行符；RPM用于返回有符号整数目标
  * 返 回 值：命令类型，非法格式或整数溢出返回COMMAND_INVALID
  */
Command_TypeDef Command_Parse(const char *Line, int32_t *RPM)
{
	uint8_t Negative = 0;
	uint32_t Value = 0, Limit;
	if (Line == 0 || RPM == 0) return COMMAND_INVALID;
	Line = Command_SkipSpace(Line);
	if (strncmp(Line, "STOP", 4) == 0 && *Command_SkipSpace(Line + 4) == '\0') return COMMAND_STOP;
	if (strncmp(Line, "CLEAR", 5) == 0 && *Command_SkipSpace(Line + 5) == '\0') return COMMAND_CLEAR;
	if (strncmp(Line, "STATUS", 6) == 0 && *Command_SkipSpace(Line + 6) == '\0') return COMMAND_STATUS;
	if (strncmp(Line, "SPEED", 5) != 0 || (Line[5] != ' ' && Line[5] != '\t')) return COMMAND_INVALID;
	Line = Command_SkipSpace(Line + 5);
	if (*Line == '+' || *Line == '-') { Negative = *Line == '-'; Line ++; }
	if (*Line < '0' || *Line > '9') return COMMAND_INVALID;
	Limit = Negative ? 2147483648U : 2147483647U;
	while (*Line >= '0' && *Line <= '9')
	{
		uint32_t Digit = (uint32_t)(*Line - '0');
		if (Value > (Limit - Digit) / 10U) return COMMAND_INVALID;
		Value = Value * 10U + Digit;
		Line ++;
	}
	if (*Command_SkipSpace(Line) != '\0') return COMMAND_INVALID;
	*RPM = Negative ? (Value == 2147483648U ? (-2147483647 - 1) : -(int32_t)Value) : (int32_t)Value;
	return COMMAND_SPEED;                            // 电机允许的RPM范围由Control_SetTarget再检查
}
