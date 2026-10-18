#include "stm32f10x.h"                  // Device header
#include "Serial.h"
#include "Delay.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#define SERIAL_DMA_SIZE 256                         // DMA循环接收缓冲区
#define SERIAL_FIFO_SIZE 512                        // 中断与主循环之间的软件FIFO
static uint8_t Serial_DMARxBuffer[SERIAL_DMA_SIZE];
static uint8_t Serial_RxFIFO[SERIAL_FIFO_SIZE];
static uint8_t Serial_DMATxBuffer[256];
static uint16_t Serial_DMAReadPos;
static volatile uint16_t Serial_Head, Serial_Tail;
static volatile uint8_t Serial_TxBusy;
static volatile uint32_t Serial_IdleCount, Serial_DropCount, Serial_ErrorCount;

/**
  * 函    数：将DMA新收到的数据搬到软件FIFO，内部使用
  * 参    数：无
  * 返 回 值：无
  */
static void Serial_CollectDMA(void)
{
	uint16_t WritePos = SERIAL_DMA_SIZE - DMA_GetCurrDataCounter(DMA1_Channel5);
	if (WritePos == SERIAL_DMA_SIZE) WritePos = 0;    // DMA计数器重装附近也统一成环形位置
	while (Serial_DMAReadPos != WritePos)
	{
		uint16_t Next = (Serial_Head + 1) % SERIAL_FIFO_SIZE;
		if (Next == Serial_Tail) Serial_DropCount ++; // FIFO满时丢新字节，不能覆盖尚未处理的数据
		else
		{
			Serial_RxFIFO[Serial_Head] = Serial_DMARxBuffer[Serial_DMAReadPos];
			__DMB();
			Serial_Head = Next;
		}
		Serial_DMAReadPos = (Serial_DMAReadPos + 1) % SERIAL_DMA_SIZE;
	}
}

void led_init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE); 
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_13;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOC, &GPIO_InitStructure);
}
/**
  * 函    数：USART1、DMA发送、循环DMA接收与空闲中断初始化
  * 参    数：无
  * 返 回 值：无
  */
void Serial_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	USART_InitTypeDef USART_InitStructure;
	DMA_InitTypeDef DMA_InitStructure;
	NVIC_InitTypeDef NVIC_InitStructure;
	/*开启时钟*/
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_USART1, ENABLE); 
	RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);
	/*GPIO初始化*/
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);            // PA9发送
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
	GPIO_Init(GPIOA, &GPIO_InitStructure);            // PA10接收
	/*USART初始化*/
	USART_StructInit(&USART_InitStructure);
	USART_InitStructure.USART_BaudRate = 115200;
	USART_InitStructure.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;
	USART_Init(USART1, &USART_InitStructure);         // 115200、8数据位、无校验、1停止位
	/*DMA接收初始化：USART1_RX固定映射到DMA1通道5*/
	DMA_DeInit(DMA1_Channel5);
	DMA_StructInit(&DMA_InitStructure);
	DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&USART1->DR;
	DMA_InitStructure.DMA_MemoryBaseAddr = (uint32_t)Serial_DMARxBuffer;
	DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralSRC;
	DMA_InitStructure.DMA_BufferSize = SERIAL_DMA_SIZE;
	DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
	DMA_InitStructure.DMA_Mode = DMA_Mode_Circular;
	DMA_InitStructure.DMA_Priority = DMA_Priority_High;
	DMA_Init(DMA1_Channel5, &DMA_InitStructure);
	DMA_ITConfig(DMA1_Channel5, DMA_IT_HT | DMA_IT_TC | DMA_IT_TE, ENABLE);
	/*DMA发送初始化：USART1_TX固定映射到DMA1通道4*/
	DMA_DeInit(DMA1_Channel4);
	DMA_InitStructure.DMA_MemoryBaseAddr = (uint32_t)Serial_DMATxBuffer;
	DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralDST;
	DMA_InitStructure.DMA_Mode = DMA_Mode_Normal;
	DMA_InitStructure.DMA_Priority = DMA_Priority_Medium;
	DMA_Init(DMA1_Channel4, &DMA_InitStructure);
	DMA_ITConfig(DMA1_Channel4, DMA_IT_TC | DMA_IT_TE, ENABLE);
	/*NVIC配置：这三个中断采用相同抢占优先级，避免搬运位置被嵌套修改*/
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 2;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_InitStructure.NVIC_IRQChannel = DMA1_Channel5_IRQn;
	NVIC_Init(&NVIC_InitStructure);
	NVIC_InitStructure.NVIC_IRQChannel = DMA1_Channel4_IRQn;
	NVIC_Init(&NVIC_InitStructure);
	NVIC_InitStructure.NVIC_IRQChannel = USART1_IRQn;
	NVIC_Init(&NVIC_InitStructure);
	USART_ITConfig(USART1, USART_IT_IDLE, ENABLE);     // 空闲中断及时交付不足半缓冲区的数据
	USART_ITConfig(USART1, USART_IT_ERR, ENABLE);
	USART_DMACmd(USART1, USART_DMAReq_Rx | USART_DMAReq_Tx, ENABLE);
	DMA_Cmd(DMA1_Channel5, ENABLE);
	USART_Cmd(USART1, ENABLE);
}

/**
  * 函    数：从软件FIFO取出一个字节
  * 参    数：Byte 输出地址
  * 返 回 值：1取到数据，0当前无数据
  */
uint8_t Serial_ReadByte(uint8_t *Byte)
{
	uint16_t Tail = Serial_Tail;
	if (Byte == 0 || Tail == Serial_Head) return 0;
	__DMB();
	*Byte = Serial_RxFIFO[Tail];
	__DMB();
	Serial_Tail = (Tail + 1) % SERIAL_FIFO_SIZE;
	return 1;
}

/**
  * 函    数：发起一次DMA发送，不阻塞等待
  * 参    数：Array 数据地址；Length 长度1~256
  * 返 回 值：1成功启动，0忙或参数错误
  */
uint8_t Serial_SendArrayDMA(const uint8_t *Array, uint16_t Length)
{
	if (Array == 0 || Length == 0 || Length > sizeof(Serial_DMATxBuffer) || Serial_TxBusy) return 0;
	memcpy(Serial_DMATxBuffer, Array, Length);         // 复制到驱动内部，调用者可立即复用原数组
	DMA_Cmd(DMA1_Channel4, DISABLE);
	DMA_ClearFlag(DMA1_FLAG_GL4);
	DMA_SetCurrDataCounter(DMA1_Channel4, Length);
	Serial_TxBusy = 1;
	DMA_Cmd(DMA1_Channel4, ENABLE);
	return 1;
}

/** 函数：轮询发送一个字节，仅供主循环调用；等待均有20ms超时 */
void Serial_SendByte(uint8_t Byte)
{
	uint32_t Start = Delay_GetTick();
	while (Serial_TxBusy || USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET)
	{
		if ((uint32_t)(Delay_GetTick() - Start) >= 20) return;
	}
	USART_SendData(USART1, Byte);
}

/** 函数：发送字符串，不能在中断中打印 */
void Serial_SendString(const char *String)
{
	while (*String) Serial_SendByte((uint8_t)*String ++);
}

/** 函数：带长度限制的格式化输出，长于191字节的日志会截断 */
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

uint8_t Serial_GetTxBusy(void) { return Serial_TxBusy; }
uint32_t Serial_GetIdleCount(void) { return Serial_IdleCount; }
uint32_t Serial_GetDropCount(void) { return Serial_DropCount; }
uint32_t Serial_GetErrorCount(void) { return Serial_ErrorCount; }

/** 函数：接收DMA半满、全满、错误中断；连续数据即使没有IDLE也会搬运 */
void DMA1_Channel5_IRQHandler(void)
{
	if (DMA_GetITStatus(DMA1_IT_TE5) != RESET)
	{
		Serial_ErrorCount ++;
		DMA_ClearITPendingBit(DMA1_IT_GL5);
		DMA_Cmd(DMA1_Channel5, DISABLE);
		DMA_SetCurrDataCounter(DMA1_Channel5, SERIAL_DMA_SIZE);
		Serial_DMAReadPos = 0;
		DMA_Cmd(DMA1_Channel5, ENABLE);
		return;
	}
	DMA_ClearITPendingBit(DMA1_IT_HT5 | DMA1_IT_TC5);
	Serial_CollectDMA();
}

/** 函数：发送DMA完成仅表示数据已写到DR，线路最终完成还要看USART TC */
void DMA1_Channel4_IRQHandler(void)
{
	if (DMA_GetITStatus(DMA1_IT_TE4) != RESET) Serial_ErrorCount ++;
	DMA_ClearITPendingBit(DMA1_IT_GL4);
	DMA_Cmd(DMA1_Channel4, DISABLE);
	Serial_TxBusy = 0;
}

/** 函数：空闲和线路错误中断，读SR再读DR清标志，不停止循环接收DMA */
void USART1_IRQHandler(void)
{
	uint32_t Status = USART1->SR;
	if (Status & (USART_SR_IDLE | USART_SR_ORE | USART_SR_FE | USART_SR_NE))
	{
		volatile uint32_t Temp = USART1->DR;
		(void)Temp;
		if (Status & (USART_SR_ORE | USART_SR_FE | USART_SR_NE)) Serial_ErrorCount ++;
		if (Status & USART_SR_IDLE) Serial_IdleCount ++;
		Serial_CollectDMA();
	}
}
