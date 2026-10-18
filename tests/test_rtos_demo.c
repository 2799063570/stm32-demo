#include "stm32f10x.h"
#include "DemoCommand.h"
#include "OLED_SPI.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

GPIO_TypeDef Test_GPIOB;
static unsigned ClockEnabled, Configured, Watching, Starts, Ends, Bits, Writes;
static unsigned DataMode;
static uint8_t Captured[8];

void RCC_APB2PeriphClockCmd(uint32_t Peripheral, FunctionalState State)
{
	assert(Peripheral == RCC_APB2Periph_GPIOB && State == ENABLE);
	ClockEnabled = 1;
}
void GPIO_Init(GPIO_TypeDef *Port, GPIO_InitTypeDef *Config)
{
	assert(Port == GPIOB && ClockEnabled);
	assert(Config->GPIO_Mode == GPIO_Mode_Out_PP && Config->GPIO_Speed == GPIO_Speed_2MHz);
	assert(Config->GPIO_Pin == (GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_12 | GPIO_Pin_14 | GPIO_Pin_15));
	Configured = 1;
}
static void SetOutput(GPIO_TypeDef *Port, uint16_t Output)
{
	uint16_t Old = Port->Output;
	assert(Port == GPIOB && ClockEnabled);
	Writes ++;
	if (Watching)
	{
		if ((Old & GPIO_Pin_0) && !(Output & GPIO_Pin_0))
		{
			Starts ++;
			DataMode = !!(Output & GPIO_Pin_1);
		}
		if (!(Old & GPIO_Pin_0) && (Output & GPIO_Pin_0)) Ends ++;
		if (!(Old & GPIO_Pin_14) && (Output & GPIO_Pin_14))
		{
			assert(!(Output & GPIO_Pin_0) && Bits < 64);
			assert((unsigned)!!(Output & GPIO_Pin_1) == DataMode);
			Captured[Bits / 8] = (uint8_t)((Captured[Bits / 8] << 1) | !!(Output & GPIO_Pin_15));
			Bits ++;
		}
	}
	Port->Output = Output;
}
void GPIO_SetBits(GPIO_TypeDef *Port, uint16_t Pins) { SetOutput(Port, Port->Output | Pins); }
void GPIO_ResetBits(GPIO_TypeDef *Port, uint16_t Pins) { SetOutput(Port, (uint16_t)(Port->Output & ~Pins)); }
void GPIO_WriteBit(GPIO_TypeDef *Port, uint16_t Pin, BitAction Value)
{
	if (Value) GPIO_SetBits(Port, Pin); else GPIO_ResetBits(Port, Pin);
}

static DemoCommand_Result Feed(DemoCommand_State *State, const char *Text, uint16_t *Period)
{
	DemoCommand_Result Result = DEMO_COMMAND_NONE;
	while (*Text)
	{
		DemoCommand_Result Next = DemoCommand_Push(State, (uint8_t)*Text ++, Period);
		if (Next != DEMO_COMMAND_NONE) { assert(Result == DEMO_COMMAND_NONE); Result = Next; }
	}
	return Result;
}

static void TestCommand(void)
{
	DemoCommand_State State;
	uint16_t Period = 777;
	const char *Invalid[] = {"LED 99\n", "LED 2001\n", "LED -100\n", "LED +100\n", "LED \n",
		"LED 100x\n", "LED 100 \n", "LED 429496729600\n", "led 100\n", "STATU\n", "HELP extra\n"};
	unsigned Index;
	DemoCommand_Init(&State);
	assert(Feed(&State, "\r\n", &Period) == DEMO_COMMAND_NONE);
	assert(Feed(&State, "HELP\r\n", &Period) == DEMO_COMMAND_HELP);
	assert(Feed(&State, "STA", &Period) == DEMO_COMMAND_NONE);
	/*模拟分多次DMA/IDLE到达：没有换行时不能提前执行*/
	assert(Feed(&State, "TUS\n", &Period) == DEMO_COMMAND_STATUS);
	assert(Feed(&State, "LED 100\r\n", &Period) == DEMO_COMMAND_LED && Period == 100);
	assert(Feed(&State, "LED 2000\n", &Period) == DEMO_COMMAND_LED && Period == 2000);
	for (Index = 0; Index < sizeof(Invalid) / sizeof(Invalid[0]); Index ++)
	{
		Period = 777;
		assert(Feed(&State, Invalid[Index], &Period) == DEMO_COMMAND_INVALID);
		assert(Period == 777);
	}
	assert(Feed(&State, "LED 100", &Period) == DEMO_COMMAND_NONE);
	DemoCommand_Invalidate(&State);                  // 模拟丢字节/线路错误
	assert(Feed(&State, "STATUS\n", &Period) == DEMO_COMMAND_INVALID);
	assert(Feed(&State, "STATUS\n", &Period) == DEMO_COMMAND_STATUS);
	for (Index = 0; Index < 100; Index ++) assert(DemoCommand_Push(&State, '9', &Period) == DEMO_COMMAND_NONE);
	assert(Feed(&State, "HELP\n", &Period) == DEMO_COMMAND_INVALID);
	assert(Feed(&State, "HELP\n", &Period) == DEMO_COMMAND_HELP);
	assert(DemoCommand_Push(&State, 0, &Period) == DEMO_COMMAND_NONE);
	assert(Feed(&State, "LED 200\n", &Period) == DEMO_COMMAND_INVALID);
	puts("FreeRTOS demo command boundaries/recovery: PASS");
}

static void TestSPI(void)
{
	static const uint8_t Data[8] = {0xAE, 0x00, 0xFF, 0x81, 0x55, 0xAA, 0x80, 0x01};
	uint8_t Length, Mode;
	unsigned Before;
	OLED_SPI_Init();
	assert(Configured && GPIOB->Output == GPIO_Pin_0); // CS高，SCK低，RES低
	OLED_SPI_SetReset(1);
	assert(GPIOB->Output & GPIO_Pin_12);
	OLED_SPI_SetReset(0);
	assert(!(GPIOB->Output & GPIO_Pin_12));
	Watching = 1;
	for (Mode = 0; Mode < 2; Mode ++)
	{
		for (Length = 1; Length <= 8; Length ++)
		{
			Starts = Ends = Bits = 0;
			memset(Captured, 0, sizeof(Captured));
			assert(OLED_SPI_Write(Mode ? 0x40 : 0, Data, Length));
			assert(Starts == 1 && Ends == 1 && Bits == (unsigned)Length * 8);
			assert(DataMode == Mode && memcmp(Data, Captured, Length) == 0);
			assert((GPIOB->Output & (GPIO_Pin_0 | GPIO_Pin_14)) == GPIO_Pin_0);
		}
	}
	Before = Writes;
	assert(!OLED_SPI_Write(0x40, 0, 1));
	assert(!OLED_SPI_Write(0x40, Data, 0));
	assert(!OLED_SPI_Write(0x40, Data, 9));
	assert(Before == Writes);
	puts("Real GPIO software SPI bit order/CS/DC/reset/length: PASS");
}

int main(void)
{
	TestCommand();
	TestSPI();
	puts("These are host logic tests; FreeRTOS scheduling and electrical timing require a board.");
	return 0;
}
