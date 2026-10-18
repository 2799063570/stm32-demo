#include "stm32f10x.h"                  // Device header
#include "Delay.h"
#include "LED.h"
#include "App.h"

int main(void)
{
	Delay_Init();                         // 仅校验时钟和配置NVIC；SysTick交给FreeRTOS
	LED_Init();
	App_Start();                          // 创建队列、互斥锁和任务，然后启动调度器
	while (1);                            // 正常运行不会返回
}
