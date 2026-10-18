#include "stm32f10x.h"                  // Device header
#include "Serial.h"
#include "Delay.h"
#include <stdio.h>
#include <stdarg.h>

/**
  * 函    数：USART1日志串口初始化，本模块不用DMA
  * 参    数：无
  * 返 回 值：无
  */
void Serial_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	USART_InitTypeDef USART_InitStructure;
	/*开启时钟*/
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_USART1, ENABLE);
	/*GPIO初始化*/
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);            // PA9发送日志
	/*USART初始化*/
	USART_StructInit(&USART_InitStructure);
	USART_InitStructure.USART_BaudRate = 115200;
	USART_InitStructure.USART_Mode = USART_Mode_Tx;
	USART_Init(USART1, &USART_InitStructure);
	USART_Cmd(USART1, ENABLE);
}

/** 函数：主循环轮询发送字节，20ms超时；禁止在中断内调用 */
void Serial_SendByte(uint8_t Byte)
{
	uint32_t Start = Delay_GetTick();
	while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET)
	{
		if ((uint32_t)(Delay_GetTick() - Start) >= 20) return;
	}
	USART_SendData(USART1, Byte);
}

/** 函数：发送字符串 */
void Serial_SendString(const char *String)
{
	while (*String) Serial_SendByte((uint8_t)*String ++);
}

/** 函数：有界格式化日志输出 */
void Serial_Printf(const char *Format, ...)
{
	char String[192];
	va_list Arg;
	va_start(Arg, Format);
	vsnprintf(String, sizeof(String), Format, Arg);
	va_end(Arg);
	String[sizeof(String) - 1] = '\0';
	Serial_SendString(String);
}
