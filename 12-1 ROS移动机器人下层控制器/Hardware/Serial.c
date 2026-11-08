/**
 * 上位机二进制串口：USART1 RX 循环 DMA + TX 单帧 DMA。
 * 中断只记录事件；main 复制接收快照、解析协议并检查错误。
 */
#include "Serial.h"
#include "Delay.h"
#include <string.h>
#define SERIAL_RX_SIZE 256u
/* RX 由 DMA 写、main 读；TX 由 main 复制后交给 DMA。 */
static volatile uint8_t Serial_Rx[SERIAL_RX_SIZE];
static uint8_t Serial_Tx[48];
/* Wraps 在 RX TC 中断累加；Consumed 是 main 已消费的累计字节位置。 */
static volatile uint32_t Serial_Wraps, Serial_Error, Serial_Drop;
static volatile uint8_t Serial_Event, Serial_RxFault, Serial_Busy;
static uint32_t Serial_Consumed, Serial_TxTick;

/** 函数：调用者短暂关中断。TC已到但ISR未运行时补一圈；DMA本身仍运行。 */
static uint32_t Serial_Produced(void)
{
	uint32_t Before, After, Remaining, Attempt;
	for (Attempt = 0; Attempt < 4; Attempt++)
	{
		Before = DMA1->ISR & DMA1_FLAG_TC5;
		Remaining = DMA_GetCurrDataCounter(DMA1_Channel5);
		After = DMA1->ISR & DMA1_FLAG_TC5;
		/* 重装瞬间NDTR可能为0，不能把TC补圈与零计数各算一整圈。 */
		if (Before == After && Remaining)
			return (Serial_Wraps + (After ? 1u : 0u)) * SERIAL_RX_SIZE + SERIAL_RX_SIZE - Remaining;
	}
	/* 有界重试，DMA异常不允许把main永久卡在临界区。 */
	Serial_Error++;
	Serial_RxFault = 1;
	return Serial_Consumed;
}

/** USART1：PA9=TX 复用推挽、PA10=RX 上拉，115200/8N1。
 * DMA1 CH5 循环接收 256 字节，CH4 普通模式发送；通信抢占优先级为 2。 */
void Serial_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	USART_InitTypeDef USART_InitStructure;
	DMA_InitTypeDef DMA_InitStructure;
	NVIC_InitTypeDef NVIC_InitStructure;
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_USART1, ENABLE);
	RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
	GPIO_Init(GPIOA, &GPIO_InitStructure);
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
	GPIO_Init(GPIOA, &GPIO_InitStructure);
	USART_StructInit(&USART_InitStructure);
	USART_InitStructure.USART_BaudRate = 115200;
	USART_Init(USART1, &USART_InitStructure);
	/* RX：外设 -> 内存，地址递增，循环模式，HT/TC 唤醒前台，TE 报错。 */
	DMA_DeInit(DMA1_Channel5);
	DMA_StructInit(&DMA_InitStructure);
	DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&USART1->DR;
	DMA_InitStructure.DMA_MemoryBaseAddr = (uint32_t)Serial_Rx;
	DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralSRC;
	DMA_InitStructure.DMA_BufferSize = SERIAL_RX_SIZE;
	DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
	DMA_InitStructure.DMA_Mode = DMA_Mode_Circular;
	DMA_InitStructure.DMA_Priority = DMA_Priority_High;
	DMA_Init(DMA1_Channel5, &DMA_InitStructure);
	DMA_ITConfig(DMA1_Channel5, DMA_IT_HT | DMA_IT_TC | DMA_IT_TE, ENABLE);
	/* TX：内存 -> 外设，普通模式；发送时再设置实际长度和使能通道。 */
	DMA_DeInit(DMA1_Channel4);
	DMA_InitStructure.DMA_MemoryBaseAddr = (uint32_t)Serial_Tx;
	DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralDST;
	DMA_InitStructure.DMA_Mode = DMA_Mode_Normal;
	DMA_InitStructure.DMA_BufferSize = sizeof(Serial_Tx);
	DMA_Init(DMA1_Channel4, &DMA_InitStructure);
	DMA_ITConfig(DMA1_Channel4, DMA_IT_TC | DMA_IT_TE, ENABLE);
	/* 串口和两路 DMA 使用相同抢占优先级，避免圈数维护相互嵌套。 */
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 2;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannel = USART1_IRQn;
	NVIC_Init(&NVIC_InitStructure);
	NVIC_InitStructure.NVIC_IRQChannel = DMA1_Channel5_IRQn;
	NVIC_Init(&NVIC_InitStructure);
	NVIC_InitStructure.NVIC_IRQChannel = DMA1_Channel4_IRQn;
	NVIC_Init(&NVIC_InitStructure);
	USART_ITConfig(USART1, USART_IT_IDLE, ENABLE);
	USART_ITConfig(USART1, USART_IT_ERR, ENABLE);
	USART_DMACmd(USART1, USART_DMAReq_Rx | USART_DMAReq_Tx, ENABLE);
	DMA_Cmd(DMA1_Channel5, ENABLE);
	USART_Cmd(USART1, ENABLE);
}

/** 函数：前台复制小块；复制后再次检查生产位置，不能用关IRQ来阻止DMA写入。 */
uint16_t Serial_ReadBlock(uint8_t *Data, uint16_t Capacity)
{
	uint32_t Produced, ProducedAfterCopy, Primask;
	uint16_t Count, Index;
	Primask = __get_PRIMASK();
	__disable_irq();
	if (!Serial_Event && !Serial_RxFault)
	{
		__set_PRIMASK(Primask);
		return 0;
	}
	Serial_Event = 0;
	if (Serial_RxFault)
	{
		DMA_Cmd(DMA1_Channel5, DISABLE);
		DMA_ClearFlag(DMA1_FLAG_GL5);
		DMA_SetCurrDataCounter(DMA1_Channel5, SERIAL_RX_SIZE);
		Serial_Wraps = Serial_Consumed = 0;
		Serial_RxFault = 0;
		DMA_Cmd(DMA1_Channel5, ENABLE);
		__set_PRIMASK(Primask);
		return 0;
	}
	/* 第一次快照：确认未消费区域尚未被循环 DMA 覆盖。 */
	Produced = Serial_Produced();
	if ((uint32_t)(Produced - Serial_Consumed) > SERIAL_RX_SIZE)
	{
		Serial_Drop++;
		Serial_Consumed = Produced;
		__set_PRIMASK(Primask);
		return 0;
	}
	Count = (uint16_t)(Produced - Serial_Consumed);
	if (Count > Capacity)
		Count = Capacity;
	__set_PRIMASK(Primask);
	/* 在临界区外复制小块；此时 DMA 仍可能继续写环形缓冲区。 */
	for (Index = 0; Index < Count; Index++)
		Data[Index] = Serial_Rx[(Serial_Consumed + Index) % SERIAL_RX_SIZE];
	/* 第二次快照：复制期间若发生覆盖，整块作废，禁止解析混合数据。 */
	Primask = __get_PRIMASK();
	__disable_irq();
	ProducedAfterCopy = Serial_Produced();
	if (Serial_RxFault || (uint32_t)(ProducedAfterCopy - Serial_Consumed) > SERIAL_RX_SIZE)
	{
		Serial_Drop++;
		Serial_Consumed = ProducedAfterCopy;
		Count = 0;
	}
	else
	{
		Serial_Consumed += Count;
		if (Serial_Consumed != ProducedAfterCopy)
			Serial_Event = 1; /* 有剩余字节，下一轮继续消费 */
	}
	__set_PRIMASK(Primask);
	return Count;
}

/** 非阻塞发送：返回 1 表示已复制并启动 DMA，返回 0 表示忙或长度非法。
 * 最长 48 字节；使用内部发送副本，调用者返回后可复用原数组。 */
uint8_t Serial_SendArrayDMA(const uint8_t *Data, uint16_t Length)
{
	if (Serial_Busy || !Length || Length > sizeof(Serial_Tx))
		return 0;
	memcpy(Serial_Tx, Data, Length);
	DMA_Cmd(DMA1_Channel4, DISABLE);
	DMA_ClearFlag(DMA1_FLAG_GL4);
	DMA_SetCurrDataCounter(DMA1_Channel4, Length);
	Serial_TxTick = Delay_GetTick();
	Serial_Busy = 1;
	DMA_Cmd(DMA1_Channel4, ENABLE);
	return 1;
}

/** 由 main 检查发送超时；20ms 未完成则终止 DMA 并累计通信错误。 */
void Serial_Service(void)
{
	uint32_t Primask = __get_PRIMASK();
	__disable_irq();
	if (Serial_Busy && (uint32_t)(Delay_GetTick() - Serial_TxTick) >= 20)
	{
		DMA_Cmd(DMA1_Channel4, DISABLE);
		DMA_ClearFlag(DMA1_FLAG_GL4);
		Serial_Busy = 0;
		Serial_Error++;
	}
	__set_PRIMASK(Primask);
}

/** 返回 UART/DMA/发送超时等错误的累计次数；应用检测变化后锁定 FAULT。 */
uint32_t Serial_GetErrorCount(void)
{
	return Serial_Error;
}

/** 返回接收覆盖累计次数；每次增加表示至少有一块数据无法可靠消费。 */
uint32_t Serial_GetDropCount(void)
{
	return Serial_Drop;
}

/** 函数：IRQ只记事件/圈数，不拷贝和解析协议；同优先级避免圈数并发。 */
void DMA1_Channel5_IRQHandler(void)
{
	if (DMA_GetITStatus(DMA1_IT_TE5) != RESET)
	{
		Serial_Error++;
		Serial_RxFault = 1;
	}
	if (DMA_GetITStatus(DMA1_IT_TC5) != RESET)
		Serial_Wraps++;
	DMA_ClearITPendingBit(DMA1_IT_GL5);
	Serial_Event = 1;
}

/** 发送 DMA 完成/错误：释放发送缓冲区；DMA TC 不代表尾字节已离开串口。 */
void DMA1_Channel4_IRQHandler(void)
{
	if (DMA_GetITStatus(DMA1_IT_TE4) != RESET)
		Serial_Error++;
	DMA_Cmd(DMA1_Channel4, DISABLE);
	DMA_ClearITPendingBit(DMA1_IT_GL4);
	Serial_Busy = 0;
}

/** IDLE 仅提示 main 消费数据，不作为帧边界；读 SR 后读 DR 清除相关状态。 */
void USART1_IRQHandler(void)
{
	uint32_t Status = USART1->SR;
	if (Status & (USART_SR_IDLE | USART_SR_ORE | USART_SR_FE | USART_SR_NE))
	{
		volatile uint32_t Temp = USART1->DR;
		(void)Temp;
		if (Status & (USART_SR_ORE | USART_SR_FE | USART_SR_NE))
		{
			Serial_Error++;
			Serial_RxFault = 1;
		}
		Serial_Event = 1;
	}
}
