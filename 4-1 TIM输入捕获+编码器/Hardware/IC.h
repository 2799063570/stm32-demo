#ifndef __IC_H
#define __IC_H
#include "stm32f10x.h"
void IC_Init(void);
uint8_t IC_GetPeriod(uint32_t *PeriodUs);
uint32_t IC_GetOverrunCount(void);
#endif
