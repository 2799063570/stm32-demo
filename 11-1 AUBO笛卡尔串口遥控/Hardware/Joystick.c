#include "stm32f10x.h"
#include "Joystick.h"
#include "Delay.h"

/** 函数：ADC等待均有超时；依赖Delay_Init已开启1ms时基 */
static uint8_t Joystick_WaitCalibration(uint8_t Reset)
{
	uint32_t Start = Delay_GetTick();
	while (Reset ? ADC_GetResetCalibrationStatus(ADC1) : ADC_GetCalibrationStatus(ADC1))
	{
		if ((uint32_t)(Delay_GetTick() - Start) >= 10) return 0;
	}
	return 1;
}

/** 函数：PA0/PA1为ADC1通道0/1；PA2上拉输入，按下接地 */
uint8_t Joystick_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	ADC_InitTypeDef ADC_InitStructure;
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_ADC1, ENABLE);
	RCC_ADCCLKConfig(RCC_PCLK2_Div6);              // 72/6=12MHz，低于14MHz上限
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;
	GPIO_Init(GPIOA, &GPIO_InitStructure);
	ADC_StructInit(&ADC_InitStructure);
	ADC_InitStructure.ADC_Mode = ADC_Mode_Independent;
	ADC_InitStructure.ADC_ScanConvMode = DISABLE;
	ADC_InitStructure.ADC_ContinuousConvMode = DISABLE;
	ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None;
	ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;
	ADC_InitStructure.ADC_NbrOfChannel = 1;
	ADC_Init(ADC1, &ADC_InitStructure);
	ADC_Cmd(ADC1, ENABLE);
	Delay_ms(1);                                 // ADC上电稳定后再校准
	ADC_ResetCalibration(ADC1);
	if (!Joystick_WaitCalibration(1)) return 0;
	ADC_StartCalibration(ADC1);
	return Joystick_WaitCalibration(0);
}

static uint8_t Joystick_ReadChannel(uint8_t Channel, uint16_t *Value)
{
	uint32_t Start = Delay_GetTick();
	ADC_RegularChannelConfig(ADC1, Channel, 1, ADC_SampleTime_239Cycles5);
	ADC_ClearFlag(ADC1, ADC_FLAG_EOC);
	ADC_SoftwareStartConvCmd(ADC1, ENABLE);
	while (ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC) == RESET)
	{
		if ((uint32_t)(Delay_GetTick() - Start) >= 2) return 0;
	}
	*Value = ADC_GetConversionValue(ADC1);
	return 1;
}

uint8_t Joystick_Read(uint16_t *X, uint16_t *Y, uint8_t *Pressed)
{
	uint16_t RawX, RawY;
	if (!Joystick_ReadChannel(ADC_Channel_0, &RawX) ||
		!Joystick_ReadChannel(ADC_Channel_1, &RawY)) return 0;
	*X = RawX;
	*Y = RawY;
	*Pressed = GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_2) == Bit_RESET;
	return 1;
}
