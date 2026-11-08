#include "stm32f10x.h"
#include "JogKeys.h"
#include "JogRemote.h"

void JogKeys_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);
}

uint8_t JogKeys_Read(void)
{
	uint8_t Keys = 0;
	if (GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_2) == Bit_RESET) Keys |= JOG_KEY_MENU;
	return Keys;
}
