#include "stm32f10x.h"                  // Device header
#include "OLED_SPI.h"

/*普通GPIO软件SPI：不占用SPI外设，不重映射，不关闭JTAG/SWD*/
#define OLED_W_CS(Value) GPIO_WriteBit(GPIOB, GPIO_Pin_0, (Value) ? Bit_SET : Bit_RESET)
#define OLED_W_DC(Value) GPIO_WriteBit(GPIOB, GPIO_Pin_1, (Value) ? Bit_SET : Bit_RESET)
#define OLED_W_SCK(Value) GPIO_WriteBit(GPIOB, GPIO_Pin_14, (Value) ? Bit_SET : Bit_RESET)
#define OLED_W_MOSI(Value) GPIO_WriteBit(GPIOB, GPIO_Pin_15, (Value) ? Bit_SET : Bit_RESET)

/** 函数：五个引脚全部配置为普通推挽输出；PB14不是硬件SPI2时钟 */
void OLED_SPI_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
	GPIO_SetBits(GPIOB, GPIO_Pin_0);
	GPIO_ResetBits(GPIOB, GPIO_Pin_1 | GPIO_Pin_12 | GPIO_Pin_14 | GPIO_Pin_15);
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_12 | GPIO_Pin_14 | GPIO_Pin_15;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
	GPIO_Init(GPIOB, &GPIO_InitStructure);
}

void OLED_SPI_SetReset(uint8_t Level)
{
	GPIO_WriteBit(GPIOB, GPIO_Pin_12, Level ? Bit_SET : Bit_RESET);
}

/**
  * 函    数：Mode0、MSB先行，每次最多8字节，由主循环中的OLED服务调用
  * 说    明：不关中断、不等待硬件标志；中断只延长时钟间隔
  * 返 回 值：1已输出时序，0参数非法；单向SPI不能检测屏幕是否连接
  */
uint8_t OLED_SPI_Write(uint8_t Control, const uint8_t *Data, uint8_t Length)
{
	uint8_t Index, Bit, Value;
	if (Data == 0 || Length == 0 || Length > 8) return 0;
	OLED_W_DC(Control == 0x40);
	OLED_W_CS(0);
	for (Index = 0; Index < Length; Index ++)
	{
		Value = Data[Index];
		for (Bit = 0; Bit < 8; Bit ++)
		{
			OLED_W_MOSI(Value & 0x80);
			OLED_W_SCK(1);
			OLED_W_SCK(0);
			Value <<= 1;
		}
	}
	OLED_W_CS(1);
	return 1;
}
