#include "Game.h"
#include <string.h>

/* 机头朝上：第0行是机头，第6行是机翼，第8行是尾翼。
 * 每行低9位从左到右对应像素；绘图和碰撞共用点阵，透明边角不算碰撞。 */
static const uint16_t Game_PlaneRows[9] = {0x010, 0x010, 0x038, 0x038, 0x07C, 0x0FE, 0x1FF, 0x010, 0x038};

uint8_t Game_PlanePixel(uint8_t X, uint8_t Y)
{
	return X < 9 && Y < 9 && (Game_PlaneRows[Y] & (1u << X)) != 0;
}

uint8_t Game_Level(const Game_TypeDef *Game)
{
	/* 每躲过10个障碍升一级，共5级；速度和生成间隔都随等级调整。 */
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
	Game->X = GAME_START_X;
	Game->Y = GAME_START_Y;
}

static void Game_Start(Game_TypeDef *Game)
{
	memset(Game->Obstacles, 0, sizeof(Game->Obstacles));
	/* 从下方中央出发；向上飞行的视觉由障碍向下滚动形成。 */
	Game->X = GAME_START_X;
	Game->Y = GAME_START_Y;
	Game->Score = 0;
	Game->SpawnTicks = 20;
	Game->State = GAME_PLAY;
}

static uint8_t Game_Collides(const Game_TypeDef *Game, const Game_ObstacleTypeDef *Obstacle)
{
	uint8_t X, Y;
	/* 先排除包围盒不相交，再检测机身像素；无需给每个障碍分配显存。 */
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
		/* 各方向分别检查边界：到顶只挡住上移，下移仍独立允许。
		 * 明确区分方向与速度，避免边界处理夹带另一个方向或另一个轴。
		 * 只有速度绝对值覆盖本子步时才走1像素，保留逐子步碰撞检测。 */
		if (DX > 0 && Substep < (uint8_t)DX && Game->X < GAME_WIDTH - GAME_PLANE_SIZE) Game->X ++;
		if (DX < 0 && Substep < (uint8_t)(-DX) && Game->X > 0) Game->X --;
		if (DY > 0 && Substep < (uint8_t)DY && Game->Y < GAME_HEIGHT - GAME_PLANE_SIZE) Game->Y ++;
		if (DY < 0 && Substep < (uint8_t)(-DY) && Game->Y > GAME_TOP) Game->Y --;
		/* 保留最终限幅，外部重置/异常坐标也不能将机身留在游戏区之外。 */
		if (Game->X < 0) Game->X = 0;
		if (Game->X > GAME_WIDTH - GAME_PLANE_SIZE) Game->X = GAME_WIDTH - GAME_PLANE_SIZE;
		if (Game->Y < GAME_TOP) Game->Y = GAME_TOP;
		if (Game->Y > GAME_HEIGHT - GAME_PLANE_SIZE) Game->Y = GAME_HEIGHT - GAME_PLANE_SIZE;
		for (Index = 0; Index < GAME_OBSTACLES; Index ++)
		{
			Game_ObstacleTypeDef *Obstacle = &Game->Obstacles[Index];
			if (!Obstacle->Active) continue;
			/*先检查玩家移动，再移动障碍，避免同一子步交叉穿过。*/
			if (Game_Collides(Game, Obstacle)) { Game->State = GAME_OVER; return; }
			/* 屏幕Y向下增大；障碍持续下落，飞机机头始终朝上。 */
			if (Substep < Speed) Obstacle->Y ++;
			if (Game_Collides(Game, Obstacle)) { Game->State = GAME_OVER; return; }
			/* 整个障碍越过底边才加分，立即释放槽位，防止重复计分。 */
			if (Obstacle->Y >= GAME_HEIGHT)
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
			/* 随机横向位置包含左右两端；底边刚好在飞行区上沿之外。
			 * 之后逐像素落入游戏区，绘图层会裁掉计分栏中的部分。 */
			Obstacle->X = (int16_t)(Game_Random(Game) % (GAME_WIDTH - Obstacle->Width + 1));
			Obstacle->Y = (int16_t)(GAME_TOP - Obstacle->Height);
			Obstacle->Active = 1;
			break;
		}
		Game->SpawnTicks = (uint16_t)(28 - Level * 3);
	}
}
