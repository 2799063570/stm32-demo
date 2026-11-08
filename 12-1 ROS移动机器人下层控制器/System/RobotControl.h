#ifndef __ROBOT_CONTROL_H
#define __ROBOT_CONTROL_H
#include "RobotProtocol.h"

typedef enum
{
	ROBOT_STOP,      /* 0：正常停机，可接受新 ENABLE */
	ROBOT_RUN,       /* 1：速度闭环运行 */
	ROBOT_DECEL,     /* 2：通信超时，目标降为 0，闭环减速 */
	ROBOT_TIMEOUT,   /* 3：超时锁定，需要零目标 CLEAR */
	ROBOT_EMERGENCY, /* 4：急停锁定，释放按钮后仍需 CLEAR */
	ROBOT_FAULT      /* 5：通信完整性/main 存活故障锁定 */
} RobotState_TypeDef;

typedef struct
{
	float Integral; /* 已乘 Ki 的积分贡献，限幅到 +/-300 */
	float Previous; /* 上周期速度误差，RPM，用于微分 */
} RobotPID_TypeDef;

typedef struct
{
	RobotState_TypeDef State;
	RobotPID_TypeDef PID[2]; /* 所有双轮数组：[0]=左轮，[1]=右轮 */
	float Target[2];         /* 接纳的命令目标，RPM */
	float Ramp[2];           /* 斜坡后的闭环目标，RPM */
	float Speed[2];          /* 低通滤波后的测量速度，RPM */
	uint32_t Counts[2];      /* 累计编码器计数，uint32 模计数 */
	uint32_t Now;            /* TIM2 控制毫秒计数 */
	uint32_t LastCommand;    /* 最后接纳命令的控制时间 */
	uint32_t DecelStart;     /* 进入减速的控制时间 */
	uint32_t MainTick;       /* main 最近刷新时的控制时间 */
	uint16_t Sequence;       /* 最后接纳的命令序号 */
	int16_t PWM[2];          /* 有符号输出，-1000..1000 */
	uint8_t HasSequence;     /* 是否已有序号基准 */
	uint8_t Button;          /* 最近读取的实体急停状态，1=按下 */
} RobotControl_TypeDef;

/* 调用者负责 main/ISR 之间短临界区，核心逻辑不依赖外设。 */
/** 初始化为 STOP；本模块不直接访问 STM32 外设。 */
void RobotControl_Init(RobotControl_TypeDef *Control);
/** 返回 1=接纳，0=拒绝；调用者保证帧结构/CRC 已验证并保护与 Tick 的并发。 */
uint8_t RobotControl_Command(RobotControl_TypeDef *Control, const RobotCommand_TypeDef *Command);
/** 每 1ms 调用；Left/Right 为本周期编码器增量计数，Emergency=1 表示实体急停。 */
void RobotControl_Tick(RobotControl_TypeDef *Control, int16_t Left, int16_t Right, uint8_t Emergency);
/** 锁定 FAULT，清空输出；已有 EMERGENCY 状态优先保留。 */
void RobotControl_Fault(RobotControl_TypeDef *Control);
#endif
