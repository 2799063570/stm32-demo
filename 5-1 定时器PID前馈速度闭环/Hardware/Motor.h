#ifndef __MOTOR_H
#define __MOTOR_H
#include "stm32f10x.h"
void Motor_Init(void);
void Motor_SetPWM(int16_t PWM);
void Motor_Stop(void);
int16_t Motor_GetPWM(void);
#endif
