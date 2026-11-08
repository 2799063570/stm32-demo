#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "RobotControl.h"
#include "RobotConfig.h"
static void step(RobotControl_TypeDef *C, unsigned Count)
{
	while (Count--) { C->MainTick = C->Now; RobotControl_Tick(C, 0, 0, 0); }
}
static void frame(uint8_t *F, uint16_t Sequence, int16_t RPM, uint8_t Flags)
{
	Robot_FrameInit(F, 1, Sequence); Robot_Put16(F + 8, (uint16_t)RPM);
	Robot_Put16(F + 10, (uint16_t)-RPM); F[12] = Flags; Robot_FrameFinish(F);
}
int main(void)
{
	RobotParser_TypeDef P = {0}; RobotCommand_TypeDef Cmd = {65535, 100, -100, ROBOT_ENABLE};
	RobotControl_TypeDef C; uint8_t F[48], Bad[48]; unsigned i, Good = 0; FILE *Golden;
	assert(Robot_CRC((const uint8_t *)"123456789", 9) == 0x29B1);
	frame(F, 65535, 100, 1);
	Golden = fopen("robot_golden.bin", "wb"); assert(Golden); assert(fwrite(F, 1, 48, Golden) == 48); fclose(Golden);
	for (i = 0; i < 47; i++) assert(!Robot_ParseByte(&P, F[i], &Cmd));
	assert(Robot_ParseByte(&P, F[47], &Cmd)); assert(Cmd.Sequence == 65535 && Cmd.RightRPM == -100);
	/* 每个字节位置损坏/删除/插入后都能恢复到后续两帧；包含帧内假帧头。 */
	for (i = 0; i < 48; i++)
	{
		unsigned j, kind;
		for (kind = 0; kind < 3; kind++)
		{
			memset(&P, 0, sizeof(P)); memcpy(Bad, F, 48); Bad[i] ^= 0x5A; Good = 0;
			for (j = 0; j < 48; j++)
			{
				if (kind == 2 && j == i) Good += Robot_ParseByte(&P, 0xAA, &Cmd);
				if (kind != 1 || j != i) Good += Robot_ParseByte(&P, kind == 0 ? Bad[j] : F[j], &Cmd);
			}
			for (j = 0; j < 96; j++) Good += Robot_ParseByte(&P, F[j % 48], &Cmd);
			assert(Good >= 2 && P.Used == 0);
		}
	}
	memset(&P, 0, sizeof(P)); frame(Bad, 1, 0, 8);
	for (i = 0; i < 48; i++) assert(!Robot_ParseByte(&P, Bad[i], &Cmd));
	/* 即使CRC重新算对，版本/方向/长度/保留字段错误仍不接受。 */
	{
		static const unsigned Offsets[] = {2, 3, 4, 5, 12, 13, 43}; unsigned k;
		for (k = 0; k < sizeof(Offsets) / sizeof(Offsets[0]); k++)
		{
			memset(&P, 0, sizeof(P)); memcpy(Bad, F, 48);
			Bad[Offsets[k]] = 0xAA; Robot_FrameFinish(Bad);
			for (i = 0; i < 48; i++) assert(!Robot_ParseByte(&P, Bad[i], &Cmd));
			Good = 0;
			for (i = 0; i < 48; i++) Good += Robot_ParseByte(&P, F[i], &Cmd);
			assert(Good == 1);
		}
	}
	RobotControl_Init(&C); Cmd.Sequence = 65535; Cmd.LeftRPM = 100; Cmd.RightRPM = -100; Cmd.Flags = 1;
	assert(C.State == ROBOT_STOP && RobotControl_Command(&C, &Cmd));
	step(&C, 50); assert(C.Ramp[0] > 29 && C.Ramp[0] < 31);
	assert(!RobotControl_Command(&C, &Cmd)); assert(C.LastCommand == 0);
	Cmd.Sequence = 0; assert(RobotControl_Command(&C, &Cmd)); /* 序号回绕 */
	Cmd.Sequence = 65535; assert(!RobotControl_Command(&C, &Cmd));
	Cmd.Sequence = 1; Cmd.LeftRPM = 201; assert(!RobotControl_Command(&C, &Cmd));
	Cmd.LeftRPM = 100; step(&C, 200); assert(C.State == ROBOT_DECEL);
	assert(!RobotControl_Command(&C, &Cmd));
	step(&C, 600); assert(C.State == ROBOT_TIMEOUT && !C.PWM[0]);
	Cmd.Flags = 4; Cmd.LeftRPM = Cmd.RightRPM = 0;
	assert(RobotControl_Command(&C, &Cmd)); assert(C.State == ROBOT_STOP);
	Cmd.Flags = 1; Cmd.Sequence++; assert(RobotControl_Command(&C, &Cmd));
	step(&C, 10); RobotControl_Tick(&C, 0, 0, 1);
	assert(C.State == ROBOT_EMERGENCY && !C.PWM[0]);
	Cmd.Flags = 4; assert(!RobotControl_Command(&C, &Cmd));
	RobotControl_Tick(&C, 0, 0, 0); assert(RobotControl_Command(&C, &Cmd));
	Cmd.Flags = 1; Cmd.Sequence++; assert(RobotControl_Command(&C, &Cmd));
	for (i = 0; i < 101; i++) RobotControl_Tick(&C, 0, 0, 0);
	assert(C.State == ROBOT_FAULT && !C.PWM[0]); /* 主循环卡死 */
	RobotControl_Init(&C); C.Now = 0xFFFFFFF0u; C.MainTick = C.Now;
	Cmd.Flags = 1; Cmd.Sequence = 3; assert(RobotControl_Command(&C, &Cmd));
	step(&C, 200); assert(C.State == ROBOT_DECEL); /* 毫秒回绕 */
	Cmd.Flags = 2; Cmd.Sequence = 2; assert(RobotControl_Command(&C, &Cmd));
	assert(C.State == ROBOT_EMERGENCY && !C.PWM[1]); /* 旧序号急停仍有效 */
	RobotControl_Init(&C); Cmd.Flags = 1; Cmd.Sequence = 1;
	assert(RobotControl_Command(&C, &Cmd)); step(&C, 200);
	Cmd.Flags = 0; Cmd.Sequence = 2; assert(RobotControl_Command(&C, &Cmd));
	assert(C.State == ROBOT_TIMEOUT && !C.PWM[0]); /* 减速途中显式禁用仍锁定 */
	RobotControl_Init(&C); Cmd.Flags = 1; Cmd.LeftRPM = 200; Cmd.RightRPM = -200;
	for (i = 0; i < 100; i++)
	{
		Cmd.Sequence = (uint16_t)i; assert(RobotControl_Command(&C, &Cmd)); step(&C, 20);
		assert(C.PWM[0] <= 1000 && C.PWM[0] >= -1000);
		assert(C.PID[0].Integral <= 300 && C.PID[0].Integral >= -300);
	}
	puts("robot: CRC, stream recovery, validation, sequence, ramp, timeout, estop, main stall, tick wrap passed");
	return 0;
}
