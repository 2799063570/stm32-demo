#include "stm32f10x.h"                  // Device header
#include "OLED_SPI.h"
#include "Delay.h"

/*SPI1重映射：PB3=SCK、PB5=MOSI，PB0=CS、PB1=DC、PB12=RES，MISO不用*/
#define OLED_W_CS(Value) GPIO_WriteBit(GPIOB, GPIO_Pin_0, (Value) ? Bit_SET : Bit_RESET)
#define OLED_W_DC(Value) GPIO_WriteBit(GPIOB, GPIO_Pin_1, (Value) ? Bit_SET : Bit_RESET)

/**
  * 函    数：初始化硬件SPI1单线发送，Mode0、2.25MHz，不占DMA
  * 参    数：无
  * 返 回 值：无；RES初始低，后续复位时序由OLED_Service分步执行
  */
void OLED_SPI_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	SPI_InitTypeDef SPI_InitStructure;
	/*开启时钟、关闭JTAG保留SWD，释放PB3；不影响PA13/PA14下载*/
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB | RCC_APB2Periph_AFIO | RCC_APB2Periph_SPI1, ENABLE);
	GPIO_PinRemapConfig(GPIO_Remap_SPI1, ENABLE);
	GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable, ENABLE); // SWJ配置最后写，避免其他MAPR重映射覆盖调试配置
	/*控制引脚初始化，先不选中屏幕、保持复位*/
	GPIO_SetBits(GPIOB, GPIO_Pin_0);
	GPIO_ResetBits(GPIOB, GPIO_Pin_1 | GPIO_Pin_12);
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_12;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
	GPIO_Init(GPIOB, &GPIO_InitStructure);
	/*SPI引脚初始化：没有接收线，PA6/PA7可继续用于编码器*/
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3 | GPIO_Pin_5;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOB, &GPIO_InitStructure);
	/*SPI初始化*/
	SPI_I2S_DeInit(SPI1);
	SPI_StructInit(&SPI_InitStructure);
	SPI_InitStructure.SPI_Direction = SPI_Direction_1Line_Tx;
	SPI_InitStructure.SPI_Mode = SPI_Mode_Master;
	SPI_InitStructure.SPI_DataSize = SPI_DataSize_8b;
	SPI_InitStructure.SPI_CPOL = SPI_CPOL_Low;
	SPI_InitStructure.SPI_CPHA = SPI_CPHA_1Edge;
	SPI_InitStructure.SPI_NSS = SPI_NSS_Soft;
	SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_32;
	SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB;
	SPI_Init(SPI1, &SPI_InitStructure);
	SPI_NSSInternalSoftwareConfig(SPI1, SPI_NSSInternalSoft_Set);
	SPI_Cmd(SPI1, ENABLE);
}

void OLED_SPI_SetReset(uint8_t Level)
{
	GPIO_WriteBit(GPIOB, GPIO_Pin_12, Level ? Bit_SET : Bit_RESET);
}

/** 函数：整笔事务共用2ms截止时间，不在中断内调用 */
static uint8_t OLED_SPI_Wait(uint16_t Flag, FlagStatus Wanted, uint32_t Start)
{
	while (SPI_I2S_GetFlagStatus(SPI1, Flag) != Wanted)
	{
		if ((uint32_t)(Delay_GetTick() - Start) >= 2) return 0;
	}
	return 1;
}

/**
  * 函    数：一个有界SPI事务，最后等BSY清零才释放CS
  * 参    数：Control=0命令、0x40数据；Length范围1~8
  * 返 回 值：1代表STM32传输完成，SPI没有ACK，不能证明屏幕已连接
  */
uint8_t OLED_SPI_Write(uint8_t Control, const uint8_t *Data, uint8_t Length)
{
	uint8_t Index, Result = 1;
	uint32_t Start;
	if (Data == 0 || Length == 0 || Length > 8) return 0;
	Start = Delay_GetTick();
	OLED_W_DC(Control == 0x40);
	OLED_W_CS(0);
	for (Index = 0; Index < Length; Index ++)
	{
		if (!OLED_SPI_Wait(SPI_I2S_FLAG_TXE, SET, Start)) { Result = 0; break; }
		SPI_I2S_SendData(SPI1, Data[Index]);
	}
	if (Result) Result = OLED_SPI_Wait(SPI_I2S_FLAG_TXE, SET, Start);
	if (Result) Result = OLED_SPI_Wait(SPI_I2S_FLAG_BSY, RESET, Start);
	OLED_W_CS(1);
	return Result;
}
