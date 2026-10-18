#include "Game.h"
#include <string.h>

/*机头向右，9x9点阵；显示和碰撞共用同一图案，透明角不会误撞。*/
static const uint16_t Game_PlaneRows[9] = {0x008, 0x018, 0x038, 0x07E, 0x1FF, 0x07E, 0x038, 0x018, 0x008};

uint8_t Game_PlanePixel(uint8_t X, uint8_t Y)
{
	return X < 9 && Y < 9 && (Game_PlaneRows[Y] & (1u << X)) != 0;
}

uint8_t Game_Level(const Game_TypeDef *Game)
{
	uint16_t Level = Game->Score / 10 + 1;
	return Level > 5 ? 5 : (uint8_t)Level;
}

static uint32_t Game_Random(Game_TypeDef *Game)
{
	Game->Random ^= Game->Random << 13;
	Game->Random ^= Game->Random >> 17;
	Game->Random ^= Game->Random << 5;
	return Game->Random;
}

void Game_Init(Game_TypeDef *Game, uint32_t Seed)
{
	memset(Game, 0, sizeof(*Game));
	Game->State = GAME_TITLE;
	Game->Random = Seed ? Seed : 0x13579BDFu;
	Game->X = 12;
	Game->Y = 36;
}

static void Game_Start(Game_TypeDef *Game)
{
	memset(Game->Obstacles, 0, sizeof(Game->Obstacles));
	Game->X = 12;
	Game->Y = 36;
	Game->Score = 0;
	Game->SpawnTicks = 20;
	Game->State = GAME_PLAY;
}

static uint8_t Game_Collides(const Game_TypeDef *Game, const Game_ObstacleTypeDef *Obstacle)
{
	uint8_t X, Y;
	if (Game->X + 9 <= Obstacle->X || Game->X >= Obstacle->X + Obstacle->Width ||
		Game->Y + 9 <= Obstacle->Y || Game->Y >= Obstacle->Y + Obstacle->Height) return 0;
	for (Y = 0; Y < 9; Y ++)
	{
		for (X = 0; X < 9; X ++)
		{
			if (Game_PlanePixel(X, Y) && Game->X + X >= Obstacle->X &&
				Game->X + X < Obstacle->X + Obstacle->Width && Game->Y + Y >= Obstacle->Y &&
				Game->Y + Y < Obstacle->Y + Obstacle->Height) return 1;
		}
	}
	return 0;
}

/** 函数：固定40ms一步，无动态分配；速度最大2px，障碍最大3px，逐子步检测防穿透 */
void Game_Step(Game_TypeDef *Game, int8_t DX, int8_t DY, uint8_t Pressed)
{
	uint8_t Index, Substep, Level, Speed;
	if (Pressed)
	{
		if (Game->State == GAME_TITLE || Game->State == GAME_OVER) Game_Start(Game);
		else Game->State = Game->State == GAME_PLAY ? GAME_PAUSE : GAME_PLAY;
		return;
	}
	if (Game->State != GAME_PLAY) return;
	if (DX > 2) DX = 2;
	if (DX < -2) DX = -2;
	if (DY > 2) DY = 2;
	if (DY < -2) DY = -2;
	Level = Game_Level(Game);
	Speed = 1 + (Level - 1) / 2;
	for (Substep = 0; Substep < 3; Substep ++)
	{
		if (DX > Substep) Game->X ++;
		if (-DX > Substep) Game->X --;
		if (DY > Substep) Game->Y ++;
		if (-DY > Substep) Game->Y --;
		if (Game->X < 0) Game->X = 0;
		if (Game->X > 119) Game->X = 119;
		if (Game->Y < GAME_TOP) Game->Y = GAME_TOP;
		if (Game->Y > 55) Game->Y = 55;
		for (Index = 0; Index < GAME_OBSTACLES; Index ++)
		{
			Game_ObstacleTypeDef *Obstacle = &Game->Obstacles[Index];
			if (!Obstacle->Active) continue;
			/*先检查玩家移动，再移动障碍，避免同一子步交叉穿过。*/
			if (Game_Collides(Game, Obstacle)) { Game->State = GAME_OVER; return; }
			if (Substep < Speed) Obstacle->X --;
			if (Game_Collides(Game, Obstacle)) { Game->State = GAME_OVER; return; }
			if (Obstacle->X + Obstacle->Width <= 0)
			{
				Obstacle->Active = 0;
				if (Game->Score < 9999) Game->Score ++;
				if (Game->Score > Game->Best) Game->Best = Game->Score;
			}
		}
	}
	if (Game->SpawnTicks > 0) Game->SpawnTicks --;
	if (Game->SpawnTicks == 0)
	{
		for (Index = 0; Index < GAME_OBSTACLES; Index ++)
		{
			Game_ObstacleTypeDef *Obstacle = &Game->Obstacles[Index];
			if (Obstacle->Active) continue;
			Obstacle->Width = (uint8_t)(5 + Game_Random(Game) % 7);
			Obstacle->Height = (uint8_t)(6 + Game_Random(Game) % 10);
			Obstacle->X = 128;
			Obstacle->Y = (int16_t)(GAME_TOP + Game_Random(Game) % (49 - Obstacle->Height));
			Obstacle->Active = 1;
			break;
		}
		Game->SpawnTicks = (uint16_t)(28 - Level * 3);
	}
}
