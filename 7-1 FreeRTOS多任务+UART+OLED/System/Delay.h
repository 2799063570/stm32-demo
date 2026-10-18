#ifndef __DELAY_H
#define __DELAY_H
#include <stdint.h>
void Delay_Init(void);
uint32_t Delay_GetTick(void);
void Delay_ms(uint32_t ms);
#endif
