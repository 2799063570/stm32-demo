#include "stm32f10x.h"                  // Device header
#include "PWM.h"

/**
  * 函    数：TIM1_CH1初始化，PA8输出20kHz PWM
  * 参    数：无
  * 返 回 值：无
  */
void PWM_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure;
	TIM_OCInitTypeDef TIM_OCInitStructure;
	/*开启时钟、GPIO初始化*/
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM1 | RCC_APB2Periph_GPIOA, ENABLE);
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);
	/*时基单元初始化：72MHz/3600=20kHz*/
	TIM_TimeBaseStructInit(&TIM_TimeBaseInitStructure);
	TIM_TimeBaseInitStructure.TIM_Prescaler = 0;
	TIM_TimeBaseInitStructure.TIM_Period = 3600 - 1;
	TIM_TimeBaseInit(TIM1, &TIM_TimeBaseInitStructure);
	/*输出比较初始化*/
	TIM_OCStructInit(&TIM_OCInitStructure);
	TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
	TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
	TIM_OCInitStructure.TIM_Pulse = 0;
	TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
	TIM_OC1Init(TIM1, &TIM_OCInitStructure);
	/*CCR立即生效，方便方向切换前立即清零；TB6612的STBY另作输出总使能*/
	TIM_OC1PreloadConfig(TIM1, TIM_OCPreload_Disable);
	TIM_CtrlPWMOutputs(TIM1, ENABLE);                // 高级定时器还需要开启主输出MOE
	TIM_Cmd(TIM1, ENABLE);
}

/** 函数：设置0~1000千分比占空比 */
void PWM_SetCompare1(uint16_t Compare)
{
	if (Compare > 1000) Compare = 1000;
	TIM_SetCompare1(TIM1, (uint32_t)Compare * 3600 / 1000);
}
