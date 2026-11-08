/**
 * 程序入口：先让电机保持停机，再初始化时基、传感器和通信，最后启动控制。
 */
#include "stm32f10x.h" // Device header
#include "Delay.h"
#include "Motor.h"
#include "Encoder.h"
#include "Serial.h"
#include "IMU.h"
#include "RobotApp.h"
#include "Timer.h"

/** 初始化顺序有依赖：电机先关闭，控制状态准备好后再开启 TIM2 中断。 */
int main(void)
{
	/* 输出先关闭，控制定时器最后启动；二进制串口禁止混入printf日志。 */
	Motor_Init();
	Delay_Init();
	Encoder_Init();
	Serial_Init();
	IMU_Init();
	RobotApp_Init();
	Timer_Init();
	while (1)
		RobotApp_Service();
}
