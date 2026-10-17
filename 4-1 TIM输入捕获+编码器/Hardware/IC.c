#include "stm32f10x.h"                  // Device header
#include "IC.h"
#include "Delay.h"

static volatile uint32_t IC_OverflowBase, IC_LastCapture, IC_PeriodUs, IC_LastTick, IC_OverrunCount;
static volatile uint8_t IC_HaveFirst, IC_Valid;

/**
  * 函    数：TIM2_CH1上升沿捕获初始化，1MHz计数
  * 参    数：无
  * 返 回 值：无
  */
void IC_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure;
	TIM_ICInitTypeDef TIM_ICInitStructure;
	NVIC_InitTypeDef NVIC_InitStructure;
	/*开启时钟、GPIO初始化*/
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);            // PA0输入待测脉冲
	/*时基单元初始化：APB1分频后定时器仍为72MHz*/
	TIM_TimeBaseStructInit(&TIM_TimeBaseInitStructure);
	TIM_TimeBaseInitStructure.TIM_Prescaler = 72 - 1;
	TIM_TimeBaseInitStructure.TIM_Period = 65535;
	TIM_TimeBaseInit(TIM2, &TIM_TimeBaseInitStructure);
	/*输入捕获初始化*/
	TIM_ICStructInit(&TIM_ICInitStructure);
	TIM_ICInitStructure.TIM_Channel = TIM_Channel_1;
	TIM_ICInitStructure.TIM_ICPolarity = TIM_ICPolarity_Rising;
	TIM_ICInitStructure.TIM_ICSelection = TIM_ICSelection_DirectTI;
	TIM_ICInitStructure.TIM_ICPrescaler = TIM_ICPSC_DIV1;
	TIM_ICInitStructure.TIM_ICFilter = 3;
	TIM_ICInit(TIM2, &TIM_ICInitStructure);
	/*中断配置：溢出用于扩展时间戳，捕获用于计算相邻上升沿周期*/
	TIM_ClearFlag(TIM2, TIM_FLAG_Update | TIM_FLAG_CC1 | TIM_FLAG_CC1OF);
	TIM_ITConfig(TIM2, TIM_IT_Update | TIM_IT_CC1, ENABLE);
	NVIC_InitStructure.NVIC_IRQChannel = TIM2_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);
	TIM_Cmd(TIM2, ENABLE);
}

/** 函数：取得有效周期，超过2s无边沿返回0；数据通过短临界区一致读取 */
uint8_t IC_GetPeriod(uint32_t *PeriodUs)
{
	uint8_t Valid;
	uint32_t Primask;
	if (PeriodUs == 0) return 0;
	Primask = __get_PRIMASK();
	__disable_irq();
	Valid = IC_Valid && IC_PeriodUs != 0 && (uint32_t)(Delay_GetTick() - IC_LastTick) < 2000;
	*PeriodUs = IC_PeriodUs;
	__set_PRIMASK(Primask);
	return Valid;
}

uint32_t IC_GetOverrunCount(void) { return IC_OverrunCount; }

/** 函数：捕获/溢出中断，处理捕获和回绕在同一次中断中出现的边界 */
void TIM2_IRQHandler(void)
{
	uint16_t Status = TIM2->SR;
	if (Status & TIM_SR_CC1IF)
	{
		uint16_t Capture = TIM_GetCapture1(TIM2);
		uint32_t Stamp = IC_OverflowBase + Capture;
		if ((Status & TIM_SR_UIF) && Capture < 32768) Stamp += 65536;
		if (Status & TIM_SR_CC1OF)
		{
			IC_OverrunCount ++;
			IC_Valid = 0;
			IC_HaveFirst = 0;                        // 覆盖捕获后不能把不连续的周期当作新测量
			TIM_ClearFlag(TIM2, TIM_FLAG_CC1OF);
		}
		if ((uint32_t)(Delay_GetTick() - IC_LastTick) >= 2000) IC_HaveFirst = 0;
		if (IC_HaveFirst)
		{
			IC_PeriodUs = Stamp - IC_LastCapture;
			IC_Valid = 1;
		}
		IC_LastCapture = Stamp;
		IC_LastTick = Delay_GetTick();
		IC_HaveFirst = 1;
		/*读SR再读CCR1已清CC1IF，不能在此再次清它，以免删掉刚到的新边沿*/
	}
	if (Status & TIM_SR_UIF)
	{
		IC_OverflowBase += 65536;
		TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
	}
}
