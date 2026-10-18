#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "Game.h"
#include "GameView.h"
#include "GameConfig.h"
#include "StickInput.h"
#include "OLED.h"
#include "OLED_SPI.h"

/*真实游戏/输入/绘图代码，显式替代SPI和毫秒时钟；不冒充实板ADC或OLED。*/
static uint32_t Tick;
static uint8_t Panel[1024], Page, Column;
uint32_t Delay_GetTick(void) { return Tick; }
void OLED_SPI_Init(void) { memset(Panel, 0, sizeof(Panel)); }
void OLED_SPI_SetReset(uint8_t Level) { (void)Level; }
uint8_t OLED_SPI_Write(uint8_t Control, const uint8_t *Data, uint8_t Length)
{
	assert(Data && Length >= 1 && Length <= 8);
	if (Control == 0x40)
	{
		assert(Page < 8 && (unsigned)Column + Length <= 128);
		memcpy(Panel + Page * 128 + Column, Data, Length);
		Column = (uint8_t)(Column + Length);
	}
	else if (Length == 3 && Data[0] >= 0xB0 && Data[0] <= 0xB7)
	{
		Page = Data[0] & 7;
		Column = (uint8_t)((Data[1] & 15) | ((Data[2] & 15) << 4));
	}
	return 1;
}

static void Feed(StickInput_TypeDef *Stick, uint16_t X, uint16_t Y, uint8_t Pressed, unsigned Count)
{
	for (unsigned i = 0; i < Count; i ++) StickInput_Feed(Stick, X, Y, Pressed);
}

static void TestInput(void)
{
	StickInput_TypeDef Stick;
	StickInput_Init(&Stick);
	Feed(&Stick, 4095, 0, 0, 64);
	assert(!Stick.Ready);                          // 端点不能当中心
	for (unsigned i = 0; i < 32; i ++) Feed(&Stick, i % 2 ? 1800 : 2400, 2000, 0, 1);
	assert(!Stick.Ready);                          // 大幅摆动不能通过校准
	Feed(&Stick, 2070, 2010, 0, 32);
	assert(Stick.Ready && Stick.CenterX == 2070 && Stick.CenterY == 2010);
	Feed(&Stick, 2070 + 300, 2010 - 300, 0, 1);
	assert(Stick.DX == 0 && Stick.DY == 0);
	Feed(&Stick, 2500, 1400, 0, 1);
	assert(Stick.DX == 1 && Stick.DY == 1);         // 默认Y翻转
	Feed(&Stick, 0, 4095, 0, 1);
	assert(Stick.DX == -2 && Stick.DY == -2);
	Feed(&Stick, 2070, 2010, 1, 3);
	assert(!StickInput_TakePress(&Stick));          // 少于20ms不触发
	Feed(&Stick, 2070, 2010, 0, 1);
	Feed(&Stick, 2070, 2010, 1, 4);
	assert(StickInput_TakePress(&Stick) && !StickInput_TakePress(&Stick));
	Feed(&Stick, 2070, 2010, 1, 200);
	assert(!StickInput_TakePress(&Stick));          // 长按不重复
	Feed(&Stick, 2070, 2010, 0, 4);
	Feed(&Stick, 2070, 2010, 1, 4);
	assert(StickInput_TakePress(&Stick));
	StickInput_Init(&Stick);
	Feed(&Stick, 2048, 2048, 1, 100);
	assert(Stick.Ready && !StickInput_TakePress(&Stick)); // 上电长按不开始
	Feed(&Stick, 2048, 2048, 0, 4);
	Feed(&Stick, 2048, 2048, 1, 4);
	assert(StickInput_TakePress(&Stick));
}

static void Start(Game_TypeDef *Game)
{
	Game_Init(Game, 123);
	Game_Step(Game, 0, 0, 1);
	assert(Game->State == GAME_PLAY);
}

static void TestMovementAndStates(void)
{
	Game_TypeDef Game;
	Start(&Game);
	for (unsigned i = 0; i < 200; i ++)
	{
		Game.SpawnTicks = 100;
		Game_Step(&Game, -2, -2, 0);
	}
	assert(Game.X == 0 && Game.Y == GAME_TOP);
	for (unsigned i = 0; i < 200; i ++)
	{
		Game.SpawnTicks = 100;
		Game_Step(&Game, 127, 127, 0);               // 输入即使越界也限制最大速度
	}
	assert(Game.X == 119 && Game.Y == 55);
	Game_Step(&Game, 0, 0, 1);
	assert(Game.State == GAME_PAUSE);
	Game_TypeDef Before = Game;
	for (unsigned i = 0; i < 20; i ++) Game_Step(&Game, -2, -2, 0);
	assert(memcmp(&Game, &Before, sizeof(Game)) == 0);
	Game_Step(&Game, 0, 0, 1);
	assert(Game.State == GAME_PLAY);
	Game_Step(&Game, 0, 0, 0);
	assert(Game.X == 119 && Game.Y == 55);          // 松手停止
	Game.State = GAME_OVER;
	Game.Score = Game.Best = 123;
	Game_Step(&Game, 0, 0, 1);
	assert(Game.State == GAME_PLAY && Game.Score == 0 && Game.Best == 123);
	assert(Game.X == 12 && Game.Y == 36);
}

static void TestCollisionAndScore(void)
{
	Game_TypeDef Game;
	Start(&Game);
	Game.Obstacles[0] = (Game_ObstacleTypeDef){Game.X + 8, Game.Y + 4, 1, 1, 1};
	Game_Step(&Game, 0, 0, 0);
	assert(Game.State == GAME_OVER);               // 机头像素相撞
	Start(&Game);
	Game.Obstacles[0] = (Game_ObstacleTypeDef){Game.X, Game.Y, 1, 1, 1};
	Game_Step(&Game, 0, 0, 0);
	assert(Game.State == GAME_PLAY);               // 左上透明角不误撞
	Start(&Game);
	Game.Obstacles[0] = (Game_ObstacleTypeDef){Game.X + 10, Game.Y + 4, 1, 1, 1};
	Game_Step(&Game, 2, 0, 0);
	assert(Game.State == GAME_OVER);               // 玩家和障碍相向移动不穿透
	Start(&Game);
	Game.Score = 40;                               // 最高难度3px/帧
	Game.Obstacles[0] = (Game_ObstacleTypeDef){Game.X + 10, Game.Y + 4, 1, 1, 1};
	Game_Step(&Game, 0, 0, 0);
	assert(Game.State == GAME_OVER);
	Start(&Game);
	Game.Obstacles[0] = (Game_ObstacleTypeDef){-4, GAME_TOP, 5, 6, 1};
	Game_Step(&Game, 0, 0, 0);
	assert(Game.Score == 1 && Game.Best == 1 && !Game.Obstacles[0].Active);
	Game_Step(&Game, 0, 0, 0);
	assert(Game.Score == 1);                        // 只计分一次
	Game.Score = Game.Best = 9999;
	Game.Obstacles[0] = (Game_ObstacleTypeDef){-4, GAME_TOP, 5, 6, 1};
	Game_Step(&Game, 0, 0, 0);
	assert(Game.Score == 9999 && Game_Level(&Game) == 5);
}

static void TestSpawning(void)
{
	Game_TypeDef A, B;
	Start(&A);
	Start(&B);
	for (unsigned i = 0; i < 10000; i ++)
	{
		if (A.State == GAME_OVER) { Game_Step(&A, 0, 0, 1); Game_Step(&B, 0, 0, 1); }
		Game_Step(&A, 0, 0, 0);
		Game_Step(&B, 0, 0, 0);
		assert(memcmp(&A, &B, sizeof(A)) == 0);       // 相同种子可复现
		for (unsigned j = 0; j < GAME_OBSTACLES; j ++)
		{
			const Game_ObstacleTypeDef *O = &A.Obstacles[j];
			if (!O->Active) continue;
			assert(O->X <= 128 && O->X > -(int)O->Width);
			assert(O->Y >= GAME_TOP && O->Y + O->Height <= 64);
			assert(O->Width >= 5 && O->Width <= 11 && O->Height >= 6 && O->Height <= 15);
		}
	}
}

static void Snapshot(const char *Name)
{
	for (unsigned i = 0; i < 2000; i ++) { Tick ++; OLED_Service(); }
	assert(OLED_GetReady() && memcmp(Panel, OLED_GetBuffer(), 1024) == 0);
	FILE *File = fopen(Name, "wb");
	assert(File && fwrite(OLED_GetBuffer(), 1, 1024, File) == 1024);
	fclose(File);
}

static void TestView(void)
{
	Game_TypeDef Game;
	Game_Init(&Game, 123);
	OLED_Init();
	GameView_Draw(&Game, 1, 1);
	Snapshot("game_title.bin");
	Game_Step(&Game, 0, 0, 1);
	Game.Score = Game.Best = 23;
	Game.Obstacles[0] = (Game_ObstacleTypeDef){56, 20, 8, 12, 1};
	Game.Obstacles[1] = (Game_ObstacleTypeDef){87, 44, 7, 14, 1};
	Game.Obstacles[2] = (Game_ObstacleTypeDef){118, 26, 10, 9, 1};
	GameView_Draw(&Game, 1, 1);
	const uint8_t *Buffer = OLED_GetBuffer();
	for (unsigned y = 0; y < 9; y ++)
		for (unsigned x = 0; x < 9; x ++)
			assert(((Buffer[(Game.Y + y) / 8 * 128 + Game.X + x] >> ((Game.Y + y) % 8)) & 1) == Game_PlanePixel((uint8_t)x, (uint8_t)y));
	Snapshot("game_play.bin");
	Game_Step(&Game, 0, 0, 1);
	GameView_Draw(&Game, 1, 1);
	Snapshot("game_pause.bin");
	Game.State = GAME_OVER;
	GameView_Draw(&Game, 1, 1);
	Snapshot("game_over.bin");
	GameView_Draw(&Game, 0, 1);
	Snapshot("game_cal.bin");
	GameView_Draw(&Game, 1, 0);
	Snapshot("game_adc_error.bin");
}

int main(void)
{
	TestInput();
	TestMovementAndStates();
	TestCollisionAndScore();
	TestSpawning();
	TestView();
	puts("PASS: joystick calibration/debounce, movement/states, pixel/swept collision, score/spawn, OLED frames");
	return 0;
}
