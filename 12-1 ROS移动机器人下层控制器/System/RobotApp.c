/**
 * 应用调度：连接串口、协议、控制核心、电机、编码器和 IMU。
 * TIM2 调用 Tick 做实时控制；main 调用 Service 处理通信、采集和反馈。
 */
#include "stm32f10x.h"
#include "RobotApp.h"
#include "RobotControl.h"
#include "RobotConfig.h"
#include "Serial.h"
#include "Encoder.h"
#include "Motor.h"
#include "IMU.h"
#include "Delay.h"
/* Robot 由 TIM2 和 main 共用；main 读写必须保存/恢复 PRIMASK，不能直接无条件开中断。 */
static RobotControl_TypeDef Robot;
/* Parser、反馈节拍和诊断计数仅在 main 更新。 */
static RobotParser_TypeDef Parser;
static uint32_t LastSerialError, LastSerialDrop, LastFeedback, RejectedCommands, SkippedFeedback,
    LastParserByteTick;
static uint16_t FeedbackSequence;

/** 将内部浮点 RPM 限幅后转为反馈协议的 int16，避免转换越界。 */
static int16_t RobotApp_Speed(float RPM)
{
	if (RPM > 32767.0f)
		return 32767;
	if (RPM < -32768.0f)
		return -32768;
	return (int16_t)RPM;
}

/** 在启动 TIM2 前初始化控制核心；其余静态状态由 C 启动过程清零。 */
void RobotApp_Init(void)
{
	RobotControl_Init(&Robot);
}

/** 函数：控制状态只在TIM2或短临界区内修改。 */
void RobotApp_Tick(void)
{
	RobotControl_Tick(&Robot, Encoder_Get(0), Encoder_Get(1), Motor_Emergency());
	if (Robot.State == ROBOT_RUN || Robot.State == ROBOT_DECEL)
		Motor_SetPWM(Robot.PWM[0], Robot.PWM[1]);
	else
		Motor_Stop();
}

/** 检测串口错误/覆盖计数变化：清残帧、锁定故障、立即停机；有新故障返回 1。 */
static uint8_t RobotApp_LinkFault(void)
{
	uint32_t Error = Serial_GetErrorCount(), Drop = Serial_GetDropCount(), Primask;
	if (Error == LastSerialError && Drop == LastSerialDrop)
		return 0;
	LastSerialError = Error;
	LastSerialDrop = Drop;
	Parser.Used = 0;
	Primask = __get_PRIMASK();
	__disable_irq();
	RobotControl_Fault(&Robot);
	Motor_Stop();
	__set_PRIMASK(Primask);
	return 1;
}

/** 先在短临界区复制控制状态，再于临界区外编码 0x81 反馈帧。
 * DMA 忙则跳过本次反馈，不排队过期遥测。字段偏移详见说明.md。 */
static void RobotApp_Feedback(void)
{
	RobotControl_TypeDef Snapshot;
	IMU_DataTypeDef IMU;
	uint8_t Frame[ROBOT_FRAME_SIZE], Index;
	uint32_t Primask;
	Primask = __get_PRIMASK();
	__disable_irq();
	Snapshot = Robot;
	__set_PRIMASK(Primask);
	IMU_Get(&IMU);
	Robot_FrameInit(Frame, 0x81, FeedbackSequence++);
	Frame[5] = (uint8_t)Snapshot.State;
	Robot_Put32(Frame + 8, Snapshot.Now);
	Robot_Put32(Frame + 12, Snapshot.Counts[0]);
	Robot_Put32(Frame + 16, Snapshot.Counts[1]);
	Robot_Put16(Frame + 20, (uint16_t)RobotApp_Speed(Snapshot.Speed[0]));
	Robot_Put16(Frame + 22, (uint16_t)RobotApp_Speed(Snapshot.Speed[1]));
	for (Index = 0; Index < 6; Index++)
		Robot_Put16(Frame + 24 + Index * 2, (uint16_t)IMU.Raw[Index]);
	Robot_Put16(Frame + 36, IMU.Valid ? (uint16_t)(Delay_GetTick() - IMU.Tick) : 65535);
	Frame[38] = IMU.Valid;
	Frame[39] = Snapshot.Button;
	Robot_Put16(Frame + 40, (uint16_t)Parser.Bad);
	Robot_Put16(Frame + 42, (uint16_t)(LastSerialError + LastSerialDrop));
	Robot_FrameFinish(Frame);
	if (!Serial_SendArrayDMA(Frame, sizeof(Frame)))
		SkippedFeedback++;
}

/** 函数：main有预算解析，控制中断始终独立；合法帧完整验证后一次提交。 */
void RobotApp_Service(void)
{
	uint8_t Bytes[32];
	uint16_t Count, Index, Budget = 256;
	uint32_t Primask, Now = Delay_GetTick();
	RobotCommand_TypeDef Command;
	/* 1. 刷新 main 存活时间，再处理通信异常和超过 50ms 的残帧。 */
	Primask = __get_PRIMASK();
	__disable_irq();
	Robot.MainTick = Robot.Now;
	__set_PRIMASK(Primask);
	Serial_Service();
	RobotApp_LinkFault();
	if (Parser.Used && (uint32_t)(Now - LastParserByteTick) >= 50)
		Parser.Used = 0;
	/* 2. 每轮最多消费 256 字节，每次复制 32 字节；解析只接触 CPU 私有副本。 */
	while (Budget)
	{
		Count = Serial_ReadBlock(Bytes, sizeof(Bytes));
		if (RobotApp_LinkFault() || !Count)
			break;
		Budget -= Count;
		for (Index = 0; Index < Count; Index++)
		{
			LastParserByteTick = Now;
			if (Robot_ParseByte(&Parser, Bytes[Index], &Command))
			{
				/* 完整命令一次性提交，TIM2 不会看到半更新的左右目标。 */
				Primask = __get_PRIMASK();
				__disable_irq();
				/* 同步读取物理按钮，防止CLEAR与下一次1ms采样竞争。 */
				Robot.Button = Motor_Emergency();
				if (!RobotControl_Command(&Robot, &Command))
					RejectedCommands++;
				if (Robot.State != ROBOT_RUN && Robot.State != ROBOT_DECEL)
					Motor_Stop();
				__set_PRIMASK(Primask);
			}
		}
	}
	/* 3. 推进异步 IMU，再按反馈周期发送当前快照；不等待 I2C/UART 完成。 */
	IMU_Service();
	if ((uint32_t)(Now - LastFeedback) >= ROBOT_FEEDBACK_MS)
	{
		LastFeedback = Now;
		RobotApp_Feedback();
	}
}
