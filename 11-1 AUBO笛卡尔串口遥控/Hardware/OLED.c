#include "OLED.h"
#include "OLED_SPI.h"
#include "OLED_Config.h"
#include "OLED_Font.h"
#include "Delay.h"
#include <stdio.h>
#include <stdarg.h>

static uint8_t OLED_Buffer[8][128];                  // 1KiB页显存，不在中断中修改
static uint8_t OLED_DirtyMin[8], OLED_DirtyMax[8];
static uint8_t OLED_Phase;
static uint8_t OLED_Active, OLED_Page, OLED_Column, OLED_EndColumn, OLED_NextPage;
static uint32_t OLED_NextActionTick, OLED_ErrorCount;

/** 函数：只标记真正变化的字节，减少总线流量 */
static void OLED_SetByte(uint8_t Page, uint8_t X, uint8_t Data)
{
	if (OLED_Buffer[Page][X] == Data) return;
	OLED_Buffer[Page][X] = Data;
	if (X < OLED_DirtyMin[Page]) OLED_DirtyMin[Page] = X;
	if (X > OLED_DirtyMax[Page]) OLED_DirtyMax[Page] = X;
}

static void OLED_MarkAll(void)
{
	uint8_t Page;
	for (Page = 0; Page < 8; Page ++) { OLED_DirtyMin[Page] = 0; OLED_DirtyMax[Page] = 127; }
}

static void OLED_Fail(void)
{
	OLED_ErrorCount ++;
	OLED_Phase = 0;
	OLED_SPI_Init();
	OLED_Active = 0;
	OLED_NextActionTick = Delay_GetTick() + OLED_RETRY_MS;
}

/**
  * 函    数：初始化SPI、复位引脚与显存，不阻塞等待上电或整屏刷新
  * 参    数：无
  * 返 回 值：无；在线状态由OLED_GetReady查询；Ready不是屏幕在线检测
  */
void OLED_Init(void)
{
	uint8_t Page, X;
	OLED_SPI_Init();
	for (Page = 0; Page < 8; Page ++)
	{
		for (X = 0; X < 128; X ++) OLED_Buffer[Page][X] = 0;
		OLED_DirtyMin[Page] = 128;
		OLED_DirtyMax[Page] = 0;
	}
	OLED_Phase = OLED_Active = OLED_NextPage = 0;
	OLED_ErrorCount = 0;
	OLED_NextActionTick = Delay_GetTick() + OLED_RESET_LOW_MS;
}

/**
  * 函    数：主循环分块显示服务，每次至多一个SPI事务，不关中断、不等整屏
  * 参    数：无
  * 返 回 值：无；仅能检测SPI外设超时，单向SPI无法检测屏幕断开
  */
void OLED_Service(void)
{
	static const uint8_t Commands1[] = {0xAE, 0xD5, 0x80, 0xA8, 0x3F, 0xD3, 0x00};
	static const uint8_t Commands2[] = {0x40, 0xA1, 0xC8, 0xDA, 0x12, 0x81, 0x7F};
	static const uint8_t Commands3[] = {0xD9, 0xF1, 0xDB, 0x30, 0x8D, 0x14};
	static const uint8_t Commands4[] = {0x20, 0x02, 0xA4, 0xA6, 0x2E}; // 页寻址，关闭硬件滚动
	uint8_t Index, Length, Command[3];
	if (OLED_Phase == 0)
	{
		if ((int32_t)(Delay_GetTick() - OLED_NextActionTick) < 0) return;
		OLED_SPI_SetReset(1);
		OLED_NextActionTick = Delay_GetTick() + OLED_POWER_ON_MS;
		OLED_Phase = 1;
		return;
	}
	if (OLED_Phase == 1)
	{
		if ((int32_t)(Delay_GetTick() - OLED_NextActionTick) < 0) return;
		OLED_Phase = 2;
	}
	if (OLED_Phase <= 5)
	{
		const uint8_t *Data = Commands1;
		Length = sizeof(Commands1);
		if (OLED_Phase == 3) { Data = Commands2; Length = sizeof(Commands2); }
		if (OLED_Phase == 4) { Data = Commands3; Length = sizeof(Commands3); }
		if (OLED_Phase == 5) { Data = Commands4; Length = sizeof(Commands4); }
		if (!OLED_SPI_Write(0x00, Data, Length)) { OLED_Fail(); return; }
		OLED_Phase ++;
		if (OLED_Phase == 6) OLED_MarkAll();          // 复位后完整恢复最新界面
		return;
	}
	if (!OLED_Active)
	{
		for (Index = 0; Index < 8; Index ++)
		{
			OLED_Page = (OLED_NextPage + Index) % 8;
			if (OLED_DirtyMin[OLED_Page] < 128)
			{
				OLED_Column = OLED_DirtyMin[OLED_Page];
				OLED_EndColumn = OLED_DirtyMax[OLED_Page];
				OLED_DirtyMin[OLED_Page] = 128;
				OLED_DirtyMax[OLED_Page] = 0;
				OLED_Active = 1;
				break;
			}
		}
		if (!OLED_Active)
		{
			if (OLED_Phase == 6)
			{
				Command[0] = 0xAF;                   // 显存先准备完再亮屏，减少上电乱码
				if (!OLED_SPI_Write(0x00, Command, 1)) { OLED_Fail(); return; }
				OLED_Phase = 7;
			}
			return;
		}
		Command[0] = 0xB0 | OLED_Page;
		Command[1] = (OLED_Column + OLED_COLUMN_OFFSET) & 0x0F;
		Command[2] = 0x10 | ((OLED_Column + OLED_COLUMN_OFFSET) >> 4);
		if (!OLED_SPI_Write(0x00, Command, 3)) OLED_Fail();
		return;
	}
	Length = OLED_EndColumn - OLED_Column + 1;
	if (Length > OLED_DATA_CHUNK) Length = OLED_DATA_CHUNK;
	if (!OLED_SPI_Write(0x40, &OLED_Buffer[OLED_Page][OLED_Column], Length)) { OLED_Fail(); return; }
	OLED_Column += Length;
	if (OLED_Column > OLED_EndColumn)
	{
		OLED_Active = 0;
		OLED_NextPage = (OLED_Page + 1) % 8;           // 轮转页面，避免高频变化的第一行饿死其他行
	}
}

uint8_t OLED_GetReady(void) { return OLED_Phase == 7; }
uint32_t OLED_GetErrorCount(void) { return OLED_ErrorCount; }
const uint8_t *OLED_GetBuffer(void) { return &OLED_Buffer[0][0]; }

/** 函数：只清软件显存，真正SPI发送交给Service */
void OLED_Clear(void)
{
	uint8_t Page, X;
	for (Page = 0; Page < 8; Page ++)
		for (X = 0; X < 128; X ++) OLED_SetByte(Page, X, 0);
}

/** 函数：沿用参考教程的1~4行、1~16列，越界丢弃，非法字符显示问号 */
void OLED_ShowChar(uint8_t Line, uint8_t Column, char Char)
{
	uint8_t Index, Code = (uint8_t)Char;
	if (Line < 1 || Line > 4 || Column < 1 || Column > 16) return;
	if (Code < 32 || Code > 126) Code = '?';
	for (Index = 0; Index < 8; Index ++)
	{
		OLED_SetByte((Line - 1) * 2, (Column - 1) * 8 + Index, OLED_F8x16[Code - 32][Index]);
		OLED_SetByte((Line - 1) * 2 + 1, (Column - 1) * 8 + Index, OLED_F8x16[Code - 32][Index + 8]);
	}
}

void OLED_ShowString(uint8_t Line, uint8_t Column, const char *String)
{
	if (String == 0 || Line < 1 || Line > 4 || Column < 1 || Column > 16) return;
	while (*String && Column <= 16) OLED_ShowChar(Line, Column ++, *String ++);
}

/** 函数：一整行有界格式化，不足16字符补空格，避免新数字变短留下旧字符 */
void OLED_PrintLine(uint8_t Line, const char *Format, ...)
{
	char Text[17];
	uint8_t Index, End = 0;
	va_list Arg;
	if (Format == 0 || Line < 1 || Line > 4) return;
	va_start(Arg, Format);
	vsnprintf(Text, sizeof(Text), Format, Arg);
	va_end(Arg);
	Text[16] = '\0';
	for (Index = 0; Index < 16; Index ++)
	{
		if (!End && !Text[Index]) End = 1;
		OLED_ShowChar(Line, Index + 1, End ? ' ' : Text[Index]);
	}
}

void OLED_DrawPixel(int16_t X, int16_t Y, uint8_t On)
{
	uint8_t Data, Mask;
	if (X < 0 || X >= 128 || Y < 0 || Y >= 64) return;
	Mask = (uint8_t)(1U << (Y % 8));
	Data = OLED_Buffer[Y / 8][X];
	OLED_SetByte((uint8_t)(Y / 8), (uint8_t)X, (uint8_t)(On ? (Data | Mask) : (Data & (uint8_t)~Mask)));
}

/** 函数：屏内整数线段，超出屏幕端点直接拒绝，保证循环次数有界 */
void OLED_DrawLine(int16_t X1, int16_t Y1, int16_t X2, int16_t Y2, uint8_t On)
{
	int16_t DX, DY, SX, SY, Error, E2;
	if (X1 < 0 || X1 >= 128 || X2 < 0 || X2 >= 128 || Y1 < 0 || Y1 >= 64 || Y2 < 0 || Y2 >= 64) return;
	DX = X2 > X1 ? X2 - X1 : X1 - X2;
	DY = Y2 > Y1 ? Y1 - Y2 : Y2 - Y1;
	SX = X1 < X2 ? 1 : -1;
	SY = Y1 < Y2 ? 1 : -1;
	Error = DX + DY;
	while (1)
	{
		OLED_DrawPixel(X1, Y1, On);
		if (X1 == X2 && Y1 == Y2) break;
		E2 = 2 * Error;
		if (E2 >= DY) { Error += DY; X1 += SX; }
		if (E2 <= DX) { Error += DX; Y1 += SY; }
	}
}

/** 函数：双向条形图，中心为0，左负右正；自动限幅并覆盖旧图 */
void OLED_DrawBar(uint8_t X, uint8_t Y, uint8_t Width, uint8_t Height, int32_t Value, int32_t Limit)
{
	int16_t IX, IY, Center, End;
	if (Width < 3 || Height < 3 || (uint16_t)X + Width > 128 || (uint16_t)Y + Height > 64 || Limit <= 0) return;
	if (Value > Limit) Value = Limit;
	if (Value < -Limit) Value = -Limit;
	Center = Width / 2;
	End = Center + (int16_t)((int64_t)Value * (Width / 2 - 1) / Limit);
	for (IY = 0; IY < Height; IY ++)
		for (IX = 0; IX < Width; IX ++)
		{
			uint8_t On = IY == 0 || IY == Height - 1 || IX == 0 || IX == Width - 1 || IX == Center ||
				(IX >= End && IX < Center && Value < 0) || (IX > Center && IX <= End && Value > 0);
			OLED_DrawPixel(X + IX, Y + IY, On);
		}
}
