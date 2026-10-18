#ifndef __REMOTE_LINK_H
#define __REMOTE_LINK_H
#include "RemoteProtocol.h"
#include "RemoteInput.h"
typedef struct
{
	uint32_t Session, PollTick, FeedbackTick;
	uint16_t Sequence;
	uint8_t HasPoll, HasFeedback, Source, JointMask;
	int16_t Joints[6];                            // 毫弧度，按约定的六个关节名排序
	uint16_t AckId;
	uint8_t AckCommand, AckState, MotionState, MotionTarget;
} Remote_LinkTypeDef;
void Remote_LinkInit(Remote_LinkTypeDef *Link);
uint8_t Remote_LinkFrame(Remote_LinkTypeDef *Link, const Remote_FrameTypeDef *Frame,
	const Remote_InputTypeDef *Input, uint32_t Now, Remote_FrameTypeDef *Reply);
uint8_t Remote_LinkOnline(const Remote_LinkTypeDef *Link, uint32_t Now);
#endif
