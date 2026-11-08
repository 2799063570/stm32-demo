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
	assert(Stick.DX == -1 && Stick.DY == 1);        // 用户确认只有左右反向：X修正，Y保持原版
	Feed(&Stick, 0, 4095, 0, 1);
	assert(Stick.DX == 2 && Stick.DY == -2);
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
	assert(Game.X == GAME_START_X && Game.Y == GAME_START_Y);
}

/* 由ADC原始值驱动真实输入和游戏，覆盖用户反馈的“到顶后反向下移”。
 * 用户确认原Y方向正确：上推Y上升、下推Y下降、左推X上升、右推X下降。 */
static void TestStickMovementRoundTrip(void)
{
	Game_TypeDef Game;
	StickInput_TypeDef Stick;
	const uint16_t Centers[] = {1200, 2048, 2900};
	for (unsigned c = 0; c < sizeof(Centers) / sizeof(Centers[0]); c ++)
	{
		uint16_t Center = Centers[c];
		StickInput_Init(&Stick);
		Feed(&Stick, Center, Center, 0, STICK_CAL_SAMPLES);
		assert(Stick.Ready);
		Start(&Game);
		for (unsigned i = 0; i < 100; i ++)
		{
			Feed(&Stick, Center, 4095, 0, 8);
			Game.SpawnTicks = 100; // 隔离障碍，只验证输入反向与边界
			Game_Step(&Game, Stick.DX, Stick.DY, StickInput_TakePress(&Stick));
		}
		assert(Game.Y == GAME_TOP && Game.State == GAME_PLAY);
		Feed(&Stick, Center, 0, 0, 1); // 不要求先松手，直接从上推切换到下推
		assert(Stick.DX == 0 && Stick.DY == 2);
		Game.SpawnTicks = 100;
		Game_Step(&Game, Stick.DX, Stick.DY, StickInput_TakePress(&Stick));
		assert(Game.Y == GAME_TOP + 2); // 到顶不锁存，第一帧就必须向下离开
		for (unsigned i = 0; i < 100; i ++)
		{
			Game.SpawnTicks = 100;
			Game_Step(&Game, Stick.DX, Stick.DY, 0);
		}
		assert(Game.Y == GAME_HEIGHT - GAME_PLANE_SIZE);
		Feed(&Stick, Center, 4095, 0, 1);
		Game.SpawnTicks = 100;
		Game_Step(&Game, Stick.DX, Stick.DY, 0);
		assert(Game.Y == GAME_HEIGHT - GAME_PLANE_SIZE - 2);
		for (unsigned direction = 0; direction < 2; direction ++)
		{
			Feed(&Stick, direction ? 0 : 4095, Center, 0, 1);
			for (unsigned i = 0; i < 100; i ++)
			{
				Game.SpawnTicks = 100;
				Game_Step(&Game, Stick.DX, Stick.DY, 0);
			}
			assert(Game.X == (direction ? GAME_WIDTH - GAME_PLANE_SIZE : 0));
		}
		Feed(&Stick, Center, Center, 0, 1);
		assert(!Stick.DX && !Stick.DY);
		Game_TypeDef Before = Game;
		Game.SpawnTicks = 100;
		Game_Step(&Game, Stick.DX, Stick.DY, 0);
		assert(Game.X == Before.X && Game.Y == Before.Y);
	}
}

/* 顶边横移后，用慢档向下离开；左/右边界不得把纵向输入一起吞掉。 */
static void TestTopEdgeEscape(void)
{
	const uint16_t RawXs[] = {0, 2048, 4095};
	Game_TypeDef Game;
	StickInput_TypeDef Stick;
	for (unsigned x = 0; x < 3; x ++)
	{
		StickInput_Init(&Stick);
		Feed(&Stick, 2048, 2048, 0, STICK_CAL_SAMPLES);
		Start(&Game);
		Game.Y = GAME_TOP;
		for (unsigned i = 0; i < 100; i ++)
		{
			Feed(&Stick, RawXs[x], 4095, 0, 8);
			Game.SpawnTicks = 100;
			Game_Step(&Game, Stick.DX, Stick.DY, 0);
			assert(Game.Y == GAME_TOP && Game.State == GAME_PLAY);
		}
		Feed(&Stick, RawXs[x], 2048 - STICK_DEAD_ZONE - 1, 0, 8);
		assert(Stick.DY == 1); // 仅略过死区的下推也必须能离开顶边
		Game.SpawnTicks = 100;
		Game_Step(&Game, Stick.DX, Stick.DY, 0);
		assert(Game.Y == GAME_TOP + 1 && Game.State == GAME_PLAY);
		Feed(&Stick, RawXs[x], 0, 0, 8);
		Game.SpawnTicks = 100;
		Game_Step(&Game, Stick.DX, Stick.DY, 0);
		assert(Game.Y == GAME_TOP + 3); // 紧接着切换快档，不用回中或重开
	}
}

static void TestCollisionAndScore(void)
{
	Game_TypeDef Game;
	Start(&Game);
	Game.Obstacles[0] = (Game_ObstacleTypeDef){Game.X + 4, Game.Y, 1, 1, 1};
	Game_Step(&Game, 0, 0, 0);
	assert(Game.State == GAME_OVER);               // 机头像素相撞
	Start(&Game);
	Game.Obstacles[0] = (Game_ObstacleTypeDef){Game.X, Game.Y, 1, 1, 1};
	Game_Step(&Game, 0, 0, 0);
	assert(Game.State == GAME_PLAY && Game.Obstacles[0].Y == Game.Y + 1); // 透明角不误撞，障碍向下
	Start(&Game);
	Game.Obstacles[0] = (Game_ObstacleTypeDef){Game.X + 4, Game.Y - 2, 1, 1, 1};
	Game_Step(&Game, 0, -2, 0);
	assert(Game.State == GAME_OVER);               // 玩家和障碍相向移动不穿透
	Start(&Game);
	Game.Score = 40;                               // 最高难度3px/帧
	Game.Obstacles[0] = (Game_ObstacleTypeDef){Game.X + 4, Game.Y - 2, 1, 1, 1};
	Game_Step(&Game, 0, 0, 0);
	assert(Game.State == GAME_OVER);
	Start(&Game);
	Game.Obstacles[0] = (Game_ObstacleTypeDef){0, 63, 5, 6, 1};
	Game_Step(&Game, 0, 0, 0);
	assert(Game.Score == 1 && Game.Best == 1 && !Game.Obstacles[0].Active);
	Game_Step(&Game, 0, 0, 0);
	assert(Game.Score == 1);                        // 只计分一次
	Game.Score = Game.Best = 9999;
	Game.Obstacles[0] = (Game_ObstacleTypeDef){0, 63, 5, 6, 1};
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
			assert(O->X >= 0 && O->X + O->Width <= GAME_WIDTH);
			assert(O->Y >= GAME_TOP - O->Height && O->Y < GAME_HEIGHT);
			assert(O->Width >= 5 && O->Width <= 11 && O->Height >= 6 && O->Height <= 15);
		}
	}
}


/* 竖向版本的关键契约：机头在顶部，障碍X不变、Y增大，飞出底边才计分。 */
static void TestUpwardFlight(void)
{
    Game_TypeDef Game;
    assert(Game_PlanePixel(4, 0) && !Game_PlanePixel(0, 0));
    assert(Game_PlanePixel(0, 6) && Game_PlanePixel(8, 6));
    assert(!Game_PlanePixel(9, 0) && !Game_PlanePixel(0, 9));
    for (unsigned level = 1; level <= 5; level ++)
    {
        Start(&Game);
        Game.Score = (uint16_t)((level - 1) * 10);
        Game.Obstacles[0] = (Game_ObstacleTypeDef){0, GAME_TOP, 5, 6, 1};
        Game_Step(&Game, 0, 0, 0);
        assert(Game.Obstacles[0].X == 0);
        assert(Game.Obstacles[0].Y == (int16_t)(GAME_TOP + 1 + (level - 1) / 2));
        assert(Game.X == GAME_START_X && Game.Y == GAME_START_Y);
    }
    Start(&Game);
    Game.SpawnTicks = 1;
    Game_Step(&Game, 0, 0, 0);
    assert(Game.Obstacles[0].Active);
    assert(Game.Obstacles[0].Y + Game.Obstacles[0].Height == GAME_TOP);
    Start(&Game);
    Game.Obstacles[0] = (Game_ObstacleTypeDef){0, 62, 5, 6, 1};
    Game_Step(&Game, 0, 0, 0);
    assert(Game.Obstacles[0].Active && Game.Score == 0); // 还剩最后一行，不能提前计分
    Game_Step(&Game, 0, 0, 0);
    assert(!Game.Obstacles[0].Active && Game.Score == 1);
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
	StickInput_TypeDef Stick;
	StickInput_Init(&Stick);
	Feed(&Stick, 2048, 2048, 0, STICK_CAL_SAMPLES);
	GameView_DrawStick(&Stick, 2048, 2048);
	Snapshot("game_up_title.bin");
	Game_Step(&Game, 0, 0, 1);
	Game.Score = Game.Best = 23;

    /* 覆盖从上方入场、部分飞出底边的两种裁剪情况；计分栏必须逐字节不变。 */
    uint8_t HUD[GAME_TOP / 8 * GAME_WIDTH];
    GameView_Draw(&Game, 1, 1);
    memcpy(HUD, OLED_GetBuffer(), sizeof(HUD));
    Game.Obstacles[0] = (Game_ObstacleTypeDef){0, GAME_TOP - 6, 11, 8, 1};
    Game.Obstacles[1] = (Game_ObstacleTypeDef){120, 63, 8, 6, 1};
    GameView_Draw(&Game, 1, 1);
    assert(memcmp(HUD, OLED_GetBuffer(), sizeof(HUD)) == 0);
    const uint8_t *Clipped = OLED_GetBuffer();
    assert((Clipped[GAME_TOP / 8 * GAME_WIDTH] & 3) == 3); // 只露出飞行区内的两行
    assert((Clipped[7 * GAME_WIDTH + 127] & 0x80) != 0);  // 屏幕右下角仍正确显示
    memset(Game.Obstacles, 0, sizeof(Game.Obstacles));
	Game.Obstacles[0] = (Game_ObstacleTypeDef){25, 21, 10, 12, 1};
	Game.Obstacles[1] = (Game_ObstacleTypeDef){88, 37, 11, 14, 1};
	Game.Obstacles[2] = (Game_ObstacleTypeDef){60, 25, 10, 9, 1};
	GameView_Draw(&Game, 1, 1);
	const uint8_t *Buffer = OLED_GetBuffer();
	for (unsigned y = 0; y < 9; y ++)
		for (unsigned x = 0; x < 9; x ++)
			assert(((Buffer[(Game.Y + y) / 8 * 128 + Game.X + x] >> ((Game.Y + y) % 8)) & 1) == Game_PlanePixel((uint8_t)x, (uint8_t)y));
	Snapshot("game_up_play.bin");
	/* 诊断HUD只改顶部16行，不得遮挡机身或障碍。 */
	uint8_t PlayArea[6 * 128];
	memcpy(PlayArea, OLED_GetBuffer() + 2 * 128, sizeof(PlayArea));
	GameView_DrawInputDebug(&Game, &Stick, 2048);
	assert(memcmp(PlayArea, OLED_GetBuffer() + 2 * 128, sizeof(PlayArea)) == 0);
	Snapshot("game_up_debug.bin");
	Game_Step(&Game, 0, 0, 1);
	GameView_Draw(&Game, 1, 1);
	Snapshot("game_up_pause.bin");
	Game.State = GAME_OVER;
	GameView_Draw(&Game, 1, 1);
	Snapshot("game_up_over.bin");
	GameView_Draw(&Game, 0, 1);
	Snapshot("game_up_cal.bin");
	GameView_Draw(&Game, 1, 0);
	Snapshot("game_up_adc_error.bin");
}

int main(void)
{
	TestInput();
	TestMovementAndStates();
	TestStickMovementRoundTrip();
	TestTopEdgeEscape();
	TestCollisionAndScore();
	TestSpawning();
	TestUpwardFlight();
	TestView();
	puts("PASS: X-only reversal, top-edge slow/fast escape with sideways input, round trips, debug HUD, collisions, scoring, 10000 steps");
	return 0;
}
