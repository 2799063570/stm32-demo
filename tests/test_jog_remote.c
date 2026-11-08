#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "JogRemote.h"
#include "JogConfig.h"
#include "JogView.h"
#include "OLED.h"

static uint32_t Now;
static uint16_t Sequence;
static uint32_t Session = 0x12345678;
static RemoteJog_Feedback PC;
static char Lines[4][17];

/* 记录真实视图的文本输出，避免将测试桩描述为OLED硬件仿真。 */
#include <stdarg.h>
void OLED_PrintLine(uint8_t Line, const char *Format, ...)
{
	va_list Args;
	assert(Line >= 1 && Line <= 4);
	va_start(Args, Format);
	vsnprintf(Lines[Line - 1], 17, Format, Args);
	va_end(Args);
}

static Remote_FrameTypeDef Poll(uint16_t Seq)
{
	Remote_FrameTypeDef Frame = {0};
	Frame.Type = REMOTE_JOG_POLL; Frame.Length = 4; Frame.Sequence = Seq;
	Remote_PutU32(Frame.Payload, Session);
	return Frame;
}

static RemoteJog_Input Exchange(Jog_Remote *Remote)
{
	Remote_FrameTypeDef Frame = Poll(++ Sequence), Reply;
	RemoteJog_Input Input;
	assert(Jog_OnFrame(Remote, &Frame, Now, &Reply));
	assert(RemoteJog_DecodeInput(&Reply, &Input));
	assert(Input.session == Session && Reply.Sequence == Sequence);
	Jog_TxResult(Remote, 1);
	PC.session = Session;
	RemoteJog_EncodeFeedback(&PC, Sequence, &Frame);
	Jog_OnFrame(Remote, &Frame, Now, &Reply);
	return Input;
}

static void Samples(Jog_Remote *Remote, uint16_t X, uint16_t Y, uint8_t Keys, unsigned Count, uint8_t Live)
{
	for (unsigned i = 0; i < Count; ++i)
	{
		Now += 5;
		Jog_Feed(Remote, X, Y, Keys, 1, Now);
		if (Live && !(i % 4)) Exchange(Remote);
	}
}

static void Setup(Jog_Remote *Remote)
{
	Now = 0; Sequence = 0; Session = 0x12345678;
	memset(&PC, 0, sizeof(PC));
	PC.flags = 5; PC.controller_state = REMOTE_JOG_DISABLED;
	PC.home_state = 255; PC.home_error = 65535;
	Jog_Init(Remote);
	Samples(Remote, 2048, 2048, 0, 40, 0);
	assert(Remote->Ready && Remote->X == 0 && Remote->Y == 0);
	Exchange(Remote);
	Samples(Remote, 2048, 2048, 0, 1, 0);
	assert(Jog_Online(Remote, Now) && !Remote->NeedNeutral);
}

static void Enable(Jog_Remote *Remote)
{
	Remote->Page = JOG_PAGE_DRIVE;
	assert(Jog_Request(Remote, REMOTE_JOG_ENABLE, Now));
	PC.request_id = Remote->RequestId; PC.command = REMOTE_JOG_ENABLE;
	PC.ack_state = JOG_ACK_DONE; PC.flags = 7; PC.controller_state = REMOTE_JOG_IDLE;
	Exchange(Remote);
	assert(Remote->Authorized && !Remote->EnablePending);
}

static void TestWire(void)
{
	/* 与提供的ROS协议文档逐字节比对，不使用本地编码器生成期望值。 */
	static const uint8_t GoldenPoll[] = {0xA5,0x5A,2,0x11,1,0,4,0x78,0x56,0x34,0x12,0xEF,0x45};
	static const uint8_t GoldenInput[] = {0xA5,0x5A,2,0x12,1,0,0x12,0x78,0x56,0x34,0x12,1,0,1,3,0,0,0x64,0,0,0,0,0,0,0,0x3F,0x4E};
	Remote_ParserTypeDef Parser;
	Remote_FrameTypeDef Frame = Poll(1), Decoded;
	RemoteJog_Input Input = {0}, Check;
	uint8_t Wire[REMOTE_MAX_FRAME];
	assert(Remote_CRC((const uint8_t *)"123456789", 9) == 0x29B1);
	assert(Remote_Encode(&Frame, Wire) == sizeof(GoldenPoll) && !memcmp(Wire, GoldenPoll, sizeof(GoldenPoll)));
	Input.session = Session; Input.request_id = 1; Input.command = REMOTE_JOG_ENABLE; Input.flags = 3; Input.speed = 100;
	assert(RemoteJog_EncodeInput(&Input, 1, &Frame));
	assert(Remote_Encode(&Frame, Wire) == sizeof(GoldenInput) && !memcmp(Wire, GoldenInput, sizeof(GoldenInput)));
	for (unsigned Split = 0; Split <= sizeof(GoldenInput); ++Split)
	{
		unsigned Count = 0;
		Remote_ParserInit(&Parser);
		for (unsigned i = 0; i < Split; ++i) Count += Remote_ParseByte(&Parser, Wire[i], &Decoded);
		for (unsigned i = Split; i < sizeof(GoldenInput); ++i) Count += Remote_ParseByte(&Parser, Wire[i], &Decoded);
		assert(Count == 1 && RemoteJog_DecodeInput(&Decoded, &Check) && Check.request_id == 1);
	}
	Remote_ParserInit(&Parser); Wire[10] ^= 1;
	for (unsigned i = 0; i < sizeof(GoldenInput); ++i) assert(!Remote_ParseByte(&Parser, Wire[i], &Decoded));
	unsigned Count = 0;
	for (unsigned i = 0; i < 3 * sizeof(GoldenPoll); ++i) Count += Remote_ParseByte(&Parser, GoldenPoll[i % sizeof(GoldenPoll)], &Decoded);
	assert(Count == 3 && Parser.Errors);
	Input.axis[0] = -1000; Input.axis[1] = 1000; Input.axis[2] = -1000; Input.frame = 1; Input.speed = 1000;
	assert(RemoteJog_EncodeInput(&Input, 65535, &Frame) && RemoteJog_DecodeInput(&Frame, &Check));
	assert(Check.axis[0] == -1000 && Check.axis[2] == -1000 && Check.speed == 1000 && Check.frame == 1);
	Input.axis[2] = 1001; assert(!RemoteJog_EncodeInput(&Input, 1, &Frame));
	Input.axis[2] = 0; Input.speed = 0; assert(!RemoteJog_EncodeInput(&Input, 1, &Frame));
	Input.speed = 1; Input.flags = 8; assert(!RemoteJog_EncodeInput(&Input, 1, &Frame));
	Input.flags = 3; Input.command = 0; assert(!RemoteJog_EncodeInput(&Input, 1, &Frame));
}

static void TestDrive(void)
{
	Jog_Remote Remote;
	RemoteJog_Input Input;
	Setup(&Remote); Enable(&Remote);
	Samples(&Remote, 4095, 0, JOG_KEY_MENU, 4, 1);
	Input = Exchange(&Remote);
	assert(Input.axis[0] == 1000 && Input.axis[1] == 1000 && Input.axis[2] == 0 && (Input.flags & 4));
	/* 点动按住超过0.8秒不能误发CANCEL；松开首样本清deadman，整段运动不作为菜单短按。 */
	Samples(&Remote, 4095, 0, JOG_KEY_MENU, 200, 1);
	assert(Remote.Authorized && Remote.Command == REMOTE_JOG_ENABLE && Remote.RequestId == 1);
	Samples(&Remote, 2048, 2048, 0, 1, 0); Input = Exchange(&Remote);
	assert(!(Input.flags & 4) && Remote.Authorized);
	Samples(&Remote, 2048, 2048, 0, 3, 0); assert(Remote.Page == JOG_PAGE_DRIVE);
	/* 短于长按阈值的点动，也不会因松开前回中而被误当菜单点击。 */
	Samples(&Remote, 2048, 2048, JOG_KEY_MENU, 4, 1);
	Samples(&Remote, 4095, 2048, JOG_KEY_MENU, 4, 1);
	Samples(&Remote, 2048, 2048, 0, 4, 1); assert(Remote.Page == JOG_PAGE_DRIVE);
	/* 真正保持中位的短按才进入菜单，菜单内按住并偏转不发送点动。 */
	Samples(&Remote, 2048, 2048, JOG_KEY_MENU, 4, 1); Samples(&Remote, 2048, 2048, 0, 4, 1);
	assert(Remote.Page == JOG_PAGE_MENU);
	Samples(&Remote, 4095, 0, JOG_KEY_MENU, 4, 1); Input = Exchange(&Remote);
	assert(!(Input.flags & 4) && !Input.axis[0] && !Input.axis[1] && !Input.axis[2]);
	Samples(&Remote, 2048, 2048, 0, 4, 1);
	Remote.Selection = JOG_MENU_AXIS;
	Samples(&Remote, 2048, 2048, JOG_KEY_MENU, 4, 1); Samples(&Remote, 2048, 2048, 0, 4, 1);
	assert(Remote.AxisMode == JOG_AXIS_Z && Remote.Authorized && Remote.Page == JOG_PAGE_MENU);
	Remote.Selection = JOG_MENU_BACK;
	Samples(&Remote, 2048, 2048, JOG_KEY_MENU, 4, 1); Samples(&Remote, 2048, 2048, 0, 4, 1);
	assert(Remote.Page == JOG_PAGE_DRIVE);
	Samples(&Remote, 2048, 0, JOG_KEY_MENU, 4, 1); Input = Exchange(&Remote);
	assert((Input.flags & 4) && !Input.axis[0] && !Input.axis[1] && Input.axis[2] == 1000);
	Jog_View(&Remote, Now); assert(strstr(Lines[0], "BASE Z") && strstr(Lines[1], "Z:+1000") && strstr(Lines[2], "HOLD SW"));
	Samples(&Remote, 4095, 4095, JOG_KEY_MENU, 4, 1); Input = Exchange(&Remote);
	assert(!Input.axis[0] && !Input.axis[1] && Input.axis[2] == -1000);
	Samples(&Remote, 2048, 2048, 0, 4, 1);
	Samples(&Remote, 2048, 0, 0, 4, 1); Input = Exchange(&Remote);
	assert(!(Input.flags & 4) && Remote.Authorized);
	Samples(&Remote, 4096, 2048, 0, 1, 0);
	Input = Exchange(&Remote); assert(!(Input.flags & 1) && !Remote.Authorized && !Remote.Ready);
	uint16_t OldId = Remote.RequestId;
	Samples(&Remote, 2048, 2048, 0, 40, 1);
	assert(Remote.Ready && !Remote.Authorized && Remote.RequestId == OldId);
}

static void TestSwitchDiagnostics(void)
{
	Jog_Remote Remote;
	RemoteJog_Input Input;
	Setup(&Remote); Enable(&Remote);
	Remote.Speed = 1000;
	/* 已授权、轴偏转，但没有SW电平输入：依然提示HOLD SW，许可位必须为0。 */
	Samples(&Remote, 4095, 2048, 0, 4, 1);
	Input = Exchange(&Remote); Jog_View(&Remote, Now);
	assert(Input.axis[0] == 1000 && !(Input.flags & REMOTE_JOG_DEADMAN));
	assert(!strcmp(Lines[2], "HOLD SW K00 V100"));
	/* 按下首样本尚未消抖；连续4个采样后才允许发deadman。 */
	Samples(&Remote, 4095, 2048, JOG_KEY_MENU, 1, 1);
	Input = Exchange(&Remote); Jog_View(&Remote, Now);
	assert(!(Input.flags & REMOTE_JOG_DEADMAN) && strstr(Lines[2], "K10"));
	Samples(&Remote, 4095, 2048, JOG_KEY_MENU, 3, 1);
	Input = Exchange(&Remote); Jog_View(&Remote, Now);
	assert((Input.flags & REMOTE_JOG_DEADMAN) && strstr(Lines[2], "K11"));
	/* 松开首样本即撤销许可，消抖状态稍后才归零。 */
	Samples(&Remote, 4095, 2048, 0, 1, 1);
	Input = Exchange(&Remote); Jog_View(&Remote, Now);
	assert(!(Input.flags & REMOTE_JOG_DEADMAN) && strstr(Lines[2], "K01"));
	Samples(&Remote, 4095, 2048, 0, 3, 1);
	Jog_View(&Remote, Now); assert(strstr(Lines[2], "K00") && Remote.Authorized);
}

static void TestLink(void)
{
	Jog_Remote Remote;
	Remote_FrameTypeDef Frame, Reply;
	Setup(&Remote); Enable(&Remote);
	uint32_t FeedbackTick = Remote.FeedbackTick;
	RemoteJog_EncodeFeedback(&PC, Sequence, &Frame);
	Now += 5; Jog_OnFrame(&Remote, &Frame, Now, &Reply);
	assert(Remote.FeedbackTick == FeedbackTick); /* 重复FEEDBACK不续命 */
	Frame = Poll(++ Sequence); assert(Jog_OnFrame(&Remote, &Frame, Now, &Reply)); Jog_TxResult(&Remote, 1);
	PC.session = Session + 1; RemoteJog_EncodeFeedback(&PC, Sequence, &Frame);
	Jog_OnFrame(&Remote, &Frame, Now, &Reply); assert(Remote.FeedbackTick == FeedbackTick);
	PC.session = Session; RemoteJog_EncodeFeedback(&PC, Sequence - 1, &Frame);
	Jog_OnFrame(&Remote, &Frame, Now, &Reply); assert(Remote.FeedbackTick == FeedbackTick);
	RemoteJog_EncodeFeedback(&PC, Sequence, &Frame); Frame.Payload[11] = 64;
	Jog_OnFrame(&Remote, &Frame, Now, &Reply); assert(Remote.FeedbackTick == FeedbackTick);
	/* 采样继续但PC反馈停止，100ms撤销授权，旧DONE永远不重新授权。 */
	Samples(&Remote, 2048, 2048, 0, 20, 0);
	assert(!Remote.Authorized && !Jog_Online(&Remote, Now));
	Exchange(&Remote); assert(!Remote.Authorized);
	Session ++; Sequence = 0;
	uint16_t OldId = Remote.RequestId;
	Samples(&Remote, 4095, 2048, JOG_KEY_MENU, 4, 0);
	RemoteJog_Input Input = Exchange(&Remote);
	assert(Input.request_id == OldId && !(Input.flags & 4) && !Remote.Authorized && Remote.NeedNeutral);
	Samples(&Remote, 2048, 2048, 0, 4, 0); Exchange(&Remote); assert(!Remote.Authorized);
	PC.flags = 5; PC.controller_state = REMOTE_JOG_DISABLED; Exchange(&Remote);
	Enable(&Remote); assert(Remote.RequestId == OldId + 1);
	Frame = Poll(++ Sequence); assert(Jog_OnFrame(&Remote, &Frame, Now, &Reply)); Jog_TxResult(&Remote, 0);
	RemoteJog_EncodeFeedback(&PC, Sequence, &Frame); Jog_OnFrame(&Remote, &Frame, Now, &Reply);
	assert(!Remote.Authorized && !Jog_Online(&Remote, Now));
	Frame = Poll(Sequence); assert(!Jog_OnFrame(&Remote, &Frame, Now, &Reply));
	Frame = Poll(Sequence - 1); assert(!Jog_OnFrame(&Remote, &Frame, Now, &Reply));
}

static void TestRequests(void)
{
	Jog_Remote Remote;
	Setup(&Remote);
	assert(Jog_Request(&Remote, REMOTE_JOG_HOME, Now));
	assert(!Jog_Request(&Remote, REMOTE_JOG_ENABLE, Now)); /* 上个请求尚无应答 */
	PC.request_id = Remote.RequestId; PC.command = REMOTE_JOG_HOME; PC.ack_state = 3;
	PC.controller_state = REMOTE_JOG_HOMING;
	Exchange(&Remote);
	assert(!Jog_Request(&Remote, REMOTE_JOG_ENABLE, Now));
	assert(Jog_Request(&Remote, REMOTE_JOG_CANCEL, Now));
	PC.request_id = Remote.RequestId; PC.command = REMOTE_JOG_CANCEL; PC.ack_state = 6;
	PC.flags = 13; PC.controller_state = REMOTE_JOG_HOME_STOPPING;
	Exchange(&Remote); assert(Remote.RequestWaiting && !Remote.Authorized && !Jog_Request(&Remote, REMOTE_JOG_HOME, Now));
	PC.ack_state = 7; PC.flags = 5; PC.controller_state = REMOTE_JOG_DISABLED;
	Exchange(&Remote); Samples(&Remote, 2048, 2048, 0, 1, 0);
	assert(Jog_Request(&Remote, REMOTE_JOG_HOME, Now));
	PC.request_id = Remote.RequestId; PC.command = REMOTE_JOG_HOME; PC.ack_state = 4;
	Exchange(&Remote); assert(!Remote.Authorized); /* HOME成功不自动使能 */
	Remote.RequestId = 65535;
	assert(Jog_Request(&Remote, REMOTE_JOG_STOP, Now) && Remote.RequestId == 1);
	assert(Jog_Request(&Remote, REMOTE_JOG_CANCEL, Now) && Remote.RequestId == 2);
	PC.controller_state = REMOTE_JOG_FAULT; PC.flags = 5; PC.request_id = 2; PC.command = REMOTE_JOG_CANCEL; PC.ack_state = 7;
	Exchange(&Remote); Samples(&Remote, 2048, 2048, 0, 1, 0);
	assert(Jog_Request(&Remote, REMOTE_JOG_RESET, Now));
	assert(!Jog_Request(&Remote, REMOTE_JOG_ENABLE, Now));
}

static void TestControllerLoss(void)
{
	Jog_Remote Remote;
	/* 控制器断更、失去输入新鲜度、故障或禁用处理中，都撤销本地授权。 */
	static const uint8_t Flags[] = {3, 6, 7, 15};
	static const uint8_t States[] = {REMOTE_JOG_IDLE, REMOTE_JOG_IDLE, REMOTE_JOG_FAULT, REMOTE_JOG_IDLE};
	for (unsigned i = 0; i < sizeof(Flags); ++i)
	{
		Setup(&Remote); Enable(&Remote);
		PC.flags = Flags[i]; PC.controller_state = States[i];
		Exchange(&Remote); assert(!Remote.Authorized);
		PC.flags = 7; PC.controller_state = REMOTE_JOG_IDLE;
		Samples(&Remote, 2048, 2048, 0, 1, 0); Exchange(&Remote);
		assert(!Remote.Authorized); /* 旧DONE不能重新授权 */
	}
}

static void TestPendingAndSampling(void)
{
	Jog_Remote Remote;
	Setup(&Remote); Remote.Page = JOG_PAGE_DRIVE;
	assert(Jog_Request(&Remote, REMOTE_JOG_ENABLE, Now));
	Samples(&Remote, 2048, 2048, JOG_KEY_MENU, 1, 0);
	assert(!Remote.EnablePending && Remote.InputInhibit);
	PC.request_id = Remote.RequestId; PC.command = REMOTE_JOG_ENABLE; PC.ack_state = 4;
	PC.flags = 7; PC.controller_state = REMOTE_JOG_IDLE;
	RemoteJog_Input Input = Exchange(&Remote);
	assert(!(Input.flags & 1) && !(Input.flags & 4) && !Remote.Authorized);
	Samples(&Remote, 2048, 2048, 0, 4, 0); Exchange(&Remote); assert(!Remote.Authorized);
	PC.controller_state = REMOTE_JOG_DISABLED; PC.flags = 4;
	Exchange(&Remote); assert(!Remote.InputInhibit && !Remote.Authorized);
	PC.flags = 5; Exchange(&Remote); Samples(&Remote, 2048, 2048, 0, 1, 0); Enable(&Remote);
	uint16_t OldId = Remote.RequestId;
	Jog_Feed(&Remote, 0, 0, 0, 0, Now + 5); Now += 5;
	assert(!Remote.Authorized && !Remote.Ready && Remote.RequestId == OldId);
	Samples(&Remote, 0, 2048, 0, 40, 1); assert(!Remote.Ready);
	Samples(&Remote, 2048, 2048, JOG_KEY_MENU, 40, 1); assert(!Remote.Ready);
	Samples(&Remote, 2048, 2048, 0, 40, 1); assert(Remote.Ready && !Remote.Authorized);
	Now += 20; Jog_Tick(&Remote, Now); assert(!Remote.Ready && !Remote.ADCValid);
	/* uint32毫秒计数回卷后仍按时间差失效。 */
	Remote.HasSample = Remote.ADCValid = Remote.Ready = 1; Remote.SampleTick = 0xFFFFFFF0u;
	Jog_Tick(&Remote, 3); assert(Remote.Ready);
	Jog_Tick(&Remote, 4); assert(!Remote.Ready);
}

static void TestMenuAndView(void)
{
	Jog_Remote Remote;
	Setup(&Remote);
	/* 实际消抖路径：短按KEEP不发HOME；上下导航需回中，确认ENABLE只分配一次编号。 */
	Samples(&Remote, 2048, 2048, JOG_KEY_MENU, 4, 0);
	Samples(&Remote, 2048, 2048, 0, 4, 0);
	assert(Remote.Page == JOG_PAGE_DRIVE && !Remote.RequestId);
	Samples(&Remote, 2048, 2048, JOG_KEY_MENU, 4, 0); Samples(&Remote, 2048, 2048, 0, 4, 0);
	assert(Remote.Page == JOG_PAGE_MENU);
	Samples(&Remote, 2048, 4095, 0, 5, 0); assert(Remote.Selection == JOG_MENU_ENABLE);
	Samples(&Remote, 2048, 4095, 0, 5, 0); assert(Remote.Selection == JOG_MENU_ENABLE);
	Samples(&Remote, 2048, 2048, 0, 1, 0); Exchange(&Remote);
	Samples(&Remote, 2048, 2048, JOG_KEY_MENU, 4, 0); Samples(&Remote, 2048, 2048, 0, 4, 0);
	assert(Remote.RequestId == 1 && Remote.Command == REMOTE_JOG_ENABLE && Remote.EnablePending);
	PC.request_id = 1; PC.command = REMOTE_JOG_ENABLE; PC.ack_state = 4; PC.flags = 7; PC.controller_state = REMOTE_JOG_IDLE;
	Exchange(&Remote); assert(Remote.Authorized);
	/* 点动页的SW短按先进入菜单；仅在菜单中长按才发送CANCEL。 */
	Samples(&Remote, 2048, 2048, JOG_KEY_MENU, 4, 0); Samples(&Remote, 2048, 2048, 0, 4, 0);
	assert(Remote.Page == JOG_PAGE_MENU);
	Samples(&Remote, 2048, 2048, JOG_KEY_MENU, 170, 1);
	assert(Remote.Command == REMOTE_JOG_CANCEL && Remote.RequestId == 2 && !Remote.Authorized);
	Samples(&Remote, 2048, 2048, 0, 4, 1); assert(Remote.RequestId == 2);
	Remote.Page = 2; PC.joint_mask = 1; PC.joints[0] = 1629;
	Exchange(&Remote); Jog_View(&Remote, Now);
	assert(strstr(Lines[1], "J1:+93.3") && !strcmp(Lines[2], "J2: --"));
	PC.joint_mask = 0; Exchange(&Remote); Jog_View(&Remote, Now); assert(!strcmp(Lines[1], "J1: --"));
	Samples(&Remote, 2048, 2048, 0, 21, 0); Jog_View(&Remote, Now);
	assert(strstr(Lines[3], "OFFLINE"));
	/* 新会话在按住确认键时到来，松开不会执行旧菜单选择。 */
	Setup(&Remote); Remote.Selection = JOG_MENU_HOME;
	Samples(&Remote, 2048, 2048, JOG_KEY_MENU, 4, 0); ++ Session; Sequence = 0; Exchange(&Remote);
	Samples(&Remote, 2048, 2048, 0, 4, 0); assert(Remote.RequestId == 0);
	Remote.Selection = JOG_MENU_SPEED;
	Samples(&Remote, 2048, 2048, JOG_KEY_MENU, 4, 0); Samples(&Remote, 2048, 2048, 0, 4, 0);
	assert(Remote.Speed == 250 && !Remote.RequestId);
}

static void TestSingleButtonHomeCancel(void)
{
	Jog_Remote Remote;
	Setup(&Remote);
	Remote.Selection = JOG_MENU_HOME;
	Samples(&Remote, 2048, 2048, JOG_KEY_MENU, 4, 0); Samples(&Remote, 2048, 2048, 0, 4, 0);
	assert(Remote.RequestId == 1 && Remote.Command == REMOTE_JOG_HOME);
	PC.request_id = 1; PC.command = REMOTE_JOG_HOME; PC.ack_state = 3;
	PC.controller_state = REMOTE_JOG_HOMING;
	Samples(&Remote, 2048, 2048, 0, 4, 1);
	Samples(&Remote, 2048, 2048, JOG_KEY_MENU, 200, 1);
	assert(Remote.RequestId == 2 && Remote.Command == REMOTE_JOG_CANCEL && !Remote.Authorized);
	Samples(&Remote, 2048, 2048, 0, 4, 1); assert(Remote.RequestId == 2);
}

static unsigned AssistantFile(Jog_Remote *Remote, const char *Name, RemoteJog_Input *Input)
{
	FILE *File = fopen(Name, "rb");
	uint8_t Wire[128], ReplyWire[REMOTE_MAX_FRAME];
	Remote_ParserTypeDef Parser;
	Remote_FrameTypeDef Frame, Reply;
	unsigned Replies = 0;
	assert(File);
	size_t Length = fread(Wire, 1, sizeof(Wire), File);
	assert(!ferror(File) && Length < sizeof(Wire)); fclose(File);
	Remote_ParserInit(&Parser);
	for (size_t i = 0; i < Length; ++i)
	{
		if (Remote_ParseByte(&Parser, Wire[i], &Frame) && Jog_OnFrame(Remote, &Frame, Now, &Reply))
		{
			assert(RemoteJog_DecodeInput(&Reply, Input));
			Jog_TxResult(Remote, 1); ++ Replies;
			if (!strcmp(Name, "01_poll_seq1.bin"))
			{
				uint8_t Size = Remote_Encode(&Reply, ReplyWire);
				FILE *Output = fopen("jog_assistant_input.bin", "wb");
				assert(Output && fwrite(ReplyWire, 1, Size, Output) == Size); fclose(Output);
			}
		}
	}
	assert(!Parser.Errors);
	return Replies;
}

static void TestAssistantFiles(void)
{
	Jog_Remote Remote;
	RemoteJog_Input Input;
	Jog_Init(&Remote); Now = 0;
	Samples(&Remote, 2048, 2048, 0, 40, 0);
	assert(AssistantFile(&Remote, "01_poll_seq1.bin", &Input) == 1);
	assert(Input.session == 0x12345678 && Input.flags == 3 && Input.speed == 100 && !Input.request_id && !Input.command);
	assert(AssistantFile(&Remote, "02_joints_seq2.bin", &Input) == 1);
	assert(Jog_Online(&Remote, Now) && Remote.Feedback.joint_mask == 63);
	assert(Remote.Feedback.joints[1] == 1571 && Remote.Feedback.joints[2] == -1571 && Remote.Feedback.joints[5] == 500);
	uint32_t LastFeedback = Remote.FeedbackTick;
	Samples(&Remote, 2048, 2048, 0, 1, 0);
	assert(!AssistantFile(&Remote, "02_joints_seq2.bin", &Input) && Remote.FeedbackTick == LastFeedback);
	assert(AssistantFile(&Remote, "03_joints_invalid_seq3.bin", &Input) == 1 && Remote.Feedback.joint_mask == 0);
	assert(AssistantFile(&Remote, "04_fault_seq4.bin", &Input) == 1);
	assert(Remote.Feedback.fault == 5 && Remote.Feedback.controller_state == REMOTE_JOG_FAULT && !Remote.Authorized);
	Samples(&Remote, 2048, 2048, 0, 21, 0);
	assert(!Jog_Online(&Remote, Now));
}

int main(void)
{
	TestWire(); TestDrive(); TestSwitchDiagnostics(); TestLink(); TestRequests(); TestControllerLoss(); TestPendingAndSampling(); TestMenuAndView(); TestSingleButtonHomeCancel();
	TestAssistantFiles();
	puts("Cartesian remote: wire vectors, XYZ/deadman, sessions, faults, requests, menu, view PASS");
	return 0;
}
