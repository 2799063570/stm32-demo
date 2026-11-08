/**
 * 双轮编码器：TIM3/TIM4 硬件计数，每次读取返回自上次读取以来的增量。
 * 左右轮下标统一为 0/1；方向标定参数见 RobotConfig.h。
 */
#include "Encoder.h"
#include "RobotConfig.h"
static uint16_t Encoder_Last[2];

/** 函数：TIM3 PA6/7、TIM4 PB6/7，AB相四倍频，读取不清CNT。 */
void Encoder_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	TIM_TimeBaseInitTypeDef TIM_InitStructure;
	TIM_ICInitTypeDef TIM_ICInitStructure;
	uint8_t EncoderIndex;
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB, ENABLE);
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3 | RCC_APB1Periph_TIM4, ENABLE);
	/* PA6/PA7=左轮 TIM3_CH1/CH2；PB6/PB7=右轮 TIM4_CH1/CH2，均上拉输入。 */
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_7;
	GPIO_Init(GPIOA, &GPIO_InitStructure);
	GPIO_Init(GPIOB, &GPIO_InitStructure);
	/* 两组定时器均使用 16 位自由计数、输入滤波和 TI12 四倍频模式。 */
	for (EncoderIndex = 0; EncoderIndex < 2; EncoderIndex++)
	{
		TIM_TypeDef *Timer = EncoderIndex ? TIM4 : TIM3;
		TIM_TimeBaseStructInit(&TIM_InitStructure);
		TIM_InitStructure.TIM_Period = 65535;
		TIM_TimeBaseInit(Timer, &TIM_InitStructure);
		TIM_ICStructInit(&TIM_ICInitStructure);
		TIM_ICInitStructure.TIM_ICFilter = 6;
		TIM_ICInitStructure.TIM_Channel = TIM_Channel_1;
		TIM_ICInit(Timer, &TIM_ICInitStructure);
		TIM_ICInitStructure.TIM_Channel = TIM_Channel_2;
		TIM_ICInit(Timer, &TIM_ICInitStructure);
		TIM_EncoderInterfaceConfig(Timer, TIM_EncoderMode_TI12, TIM_ICPolarity_Rising, TIM_ICPolarity_Rising);
		TIM_SetCounter(Timer, 0);
		Encoder_Last[EncoderIndex] = 0;
		TIM_Cmd(Timer, ENABLE);
	}
}

/** 读取单轮增量计数，Wheel 仅允许 0（左）或 1（右），由 1ms 控制中断调用。
 * CNT 不清零；按 16 位模减法处理回绕，单次实际增量绝对值必须小于 32768。 */
int16_t Encoder_Get(uint8_t Wheel)
{
	uint16_t Count = TIM_GetCounter(Wheel ? TIM4 : TIM3);
	uint16_t Difference = (uint16_t)(Count - Encoder_Last[Wheel]);
	int32_t Delta = Difference < 32768 ? Difference : (int32_t)Difference - 65536;
	Encoder_Last[Wheel] = Count;
	return (int16_t)(Delta * (Wheel ? ROBOT_ENCODER_RIGHT : ROBOT_ENCODER_LEFT));
}
