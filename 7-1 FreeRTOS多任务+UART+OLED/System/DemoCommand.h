#ifndef __DEMO_COMMAND_H
#define __DEMO_COMMAND_H
#include <stdint.h>

typedef enum
{
	DEMO_COMMAND_NONE, DEMO_COMMAND_HELP, DEMO_COMMAND_STATUS,
	DEMO_COMMAND_LED, DEMO_COMMAND_INVALID
} DemoCommand_Result;

typedef struct
{
	char Line[32];
	uint8_t Length, Discard;
} DemoCommand_State;

void DemoCommand_Init(DemoCommand_State *State);
void DemoCommand_Invalidate(DemoCommand_State *State);
DemoCommand_Result DemoCommand_Push(DemoCommand_State *State, uint8_t Byte, uint16_t *Period);
#endif
