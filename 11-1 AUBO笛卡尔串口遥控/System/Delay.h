#ifndef __DELAY_H
#define __DELAY_H
#include "stm32f10x.h"
void Delay_Init(void);
void Delay_Tick(void);
uint32_t Delay_GetTick(void);
void Delay_ms(uint32_t ms);
#endif
