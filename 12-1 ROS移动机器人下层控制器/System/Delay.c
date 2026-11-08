/**
 * SysTick 公共毫秒时基：用于前台超时和采样调度，与 TIM2 控制节拍独立。
 */
#include "stm32f10x.h" // Device header
#include "Delay.h"

static volatile uint32_t Delay_TickCount; // 系统毫秒计数

/**
  * 函    数：初始化1ms公共时基
  * 参    数：无
  * 返 回 值：无
  */
void Delay_Init(void)
{
	RCC_ClocksTypeDef RCC_ClocksStructure;
	SystemCoreClockUpdate(); // 启动文件已配置8MHz晶振、PLL九倍频
	RCC_GetClocksFreq(&RCC_ClocksStructure);
	if (SystemCoreClock != 72000000 || RCC_ClocksStructure.PCLK1_Frequency != 36000000)
	{
		while (1); // 晶振或时钟异常，在此处下断点检查
	}
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2); // 全工程只在这里配置一次分组
	SysTick_Config(SystemCoreClock / 1000);         // 每1ms进入一次SysTick中断
	NVIC_SetPriority(SysTick_IRQn, 0);              // 时基优先级最高
}

/**
  * 函    数：时基计数，由SysTick_Handler调用
  * 参    数：无
  * 返 回 值：无
  */
void Delay_Tick(void)
{
	Delay_TickCount++;
}

/**
  * 函    数：获取系统运行毫秒数
  * 参    数：无
  * 返 回 值：32位毫秒计数，允许自然回绕
  */
uint32_t Delay_GetTick(void)
{
	return Delay_TickCount;
}

/**
  * 函    数：毫秒延时，仅供主循环使用
  * 参    数：ms 延时时间
  * 返 回 值：无
  */
void Delay_ms(uint32_t ms)
{
	uint32_t Start = Delay_GetTick();
	while ((uint32_t)(Delay_GetTick() - Start) < ms); // 无符号减法兼容计数回绕
}
