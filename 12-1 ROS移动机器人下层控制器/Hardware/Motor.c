/**
 * TB6612 电机输出：左右方向、TIM1 双路 PWM、共用 STBY 和实体急停输入。
 * Wheel=0 为左轮/A 通道，Wheel=1 为右轮/B 通道。
 */
#include "Motor.h"
/* 记录每轮上次请求方向：-1 反向、0 停止、1 正向；用于反转空档。 */
static int8_t Motor_Direction[2];

/**
 * 函数：初始化 TB6612 双路电机驱动，以及实体急停输入。
 *
 * 引脚接线（左轮为 A 通道，右轮为 B 通道）：
 *   左轮方向：PA4 -> AIN1，PA5 -> AIN2（普通推挽输出）
 *   右轮方向：PB0 -> BIN1，PB1 -> BIN2（普通推挽输出）
 *   左轮调速：PA8  -> PWMA / TIM1_CH1（复用推挽输出）
 *   右轮调速：PA11 -> PWMB / TIM1_CH4（复用推挽输出）
 *   共用使能：PB13 -> STBY，低电平待机，高电平允许运行
 *   实体急停：PB12 -> 急停按键，内部上拉，按键接 GND，低电平有效
 *
 * 方向电平由 Motor_SetPWM() 设置；PWM 由 TIM1 输出，频率为 20 kHz。
 */
void Motor_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	TIM_TimeBaseInitTypeDef TIM_InitStructure;
	TIM_OCInitTypeDef TIM_OCInitStructure;

	/* 1. 开启方向/待机/急停 GPIO 和 PWM 定时器的时钟。 */
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB | RCC_APB2Periph_TIM1, ENABLE);

	/* 2. 配置输出前预置低电平：驱动待机，左右轮方向输入均为 0。 */
	GPIO_ResetBits(GPIOB, GPIO_Pin_13 | GPIO_Pin_0 | GPIO_Pin_1); /* STBY（电机开启开关）、右轮 BIN1/BIN2 方向控制*/
	GPIO_ResetBits(GPIOA, GPIO_Pin_4 | GPIO_Pin_5);               /* 左轮 AIN1/AIN2 方向控制 */

	/* 3. 方向和待机引脚：普通推挽输出，不是 PWM 定时器输出。 */
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz; /* GPIO 输出速度等级，不是 PWM 频率 */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_13 | GPIO_Pin_0 | GPIO_Pin_1; /* PB13=STBY，PB0=BIN1，PB1=BIN2 */
	GPIO_Init(GPIOB, &GPIO_InitStructure);
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4 | GPIO_Pin_5; /* PA4=AIN1，PA5=AIN2 */
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	/* 4. 实体急停：PB12 上拉输入，按下接地；由 Motor_Emergency() 读取。 */
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12;
	GPIO_Init(GPIOB, &GPIO_InitStructure);

	/* 5. 调速引脚：复用推挽输出，由 TIM1 的 CH1/CH4 产生 PWM。 */
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8 | GPIO_Pin_11; /* PA8=左轮 PWMA，PA11=右轮 PWMB */
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	/* 6. 两轮共用 TIM1 时基：默认 PSC=0，72 MHz / (3599 + 1) = 20 kHz。 */
	TIM_TimeBaseStructInit(&TIM_InitStructure);
	TIM_InitStructure.TIM_Period = 3599;
	TIM_TimeBaseInit(TIM1, &TIM_InitStructure);
	TIM_OCStructInit(&TIM_OCInitStructure);
	TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
	TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
	TIM_OCInitStructure.TIM_Pulse = 0;       /* 初始占空比为 0 */
	TIM_OC1Init(TIM1, &TIM_OCInitStructure); /* CH1 -> PA8，左轮 */
	TIM_OC4Init(TIM1, &TIM_OCInitStructure); /* CH4 -> PA11，右轮 */
	TIM_OC1PreloadConfig(TIM1, TIM_OCPreload_Disable);
	TIM_OC4PreloadConfig(TIM1, TIM_OCPreload_Disable);
	TIM_CtrlPWMOutputs(TIM1, ENABLE); /* TIM1 高级定时器主输出使能 */
	TIM_Cmd(TIM1, ENABLE);

	/* 7. 初始化结束保持停机：STBY=0、两路 PWM=0，并清零方向记录。 */
	Motor_Stop();
}

/** 关闭两路 PWM、拉低共用 STBY 和方向引脚，清除上次方向记录。 */
void Motor_Stop(void)
{
	GPIO_ResetBits(GPIOB, GPIO_Pin_13);
	TIM_SetCompare1(TIM1, 0);
	TIM_SetCompare4(TIM1, 0);
	GPIO_ResetBits(GPIOA, GPIO_Pin_4 | GPIO_Pin_5);
	GPIO_ResetBits(GPIOB, GPIO_Pin_0 | GPIO_Pin_1);
	Motor_Direction[0] = Motor_Direction[1] = 0;
}

/** 设置左右轮有符号 PWM，范围 -1000..1000；符号控制方向，绝对值控制占空比。
 * 反转时本次输出置零；设计调用周期为 1ms。任一轮有输出才拉高共用 STBY。 */
void Motor_SetPWM(int16_t Left, int16_t Right)
{
	int16_t PWMValues[2];
	uint8_t Wheel;
	PWMValues[0] = Left;
	PWMValues[1] = Right;
	/* 左右轮采用相同流程：先关 PWM，再改方向，最后更新占空比。 */
	for (Wheel = 0; Wheel < 2; Wheel++)
	{
		int8_t Direction = PWMValues[Wheel] > 0 ? 1 : (PWMValues[Wheel] < 0 ? -1 : 0);		// 方向 正1 负-1 停止0
		uint16_t DirectionPin1 = Wheel ? GPIO_Pin_0 : GPIO_Pin_4,
		         DirectionPin2 = Wheel ? GPIO_Pin_1 : GPIO_Pin_5;// 获取左右轮子电机的方向引脚
		GPIO_TypeDef *Port = Wheel ? GPIOB : GPIOA;
		if (Motor_Direction[Wheel] && Direction && Direction != Motor_Direction[Wheel])
			PWMValues[Wheel] = 0;
		Motor_Direction[Wheel] = Direction; /* 反转时空出一个1ms周期，下一次才加电 */
		if (Wheel)
			TIM_SetCompare4(TIM1, 0);
		else
			TIM_SetCompare1(TIM1, 0);
		GPIO_WriteBit(Port, DirectionPin1, PWMValues[Wheel] > 0 ? Bit_SET : Bit_RESET);
		GPIO_WriteBit(Port, DirectionPin2, PWMValues[Wheel] < 0 ? Bit_SET : Bit_RESET);
		if (PWMValues[Wheel] > 1000)
			PWMValues[Wheel] = 1000;
		if (PWMValues[Wheel] < -1000)
			PWMValues[Wheel] = -1000;
		if (Wheel)
			TIM_SetCompare4(TIM1, (uint32_t)(PWMValues[Wheel] < 0 ? -PWMValues[Wheel] : PWMValues[Wheel]) *
			                          3600 / 1000);
		else
			TIM_SetCompare1(TIM1, (uint32_t)(PWMValues[Wheel] < 0 ? -PWMValues[Wheel] : PWMValues[Wheel]) *
			                          3600 / 1000);
	}
	/* 两路共用 STBY；任一路有效输出即可唤醒驱动器。 */
	GPIO_WriteBit(GPIOB, GPIO_Pin_13, (PWMValues[0] || PWMValues[1]) ? Bit_SET : Bit_RESET);
}

/** 返回 1 表示 PB12 急停按下（低电平）；此处只读取，锁定/恢复由控制核心处理。 */
uint8_t Motor_Emergency(void)
{
	return GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_12) == Bit_RESET;
}
