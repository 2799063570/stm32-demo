#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "PID.h"
#include "Command.h"
#include "Control.h"
#include "Config.h"

/*用显式桩输入测试真实Control.c，不验证GPIO、电气或中断时序。*/
static uint32_t Tick;
static int16_t EncoderDelta, MotorPWM;
uint32_t Delay_GetTick(void) { return Tick; }
int16_t Encoder_Get(void) { return EncoderDelta; }
void Motor_SetPWM(int16_t Value) { MotorPWM = Value; }
void Motor_Stop(void) { MotorPWM = 0; }
int16_t Motor_GetPWM(void) { return MotorPWM; }

static void TestCommand(void)
{
	int32_t RPM;
	assert(Command_Parse("SPEED 120", &RPM) == COMMAND_SPEED && RPM == 120);
	assert(Command_Parse("  SPEED -60  ", &RPM) == COMMAND_SPEED && RPM == -60);
	assert(Command_Parse("SPEED +0", &RPM) == COMMAND_SPEED && RPM == 0);
	assert(Command_Parse("SPEED 2147483647", &RPM) == COMMAND_SPEED && RPM == 2147483647);
	assert(Command_Parse("SPEED -2147483648", &RPM) == COMMAND_SPEED && RPM == (-2147483647 - 1));
	assert(Command_Parse("STOP ", &RPM) == COMMAND_STOP);
	assert(Command_Parse("CLEAR", &RPM) == COMMAND_CLEAR);
	assert(Command_Parse("STATUS", &RPM) == COMMAND_STATUS);
	const char *Bad[] = {"", "SPEED", "SPEED ", "SPEED +", "SPEED nan", "SPEED 1.5", "SPEED 20x",
		"SPEED 2147483648", "SPEED -2147483649", "SPEED 9999999999999999999999", "STOPNOW", "SPEED1", "speed 1"};
	for (unsigned i = 0; i < sizeof(Bad) / sizeof(Bad[0]); i ++)
		assert(Command_Parse(Bad[i], &RPM) == COMMAND_INVALID);
	puts("PASS command syntax, signed integer bounds and malformed input");
}

static void TestPID(void)
{
	PID_TypeDef PID = {0};
	PID.Kp = 10; PID.Ki = 5; PID.Kd = 0.5f; PID.Dt = 0.01f; PID.Limit = 600; PID.DAlpha = 0.2f;
	PID_Reset(&PID);
	for (unsigned i = 0; i < 1000; i ++) assert(PID_Calculate(&PID, 200, 0) == 600);
	assert(PID.Integral == 0);                       /*饱和时不能继续累积积分*/
	assert(PID_Calculate(&PID, -200, 0) == -600);
	PID_Reset(&PID);
	PID.Kp = PID.Ki = 0; PID.Kd = 1; PID.Kv = 1.5f; PID.Ks = 40;
	assert(fabsf(PID_Calculate(&PID, 100, 0) - 190) < 0.01f); /*目标变化没有测量微分冲击*/
	assert(fabsf(PID_Calculate(&PID, -100, 0) + 190) < 0.01f);
	assert(PID_Calculate(&PID, 0, 0) == 0);
	puts("PASS PID saturation, anti-windup, feedforward sign and derivative-on-measurement");
}

static void ResetControl(void)
{
	Tick = 0; EncoderDelta = 0; MotorPWM = 0;
	Control_Init();
}

static void Simulate(unsigned Count, int RPM, float *Speed, double *Fraction)
{
	for (unsigned i = 0; i < Count; i ++)
	{
		if (i % 50 == 0) assert(Control_SetTarget(RPM));
		/*简化的一阶电机模型，仅用于软件回归；不是实际电机性能保证。*/
		float Equilibrium = MotorPWM == 0 ? 0 : (MotorPWM - (MotorPWM > 0 ? 40.0f : -40.0f)) / 1.5f;
		*Speed += 0.05f * (Equilibrium - *Speed);
		*Fraction += *Speed * ENCODER_COUNTS_PER_REV * 0.01 / 60.0;
		EncoderDelta = (int16_t)*Fraction;
		*Fraction -= EncoderDelta;
		Tick += 10;
		Control_Update();
		Control_StatusTypeDef Status;
		Control_GetStatus(&Status);
		assert(Status.Fault == CONTROL_FAULT_NONE);
		assert(abs(MotorPWM) <= (int)CONTROL_PWM_LIMIT);
	}
}

static void TestControl(void)
{
	Control_StatusTypeDef Status;
	float Speed = 0;
	double Fraction = 0;
	ResetControl();
	Control_GetStatus(&Status);
	assert(!Status.Running && MotorPWM == 0);
	assert(!Control_SetTarget(301));
	assert(!Control_SetTarget(-301));
	Simulate(400, 100, &Speed, &Fraction);
	assert(fabsf(Speed - 100) < 10);
	Simulate(500, -60, &Speed, &Fraction);
	assert(fabsf(Speed + 60) < 10);
	assert(Control_SetTarget(0));
	assert(MotorPWM == 0);
	puts("PASS 10ms control, forward/reverse tracking, output bound and zero-target stop");

	ResetControl();
	assert(Control_SetTarget(50));
	for (unsigned i = 0; i < 100; i ++) { Tick += 10; EncoderDelta = 10; Control_Update(); }
	Control_GetStatus(&Status);
	assert(Status.Fault == CONTROL_FAULT_TIMEOUT && !Status.Running && MotorPWM == 0);
	assert(!Control_SetTarget(50));
	Control_ClearFault();
	Control_GetStatus(&Status);
	assert(Status.Fault == 0 && !Status.Running && MotorPWM == 0);
	puts("PASS 1000ms timeout, fault latch and explicit CLEAR without auto restart");

	ResetControl();
	for (unsigned i = 0; i < 400; i ++)
	{
		if (i % 50 == 0) (void)Control_SetTarget(200);
		Tick += 10; EncoderDelta = 0; Control_Update();
	}
	Control_GetStatus(&Status);
	assert(Status.Fault == CONTROL_FAULT_STALL && !Status.Running && MotorPWM == 0);
	puts("PASS missing encoder/stall detection");

	ResetControl();
	for (unsigned i = 0; i < 250; i ++)
	{
		if (i % 50 == 0) (void)Control_SetTarget(200);
		Tick += 10; EncoderDelta = -10; Control_Update();
	}
	Control_GetStatus(&Status);
	assert(Status.Fault == CONTROL_FAULT_DIRECTION && MotorPWM == 0);
	Control_ClearFault();
	assert(Control_SetTarget(50));
	Control_SerialFault();
	Control_GetStatus(&Status);
	assert(Status.Fault == CONTROL_FAULT_SERIAL && MotorPWM == 0);
	puts("PASS inverted feedback and serial integrity fault stop");

	ResetControl();
	Tick = 0xFFFFFF00U;
	assert(Control_SetTarget(50));
	for (unsigned i = 0; i < 99; i ++) { Tick += 10; EncoderDelta = 10; Control_Update(); }
	Control_GetStatus(&Status);
	assert(Status.Fault == CONTROL_FAULT_NONE);
	Tick += 10; Control_Update();
	Control_GetStatus(&Status);
	assert(Status.Fault == CONTROL_FAULT_TIMEOUT && MotorPWM == 0);
	puts("PASS communication timeout across 32-bit millisecond wrap");
}

int main(void)
{
	TestCommand();
	TestPID();
	TestControl();
	puts("All host logic tests passed. Hardware remains untested.");
	return 0;
}
