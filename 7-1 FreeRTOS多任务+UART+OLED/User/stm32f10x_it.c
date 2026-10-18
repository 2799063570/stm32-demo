#include "stm32f10x.h"                  // Device header
#include "App.h"

/*SVC/PendSV/SysTick由官方port.c提供，不能再定义同名空函数*/
void NMI_Handler(void) { App_Assert("NMI", 0); }
void HardFault_Handler(void) { App_Assert("HardFault", 0); }
void MemManage_Handler(void) { App_Assert("MemManage", 0); }
void BusFault_Handler(void) { App_Assert("BusFault", 0); }
void UsageFault_Handler(void) { App_Assert("UsageFault", 0); }
void DebugMon_Handler(void) { }

void assert_failed(uint8_t *File, uint32_t Line)
{
	App_Assert((const char *)File, Line);
}
