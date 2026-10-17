#include "stm32f10x.h"                  // Device header
#include "Encoder.h"

static uint16_t Encoder_LastCount;

/**
  * 函    数：TIM3编码器接口初始化
  * 参    数：无
  * 返 回 值：无
  */
void Encoder_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure;
	TIM_ICInitTypeDef TIM_ICInitStructure;
	/*开启时钟*/
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
	/*GPIO初始化*/
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_7;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);            // PA6=编码器A，PA7=编码器B
	/*时基单元初始化*/
	TIM_TimeBaseStructInit(&TIM_TimeBaseInitStructure);
	TIM_TimeBaseInitStructure.TIM_Period = 65535;
	TIM_TimeBaseInitStructure.TIM_Prescaler = 0;
	TIM_TimeBaseInit(TIM3, &TIM_TimeBaseInitStructure);
	/*输入滤波初始化*/
	TIM_ICStructInit(&TIM_ICInitStructure);
	TIM_ICInitStructure.TIM_Channel = TIM_Channel_1;
	TIM_ICInitStructure.TIM_ICFilter = 6;
	TIM_ICInit(TIM3, &TIM_ICInitStructure);
	TIM_ICInitStructure.TIM_Channel = TIM_Channel_2;
	TIM_ICInit(TIM3, &TIM_ICInitStructure);
	/*编码器模式必须放在输入捕获初始化之后，避免配置被覆盖*/
	TIM_EncoderInterfaceConfig(TIM3, TIM_EncoderMode_TI12, TIM_ICPolarity_Rising, TIM_ICPolarity_Rising);
	TIM_SetCounter(TIM3, 0);
	Encoder_LastCount = 0;
	TIM_Cmd(TIM3, ENABLE);                           // AB两相四倍频计数，方向由硬件自动判断
}

/**
  * 函    数：获取相邻两次调用间的编码器增量，仅允许一个调用者
  * 参    数：无
  * 返 回 值：有符号增量；相邻采样实际位移绝对值必须小于32768计数
  */
int16_t Encoder_Get(void)
{
	uint16_t Count = TIM_GetCounter(TIM3);
	uint16_t Difference = (uint16_t)(Count - Encoder_LastCount);
	Encoder_LastCount = Count;                       // 不清CNT，避免读计数和清零之间丢脉冲
	return Difference < 32768 ? (int16_t)Difference : (int16_t)((int32_t)Difference - 65536);
}

/** 函数：只读原始计数，不影响Encoder_Get的采样基准 */
uint16_t Encoder_GetCounter(void)
{
	return TIM_GetCounter(TIM3);
}
