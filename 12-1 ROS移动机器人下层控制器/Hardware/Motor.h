#ifndef __MOTOR_H
#define __MOTOR_H
#include "stm32f10x.h"
/** 初始化引脚和 20kHz PWM，结束后保持待机。 */
void Motor_Init(void);
/** 立即关闭两轮输出；可由 main 的短临界区或控制中断调用。 */
void Motor_Stop(void);
/** 每 1ms 设置左右轮输出，范围 -1000..1000；正负为方向，绝对值为占空比。 */
void Motor_SetPWM(int16_t Left, int16_t Right);
/** 返回 1=急停按下，0=释放；不在本函数解除控制状态锁定。 */
uint8_t Motor_Emergency(void);
#endif
