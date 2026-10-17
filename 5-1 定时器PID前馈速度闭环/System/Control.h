#ifndef __CONTROL_H
#define __CONTROL_H
#include "stm32f10x.h"
typedef enum
{
	CONTROL_FAULT_NONE = 0, CONTROL_FAULT_TIMEOUT, CONTROL_FAULT_STALL,
	CONTROL_FAULT_DIRECTION, CONTROL_FAULT_SERIAL
} Control_FaultTypeDef;
typedef struct
{
	int32_t TargetRPM, RampRPM, SpeedRPM;
	int16_t PWM;
	uint8_t Running;
	Control_FaultTypeDef Fault;
} Control_StatusTypeDef;
void Control_Init(void);
void Control_Update(void);
uint8_t Control_SetTarget(int32_t RPM);
void Control_Stop(void);
void Control_ClearFault(void);
void Control_SerialFault(void);
void Control_GetStatus(Control_StatusTypeDef *Status);
#endif
