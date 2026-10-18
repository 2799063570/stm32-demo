#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "OLED.h"
#include "OLED_SPI.h"
#include "OLED_Config.h"
#include "Dashboard.h"
#include "Control.h"

/*显式SPI接口桩：只验证软件协议/显存，不模拟STM32引脚、电气和真实面板。*/
static uint32_t Tick, ResetHighTick, Writes, BusInitCount;
static uint8_t Panel[1024], Page, Column, ResetLevel, FailNext, NoPanel;
static Control_StatusTypeDef ControlStatus;

uint32_t Delay_GetTick(void) { return Tick; }
void Control_GetStatus(Control_StatusTypeDef *Status) { *Status = ControlStatus; }
void OLED_SPI_Init(void)
{
	BusInitCount ++;
	ResetLevel = 0;
	memset(Panel, 0xA5, sizeof(Panel));               // 不假设复位会清显存
}
void OLED_SPI_SetReset(uint8_t Level)
{
	ResetLevel = Level;
	if (Level) ResetHighTick = Tick;
}
uint8_t OLED_SPI_Write(uint8_t Control, const uint8_t *Data, uint8_t Length)
{
	Writes ++;
	assert(Length > 0 && Length <= 8);
	assert(ResetLevel && (uint32_t)(Tick - ResetHighTick) >= OLED_POWER_ON_MS);
	if (FailNext) { FailNext = 0; return 0; }
	if (NoPanel) return 1;                           // SPI发送成功与屏幕存在是两件事
	if (Control == 0x40)
	{
		assert(Page < 8 && (unsigned)Column + Length <= 128);
		memcpy(&Panel[Page * 128 + Column], Data, Length);
		Column = (uint8_t)(Column + Length);
	}
	else
	{
		assert(Control == 0);
		if (Length == 3 && Data[0] >= 0xB0 && Data[0] <= 0xB7)
		{
			Page = Data[0] & 7;
			Column = (uint8_t)((Data[1] & 15) | ((Data[2] & 15) << 4));
		}
		if (Length == 1 && Data[0] == 0xAF)
			assert(memcmp(Panel, OLED_GetBuffer(), sizeof(Panel)) == 0); // 全部显存准备好才亮屏
	}
	return 1;
}

static void Pump(unsigned Count)
{
	for (unsigned i = 0; i < Count; i ++)
	{
		uint32_t Before = Writes;
		Tick ++;
		OLED_Service();
		assert(Writes - Before <= 1);                // 单次服务不能退化为整屏阻塞循环
	}
}

static void Snapshot(const char *Name)
{
	FILE *File = fopen(Name, "wb");
	assert(File);
	assert(fwrite(OLED_GetBuffer(), 1, 1024, File) == 1024);
	fclose(File);
}

static void TestRefresh(void)
{
	Tick = 0;
	OLED_Init();
	OLED_PrintLine(1, "HELLO 123456789");
	OLED_DrawPixel(127, 63, 1);
	Pump(4);
	assert(Writes == 0 && !OLED_GetReady());
	Pump(1000);
	assert(OLED_GetReady() && memcmp(Panel, OLED_GetBuffer(), 1024) == 0);
	uint32_t Before = Writes;
	OLED_PrintLine(1, "HELLO 123456789");
	Pump(100);
	assert(Writes == Before);                        // 未改变的界面不反复发送
	OLED_ShowChar(1, 2, 'Z');
	Pump(100);
	assert(Writes - Before < 10 && memcmp(Panel, OLED_GetBuffer(), 1024) == 0);
	puts("PASS reset timing, <=8-byte bursts, one transaction/service and dirty-region refresh");

	OLED_PrintLine(2, "LONG LINE 123456");
	Pump(1);                                        // 刷新途中再次修改，包括已发送位置
	OLED_PrintLine(2, "X");
	Pump(300);
	assert(memcmp(Panel, OLED_GetBuffer(), 1024) == 0);
	for (unsigned page = 2; page < 4; page ++)
		for (unsigned x = 8; x < 128; x ++) assert(OLED_GetBuffer()[page * 128 + x] == 0);
	puts("PASS updates during refresh and short text clears trailing old characters");

	uint8_t Saved[1024];
	memcpy(Saved, OLED_GetBuffer(), sizeof(Saved));
	OLED_ShowChar(0, 1, 'A'); OLED_ShowChar(1, 17, 'A'); OLED_ShowString(4, 0, "BAD");
	OLED_DrawPixel(-1, 0, 1); OLED_DrawPixel(128, 0, 1);
	OLED_DrawLine(-1, 0, 127, 63, 1);
	OLED_DrawBar(127, 63, 20, 20, 1, 1);
	assert(memcmp(Saved, OLED_GetBuffer(), sizeof(Saved)) == 0);
	OLED_Clear(); OLED_ShowChar(1, 1, (char)-1);
	memcpy(Saved, OLED_GetBuffer(), sizeof(Saved));
	OLED_Clear(); OLED_ShowChar(1, 1, '?');
	assert(memcmp(Saved, OLED_GetBuffer(), sizeof(Saved)) == 0);
	OLED_PrintLine(2, "%ld", (long)(-2147483647 - 1));
	OLED_DrawBar(40, 52, 88, 11, (-2147483647 - 1), 1000);
	Pump(300);
	assert(memcmp(Panel, OLED_GetBuffer(), 1024) == 0);
	puts("PASS bounds, invalid glyph, signed integer minimum and bar saturation");

	FailNext = 1;
	OLED_ShowChar(1, 1, 'R');
	Pump(1);
	assert(!OLED_GetReady() && OLED_GetErrorCount() == 1);
	Before = Writes;
	Pump(999);
	assert(Writes == Before);
	Pump(500);
	assert(OLED_GetReady() && BusInitCount == 2 && memcmp(Panel, OLED_GetBuffer(), 1024) == 0);
	puts("PASS SPI failure, timed reinitialization and full framebuffer restoration");

	NoPanel = 1;
	OLED_Init(); Pump(1000);
	assert(OLED_GetReady() && OLED_GetErrorCount() == 0);
	NoPanel = 0;
	puts("PASS ready does not claim physical presence on one-way SPI");
	Tick = 0xFFFFFFFCU;
	OLED_Init(); Pump(1000);
	assert(OLED_GetReady() && memcmp(Panel, OLED_GetBuffer(), 1024) == 0);
	puts("PASS asynchronous reset and refresh across millisecond wrap");
}

static void MakePreviews(void)
{
	OLED_Clear();
	OLED_PrintLine(1, "UART DMA+IDLE"); OLED_PrintLine(2, "RX:1024");
	OLED_PrintLine(3, "TX:1024"); OLED_PrintLine(4, "I42 D0"); Snapshot("oled_uart.bin");
	OLED_Clear();
	OLED_PrintLine(1, "MPU6050 E:0"); OLED_PrintLine(2, "AX:%6d", 123);
	OLED_PrintLine(3, "AY:%6d", -450); OLED_PrintLine(4, "AZ:%6d", 16384); Snapshot("oled_mpu.bin");
	OLED_Clear();
	OLED_PrintLine(1, "CAN 500K LOOP"); OLED_PrintLine(2, "TX:12");
	OLED_PrintLine(3, "RX:12"); OLED_PrintLine(4, "E0 D0"); Snapshot("oled_can.bin");
	OLED_Clear();
	OLED_PrintLine(1, "F:1000.000Hz"); OLED_PrintLine(2, "P:1000us");
	OLED_PrintLine(3, "CNT: 1200"); OLED_PrintLine(4, "D:    48 O:0"); Snapshot("oled_capture.bin");

	Tick = 0;
	Dashboard_Init();
	ControlStatus.TargetRPM = 60; ControlStatus.RampRPM = 60; ControlStatus.SpeedRPM = 57;
	ControlStatus.PWM = 130; ControlStatus.Running = 1; ControlStatus.Fault = CONTROL_FAULT_NONE;
	Tick = 200;
	Dashboard_Service();
	Snapshot("oled_motor.bin");
	for (unsigned i = 1; i < 32; i ++)
	{
		Tick = (i + 1) * 200;
		ControlStatus.SpeedRPM = (int32_t)(i < 12 ? i * 5 : 60 + i % 3);
		Dashboard_Service();
	}
	Snapshot("oled_trend.bin");
	ControlStatus.Fault = CONTROL_FAULT_TIMEOUT;
	ControlStatus.Running = 0; ControlStatus.PWM = 0;
	ControlStatus.TargetRPM = 0; ControlStatus.RampRPM = 0;
	Tick += 200;
	Dashboard_Service();
	Snapshot("oled_fault.bin");
	uint8_t Saved[1024];
	memcpy(Saved, OLED_GetBuffer(), sizeof(Saved));
	OLED_PrintLine(1, "FAULT:TIMEOUT");
	assert(memcmp(Saved, OLED_GetBuffer(), sizeof(Saved)) == 0); // 故障时不用曲线页遮盖状态
	puts("PASS motor dashboard history and forced fault page; preview buffers exported");
}

int main(void)
{
	TestRefresh();
	MakePreviews();
	puts("OLED software tests passed; hardware/display controller remains unverified.");
	return 0;
}
