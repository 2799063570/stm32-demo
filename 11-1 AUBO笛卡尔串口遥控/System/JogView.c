#include "JogView.h"
#include "OLED.h"

static const char *Jog_State(uint8_t State)
{
	static const char *Names[] = {"UNKNOWN", "WAIT FB", "DISABLED", "ENABLED", "JOGGING", "STOPPING", "HOME RUN", "HOME STOP", "FAULT"};
	return Names[State <= 8 ? State : 0];
}

static const char *Jog_Ack(uint8_t State)
{
	static const char *Names[] = {"IDLE", "ACCEPT", "PLAN", "RUN", "DONE", "FAILED", "STOPPING", "CANCELED", "REJECT", "VERIFY", "UNKNOWN"};
	return Names[State <= 10 ? State : 10];
}

static void Jog_Joint(uint8_t Line, uint8_t Joint, const Jog_Remote *Remote, uint8_t Online)
{
	int32_t Tenths = (int32_t)Remote->Feedback.joints[Joint] * 57296 / 100000;
	char Sign = Tenths < 0 ? '-' : '+';
	if (!Online || !(Remote->Feedback.joint_mask & (1u << Joint)))
	{ OLED_PrintLine(Line, "J%u: --", (unsigned)(Joint + 1)); return; }
	if (Tenths < 0) Tenths = -Tenths;
	OLED_PrintLine(Line, "J%u:%c%ld.%ld DEG", (unsigned)(Joint + 1), Sign, (long)(Tenths / 10), (long)(Tenths % 10));
}

void Jog_View(const Jog_Remote *Remote, uint32_t Now)
{
	static const char *Menu[] = {"KEEP POSITION", "ENABLE", "STOP / DISABLE", "HOME", "CANCEL HOME", "RESET FAULT", "BASE / TOOL", "SET SPEED", "XY / Z MODE", "BACK TO JOG"};
	uint8_t Online = Jog_Online(Remote, Now);
	OLED_PrintLine(1, "PC:%s %s %s", Online ? "OK" : "--", Remote->Frame ? "TOOL" : "BASE", Remote->AxisMode ? "Z" : "XY");
	if (!Remote->Ready || !Remote->ADCValid)
	{
		OLED_PrintLine(2, Remote->ADCValid ? "CENTER + RELEASE" : "INPUT ERROR");
		OLED_PrintLine(3, "CAL:%2u/32", (unsigned)Remote->Samples);
	}
	else if (Remote->NeedNeutral)
	{
		OLED_PrintLine(2, "CENTER + RELEASE");
		OLED_PrintLine(3, "RE-ENABLE NEEDED");
	}
	else if (Remote->Page == JOG_PAGE_MENU)
	{
		OLED_PrintLine(2, ">%s", Menu[Remote->Selection < JOG_MENU_COUNT ? Remote->Selection : 0]);
		OLED_PrintLine(3, "V:%u.%u MM/S", (unsigned)(Remote->Speed / 10), (unsigned)(Remote->Speed % 10));
	}
	else if (Remote->Page == JOG_PAGE_DRIVE)
	{
		if (Remote->AxisMode == JOG_AXIS_Z) OLED_PrintLine(2, "Z:%+5d UP/DOWN", (int)Remote->Y);
		else OLED_PrintLine(2, "X:%+5d Y:%+5d", (int)Remote->X, (int)Remote->Y);
		/* K的两位依次为PA2原始按下状态、消抖后状态；HOLD SW本身只表示已授权。
		 * 菜单速度为整数mm/s预设，点动行省略小数，最大100时仍不超过16字符。 */
		OLED_PrintLine(3, "%s K%u%u V%u", Remote->Authorized ? "HOLD SW" : "LOCKED",
			(unsigned)!!(Remote->RawKeys & JOG_KEY_MENU), (unsigned)!!(Remote->Keys & JOG_KEY_MENU),
			(unsigned)(Remote->Speed / 10));
	}
	else if (Remote->Page < JOG_PAGE_STATUS)
	{
		uint8_t Joint = (uint8_t)((Remote->Page - 2) * 2);
		Jog_Joint(2, Joint, Remote, Online); Jog_Joint(3, (uint8_t)(Joint + 1), Remote, Online);
	}
	else
	{
		if (Online) OLED_PrintLine(2, "F:%u H:%u", (unsigned)Remote->Feedback.fault, (unsigned)Remote->Feedback.home_state);
		else OLED_PrintLine(2, "F:-- H:--");
		if (Online && Remote->Feedback.home_error != 65535)
			OLED_PrintLine(3, "ERR:%u MRAD", (unsigned)Remote->Feedback.home_error);
		else OLED_PrintLine(3, "ERR:--");
	}
	/* 4行屏幕交替呈现控制器状态和请求结果，互不替代；离线绝不显示旧使能。 */
	if (!Online) OLED_PrintLine(4, "OFFLINE / LOCKED");
	else if ((Now / 1000) & 1)
		OLED_PrintLine(4, "#%u %s", (unsigned)Remote->RequestId,
			Remote->Feedback.request_id == Remote->RequestId && Remote->Feedback.command == Remote->Command ? Jog_Ack(Remote->Feedback.ack_state) : "WAIT ACK");
	else OLED_PrintLine(4, "%s", Jog_State(Remote->Feedback.controller_state));
}
