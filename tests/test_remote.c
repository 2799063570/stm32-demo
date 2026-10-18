#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "RemoteView.h"
#include "OLED.h"
#include "OLED_SPI.h"

static uint32_t Tick;
static uint8_t Panel[1024], Page, Column;
uint32_t Delay_GetTick(void) { return Tick; }
void OLED_SPI_Init(void) { memset(Panel, 0, sizeof(Panel)); }
void OLED_SPI_SetReset(uint8_t Level) { (void)Level; }
uint8_t OLED_SPI_Write(uint8_t Control, const uint8_t *Data, uint8_t Length)
{
	assert(Length >= 1 && Length <= 8);
	if (Control == 0x40)
	{
		assert(Page < 8 && (unsigned)Column + Length <= 128);
		memcpy(Panel + Page * 128 + Column, Data, Length); Column = (uint8_t)(Column + Length);
	}
	else if (Length == 3 && Data[0] >= 0xB0 && Data[0] <= 0xB7)
	{
		Page = Data[0] & 7; Column = (uint8_t)((Data[1] & 15) | ((Data[2] & 15) << 4));
	}
	return 1;
}

static unsigned Parse(Remote_ParserTypeDef *Parser, const uint8_t *Data, unsigned Length, Remote_FrameTypeDef *Frame)
{
	unsigned Count = 0;
	for (unsigned i = 0; i < Length; i ++) Count += Remote_ParseByte(Parser, Data[i], Frame);
	return Count;
}
static void Save(const char *Name, const uint8_t *Data, unsigned Length)
{
	FILE *File = fopen(Name, "wb");
	assert(File && fwrite(Data, 1, Length, File) == Length); fclose(File);
}

static void TestProtocol(void)
{
	Remote_ParserTypeDef Parser;
	Remote_FrameTypeDef Frame = {0}, Decoded;
	uint8_t Data[REMOTE_MAX_FRAME], Bad[REMOTE_MAX_FRAME];
	assert(Remote_CRC((const uint8_t *)"123456789", 9) == 0x29B1);
	Frame.Type = REMOTE_POLL; Frame.Sequence = 42; Frame.Length = 4;
	Remote_PutU32(Frame.Payload, 0x10203040);
	unsigned Length = Remote_Encode(&Frame, Data);
	assert(Length == 13);
	Save("remote_poll_frame.bin", Data, Length);
	for (unsigned Split = 0; Split <= Length; Split ++)
	{
		Remote_ParserInit(&Parser);
		assert(Parse(&Parser, Data, Split, &Decoded) + Parse(&Parser, Data + Split, Length - Split, &Decoded) == 1);
		assert(Decoded.Type == REMOTE_POLL && Decoded.Sequence == 42 && Remote_GetU32(Decoded.Payload) == 0x10203040);
	}
	Remote_ParserInit(&Parser);
	memcpy(Bad, Data, Length); Bad[7] ^= 1;
	assert(Parse(&Parser, Bad, Length, &Decoded) == 0);
	assert(Parse(&Parser, Data, Length, &Decoded) == 1 && Parser.Errors > 0);
	for (unsigned i = 0; i < 4096; i ++) Remote_ParseByte(&Parser, (uint8_t)(i * 17u), &Decoded);
	assert(Parse(&Parser, Data, Length, &Decoded) == 1);
	assert(Parser.Used < REMOTE_MAX_FRAME);
	Frame.Length = REMOTE_MAX_PAYLOAD;
	memset(Frame.Payload, 0xA5, sizeof(Frame.Payload));
	Length = Remote_Encode(&Frame, Data);
	assert(Length == REMOTE_MAX_FRAME && Parse(&Parser, Data, Length, &Decoded) == 1);
	Frame.Length = REMOTE_MAX_PAYLOAD + 1;
	assert(Remote_Encode(&Frame, Data) == 0);
	Remote_PutU16(Data, (uint16_t)-32768);
	assert(Remote_GetI16(Data) == -32768);
}

static void Feed(Remote_InputTypeDef *Input, uint16_t X, uint16_t Y, uint8_t Button, unsigned Count)
{
	for (unsigned i = 0; i < Count; i ++) Remote_InputFeed(Input, X, Y, Button);
}
static void TestInput(void)
{
	Remote_InputTypeDef Input;
	Remote_InputInit(&Input);
	Feed(&Input, 4095, 0, 0, 64); assert(!Input.Ready);
	for (unsigned i = 0; i < 32; i ++) Feed(&Input, i % 2 ? 1800 : 2300, 2048, 0, 1);
	assert(!Input.Ready);
	Feed(&Input, 2048, 2048, 0, 32); assert(Input.Ready && Input.ADCValid);
	Feed(&Input, 2348, 1748, 0, 1); assert(Input.X == 0 && Input.Y == 0);
	Feed(&Input, 3000, 1000, 0, 1); assert(Input.X > 0 && Input.X < 1000 && Input.Y > 0 && Input.Y < 1000);
	Feed(&Input, 4095, 0, 0, 1); assert(Input.X == 1000 && Input.Y == 1000);
	Feed(&Input, 0, 4095, 0, 1); assert(Input.X == -1000 && Input.Y == -1000);
	Input.Page = 0;
	Feed(&Input, 2048, 2048, 1, 3); assert(!Remote_InputButton(&Input) && Input.RequestId == 0);
	Feed(&Input, 2048, 2048, 0, 1);
	Feed(&Input, 2048, 2048, 1, 4); assert(Remote_InputButton(&Input) && Input.RequestId == 0);
	Feed(&Input, 2048, 2048, 1, 100); assert(Input.RequestId == 0);
	Feed(&Input, 2048, 2048, 0, 4); assert(Input.RequestId == 1 && Input.Command == REMOTE_HOME);
	Feed(&Input, 2048, 2048, 1, 164); assert(Input.RequestId == 2 && Input.Command == REMOTE_CANCEL);
	Feed(&Input, 2048, 2048, 0, 4); assert(Input.RequestId == 2); // 长按松开不产生第二次HOME
	Remote_InputInit(&Input);
	Feed(&Input, 2048, 2048, 1, 64); assert(Input.Page == 0 && !Remote_InputButton(&Input));
}

static void Snapshot(const char *Name)
{
	for (unsigned i = 0; i < 2000; i ++) { Tick ++; OLED_Service(); }
	assert(OLED_GetReady() && memcmp(Panel, OLED_GetBuffer(), 1024) == 0);
	Save(Name, OLED_GetBuffer(), 1024);
}
static void TestLinkAndView(void)
{
	Remote_InputTypeDef Input;
	Remote_LinkTypeDef Link;
	Remote_FrameTypeDef Poll = {0}, Reply, Feedback = {0};
	uint8_t Data[REMOTE_MAX_FRAME];
	int16_t Joints[] = {0, 1571, -1571, 3142, -3142, 500};
	Remote_InputInit(&Input); Remote_LinkInit(&Link);
	Feed(&Input, 2048, 2048, 0, 32); Feed(&Input, 4095, 0, 0, 1);
	Input.Page = 0;
	Poll.Type = REMOTE_POLL; Poll.Length = 4; Poll.Sequence = 42;
	Remote_PutU32(Poll.Payload, 0x10203040);
	assert(Remote_LinkFrame(&Link, &Poll, &Input, 100, &Reply) && !Remote_LinkOnline(&Link, 100));
	assert(Reply.Type == REMOTE_INPUT && Reply.Sequence == 42 && Reply.Length == 18);
	assert(Remote_GetI16(Reply.Payload + 8) == 1000 && Remote_GetI16(Reply.Payload + 10) == 1000);
	Save("remote_input_frame.bin", Data, Remote_Encode(&Reply, Data));
	Feedback.Type = REMOTE_FEEDBACK; Feedback.Length = 24; Feedback.Sequence = 42;
	Remote_PutU32(Feedback.Payload, 0x10203040); Feedback.Payload[4] = REMOTE_SOURCE_ROS; Feedback.Payload[5] = 63;
	for (unsigned i = 0; i < 6; i ++) Remote_PutU16(Feedback.Payload + 6 + 2 * i, (uint16_t)Joints[i]);
	Feedback.Payload[22] = REMOTE_UNKNOWN;
	Save("remote_feedback_frame.bin", Data, Remote_Encode(&Feedback, Data));
	Remote_LinkFrame(&Link, &Feedback, &Input, 110, &Reply);
	assert(Remote_LinkOnline(&Link, 610) && !Remote_LinkOnline(&Link, 611) && Link.Joints[2] == -1571);
	Feedback.Sequence = 41; Remote_LinkFrame(&Link, &Feedback, &Input, 120, &Reply);
	assert(Link.FeedbackTick == 110);
	Feedback.Sequence = 42; Remote_LinkFrame(&Link, &Feedback, &Input, 201, &Reply);
	assert(Link.FeedbackTick == 110);                // 过期反馈不能恢复在线状态
	Remote_PutU32(Poll.Payload, 0x11223344);
	Remote_LinkFrame(&Link, &Poll, &Input, 300, &Reply);
	Remote_LinkFrame(&Link, &Feedback, &Input, 301, &Reply);
	assert(!Remote_LinkOnline(&Link, 301));           // 旧会话反馈被拒绝
	Remote_PutU32(Poll.Payload, 0x10203040);
	Remote_LinkFrame(&Link, &Poll, &Input, 0xFFFFFFF0u, &Reply);
	Remote_LinkFrame(&Link, &Feedback, &Input, 10, &Reply);
	assert(Remote_LinkOnline(&Link, 20));             // 毫秒计数回绕
	OLED_Init(); Tick = 0;
	Remote_View(&Input, &Link, 20); Snapshot("remote_menu_oled.bin");
	Input.Page = 1;
	Remote_View(&Input, &Link, 20); Snapshot("remote_input_oled.bin");
	Input.Page = 2;
	Remote_View(&Input, &Link, 20); Snapshot("remote_joints_oled.bin");
	Remote_View(&Input, &Link, 600); Snapshot("remote_offline_oled.bin");
	Link.Source = REMOTE_SOURCE_DEMO;
	Remote_View(&Input, &Link, 20); Snapshot("remote_demo_oled.bin");
	Input.Page = 0; Input.Command = REMOTE_HOME; Input.RequestId = 1;
	Link.Source = REMOTE_SOURCE_ROS; Link.AckId = 1; Link.AckCommand = REMOTE_HOME;
	Link.AckState = REMOTE_ACCEPTED; Link.MotionState = REMOTE_PLANNING; Link.MotionTarget = REMOTE_HOME;
	Remote_View(&Input, &Link, 20); Snapshot("remote_planning_oled.bin");
	Link.AckState = REMOTE_SUCCEEDED; Link.MotionState = REMOTE_SUCCEEDED;
	Remote_View(&Input, &Link, 20); Snapshot("remote_done_oled.bin");
}

int main(void)
{
	TestProtocol(); TestInput(); TestLinkAndView();
	puts("PASS: remote CRC/framing, proportional axes/calibration/button, session/timeout, OLED frames");
	return 0;
}
