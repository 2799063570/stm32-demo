#ifndef __STM32F10X_IT_H
#define __STM32F10X_IT_H
#include "stm32f10x.h"
/** 公共毫秒时基入口；外设 ISR 在各自模块实现并由启动向量表引用。 */
void SysTick_Handler(void);
#endif
