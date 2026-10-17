#include "stm32f10x.h"                  // Device header
#include "Motor.h"
#include "PWM.h"

static int8_t Motor_LastDirection;
static int16_t Motor_Output;

/**
  * 函    数：TB6612单通道接口初始化，默认待机不转
  * 参    数：无
  * 返 回 值：无
  */
void Motor_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	/*开启时钟并预置低电平，先让STBY关闭*/
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB, ENABLE);
	GPIO_ResetBits(GPIOB, GPIO_Pin_13);
	GPIO_ResetBits(GPIOA, GPIO_Pin_4 | GPIO_Pin_5);
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_13;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOB, &GPIO_InitStructure);            // PB13=STBY，低电平进入待机
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4 | GPIO_Pin_5;
	GPIO_Init(GPIOA, &GPIO_InitStructure);            // PA4=AIN1，PA5=AIN2
	PWM_Init();                                     // PA8=PWMA
	Motor_Stop();
}

/** 函数：立即待机，输出高阻；不是主动短路制动 */
void Motor_Stop(void)
{
	GPIO_ResetBits(GPIOB, GPIO_Pin_13);
	PWM_SetCompare1(0);
	GPIO_ResetBits(GPIOA, GPIO_Pin_4 | GPIO_Pin_5);
	Motor_Output = 0;
	Motor_LastDirection = 0;
}

/**
  * 函    数：设置有符号PWM，切换方向时先空出一个10ms控制周期
  * 参    数：PWM 范围-1000~1000，正负表示方向
  * 返 回 值：无
  */
void Motor_SetPWM(int16_t PWM)
{
	int8_t Direction;
	if (PWM > 1000) PWM = 1000;
	if (PWM < -1000) PWM = -1000;
	if (PWM == 0) { Motor_Stop(); return; }
	Direction = PWM > 0 ? 1 : -1;
	if (Motor_LastDirection && Direction != Motor_LastDirection)
	{
		Motor_Stop();
		Motor_LastDirection = Direction;             // 本次不加电，下一控制周期才允许新方向输出
		return;
	}
	GPIO_ResetBits(GPIOB, GPIO_Pin_13);              // 配方向前先关闭输出
	PWM_SetCompare1(0);
	GPIO_WriteBit(GPIOA, GPIO_Pin_4, Direction > 0 ? Bit_SET : Bit_RESET);
	GPIO_WriteBit(GPIOA, GPIO_Pin_5, Direction > 0 ? Bit_RESET : Bit_SET);
	PWM_SetCompare1(PWM > 0 ? PWM : -PWM);
	GPIO_SetBits(GPIOB, GPIO_Pin_13);
	Motor_LastDirection = Direction;
	Motor_Output = PWM;
}

int16_t Motor_GetPWM(void) { return Motor_Output; }
