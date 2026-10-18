#include "stm32f10x.h"                  // Device header
#include "LED.h"

/** 函数：Blue Pill板载PC13 LED，低电平点亮，初始化保持熄灭 */
void LED_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);
	GPIO_SetBits(GPIOC, GPIO_Pin_13);
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_13;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
	GPIO_Init(GPIOC, &GPIO_InitStructure);
}

void LED_Set(uint8_t On)
{
	GPIO_WriteBit(GPIOC, GPIO_Pin_13, On ? Bit_RESET : Bit_SET);
}
