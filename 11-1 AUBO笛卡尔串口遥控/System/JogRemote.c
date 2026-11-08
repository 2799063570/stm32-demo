#include "JogRemote.h"
#include "JogConfig.h"
#include <string.h>

static void Jog_Revoke(Jog_Remote *Remote)
{
	Remote->Authorized = 0;
	Remote->EnablePending = 0;
	Remote->NeedNeutral = 1;
	Remote->ButtonArmed = 0;
	Remote->MenuPressed = 0;
}

static void Jog_CalibrationReset(Jog_Remote *Remote)
{
	Remote->Ready = Remote->Samples = 0;
	Remote->SumX = Remote->SumY = 0;
	Remote->MinX = Remote->MinY = 4095;
	Remote->MaxX = Remote->MaxY = 0;
	Remote->X = Remote->Y = 0;
}

void Jog_Init(Jog_Remote *Remote)
{
	memset(Remote, 0, sizeof(*Remote));
	Remote->Page = JOG_PAGE_MENU;
	Remote->Speed = JOG_DEFAULT_SPEED;
	Remote->NeedNeutral = 1;
	Remote->Feedback.home_state = 255;
	Remote->Feedback.home_error = 65535;
	Jog_CalibrationReset(Remote);
}

uint8_t Jog_Online(const Jog_Remote *Remote, uint32_t Now)
{
	return Remote->HasFeedback && (uint32_t)(Now - Remote->FeedbackTick) < JOG_FEEDBACK_TIMEOUT_MS;
}

uint8_t Jog_Neutral(const Jog_Remote *Remote)
{
	return Remote->ADCValid && Remote->Ready && !Remote->X && !Remote->Y &&
		!Remote->RawKeys;
}

void Jog_Disconnect(Jog_Remote *Remote)
{
	Jog_Revoke(Remote);
	Remote->HasFeedback = Remote->AwaitFeedback = Remote->PollPrepared = 0;
	Remote->RequestWaiting = 0;
	/* RequestId/Command保留，作为新会话基线；不自动递增或重新使能。 */
	Remote->Feedback.flags = 0;
	Remote->Feedback.joint_mask = 0;
	Remote->Feedback.controller_state = REMOTE_JOG_UNKNOWN;
}

void Jog_Tick(Jog_Remote *Remote, uint32_t Now)
{
	if (Remote->HasSample && (uint32_t)(Now - Remote->SampleTick) >= JOG_SAMPLE_TIMEOUT_MS)
	{
		Remote->HasSample = Remote->ADCValid = 0;
		Jog_CalibrationReset(Remote);
		Jog_Revoke(Remote);
	}
	if (Remote->HasFeedback && !Jog_Online(Remote, Now)) Jog_Disconnect(Remote);
}

uint8_t Jog_Request(Jog_Remote *Remote, uint8_t Command, uint32_t Now)
{
	Jog_Tick(Remote, Now);
	if (Command < REMOTE_JOG_ENABLE || Command > REMOTE_JOG_RESET) return 0;
	if (Command != REMOTE_JOG_STOP && Command != REMOTE_JOG_CANCEL)
	{
		if (!Jog_Online(Remote, Now) || !Jog_Neutral(Remote) || Remote->NeedNeutral ||
			Remote->EnablePending || Remote->RequestWaiting || (Remote->Feedback.flags & 8) || !(Remote->Feedback.flags & 4) ||
			(Remote->Feedback.flags & 2)) return 0;
		if (Command == REMOTE_JOG_RESET)
		{
			if (Remote->Feedback.controller_state != REMOTE_JOG_FAULT &&
				Remote->Feedback.controller_state != REMOTE_JOG_DISABLED) return 0;
		}
		else if (Remote->Feedback.controller_state != REMOTE_JOG_DISABLED) return 0;
	}
	Remote->RequestId ++;
	if (!Remote->RequestId) Remote->RequestId = 1;
	Remote->Command = Command;
	Remote->RequestWaiting = 1;
	Remote->Authorized = 0;
	Remote->EnablePending = Command == REMOTE_JOG_ENABLE;
	if (!Remote->EnablePending) Remote->NeedNeutral = 1;
	return 1;
}

static int16_t Jog_Axis(uint16_t Raw, uint16_t Center)
{
	int32_t Delta = (int32_t)Raw - Center;
	int32_t Magnitude = Delta < 0 ? -Delta : Delta;
	int32_t Range = Delta < 0 ? Center : 4095 - Center;
	if (Magnitude <= JOG_DEAD_ZONE || Range <= JOG_DEAD_ZONE) return 0;
	Magnitude = (Magnitude - JOG_DEAD_ZONE) * 1000 / (Range - JOG_DEAD_ZONE);
	if (Magnitude > 1000) Magnitude = 1000;
	return (int16_t)(Delta < 0 ? -Magnitude : Magnitude);
}

static void Jog_Calibrate(Jog_Remote *Remote, uint16_t X, uint16_t Y)
{
	/* 校准窗口必须松开所有按键且处于ADC中间半区；抖动窗口整组重试。 */
	if (X < 1024 || X > 3071 || Y < 1024 || Y > 3071 || Remote->RawKeys)
	{
		Jog_CalibrationReset(Remote);
		return;
	}
	if (X < Remote->MinX) Remote->MinX = X;
	if (X > Remote->MaxX) Remote->MaxX = X;
	if (Y < Remote->MinY) Remote->MinY = Y;
	if (Y > Remote->MaxY) Remote->MaxY = Y;
	Remote->SumX += X; Remote->SumY += Y;
	if (++ Remote->Samples == 32)
	{
		if (Remote->MaxX - Remote->MinX <= 200 && Remote->MaxY - Remote->MinY <= 200)
		{
			Remote->CenterX = (uint16_t)(Remote->SumX / 32);
			Remote->CenterY = (uint16_t)(Remote->SumY / 32);
			Remote->Ready = 1;
		}
		Remote->Samples = 0; Remote->SumX = Remote->SumY = 0;
		Remote->MinX = Remote->MinY = 4095; Remote->MaxX = Remote->MaxY = 0;
	}
}

static void Jog_Select(Jog_Remote *Remote, uint32_t Now)
{
	static const uint16_t Speeds[] = {10, 50, 100, 250, 500, 1000};
	uint8_t Index;
	if (!Jog_Neutral(Remote) || Remote->NeedNeutral) return;
	if (Remote->Page != JOG_PAGE_MENU)
	{
		Remote->Page = JOG_PAGE_MENU;
		return;
	}
	if (Remote->Selection == JOG_MENU_KEEP || Remote->Selection == JOG_MENU_BACK)
		Remote->Page = JOG_PAGE_DRIVE;
	else if (Remote->Selection <= JOG_MENU_RESET)
	{
		if (Jog_Request(Remote, Remote->Selection, Now) && Remote->Selection == JOG_MENU_ENABLE)
			Remote->Page = JOG_PAGE_DRIVE;
	}
	else if (Remote->Selection == JOG_MENU_AXIS && !Remote->EnablePending && !Remote->RequestWaiting)
		Remote->AxisMode ^= 1;                   /* 回中松键切换XY/Z，下一次重新按住才点动 */
	else if (!Remote->Authorized && !Remote->EnablePending)
	{
		if (Remote->Selection == JOG_MENU_FRAME) Remote->Frame ^= 1;
		else if (Remote->Selection == JOG_MENU_SPEED)
		{
			for (Index = 0; Index < 6 && Speeds[Index] != Remote->Speed; Index ++);
			Remote->Speed = Speeds[(Index + 1) % 6];
		}
	}
}

/** 函数：同一SW在菜单中确认/取消，在点动页按住允许速度；松开立即清deadman。 */
void Jog_Feed(Jog_Remote *Remote, uint16_t X, uint16_t Y, uint8_t Keys, uint8_t Valid, uint32_t Now)
{
	uint8_t OldKeys, OldRawKeys, Released;
	Jog_Tick(Remote, Now);
	Remote->SampleTick = Now; Remote->HasSample = 1;
	OldRawKeys = Remote->RawKeys;
	Remote->RawKeys = Keys & JOG_KEY_MENU;
	Remote->RawX = X; Remote->RawY = Y;
	OldKeys = Remote->Keys;
	if (Remote->RawKeys == Remote->Keys) Remote->KeyCount = 0;
	else if (++ Remote->KeyCount >= 4)
	{
		Remote->Keys = Remote->RawKeys; Remote->KeyCount = 0;
	}
	Released = (OldKeys & JOG_KEY_MENU) && !(Remote->Keys & JOG_KEY_MENU);
	Remote->ADCValid = Valid && X <= 4095 && Y <= 4095;
	if (!Remote->ADCValid)
	{
		Jog_CalibrationReset(Remote);
		Jog_Revoke(Remote);
	}
	else if (!Remote->Ready) Jog_Calibrate(Remote, X, Y);
	else
	{
		Remote->X = Jog_Axis(X, Remote->CenterX); Remote->Y = Jog_Axis(Y, Remote->CenterY);
#if JOG_SWAP_XY
		{ int16_t Temp = Remote->X; Remote->X = Remote->Y; Remote->Y = Temp; }
#endif
#if JOG_INVERT_X
		Remote->X = -Remote->X;
#endif
#if JOG_INVERT_Y
		Remote->Y = -Remote->Y;
#endif
	}
	/* 整段按键只要偏转过，即使松开时已回中，也不把运动结束误当菜单短按。 */
	if (Remote->RawKeys && !OldRawKeys && !OldKeys)
	{
		Remote->PressMoved = 0;
		Remote->PressJog = Remote->Page == JOG_PAGE_DRIVE && Remote->Authorized;
	}
	if ((Remote->RawKeys || OldKeys) && (Remote->X || Remote->Y)) Remote->PressMoved = 1;
	/* 使能应答到达之前输入条件变化，撤销本地授权资格；实时轴仍会送PC用于否决迟到结果。 */
	if (Remote->EnablePending && !Jog_Neutral(Remote))
	{
		Remote->InputInhibit = 1;
		Jog_Revoke(Remote);
	}
	if (Jog_Neutral(Remote) && !Remote->RawKeys && !Remote->Keys)
	{
		Remote->NeedNeutral = 0;
		Remote->ButtonArmed = 1;
	}
	if (!(OldKeys & JOG_KEY_MENU) && (Remote->Keys & JOG_KEY_MENU))
	{
		Remote->PressTick = Now;
		Remote->LongSent = 0;
		Remote->MenuPressed = Remote->ButtonArmed;
	}
	/* 已授权的点动按住可以任意持续，不与0.8秒取消混用；菜单/HOME中才长按取消。 */
	if ((Remote->Keys & JOG_KEY_MENU) && Remote->ButtonArmed && !Remote->LongSent && !Remote->PressJog &&
		(uint32_t)(Now - Remote->PressTick) >= JOG_LONG_PRESS_MS)
	{
		Jog_Request(Remote, REMOTE_JOG_CANCEL, Now);
		Remote->LongSent = 1;
		Remote->Page = JOG_PAGE_MENU;
		Remote->Selection = JOG_MENU_CANCEL;
	}
	if (Released)
	{
		if (Remote->MenuPressed && !Remote->LongSent && !Remote->PressMoved &&
			(uint32_t)(Now - Remote->PressTick) < JOG_LONG_PRESS_MS) Jog_Select(Remote, Now);
		Remote->MenuPressed = 0;
	}
	if (!Remote->X && !Remote->Y) Remote->NavLatched = 0;
	if (Remote->Page != JOG_PAGE_DRIVE && Remote->Ready && !Remote->RawKeys && !Remote->Keys &&
		!Remote->NavLatched && !Remote->NeedNeutral)
	{
		if (Remote->X > 600) { Remote->Page = Remote->Page == 5 ? 1 : (uint8_t)(Remote->Page + 1); Remote->NavLatched = 1; }
		else if (Remote->X < -600) { Remote->Page = Remote->Page == 1 ? 5 : (uint8_t)(Remote->Page - 1); Remote->NavLatched = 1; }
		else if (Remote->Page == JOG_PAGE_MENU && Remote->Y > 600)
		{ Remote->Selection = (Remote->Selection + JOG_MENU_COUNT - 1) % JOG_MENU_COUNT; Remote->NavLatched = 1; }
		else if (Remote->Page == JOG_PAGE_MENU && Remote->Y < -600)
		{ Remote->Selection = (Remote->Selection + 1) % JOG_MENU_COUNT; Remote->NavLatched = 1; }
	}
}

static uint8_t Jog_FeedbackValid(const RemoteJog_Feedback *Feedback)
{
	return Feedback->command <= REMOTE_JOG_RESET && Feedback->ack_state <= 10 &&
		Feedback->controller_state <= REMOTE_JOG_FAULT && Feedback->flags <= 15 && Feedback->joint_mask <= 63 &&
		(Feedback->home_state <= 9 || Feedback->home_state == 255) &&
		((Feedback->request_id == 0) == (Feedback->command == 0));
}

/** 函数：仅对新POLL生成当前输入；只接收已成功发送回复对应的一份FEEDBACK。 */
uint8_t Jog_OnFrame(Jog_Remote *Remote, const Remote_FrameTypeDef *Frame, uint32_t Now, Remote_FrameTypeDef *Reply)
{
	RemoteJog_Input Input;
	RemoteJog_Feedback Feedback;
	uint32_t Session;
	uint8_t Drive;
	Jog_Tick(Remote, Now);
	if (Frame->Type == REMOTE_JOG_POLL && Frame->Length == 4)
	{
		Session = Remote_GetU32(Frame->Payload);
		if (!Remote->HasSession || Remote->Session != Session)
		{
			Jog_Disconnect(Remote);
			Remote->Session = Session;
			Remote->HasSession = 1;
		}
		else if (Frame->Sequence <= Remote->Sequence) return 0;
		Remote->Sequence = Frame->Sequence; Remote->PollTick = Now;
		Remote->AwaitFeedback = 0; Remote->PollPrepared = 1;
		memset(&Input, 0, sizeof(Input));
		Input.session = Session;
		Input.request_id = Remote->RequestId; Input.command = Remote->Command;
		Input.flags = (Remote->ADCValid && !Remote->InputInhibit ? REMOTE_JOG_ADC_VALID : 0) |
			(Remote->Ready ? REMOTE_JOG_CALIBRATED : 0);
		Input.frame = Remote->Frame; Input.speed = Remote->Speed;
		Drive = Remote->Page == JOG_PAGE_DRIVE && Remote->ADCValid && Remote->Ready;
		/* 即使未授权也上报实际轴/键，PC可判断使能条件变化；菜单轴不参与点动。 */
		if (Drive)
		{
			if (Remote->AxisMode == JOG_AXIS_Z) Input.axis[2] = Remote->Y;
			else { Input.axis[0] = Remote->X; Input.axis[1] = Remote->Y; }
		}
		if (Drive && Remote->Authorized && !Remote->NeedNeutral && Jog_Online(Remote, Now) &&
			(Remote->RawKeys & Remote->Keys & JOG_KEY_MENU))
			Input.flags |= REMOTE_JOG_DEADMAN;
		return RemoteJog_EncodeInput(&Input, Frame->Sequence, Reply);
	}
	if (!Remote->AwaitFeedback || Frame->Sequence != Remote->Sequence ||
		(uint32_t)(Now - Remote->PollTick) >= JOG_FEEDBACK_TIMEOUT_MS ||
		!RemoteJog_DecodeFeedback(Frame, &Feedback) || Feedback.session != Remote->Session || !Jog_FeedbackValid(&Feedback)) return 0;
	Remote->AwaitFeedback = 0;
	Remote->Feedback = Feedback; Remote->FeedbackTick = Now; Remote->HasFeedback = 1;
	if (Feedback.request_id == Remote->RequestId && Feedback.command == Remote->Command &&
		(Feedback.ack_state == 4 || Feedback.ack_state == 5 || Feedback.ack_state == 7 || Feedback.ack_state == 8))
		Remote->RequestWaiting = 0;
	if (Remote->InputInhibit && Feedback.controller_state == REMOTE_JOG_DISABLED && !(Feedback.flags & 10) && Jog_Neutral(Remote))
		Remote->InputInhibit = 0;
	if (!(Feedback.flags & 1) || !(Feedback.flags & 4) ||
		Feedback.controller_state == REMOTE_JOG_UNKNOWN || Feedback.controller_state == REMOTE_JOG_FAULT)
		Jog_Revoke(Remote);
	else if (Remote->EnablePending && Feedback.request_id == Remote->RequestId && Feedback.command == REMOTE_JOG_ENABLE)
	{
		if (Feedback.ack_state == JOG_ACK_DONE && (Feedback.flags & 2) && !(Feedback.flags & 8) &&
			Feedback.controller_state == REMOTE_JOG_IDLE && Jog_Neutral(Remote) && !Remote->NeedNeutral)
		{ Remote->Authorized = 1; Remote->EnablePending = 0; }
		else if (Feedback.ack_state >= 4) Jog_Revoke(Remote);
	}
	else if (Remote->Authorized && (!(Feedback.flags & 2) || (Feedback.flags & 8) ||
		(Feedback.controller_state != REMOTE_JOG_IDLE && Feedback.controller_state != REMOTE_JOG_MOVING))) Jog_Revoke(Remote);
	return 0;
}

void Jog_TxResult(Jog_Remote *Remote, uint8_t Sent)
{
	Remote->AwaitFeedback = Remote->PollPrepared && Sent;
	Remote->PollPrepared = 0;
	if (!Sent) Jog_Disconnect(Remote);
}
