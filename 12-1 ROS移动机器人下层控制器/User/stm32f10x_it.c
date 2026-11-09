/**
 * Cortex-M3 公共异常入口；外设中断位于各自的 Hardware/System 模块末尾。
 * 故障异常关闭中断和电机输出后停留，便于连接调试器定位。
 */
#include "stm32f10x.h" // Device header
#include "Delay.h"
#include "Motor.h"

/*公共异常函数放在User，外设中断函数放在对应Hardware或System模块末尾*/
void SysTick_Handler(void)
{
	Delay_Tick();
}

void NMI_Handler(void)
{
	__disable_irq();
	Motor_Stop();
	while (1);
}

void HardFault_Handler(void)
{
	__disable_irq();
	Motor_Stop();
	while (1);
}

void MemManage_Handler(void)
{
	__disable_irq();
	Motor_Stop();
	while (1);
}

void BusFault_Handler(void)
{
	__disable_irq();
	Motor_Stop();
	while (1);
}

void UsageFault_Handler(void)
{
	__disable_irq();
	Motor_Stop();
	while (1);
}

void SVC_Handler(void)
{
}

void DebugMon_Handler(void)
{
}

void PendSV_Handler(void)
{
}

void assert_failed(uint8_t *File, uint32_t Line)
{
	(void)File;
	(void)Line;
	__disable_irq();
	__disable_irq();
	Motor_Stop();
	while (1); // 在此处观察File和Line定位非法参数
}
