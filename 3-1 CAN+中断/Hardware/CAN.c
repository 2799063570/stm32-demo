#include "stm32f10x.h"                  // Device header
#include "CAN.h"
#include "Config.h"
#include "Delay.h"

static CanRxMsg MyCAN_RxQueue[8];
static volatile uint8_t MyCAN_Head, MyCAN_Tail;
static volatile uint8_t MyCAN_Mailbox = 0xFF;
static volatile uint8_t MyCAN_TxResult;              // 0无结果，1发送成功，2发送失败
static uint32_t MyCAN_StartTick;
static volatile uint32_t MyCAN_DropCount, MyCAN_OverrunCount;

/**
  * 函    数：CAN1初始化，500kbit/s
  * 参    数：无
  * 返 回 值：1初始化成功，0失败
  */
uint8_t MyCAN_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	CAN_InitTypeDef CAN_InitStructure;
	CAN_FilterInitTypeDef CAN_FilterInitStructure;
	NVIC_InitTypeDef NVIC_InitStructure;
	/*开启时钟及重映射*/
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB | RCC_APB2Periph_AFIO, ENABLE);
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_CAN1, ENABLE);
	GPIO_PinRemapConfig(GPIO_Remap1_CAN1, ENABLE);    // PB8接收，PB9发送
	/*GPIO初始化*/
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOB, &GPIO_InitStructure);
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
	GPIO_Init(GPIOB, &GPIO_InitStructure);
	/*CAN初始化*/
	CAN_DeInit(CAN1);
	CAN_StructInit(&CAN_InitStructure);
	CAN_InitStructure.CAN_Mode = CAN_LOOPBACK ? CAN_Mode_LoopBack : CAN_Mode_Normal;
	CAN_InitStructure.CAN_ABOM = ENABLE;             // 总线关闭后允许硬件自动恢复
	CAN_InitStructure.CAN_NART = ENABLE;             // 单次尝试，避免无ACK时无限重发
	CAN_InitStructure.CAN_RFLM = ENABLE;
	CAN_InitStructure.CAN_SJW = CAN_SJW_1tq;
	CAN_InitStructure.CAN_BS1 = CAN_BS1_13tq;
	CAN_InitStructure.CAN_BS2 = CAN_BS2_4tq;
	CAN_InitStructure.CAN_Prescaler = 4;             // 36MHz / 4 / (1+13+4) = 500kbit/s
	if (CAN_Init(CAN1, &CAN_InitStructure) != CAN_InitStatus_Success) return 0;
	/*过滤器初始化：掩码全零，接收全部ID*/
	CAN_FilterInitStructure.CAN_FilterNumber = 0;
	CAN_FilterInitStructure.CAN_FilterMode = CAN_FilterMode_IdMask;
	CAN_FilterInitStructure.CAN_FilterScale = CAN_FilterScale_32bit;
	CAN_FilterInitStructure.CAN_FilterIdHigh = 0;
	CAN_FilterInitStructure.CAN_FilterIdLow = 0;
	CAN_FilterInitStructure.CAN_FilterMaskIdHigh = 0;
	CAN_FilterInitStructure.CAN_FilterMaskIdLow = 0;
	CAN_FilterInitStructure.CAN_FilterFIFOAssignment = CAN_Filter_FIFO0;
	CAN_FilterInitStructure.CAN_FilterActivation = ENABLE;
	CAN_FilterInit(&CAN_FilterInitStructure);
	/*NVIC配置*/
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 2;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_InitStructure.NVIC_IRQChannel = USB_LP_CAN1_RX0_IRQn;
	NVIC_Init(&NVIC_InitStructure);
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
	NVIC_InitStructure.NVIC_IRQChannel = USB_HP_CAN1_TX_IRQn;
	NVIC_Init(&NVIC_InitStructure);
	CAN_ITConfig(CAN1, CAN_IT_FMP0 | CAN_IT_TME, ENABLE);
	return 1;
}

/**
  * 函    数：异步发送标准数据帧，完成结果由发送中断报告
  * 参    数：ID范围0~0x7FF；Data数据地址；Length范围0~8
  * 返 回 值：1已提交，0忙或参数错误
  */
uint8_t MyCAN_Send(uint16_t ID, const uint8_t *Data, uint8_t Length)
{
	CanTxMsg Message;
	uint8_t Index, Mailbox;
	uint32_t Primask;
	if (ID > 0x7FF || Length > 8 || (Length && Data == 0)) return 0;
	Message.StdId = ID;
	Message.ExtId = 0;
	Message.IDE = CAN_Id_Standard;
	Message.RTR = CAN_RTR_Data;
	Message.DLC = Length;
	for (Index = 0; Index < 8; Index ++) Message.Data[Index] = Index < Length ? Data[Index] : 0;
	Primask = __get_PRIMASK();
	__disable_irq();
	if (MyCAN_Mailbox != 0xFF || MyCAN_TxResult)
	{
		__set_PRIMASK(Primask);
		return 0;
	}
	Mailbox = CAN_Transmit(CAN1, &Message);
	if (Mailbox != CAN_TxStatus_NoMailBox)
	{
		MyCAN_Mailbox = Mailbox;
		MyCAN_StartTick = Delay_GetTick();
	}
	__set_PRIMASK(Primask);
	return Mailbox != CAN_TxStatus_NoMailBox;
}

/** 函数：主循环发送超时检查，无阻塞等待 */
void MyCAN_Service(void)
{
	uint32_t Primask = __get_PRIMASK();
	__disable_irq();
	if (MyCAN_Mailbox != 0xFF && (uint32_t)(Delay_GetTick() - MyCAN_StartTick) >= 50)
	{
		CAN_CancelTransmit(CAN1, MyCAN_Mailbox);
		MyCAN_Mailbox = 0xFF;
		MyCAN_TxResult = 2;
	}
	__set_PRIMASK(Primask);
}

/** 函数：读取并清除发送结果，返回0等待、1成功、2失败 */
uint8_t MyCAN_GetTxResult(void)
{
	uint8_t Result = MyCAN_TxResult;
	if (Result) MyCAN_TxResult = 0;                   // 结果产生后邮箱已空，不会再写入下一次结果
	return Result;
}

/** 函数：从接收队列取帧，队列8格，有效容量7帧 */
uint8_t MyCAN_Receive(CanRxMsg *Message)
{
	uint8_t Tail = MyCAN_Tail;
	if (Message == 0 || Tail == MyCAN_Head) return 0;
	__DMB();
	*Message = MyCAN_RxQueue[Tail];
	__DMB();
	MyCAN_Tail = (Tail + 1) % 8;
	return 1;
}

uint32_t MyCAN_GetDropCount(void) { return MyCAN_DropCount; }
uint32_t MyCAN_GetOverrunCount(void) { return MyCAN_OverrunCount; }

/** 函数：FIFO0接收中断，一次最多取硬件FIFO深度3帧 */
void USB_LP_CAN1_RX0_IRQHandler(void)
{
	uint8_t Count;
	if (CAN_GetFlagStatus(CAN1, CAN_FLAG_FOV0) != RESET)
	{
		MyCAN_OverrunCount ++;
		CAN_ClearFlag(CAN1, CAN_FLAG_FOV0);
	}
	for (Count = 0; Count < 3 && CAN_MessagePending(CAN1, CAN_FIFO0); Count ++)
	{
		CanRxMsg Message;
		uint8_t Next = (MyCAN_Head + 1) % 8;
		CAN_Receive(CAN1, CAN_FIFO0, &Message);
		if (Next == MyCAN_Tail) MyCAN_DropCount ++;
		else
		{
			MyCAN_RxQueue[MyCAN_Head] = Message;
			__DMB();
			MyCAN_Head = Next;
		}
	}
}

/** 函数：发送完成中断，清RQCP，记录成功或失败，不在ISR打印 */
void USB_HP_CAN1_TX_IRQHandler(void)
{
	if (MyCAN_Mailbox != 0xFF)
	{
		uint8_t Status = CAN_TransmitStatus(CAN1, MyCAN_Mailbox);
		if (Status != CAN_TxStatus_Pending)
		{
			MyCAN_TxResult = Status == CAN_TxStatus_Ok ? 1 : 2;
			MyCAN_Mailbox = 0xFF;
		}
	}
	CAN_ClearITPendingBit(CAN1, CAN_IT_TME);
}
