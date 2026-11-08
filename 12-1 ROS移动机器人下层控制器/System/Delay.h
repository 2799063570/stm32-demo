#ifndef __DELAY_H
#define __DELAY_H
#include "stm32f10x.h"
/** 验证 72MHz 系统时钟，设置全局中断优先级分组并启动 1ms SysTick。 */
void Delay_Init(void);
/** 仅由 SysTick_Handler 调用，递增公共毫秒计数。 */
void Delay_Tick(void);
/** 返回 uint32 毫秒计数；用无符号时间差处理回绕。 */
uint32_t Delay_GetTick(void);
/** 阻塞延时，仅供非实时前台场景；正常 RobotApp 循环使用时间差调度。 */
void Delay_ms(uint32_t ms);
#endif
