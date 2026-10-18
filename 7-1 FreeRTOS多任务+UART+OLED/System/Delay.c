#include "stm32f10x.h"                  // Device header
#include "FreeRTOS.h"
#include "task.h"
#include "Delay.h"

/** 函数：校验72MHz时钟，设置全抢占优先级；不抢占FreeRTOS的SysTick */
void Delay_Init(void)
{
	RCC_ClocksTypeDef Clocks;
	SystemCoreClockUpdate();
	RCC_GetClocksFreq(&Clocks);
	configASSERT(SystemCoreClock == 72000000 && Clocks.PCLK1_Frequency == 36000000);
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);
}

/** 函数：任务上下文获取毫秒数，本工程固定1000Hz，不另建第二套时基 */
uint32_t Delay_GetTick(void)
{
	return (uint32_t)xTaskGetTickCount();
}

/** 函数：任务阻塞延时；只能在调度器启动后的任务内调用 */
void Delay_ms(uint32_t ms)
{
	if (ms) vTaskDelay(pdMS_TO_TICKS(ms));
}
