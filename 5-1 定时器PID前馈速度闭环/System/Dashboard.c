#include "Dashboard.h"
#include "Control.h"
#include "Config.h"
#include "OLED.h"
#include "Delay.h"

static int16_t Dashboard_Target[32], Dashboard_Speed[32];
static uint8_t Dashboard_Head, Dashboard_Count;
static uint32_t Dashboard_LastDraw;

static int16_t Dashboard_Clip(int32_t Value)
{
	if (Value > CONTROL_MAX_RPM) Value = CONTROL_MAX_RPM;
	if (Value < -CONTROL_MAX_RPM) Value = -CONTROL_MAX_RPM;
	return (int16_t)Value;
}

static int16_t Dashboard_ToY(int16_t RPM)
{
	return 40 - (int32_t)RPM * 21 / CONTROL_MAX_RPM;
}

/** 函数：状态页显示实际数值，条形图为PWM千分比，不冒充实测功率 */
static void Dashboard_DrawStatus(const Control_StatusTypeDef *Status)
{
	static const char * const FaultNames[] = {"NONE", "TIMEOUT", "STALL", "DIR", "SERIAL"};
	if (Status->Fault)
		OLED_PrintLine(1, "FAULT:%s", (unsigned)Status->Fault < 5 ? FaultNames[Status->Fault] : "UNKNOWN");
	else OLED_PrintLine(1, "PID+FF 100Hz");
	OLED_PrintLine(2, "T%4ld V%4ld", (long)Status->TargetRPM, (long)Status->SpeedRPM);
	OLED_PrintLine(3, "PWM%4d F%u", Status->PWM, Status->Fault);
	OLED_PrintLine(4, Status->Running ? "RUN" : "STOP");
	OLED_DrawBar(40, 52, 88, 11, Status->PWM, 1000);
}

/** 函数：最近32次斜坡目标/实际转速，200ms采样；点线为目标，实线为反馈 */
static void Dashboard_DrawTrend(void)
{
	uint8_t Index, Start = (Dashboard_Head + 32 - Dashboard_Count) % 32;
	OLED_Clear();
	OLED_PrintLine(1, "T:DOT V:LINE RPM");
	for (Index = 0; Index < 128; Index += 8) OLED_DrawPixel(Index, 40, 1);
	for (Index = 0; Index < Dashboard_Count; Index ++)
	{
		uint8_t Position = (Start + Index) % 32;
		int16_t X = (int32_t)Index * 127 / 31;
		int16_t Y = Dashboard_ToY(Dashboard_Speed[Position]);
		OLED_DrawPixel(X, Dashboard_ToY(Dashboard_Target[Position]), 1);
		if (Index)
		{
			uint8_t Previous = (Position + 31) % 32;
			OLED_DrawLine((int32_t)(Index - 1) * 127 / 31, Dashboard_ToY(Dashboard_Speed[Previous]), X, Y, 1);
		}
		else OLED_DrawPixel(X, Y, 1);
	}
}

void Dashboard_Init(void)
{
	Dashboard_Head = Dashboard_Count = 0;
	Dashboard_LastDraw = Delay_GetTick();
	OLED_Init();
}

/** 函数：只在主循环调用；故障时固定显示状态页，不用波形页遮挡故障 */
void Dashboard_Service(void)
{
	uint32_t Now = Delay_GetTick();
	if ((uint32_t)(Now - Dashboard_LastDraw) >= 200)
	{
		Control_StatusTypeDef Status;
		Dashboard_LastDraw = Now;
		Control_GetStatus(&Status);
		Dashboard_Target[Dashboard_Head] = Dashboard_Clip(Status.RampRPM);
		Dashboard_Speed[Dashboard_Head] = Dashboard_Clip(Status.SpeedRPM);
		Dashboard_Head = (Dashboard_Head + 1) % 32;
		if (Dashboard_Count < 32) Dashboard_Count ++;
		if (Status.Fault || (Now / 4000) % 2 == 0)
		{
			Dashboard_DrawStatus(&Status);
		}
		else Dashboard_DrawTrend();
	}
	OLED_Service();
}
