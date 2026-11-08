/**
 * TIM2 产生 1ms 控制节拍；仅负责调度 RobotApp_Tick()。
 */
#include "stm32f10x.h"
#include "Timer.h"
#include "RobotApp.h"

/** 函数：TIM2 1kHz；控制ISR优先级1，通信优先级2。 */
void Timer_Init(void)
{
	TIM_TimeBaseInitTypeDef TIM_InitStructure;
	NVIC_InitTypeDef NVIC_InitStructure;
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);
	/* APB1 定时器时钟为 72MHz，先分频到 1MHz，再计数 1000 次得到 1ms。 */
	TIM_TimeBaseStructInit(&TIM_InitStructure);
	TIM_InitStructure.TIM_Prescaler = 72 - 1;
	TIM_InitStructure.TIM_Period = 1000 - 1;
	TIM_TimeBaseInit(TIM2, &TIM_InitStructure);
	TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
	TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE);
	NVIC_InitStructure.NVIC_IRQChannel = TIM2_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);
	TIM_Cmd(TIM2, ENABLE);
}

/** 每 1ms 清除更新标志并执行一次控制；通信解析和 I2C 服务留在 main。 */
void TIM2_IRQHandler(void)
{
	if (TIM_GetITStatus(TIM2, TIM_IT_Update) != RESET)
	{
		TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
		RobotApp_Tick();
	}
}
