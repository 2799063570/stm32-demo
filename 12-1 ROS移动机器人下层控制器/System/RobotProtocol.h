#ifndef __ROBOT_PROTOCOL_H
#define __ROBOT_PROTOCOL_H
#include <stdint.h>
/* 固定帧偏移：0 帧头，2 版本，3 类型，4 长度，5 状态/保留，6 序号，
 * 8..43 载荷，44..45 小端 CRC，46..47 帧尾；完整字段见说明.md。 */
#define ROBOT_FRAME_SIZE 48
#define ROBOT_ENABLE 1u /* bit0：允许闭环运行 */
#define ROBOT_ESTOP 2u  /* bit1：远程急停 */
#define ROBOT_CLEAR 4u  /* bit2：零目标清锁定，不与 ENABLE 合用 */

typedef struct
{
	uint8_t Data[ROBOT_FRAME_SIZE]; /* 待判定的滑窗候选字节 */
	uint8_t Used;                   /* 当前残帧长度 */
	uint32_t Bad;                   /* 坏候选帧累计数，不是噪声字节数 */
	uint32_t Good;                  /* 通过结构/CRC/保留字段检查的累计数 */
} RobotParser_TypeDef;

typedef struct
{
	uint16_t Sequence;
	int16_t LeftRPM, RightRPM; /* 左右目标 RPM，速度上限由控制核心检查 */
	uint8_t Flags;             /* ROBOT_ENABLE / ROBOT_ESTOP / ROBOT_CLEAR 位组合 */
} RobotCommand_TypeDef;

/** CRC16/CCITT-FALSE；标准向量 123456789 -> 0x29B1。 */
uint16_t Robot_CRC(const uint8_t *Data, uint16_t Length);
/** Parser 先清零，逐字节调用；返回 1 时 Command 有效，0 表示未得到完整合法命令。 */
uint8_t Robot_ParseByte(RobotParser_TypeDef *Parser, uint8_t Byte, RobotCommand_TypeDef *Command);
/** 清空 48 字节帧，写公共头；Type=0x01 命令、0x81 反馈。 */
void Robot_FrameInit(uint8_t *Frame, uint8_t Type, uint16_t Sequence);
/** 载荷填写完后调用，写 CRC 和帧尾。 */
void Robot_FrameFinish(uint8_t *Frame);
/** 向 Data 小端写入 16 位值，调用者保证至少 2 字节空间。 */
void Robot_Put16(uint8_t *Data, uint16_t Value);
/** 向 Data 小端写入 32 位值，调用者保证至少 4 字节空间。 */
void Robot_Put32(uint8_t *Data, uint32_t Value);
#endif
