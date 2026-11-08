#ifndef __TIMER_H
#define __TIMER_H
/** 在 RobotApp_Init 后调用；启动 TIM2 1ms 控制中断，抢占优先级 1。 */
void Timer_Init(void);
#endif
