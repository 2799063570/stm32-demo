#include "RemoteView.h"
#include "OLED.h"

static const char *Remote_State(uint8_t State)
{
	static const char *Names[] = {"IDLE", "ACCEPT", "PLAN", "RUN", "DONE", "FAILED", "STOPPING", "CANCELED", "REJECT", "VERIFY", "UNKNOWN"};
	return Names[State <= REMOTE_UNKNOWN ? State : REMOTE_UNKNOWN];
}

static void Remote_ShowJoint(uint8_t Line, uint8_t Joint, const Remote_LinkTypeDef *Link, uint8_t Online)
{
	int32_t Tenths = (int32_t)Link->Joints[Joint] * 57296 / 100000;
	char Sign = Tenths < 0 ? '-' : '+';
	if (!Online || !(Link->JointMask & (1u << Joint))) { OLED_PrintLine(Line, "J%u: --", (unsigned)(Joint + 1)); return; }
	if (Tenths < 0) Tenths = -Tenths;
	OLED_PrintLine(Line, "J%u:%c%ld.%ld DEG", (unsigned)(Joint + 1), Sign, (long)(Tenths / 10), (long)(Tenths % 10));
}

void Remote_View(const Remote_InputTypeDef *Input, const Remote_LinkTypeDef *Link, uint32_t Now)
{
	uint8_t Online = Remote_LinkOnline(Link, Now);
	OLED_PrintLine(1, "PC:%s ADC:%s", Online ? "OK" : "--", Input->ADCValid ? "OK" : "ERR");
	if (!Input->Ready)
	{
		OLED_PrintLine(2, "CENTER STICK");
		OLED_PrintLine(3, Input->ADCValid ? "RELEASE TO CAL" : "CHECK / RESET");
	}
	else if (Input->Page == 0)
	{
		OLED_PrintLine(2, Input->Selection == REMOTE_HOME ? ">HOME    ZERO" : " HOME   >ZERO");
		if (!Input->RequestId) OLED_PrintLine(3, "SHORT:RUN HOLD:X");
		else OLED_PrintLine(3, "#%u %s", (unsigned)Input->RequestId,
			Online && Link->AckId == Input->RequestId && Link->AckCommand == Input->Command ? Remote_State(Link->AckState) : "WAIT");
	}
	else if (Input->Page == 1)
	{
		OLED_PrintLine(2, "X:%+5d Y:%+5d", (int)Input->X, (int)Input->Y);
		OLED_PrintLine(3, "RAW:%4u %4u", (unsigned)Input->RawX, (unsigned)Input->RawY);
	}
	else
	{
		uint8_t Joint = (uint8_t)((Input->Page - 2) * 2);
		Remote_ShowJoint(2, Joint, Link, Online);
		Remote_ShowJoint(3, (uint8_t)(Joint + 1), Link, Online);
	}
	/*把请求应答和当前运动状态分开显示：第二次请求被拒绝时，仍能看到原动作正在执行。*/
	if (!Online) OLED_PrintLine(4, "OFFLINE P%u/5", (unsigned)(Input->Page + 1));
	else if (Link->Source == REMOTE_SOURCE_DEMO) OLED_PrintLine(4, "DEMO P%u/5", (unsigned)(Input->Page + 1));
	else OLED_PrintLine(4, "%s %s", Remote_State(Link->MotionState), Link->MotionTarget == REMOTE_HOME ? "HOME" : Link->MotionTarget == REMOTE_ZERO ? "ZERO" : "");
}
