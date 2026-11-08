#ifndef __JOG_REMOTE_H
#define __JOG_REMOTE_H
#include "RemoteJogProtocol.h"

#define JOG_KEY_MENU 1
#define JOG_AXIS_XY 0
#define JOG_AXIS_Z 1
#define JOG_PAGE_DRIVE 0
#define JOG_PAGE_MENU 1
#define JOG_PAGE_STATUS 5
#define JOG_MENU_KEEP 0
#define JOG_MENU_ENABLE 1
#define JOG_MENU_STOP 2
#define JOG_MENU_HOME 3
#define JOG_MENU_CANCEL 4
#define JOG_MENU_RESET 5
#define JOG_MENU_FRAME 6
#define JOG_MENU_SPEED 7
#define JOG_MENU_AXIS 8
#define JOG_MENU_BACK 9
#define JOG_MENU_COUNT 10
#define JOG_ACK_DONE 4

typedef struct
{
	uint32_t SumX, SumY, SampleTick, FeedbackTick, PollTick, PressTick;
	uint16_t RawX, RawY, CenterX, CenterY, MinX, MaxX, MinY, MaxY;
	int16_t X, Y;
	uint8_t Samples, ADCValid, Ready, HasSample;
	uint8_t RawKeys, Keys, KeyCount, ButtonArmed, MenuPressed, LongSent, NavLatched;
	uint8_t PressMoved, PressJog, AxisMode;
	uint8_t Page, Selection, Frame, NeedNeutral, Authorized, EnablePending;
	uint8_t HasSession, HasFeedback, AwaitFeedback, PollPrepared;
	uint8_t RequestWaiting, InputInhibit;
	uint16_t Speed, RequestId, Sequence;
	uint8_t Command;
	uint32_t Session;
	RemoteJog_Feedback Feedback;
} Jog_Remote;

void Jog_Init(Jog_Remote *Remote);
void Jog_Feed(Jog_Remote *Remote, uint16_t X, uint16_t Y, uint8_t Keys, uint8_t Valid, uint32_t Now);
void Jog_Tick(Jog_Remote *Remote, uint32_t Now);
void Jog_Disconnect(Jog_Remote *Remote);
uint8_t Jog_Online(const Jog_Remote *Remote, uint32_t Now);
uint8_t Jog_Neutral(const Jog_Remote *Remote);
/* 菜单调用：失败不会分配编号，STOP/CANCEL可抢占正在等待的操作。 */
uint8_t Jog_Request(Jog_Remote *Remote, uint8_t Command, uint32_t Now);
uint8_t Jog_OnFrame(Jog_Remote *Remote, const Remote_FrameTypeDef *Frame, uint32_t Now, Remote_FrameTypeDef *Reply);
/* 必须回报DMA是否接收本次回复；发送失败不允许接受对应FEEDBACK。 */
void Jog_TxResult(Jog_Remote *Remote, uint8_t Sent);
#endif
