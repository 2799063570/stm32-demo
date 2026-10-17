#include "stm32f10x.h"                  // Device header
#include "Control.h"
#include "Config.h"
#include "PID.h"
#include "Encoder.h"
#include "Motor.h"
#include "Delay.h"

static PID_TypeDef Control_PID;
static volatile int32_t Control_TargetRPM;
static volatile uint8_t Control_Running;
static volatile Control_FaultTypeDef Control_Fault;
static volatile uint32_t Control_LastCommand;
static float Control_RampRPM, Control_SpeedRPM;
static uint16_t Control_StallTime, Control_ReverseTime;
static uint16_t Control_DirectionGrace;

static float Control_Abs(float Value) { return Value < 0 ? -Value : Value; }

/** 函数：停止控制并清历史量，内部调用，调用时需保证不与控制中断并发 */
static void Control_StopInternal(void)
{
	Control_Running = 0;
	Control_TargetRPM = 0;
	Control_RampRPM = 0;
	Control_StallTime = Control_ReverseTime = 0;
	PID_Reset(&Control_PID);
	Motor_Stop();
}

/** 函数：闭环控制参数初始化，必须在Motor_Init和Encoder_Init之后调用 */
void Control_Init(void)
{
	Control_PID.Kp = PID_KP;
	Control_PID.Ki = PID_KI;
	Control_PID.Kd = PID_KD;
	Control_PID.Kv = PID_KV;
	Control_PID.Ks = PID_KS;
	Control_PID.Dt = CONTROL_PERIOD_MS / 1000.0f;
	Control_PID.Limit = CONTROL_PWM_LIMIT;
	Control_PID.DAlpha = PID_D_FILTER;
	Control_Fault = CONTROL_FAULT_NONE;
	Control_StopInternal();
}

/** 函数：设置输出轴目标RPM，只有有效命令才能刷新通信看门狗 */
uint8_t Control_SetTarget(int32_t RPM)
{
	uint32_t Primask;
	if (RPM < -CONTROL_MAX_RPM || RPM > CONTROL_MAX_RPM) return 0;
	Primask = __get_PRIMASK();
	__disable_irq();
	if (Control_Fault != CONTROL_FAULT_NONE)
	{
		__set_PRIMASK(Primask);
		return 0;
	}
	Control_LastCommand = Delay_GetTick();
	if (RPM == 0) Control_StopInternal();
	else
	{
		if (!Control_Running || (RPM > 0 && Control_TargetRPM < 0) || (RPM < 0 && Control_TargetRPM > 0))
			Control_DirectionGrace = 500;            // 启动/换向后留出机械减速和测速滤波时间
		Control_TargetRPM = RPM;
		Control_Running = 1;
	}
	__set_PRIMASK(Primask);
	return 1;
}

/** 函数：STOP命令立即停止，不自动清除已锁定故障 */
void Control_Stop(void)
{
	uint32_t Primask = __get_PRIMASK();
	__disable_irq();
	Control_StopInternal();
	__set_PRIMASK(Primask);
}

/** 函数：CLEAR命令停止并清故障，不自动重新启动 */
void Control_ClearFault(void)
{
	uint32_t Primask = __get_PRIMASK();
	__disable_irq();
	Control_StopInternal();
	Control_Fault = CONTROL_FAULT_NONE;
	__set_PRIMASK(Primask);
}

/** 函数：串口丢字节或线路错误时停止，避免使用可能残缺的目标命令 */
void Control_SerialFault(void)
{
	uint32_t Primask = __get_PRIMASK();
	__disable_irq();
	Control_StopInternal();
	Control_Fault = CONTROL_FAULT_SERIAL;
	__set_PRIMASK(Primask);
}

/**
  * 函    数：10ms固定周期控制，只有TIM2中断调用
  * 参    数：无
  * 返 回 值：无
  */
void Control_Update(void)
{
	float RawSpeed, Step, Output;
	int16_t Delta = Encoder_Get();                  // 编码器只在这里采样，不在主循环重复读取增量
	RawSpeed = Delta * ENCODER_DIRECTION * (60000.0f / CONTROL_PERIOD_MS / ENCODER_COUNTS_PER_REV);
	Control_SpeedRPM += CONTROL_FILTER_ALPHA * (RawSpeed - Control_SpeedRPM);
	if (!Control_Running) { Motor_Stop(); return; }
	if ((uint32_t)(Delay_GetTick() - Control_LastCommand) >= CONTROL_COMMAND_TIMEOUT)
	{
		Control_StopInternal();
		Control_Fault = CONTROL_FAULT_TIMEOUT;
		return;
	}
	/*目标斜坡：不把串口阶跃直接交给PID*/
	Step = CONTROL_RAMP_RPM_S * Control_PID.Dt;
	if (Control_RampRPM < Control_TargetRPM)
	{
		Control_RampRPM += Step;
		if (Control_RampRPM > Control_TargetRPM) Control_RampRPM = (float)Control_TargetRPM;
	}
	else if (Control_RampRPM > Control_TargetRPM)
	{
		Control_RampRPM -= Step;
		if (Control_RampRPM < Control_TargetRPM) Control_RampRPM = (float)Control_TargetRPM;
	}
	Output = PID_Calculate(&Control_PID, Control_RampRPM, Control_SpeedRPM);
	Motor_SetPWM((int16_t)Output);
	if (Control_DirectionGrace >= CONTROL_PERIOD_MS) Control_DirectionGrace -= CONTROL_PERIOD_MS;
	/*堵转和反馈方向检查：持续满足异常条件才锁定故障*/
	if (Control_Abs(Output) >= CONTROL_STALL_PWM && Control_Abs(Control_RampRPM) >= 30 && Control_Abs(Control_SpeedRPM) < CONTROL_STALL_RPM)
		Control_StallTime += CONTROL_PERIOD_MS;
	else Control_StallTime = 0;
	if (!Control_DirectionGrace && Control_Abs(Output) >= 200 && Control_Abs(Control_SpeedRPM) >= 10 &&
		Control_RampRPM * Control_SpeedRPM < 0 && Output * Control_RampRPM > 0)
		Control_ReverseTime += CONTROL_PERIOD_MS;
	else Control_ReverseTime = 0;
	if (Control_StallTime >= CONTROL_STALL_TIME_MS || Control_ReverseTime >= CONTROL_REVERSE_TIME_MS)
	{
		Control_FaultTypeDef Fault = Control_StallTime >= CONTROL_STALL_TIME_MS ? CONTROL_FAULT_STALL : CONTROL_FAULT_DIRECTION;
		Control_StopInternal();
		Control_Fault = Fault;
	}
}

/** 函数：获取同一时刻的控制状态供主循环打印，ISR不执行printf */
void Control_GetStatus(Control_StatusTypeDef *Status)
{
	uint32_t Primask;
	if (Status == 0) return;
	Primask = __get_PRIMASK();
	__disable_irq();
	Status->TargetRPM = Control_TargetRPM;
	Status->RampRPM = (int32_t)Control_RampRPM;
	Status->SpeedRPM = (int32_t)Control_SpeedRPM;
	Status->PWM = Motor_GetPWM();
	Status->Running = Control_Running;
	Status->Fault = Control_Fault;
	__set_PRIMASK(Primask);
}
