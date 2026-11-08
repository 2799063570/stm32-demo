/**
 * 与外设无关的双轮控制核心：命令准入、停机状态机、测速、斜坡和 PID。
 * Tick 必须每 1ms 调用；main 与控制中断之间的互斥由 RobotApp 负责。
 */
#include "RobotControl.h"
#include "RobotConfig.h"
#include <string.h>

/** 对称限幅到 [-Limit, Limit]，用于斜坡、积分和 PWM。 */
static float Robot_Clamp(float Value, float Limit)
{
	return Value > Limit ? Limit : (Value < -Limit ? -Limit : Value);
}

/** 进入指定停机状态，清空目标、斜坡、积分和输出；保留计数与命令序号。 */
static void RobotControl_Stop(RobotControl_TypeDef *Control, RobotState_TypeDef State)
{
	Control->State = State;
	memset(Control->PID, 0, sizeof(Control->PID));
	Control->Target[0] = Control->Target[1] = Control->Ramp[0] = Control->Ramp[1] = 0;
	Control->PWM[0] = Control->PWM[1] = 0;
}

/** 全零初始化；ROBOT_STOP 的枚举值为 0，上电后等待有效使能命令。 */
void RobotControl_Init(RobotControl_TypeDef *Control)
{
	memset(Control, 0, sizeof(*Control));
}

/** 通信或 main 存活异常进入 FAULT；已处于 EMERGENCY 时保留急停状态。 */
void RobotControl_Fault(RobotControl_TypeDef *Control)
{
	if (Control->State != ROBOT_EMERGENCY)
		RobotControl_Stop(Control, ROBOT_FAULT);
}

/** 函数：仅新序号且语义正确的命令刷新超时；急停优先于序号检查。 */
uint8_t RobotControl_Command(RobotControl_TypeDef *Control, const RobotCommand_TypeDef *Command)
{
	/* uint16 模差：1..32767 为新命令，0 为重复，大于等于 32768 为旧命令。 */
	uint16_t Distance = (uint16_t)(Command->Sequence - Control->Sequence);
	uint8_t Clear = Command->Flags == ROBOT_CLEAR && !Command->LeftRPM && !Command->RightRPM;
	/* 协议层已验证结构和 CRC；远程急停优先于序号、速度和锁定状态检查。 */
	if (Command->Flags & ROBOT_ESTOP)
	{
		RobotControl_Stop(Control, ROBOT_EMERGENCY);
		return 1;
	}
	/* 实体急停按下时，任何普通命令或 CLEAR 都不能解除锁定。 */
	if (Control->Button)
	{
		RobotControl_Stop(Control, ROBOT_EMERGENCY);
		return 0;
	}
	if ((Command->Flags & ~7u) || (Command->Flags & ROBOT_CLEAR && !Clear) ||
	    Command->LeftRPM > ROBOT_MAX_RPM || Command->LeftRPM < -ROBOT_MAX_RPM ||
	    Command->RightRPM > ROBOT_MAX_RPM || Command->RightRPM < -ROBOT_MAX_RPM)
		return 0;
	/* CLEAR仅在已停且按钮释放时允许重同步上位机重启后的序号，不自动启动。 */
	if (Clear)
	{
		if (Control->Button || Control->State == ROBOT_RUN || Control->State == ROBOT_DECEL)
			return 0;
		RobotControl_Stop(Control, ROBOT_STOP);
	}
	else
	{
		if (Control->HasSequence && (!Distance || Distance >= 0x8000))
			return 0;
		if (Control->State >= ROBOT_TIMEOUT)
			return 0;
		if (Control->State == ROBOT_DECEL)
		{
			if (Command->Flags & ROBOT_ENABLE)
				return 0;
			RobotControl_Stop(Control, ROBOT_TIMEOUT); /* 减速中禁用可立即断输出，仍保持超时锁定 */
		}
		else if (!(Command->Flags & ROBOT_ENABLE))
			RobotControl_Stop(Control, ROBOT_STOP);
		else
		{
			Control->Target[0] = (float)Command->LeftRPM;
			Control->Target[1] = (float)Command->RightRPM;
			Control->State = ROBOT_RUN;
		}
	}
	/* 仅接纳的普通命令/CLEAR 更新序号和最后命令时间；急停已在前面返回。 */
	Control->Sequence = Command->Sequence;
	Control->HasSequence = 1;
	Control->LastCommand = Control->Now;
	return 1;
}

/** 函数：每1ms调用，编码器增量滤波测速；通信超时衰减目标而不衰减PID增益。 */
void RobotControl_Tick(RobotControl_TypeDef *Control, int16_t Left, int16_t Right, uint8_t Emergency)
{
	int16_t Delta[2];
	uint8_t Wheel;
	Delta[0] = Left;
	Delta[1] = Right;
	Control->Now++;
	Control->Button = Emergency;
	/* 1ms 增量 -> RPM：60000ms/min；一阶低通系数 0.1，Counts 按 uint32 回绕。 */
	for (Wheel = 0; Wheel < 2; Wheel++)
	{
		Control->Counts[Wheel] += (uint32_t)(int32_t)Delta[Wheel]; /* 无符号自然回绕，避免有符号累加溢出 */
		Control->Speed[Wheel] +=
		    0.1f * ((float)Delta[Wheel] * 60000.0f / ROBOT_COUNTS_REV - Control->Speed[Wheel]);
	}
	/* 安全状态优先于闭环输出；减速最长持续 ROBOT_DECEL_MS 后锁定停机。 */
	if (Emergency)
		RobotControl_Stop(Control, ROBOT_EMERGENCY);
	if ((uint32_t)(Control->Now - Control->MainTick) >= ROBOT_MAIN_TIMEOUT_MS)
		RobotControl_Fault(Control);
	if (Control->State == ROBOT_RUN && (uint32_t)(Control->Now - Control->LastCommand) >= ROBOT_COMMAND_MS)
	{
		Control->State = ROBOT_DECEL;
		Control->DecelStart = Control->Now;
		Control->Target[0] = Control->Target[1] = 0;
		memset(Control->PID, 0, sizeof(Control->PID));
	}
	if (Control->State == ROBOT_DECEL && (uint32_t)(Control->Now - Control->DecelStart) >= ROBOT_DECEL_MS)
		RobotControl_Stop(Control, ROBOT_TIMEOUT);
	if (Control->State != ROBOT_RUN && Control->State != ROBOT_DECEL)
		return;
	for (Wheel = 0; Wheel < 2; Wheel++)
	{
		float Error, Derivative, Output, IntegralCandidate;
		/* 目标斜坡限制 RPM/ms；PID 的 dt 固定为 0.001s，输出单位为千分比 PWM。 */
		Control->Ramp[Wheel] += Robot_Clamp(Control->Target[Wheel] - Control->Ramp[Wheel], ROBOT_RAMP_RPM_MS);
		Error = Control->Ramp[Wheel] - Control->Speed[Wheel];
		Derivative = (Error - Control->PID[Wheel].Previous) * 1000.0f;
		/* Integral 保存已经乘 Ki 的积分贡献，幅值限制为 300。 */
		IntegralCandidate = Robot_Clamp(Control->PID[Wheel].Integral + ROBOT_KI * Error * 0.001f, 300.0f);
		Output =
		    ROBOT_KP * Error + IntegralCandidate + ROBOT_KD * Derivative + ROBOT_FF * Control->Ramp[Wheel];
		/* 饱和同向时冻结积分，误差反向时允许退出饱和。 */
		if ((Output < 1000 && Output > -1000) || (Output >= 1000 && Error < 0) ||
		    (Output <= -1000 && Error > 0))
			Control->PID[Wheel].Integral = IntegralCandidate;
		Control->PID[Wheel].Previous = Error;
		Control->PWM[Wheel] = (int16_t)Robot_Clamp(Output, 1000.0f);
	}
}
