#ifndef __GAME_H
#define __GAME_H
#include <stdint.h>
/* 屏幕坐标向右、向下递增；顶部16行专用于计分，不能作为飞行区域。 */
#define GAME_WIDTH 128
#define GAME_HEIGHT 64
#define GAME_TOP 16
#define GAME_PLANE_SIZE 9
#define GAME_OBSTACLES 6
#define GAME_START_X ((GAME_WIDTH - GAME_PLANE_SIZE) / 2)
#define GAME_START_Y (GAME_HEIGHT - GAME_PLANE_SIZE - 4)
typedef enum { GAME_TITLE, GAME_PLAY, GAME_PAUSE, GAME_OVER } Game_StateTypeDef;
typedef struct
{
	/* 障碍从计分栏下沿外侧生成，入场前Y可能小于GAME_TOP。 */
	int16_t X, Y;
	uint8_t Width, Height, Active;
} Game_ObstacleTypeDef;
typedef struct
{
	Game_StateTypeDef State;
	int16_t X, Y;
	/* 最高分仅保存在RAM，重开保留，断电清零；SpawnTicks单位为游戏帧。 */
	uint16_t Score, Best, SpawnTicks;
	uint32_t Random;
	Game_ObstacleTypeDef Obstacles[GAME_OBSTACLES];
} Game_TypeDef;
void Game_Init(Game_TypeDef *Game, uint32_t Seed);
void Game_Step(Game_TypeDef *Game, int8_t DX, int8_t DY, uint8_t Pressed);
uint8_t Game_Level(const Game_TypeDef *Game);
uint8_t Game_PlanePixel(uint8_t X, uint8_t Y);
#endif
