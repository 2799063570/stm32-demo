#ifndef __ENCODER_H
#define __ENCODER_H
#include "stm32f10x.h"
/** 左轮 PA6/PA7 -> TIM3，右轮 PB6/PB7 -> TIM4，四倍频计数。 */
void Encoder_Init(void);
/** Wheel=0 左轮、1 右轮；返回自上次读取以来的有符号增量，每 1ms 读取一次。 */
int16_t Encoder_Get(uint8_t Wheel);
#endif
