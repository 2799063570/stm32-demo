#ifndef __COMMAND_H
#define __COMMAND_H
#include <stdint.h>
typedef enum
{
	COMMAND_INVALID = 0, COMMAND_SPEED, COMMAND_STOP, COMMAND_CLEAR, COMMAND_STATUS
} Command_TypeDef;
Command_TypeDef Command_Parse(const char *Line, int32_t *RPM);
#endif
