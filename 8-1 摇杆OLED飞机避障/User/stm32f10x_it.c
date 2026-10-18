#include "stm32f10x.h"                  // Device header
#include "Delay.h"

/*公共异常函数放在User，外设中断函数放在对应Hardware或System模块末尾*/
void SysTick_Handler(void) { Delay_Tick(); }
void NMI_Handler(void) { while (1); }
void HardFault_Handler(void) { while (1); }
void MemManage_Handler(void) { while (1); }
void BusFault_Handler(void) { while (1); }
void UsageFault_Handler(void) { while (1); }
void SVC_Handler(void) { }
void DebugMon_Handler(void) { }
void PendSV_Handler(void) { }

void assert_failed(uint8_t *File, uint32_t Line)
{
	(void)File;
	(void)Line;
	__disable_irq();
	while (1);                                      // 在此处观察File和Line定位非法参数
}
