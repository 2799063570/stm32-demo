#include "stm32f10x.h"                  // Device header
#include "IIC.h"
#include "Delay.h"
#include <string.h>

typedef enum
{
	IIC_IDLE, IIC_START_W, IIC_ADDR_W, IIC_REG_SENT, IIC_TX_DMA,
	IIC_TX_BTF, IIC_RESTART, IIC_ADDR_R, IIC_RX_DMA, IIC_STOPPING
} IIC_StateTypeDef;

static volatile IIC_StateTypeDef IIC_State;
static volatile IIC_ResultTypeDef IIC_Result;
static volatile uint8_t IIC_ResultReady;
static uint8_t IIC_Address, IIC_Register, IIC_IsRead;
static uint8_t IIC_TxBuffer[33];                     // 寄存器地址+最多32字节数据
static uint8_t *IIC_RxBuffer;
static uint16_t IIC_Length;
static uint32_t IIC_StartTick;
static volatile uint32_t IIC_StopTick;

/** 函数：准备DMA通道，通道始终先关闭再改计数器 */
static void IIC_ConfigDMA(DMA_Channel_TypeDef *Channel, uint8_t *Buffer, uint16_t Length, uint32_t Direction)
{
	DMA_InitTypeDef DMA_InitStructure;
	DMA_Cmd(Channel, DISABLE);
	DMA_DeInit(Channel);
	DMA_StructInit(&DMA_InitStructure);
	DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&I2C2->DR;
	DMA_InitStructure.DMA_MemoryBaseAddr = (uint32_t)Buffer;
	DMA_InitStructure.DMA_DIR = Direction;
	DMA_InitStructure.DMA_BufferSize = Length;
	DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
	DMA_InitStructure.DMA_Mode = DMA_Mode_Normal;
	DMA_InitStructure.DMA_Priority = DMA_Priority_High;
	DMA_Init(Channel, &DMA_InitStructure);
	DMA_ITConfig(Channel, DMA_IT_TC | DMA_IT_TE, ENABLE);
}

/** 函数：开始停止/错误收尾；最终完成由主循环确认STOP释放 */
static void IIC_Stop(IIC_ResultTypeDef Result)
{
	uint32_t Status = I2C2->SR1;
	I2C_ITConfig(I2C2, I2C_IT_EVT | I2C_IT_BUF | I2C_IT_ERR, DISABLE);
	I2C_DMACmd(I2C2, DISABLE);
	DMA_Cmd(DMA1_Channel4, DISABLE);
	DMA_Cmd(DMA1_Channel5, DISABLE);
	DMA_ClearFlag(DMA1_FLAG_GL4 | DMA1_FLAG_GL5);
	I2C_DMALastTransferCmd(I2C2, DISABLE);
	if (IIC_IsRead) I2C_AcknowledgeConfig(I2C2, DISABLE);
	/*仲裁丢失时不允许向其他主机正在使用的总线发送STOP*/
	if (!(Status & I2C_SR1_ARLO) && (I2C2->SR2 & I2C_SR2_MSL)) I2C_GenerateSTOP(I2C2, ENABLE);
	I2C_ClearFlag(I2C2, I2C_FLAG_AF);
	I2C_ClearFlag(I2C2, I2C_FLAG_BERR);
	I2C_ClearFlag(I2C2, I2C_FLAG_ARLO);
	I2C_ClearFlag(I2C2, I2C_FLAG_OVR);
	IIC_Result = Result;
	IIC_StopTick = Delay_GetTick();
	IIC_State = IIC_STOPPING;
}

/**
  * 函    数：硬件I2C2和相关DMA、中断初始化
  * 参    数：无
  * 返 回 值：无
  */
void IIC_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	I2C_InitTypeDef I2C_InitStructure;
	NVIC_InitTypeDef NVIC_InitStructure;
	/*开启时钟*/
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_I2C2, ENABLE);
	RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);
	/*GPIO初始化*/
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_OD;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10 | GPIO_Pin_11;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOB, &GPIO_InitStructure);            // PB10=SCL、PB11=SDA，需外部上拉
	/*I2C初始化*/
	I2C_DeInit(I2C2);
	I2C_StructInit(&I2C_InitStructure);
	I2C_InitStructure.I2C_ClockSpeed = 100000;
	I2C_InitStructure.I2C_Ack = I2C_Ack_Enable;
	I2C_Init(I2C2, &I2C_InitStructure);
	I2C_Cmd(I2C2, ENABLE);
	/*NVIC配置：所有IIC状态机中断同一抢占优先级，防止状态嵌套修改*/
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 2;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_InitStructure.NVIC_IRQChannel = I2C2_ER_IRQn;
	NVIC_Init(&NVIC_InitStructure);
	NVIC_InitStructure.NVIC_IRQChannel = DMA1_Channel4_IRQn;
	NVIC_Init(&NVIC_InitStructure);
	NVIC_InitStructure.NVIC_IRQChannel = DMA1_Channel5_IRQn;
	NVIC_Init(&NVIC_InitStructure);
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
	NVIC_InitStructure.NVIC_IRQChannel = I2C2_EV_IRQn;
	NVIC_Init(&NVIC_InitStructure);
	IIC_State = IIC_IDLE;
}

/** 函数：启动事务；内部调用，参数在临界区中发布给中断 */
static IIC_ResultTypeDef IIC_Start(uint8_t Address, uint8_t Reg, uint8_t *Data, uint16_t Length, uint8_t Read)
{
	uint32_t Primask = __get_PRIMASK();
	if (Address < 8 || Address > 0x77 || Data == 0 || Length == 0 || Length > 32 || (Read && Length < 2)) return IIC_ARGUMENT;
	__disable_irq();
	if (IIC_State != IIC_IDLE || IIC_ResultReady || I2C_GetFlagStatus(I2C2, I2C_FLAG_BUSY) == SET)
	{
		__set_PRIMASK(Primask);
		return IIC_BUSY;                              // 旧结果未消费时也不允许覆盖
	}
	IIC_Address = Address;                          // API统一使用未左移的7位地址
	IIC_Register = Reg;
	IIC_IsRead = Read;
	IIC_Length = Length;
	IIC_RxBuffer = Data;
	if (!Read)
	{
		IIC_TxBuffer[0] = Reg;
		memcpy(&IIC_TxBuffer[1], Data, Length);       // 写数据在启动时复制，外部数组可立即复用
	}
	IIC_StartTick = Delay_GetTick();
	IIC_Result = IIC_BUSY;
	IIC_State = IIC_START_W;
	I2C_NACKPositionConfig(I2C2, I2C_NACKPosition_Current);
	I2C_AcknowledgeConfig(I2C2, ENABLE);
	I2C_ITConfig(I2C2, I2C_IT_ERR | I2C_IT_EVT, ENABLE);
	I2C_GenerateSTART(I2C2, ENABLE);
	__set_PRIMASK(Primask);
	return IIC_OK;                                  // 仅表示成功启动，不表示事务已经完成
}

/** 函数：发起寄存器DMA写入，Length范围1~32 */
IIC_ResultTypeDef IIC_WriteRegsDMA(uint8_t Address, uint8_t Reg, const uint8_t *Data, uint16_t Length)
{
	return IIC_Start(Address, Reg, (uint8_t *)Data, Length, 0);
}

/** 函数：发起寄存器DMA读取，Length范围2~32，Data在完成前必须一直有效 */
IIC_ResultTypeDef IIC_ReadRegsDMA(uint8_t Address, uint8_t Reg, uint8_t *Data, uint16_t Length)
{
	return IIC_Start(Address, Reg, Data, Length, 1);
}

/**
  * 函    数：事务超时检查与STOP完成检查，主循环必须持续调用
  * 参    数：无
  * 返 回 值：无
  */
void IIC_Service(void)
{
	uint32_t Primask = __get_PRIMASK();
	__disable_irq();
	if (IIC_State == IIC_RESTART && (I2C2->SR1 & I2C_SR1_SB))
		I2C_ITConfig(I2C2, I2C_IT_EVT, ENABLE);        // START就绪再开事件中断，避免旧BTF导致中断风暴
	if (IIC_State != IIC_IDLE && IIC_State != IIC_STOPPING && (uint32_t)(Delay_GetTick() - IIC_StartTick) >= 50)
		IIC_Stop(IIC_TIMEOUT);
	if (IIC_State == IIC_STOPPING)
	{
		if (!(I2C2->CR1 & I2C_CR1_STOP) && !(I2C2->SR2 & I2C_SR2_MSL))
		{
			I2C_AcknowledgeConfig(I2C2, ENABLE);
			IIC_State = IIC_IDLE;
			IIC_ResultReady = 1;
		}
		else if ((uint32_t)(Delay_GetTick() - IIC_StopTick) >= 5)
		{
			IIC_Result = IIC_TIMEOUT;                 // 外部总线一直卡住也能向应用报告失败
			IIC_State = IIC_IDLE;
			IIC_ResultReady = 1;
		}
	}
	__set_PRIMASK(Primask);
}

/** 函数：消费事务结果，返回1表示取得新结果，0表示尚未完成 */
uint8_t IIC_GetResult(IIC_ResultTypeDef *Result)
{
	if (Result == 0 || !IIC_ResultReady) return 0;
	*Result = IIC_Result;
	IIC_ResultReady = 0;                             // 状态已IDLE，不会有DMA中断再修改该结果
	return 1;
}

/** 函数：I2C事件中断；START/ADDR/BTF由CPU处理，大批量数据由DMA搬运 */
void I2C2_EV_IRQHandler(void)
{
	uint32_t Status = I2C2->SR1;
	volatile uint32_t Temp;
	if (Status & (I2C_SR1_AF | I2C_SR1_ARLO | I2C_SR1_BERR | I2C_SR1_OVR)) return; // 交给错误中断
	if ((Status & I2C_SR1_SB) && (IIC_State == IIC_START_W || IIC_State == IIC_RESTART))
	{
		uint8_t Read = IIC_State == IIC_RESTART;
		IIC_State = Read ? IIC_ADDR_R : IIC_ADDR_W;
		I2C_Send7bitAddress(I2C2, IIC_Address << 1, Read ? I2C_Direction_Receiver : I2C_Direction_Transmitter);
	}
	else if ((Status & I2C_SR1_ADDR) && IIC_State == IIC_ADDR_W)
	{
		Temp = I2C2->SR2;                            // 先读SR1后读SR2，清ADDR
		(void)Temp;
		if (IIC_IsRead)
		{
			IIC_State = IIC_REG_SENT;
			I2C_SendData(I2C2, IIC_Register);         // 读事务先发送寄存器地址
		}
		else
		{
			IIC_ConfigDMA(DMA1_Channel4, IIC_TxBuffer, IIC_Length + 1, DMA_DIR_PeripheralDST);
			IIC_State = IIC_TX_DMA;
			I2C_ITConfig(I2C2, I2C_IT_EVT, DISABLE);   // DMA期间避免BTF反复进入事件中断
			DMA_Cmd(DMA1_Channel4, ENABLE);
			I2C_DMACmd(I2C2, ENABLE);
		}
	}
	else if ((Status & I2C_SR1_BTF) && IIC_State == IIC_REG_SENT)
	{
		IIC_State = IIC_RESTART;
		I2C_ITConfig(I2C2, I2C_IT_EVT, DISABLE);
		I2C_GenerateSTART(I2C2, ENABLE);              // 保持总线所有权，产生重复起始
	}
	else if ((Status & I2C_SR1_ADDR) && IIC_State == IIC_ADDR_R)
	{
		/*多字节DMA接收：在清ADDR前配置ACK、LAST和DMA，硬件对末字节自动NACK*/
		IIC_ConfigDMA(DMA1_Channel5, IIC_RxBuffer, IIC_Length, DMA_DIR_PeripheralSRC);
		I2C_AcknowledgeConfig(I2C2, ENABLE);
		I2C_DMALastTransferCmd(I2C2, ENABLE);
		DMA_Cmd(DMA1_Channel5, ENABLE);
		I2C_DMACmd(I2C2, ENABLE);
		IIC_State = IIC_RX_DMA;
		I2C_ITConfig(I2C2, I2C_IT_EVT, DISABLE);
		Temp = I2C2->SR1;
		Temp = I2C2->SR2;
		(void)Temp;
	}
	else if ((Status & I2C_SR1_BTF) && IIC_State == IIC_TX_BTF)
	{
		IIC_Stop(IIC_OK);                            // DMA TC只代表DR装载完成，BTF才代表总线发送完成
	}
}

/** 函数：I2C错误中断 */
void I2C2_ER_IRQHandler(void)
{
	if (IIC_State == IIC_IDLE || IIC_State == IIC_STOPPING) return;
	IIC_Stop((I2C2->SR1 & I2C_SR1_AF) ? IIC_NACK : IIC_BUS_ERROR);
}

/** 函数：I2C2_TX的DMA1通道4中断 */
void DMA1_Channel4_IRQHandler(void)
{
	if (IIC_State != IIC_TX_DMA) { DMA_ClearITPendingBit(DMA1_IT_GL4); return; }
	if (DMA_GetITStatus(DMA1_IT_TE4) != RESET) { IIC_Stop(IIC_DMA_ERROR); return; }
	DMA_ClearITPendingBit(DMA1_IT_GL4);
	DMA_Cmd(DMA1_Channel4, DISABLE);
	I2C_DMACmd(I2C2, DISABLE);
	IIC_State = IIC_TX_BTF;
	I2C_ITConfig(I2C2, I2C_IT_EVT, ENABLE);            // 等待最后一字节真正发送完毕
}

/** 函数：I2C2_RX的DMA1通道5中断 */
void DMA1_Channel5_IRQHandler(void)
{
	if (IIC_State != IIC_RX_DMA) { DMA_ClearITPendingBit(DMA1_IT_GL5); return; }
	if (DMA_GetITStatus(DMA1_IT_TE5) != RESET) { IIC_Stop(IIC_DMA_ERROR); return; }
	IIC_Stop(IIC_OK);                                // LAST已使末字节NACK，现在请求STOP
}
