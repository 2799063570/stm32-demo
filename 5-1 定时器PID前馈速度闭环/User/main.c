#include "stm32f10x.h"                  // Device header
#include "Delay.h"
#include "Serial.h"
#include "Encoder.h"
#include "Motor.h"
#include "Control.h"
#include "Timer.h"
#include "Command.h"
#include "Dashboard.h"

static char RxLine[64];
static uint8_t RxLength, RxDiscard;

/** 函数：处理完整命令行；以换行分包，不以一次IDLE代表一条命令 */
static void App_ProcessLine(void)
{
	int32_t RPM = 0;
	Command_TypeDef Command = Command_Parse(RxLine, &RPM);
	if (Command == COMMAND_SPEED)
	{
		if (Control_SetTarget(RPM)) Serial_Printf("ACK SPEED %ld\r\n", (long)RPM);
		else
		{
			Control_Stop();
			Serial_SendString("ERR RANGE_OR_LATCHED_FAULT\r\n");
		}
	}
	else if (Command == COMMAND_STOP) { Control_Stop(); Serial_SendString("ACK STOP\r\n"); }
	else if (Command == COMMAND_CLEAR) { Control_ClearFault(); Serial_SendString("ACK CLEAR; motor remains stopped\r\n"); }
	else if (Command != COMMAND_STATUS)
	{
		Control_Stop();
		Serial_SendString("ERR COMMAND; stopped\r\n");
	}
}

int main(void)
{
	uint8_t Byte;
	uint32_t LastLog = 0, LastDrop = 0, LastError = 0;
	/*模块初始化，控制定时器最后启动，所有输出默认关闭*/
	Delay_Init();
	Motor_Init();
	Encoder_Init();
	Serial_Init();
	Control_Init();
	Dashboard_Init();
	Timer_Init();
	Serial_SendString("100Hz PID+FF; SPEED <integer RPM> / STOP / CLEAR / STATUS, end with CR or LF\r\n");
	Serial_SendString("Send SPEED at least every 500ms; timeout=1000ms. Starts stopped.\r\n");
	while (1)
	{
		/*发现串口数据完整性异常时停机；丢弃到下一换行，防止拼接残缺指令*/
		uint32_t Drop = Serial_GetDropCount(), Error = Serial_GetErrorCount();
		if (Drop != LastDrop || Error != LastError)
		{
			LastDrop = Drop;
			LastError = Error;
			Control_SerialFault();
			RxDiscard = 1;
			RxLength = 0;
		}
		/*每轮最多处理128字节，避免持续串口输入饿死日志等前台任务*/
		for (uint16_t Count = 0; Count < 128 && Serial_ReadByte(&Byte); Count ++)
		{
			if (Byte == '\r' || Byte == '\n')
			{
				if (!RxDiscard && RxLength)
				{
					RxLine[RxLength] = '\0';
					App_ProcessLine();
				}
				RxDiscard = 0;
				RxLength = 0;
			}
			else if (!RxDiscard)
			{
				if (Byte < 32 || Byte > 126 || RxLength >= sizeof(RxLine) - 1)
				{
					RxDiscard = 1;
					RxLength = 0;
					Control_SerialFault();
				}
				else RxLine[RxLength ++] = (char)Byte;
			}
		}
		if ((uint32_t)(Delay_GetTick() - LastLog) >= 100)
		{
			Control_StatusTypeDef Status;
			LastLog = Delay_GetTick();
			Control_GetStatus(&Status);
			Serial_Printf("TARGET=%ld RAMP=%ld RPM=%ld PWM=%d RUN=%u FAULT=%u\r\n",
				(long)Status.TargetRPM, (long)Status.RampRPM, (long)Status.SpeedRPM,
				Status.PWM, Status.Running, Status.Fault);
		}
		Dashboard_Service();                        // 200ms绘制界面、分块SPI传输，控制仍由TIM2独立执行
	}
}
