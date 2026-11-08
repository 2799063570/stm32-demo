/**
 * 固定 48 字节协议：显式小端编码、CRC 校验和逐字节滑窗重同步。
 * 本层检查帧结构；速度限幅、序号新旧和状态准入由 RobotControl 判断。
 */
#include "RobotProtocol.h"
#include <string.h>

/** 从字节数组读取小端 16 位值，避免依赖结构体对齐和主机字节序。 */
static uint16_t Robot_Get16(const uint8_t *Data)
{
	return (uint16_t)(Data[0] | ((uint16_t)Data[1] << 8));
}

/** 按小端写入 16 位值；有符号字段由调用者保留位模式后传入。 */
void Robot_Put16(uint8_t *Data, uint16_t Value)
{
	Data[0] = (uint8_t)Value;
	Data[1] = (uint8_t)(Value >> 8);
}

/** 按小端写入 32 位值。 */
void Robot_Put32(uint8_t *Data, uint32_t Value)
{
	Robot_Put16(Data, (uint16_t)Value);
	Robot_Put16(Data + 2, (uint16_t)(Value >> 16));
}

/** 函数：CRC16/CCITT-FALSE，poly=1021、init=FFFF、不反射、xorout=0 */
uint16_t Robot_CRC(const uint8_t *Data, uint16_t Length)
{
	uint16_t CRC = 0xFFFF, Index;
	while (Length--)
	{
		CRC ^= (uint16_t)*Data++ << 8;
		for (Index = 0; Index < 8; Index++)
			CRC = (uint16_t)((CRC & 0x8000) ? (CRC << 1) ^ 0x1021 : CRC << 1);
	}
	return CRC;
}

/** 清空整帧并写入帧头、版本、类型、长度、序号；调用者随后填写载荷。 */
void Robot_FrameInit(uint8_t *Frame, uint8_t Type, uint16_t Sequence)
{
	memset(Frame, 0, ROBOT_FRAME_SIZE);
	Frame[0] = 0xAA;
	Frame[1] = 0x55;
	Frame[2] = 1;
	Frame[3] = Type;
	Frame[4] = ROBOT_FRAME_SIZE;
	Robot_Put16(Frame + 6, Sequence);
}

/** 载荷写完后计算字节 0..43 的 CRC，写入偏移 44..45，再补 0D 0A 帧尾。 */
void Robot_FrameFinish(uint8_t *Frame)
{
	Robot_Put16(Frame + 44, Robot_CRC(Frame, 44));
	Frame[46] = 0x0D;
	Frame[47] = 0x0A;
}

/** 函数：前台逐字节滑窗；坏帧仅移走一个字节，保留后续潜在帧头。 */
uint8_t Robot_ParseByte(RobotParser_TypeDef *Parser, uint8_t Byte, RobotCommand_TypeDef *Command)
{
	uint8_t *CandidateFrame = Parser->Data, Index;
	/* Used 最多保留 47 字节；凑齐 48 字节后要么接纳，要么至少移走一个字节。 */
	CandidateFrame[Parser->Used++] = Byte;
	while (Parser->Used)
	{
		if (CandidateFrame[0] == 0xAA && (Parser->Used < 2 || CandidateFrame[1] == 0x55))
		{
			/* 找到候选帧头但数据未齐时保留残帧，等待下一次喂入。 */
			if (Parser->Used < ROBOT_FRAME_SIZE)
				return 0;
			if (CandidateFrame[2] == 1 && CandidateFrame[3] == 1 && CandidateFrame[4] == ROBOT_FRAME_SIZE &&
			    CandidateFrame[5] == 0 && CandidateFrame[46] == 0x0D && CandidateFrame[47] == 0x0A &&
			    Robot_Get16(CandidateFrame + 44) == Robot_CRC(CandidateFrame, 44))
			{
				/* 未定义标志和保留字段也不能悄悄接受。 */
				uint8_t Reserved = (uint8_t)(CandidateFrame[12] & ~7u);
				for (Index = 13; Index < 44; Index++)
					Reserved |= CandidateFrame[Index];
				if (!Reserved)
				{
					Command->Sequence = Robot_Get16(CandidateFrame + 6);
					Command->LeftRPM = (int16_t)Robot_Get16(CandidateFrame + 8);
					Command->RightRPM = (int16_t)Robot_Get16(CandidateFrame + 10);
					Command->Flags = CandidateFrame[12];
					Parser->Used = 0;
					Parser->Good++;
					return 1;
				}
			}
			Parser->Bad++;
		}
		/* 候选无效时滑动 1 字节，不能整帧丢弃，否则可能跳过真正帧头。 */
		Parser->Used--;
		memmove(CandidateFrame, CandidateFrame + 1, Parser->Used);
	}
	return 0;
}
