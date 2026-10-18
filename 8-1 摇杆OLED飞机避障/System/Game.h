#ifndef __GAME_H
#define __GAME_H
#include <stdint.h>
#define GAME_TOP 16
#define GAME_PLANE_SIZE 9
#define GAME_OBSTACLES 6
typedef enum { GAME_TITLE, GAME_PLAY, GAME_PAUSE, GAME_OVER } Game_StateTypeDef;
typedef struct
{
	int16_t X, Y;
	uint8_t Width, Height, Active;
} Game_ObstacleTypeDef;
typedef struct
{
	Game_StateTypeDef State;
	int16_t X, Y;
	uint16_t Score, Best, SpawnTicks;
	uint32_t Random;
	Game_ObstacleTypeDef Obstacles[GAME_OBSTACLES];
} Game_TypeDef;
void Game_Init(Game_TypeDef *Game, uint32_t Seed);
void Game_Step(Game_TypeDef *Game, int8_t DX, int8_t DY, uint8_t Pressed);
uint8_t Game_Level(const Game_TypeDef *Game);
uint8_t Game_PlanePixel(uint8_t X, uint8_t Y);
#endif
