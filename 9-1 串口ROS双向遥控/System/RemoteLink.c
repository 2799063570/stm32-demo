#include "RemoteLink.h"
#include "RemoteConfig.h"
#include <string.h>

void Remote_LinkInit(Remote_LinkTypeDef *Link) { memset(Link, 0, sizeof(*Link)); Link->MotionState = REMOTE_UNKNOWN; }
uint8_t Remote_LinkOnline(const Remote_LinkTypeDef *Link, uint32_t Now)
{
	return Link->HasFeedback && (uint32_t)(Now - Link->FeedbackTick) <= REMOTE_FEEDBACK_MS;
}

/** 函数：PC轮询触发一次输入回复；反馈必须关联本次会话和最近请求 */
uint8_t Remote_LinkFrame(Remote_LinkTypeDef *Link, const Remote_FrameTypeDef *Frame,
	const Remote_InputTypeDef *Input, uint32_t Now, Remote_FrameTypeDef *Reply)
{
	uint32_t Session;
	uint8_t Index;
	if (Frame->Type == REMOTE_POLL && Frame->Length == 4)
	{
		Session = Remote_GetU32(Frame->Payload);
		if (!Link->HasPoll || Session != Link->Session) Link->HasFeedback = 0;
		Link->Session = Session; Link->Sequence = Frame->Sequence;
		Link->PollTick = Now; Link->HasPoll = 1;
		Reply->Type = REMOTE_INPUT; Reply->Sequence = Frame->Sequence; Reply->Length = 18;
		Remote_PutU32(Reply->Payload, Session);
		Remote_PutU16(Reply->Payload + 4, Input->RawX);
		Remote_PutU16(Reply->Payload + 6, Input->RawY);
		Remote_PutU16(Reply->Payload + 8, (uint16_t)(Input->Ready && Input->ADCValid ? Input->X : 0));
		Remote_PutU16(Reply->Payload + 10, (uint16_t)(Input->Ready && Input->ADCValid ? Input->Y : 0));
		Reply->Payload[12] = Remote_InputButton(Input);
		Reply->Payload[13] = (Input->Ready ? 1 : 0) | (Input->ADCValid ? 2 : 0);
		Reply->Payload[14] = Input->Page;
		Reply->Payload[15] = Input->Command;
		Remote_PutU16(Reply->Payload + 16, Input->RequestId);
		return 1;
	}
	if (Frame->Type != REMOTE_FEEDBACK || Frame->Length != 24 || !Link->HasPoll ||
		Frame->Sequence != Link->Sequence || Remote_GetU32(Frame->Payload) != Link->Session ||
		(uint32_t)(Now - Link->PollTick) > REMOTE_REPLY_MS || Frame->Payload[4] > REMOTE_SOURCE_DEMO ||
		Frame->Payload[5] > 63 || (Frame->Payload[4] == REMOTE_SOURCE_NONE && Frame->Payload[5] != 0) ||
		Frame->Payload[20] > REMOTE_CANCEL || Frame->Payload[21] > REMOTE_UNKNOWN ||
		Frame->Payload[22] > REMOTE_UNKNOWN || Frame->Payload[23] > REMOTE_CANCEL) return 0;
	Link->Source = Frame->Payload[4]; Link->JointMask = Frame->Payload[5];
	for (Index = 0; Index < 6; Index ++) Link->Joints[Index] = Remote_GetI16(Frame->Payload + 6 + 2 * Index);
	Link->AckId = Remote_GetU16(Frame->Payload + 18); Link->AckCommand = Frame->Payload[20];
	Link->AckState = Frame->Payload[21]; Link->MotionState = Frame->Payload[22]; Link->MotionTarget = Frame->Payload[23];
	Link->FeedbackTick = Now; Link->HasFeedback = 1;
	return 0;
}
