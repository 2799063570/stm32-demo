#include "GameView.h"
#include "OLED.h"

void GameView_Draw(const Game_TypeDef *Game, uint8_t Calibrated, uint8_t ADCValid)
{
	uint8_t Index, X, Y;
	OLED_Clear();
	if (!ADCValid || !Calibrated)
	{
		OLED_PrintLine(1, "JOYSTICK + OLED");
		OLED_PrintLine(2, ADCValid ? "CENTER STICK" : "ADC ERROR");
		OLED_PrintLine(3, ADCValid ? "RELEASE TO CAL" : "CHECK / RESET");
		OLED_PrintLine(4, "X:PA0 Y:PA1");
		return;
	}
	if (Game->State == GAME_TITLE)
	{
		OLED_PrintLine(1, "PLANE DODGE");
		OLED_PrintLine(2, "STICK TO MOVE");
		OLED_PrintLine(3, "DODGE THE BLOCKS");
		OLED_PrintLine(4, "PRESS TO START");
		return;
	}
	if (Game->State == GAME_OVER || Game->State == GAME_PAUSE)
	{
		OLED_PrintLine(1, Game->State == GAME_OVER ? "GAME OVER" : "PAUSED");
		OLED_PrintLine(2, "SCORE:%u", (unsigned)Game->Score);
		OLED_PrintLine(3, "BEST:%u", (unsigned)Game->Best);
		OLED_PrintLine(4, Game->State == GAME_OVER ? "PRESS TO RETRY" : "PRESS TO RESUME");
		return;
	}
	OLED_PrintLine(1, "S:%04u LV:%u", (unsigned)Game->Score, (unsigned)Game_Level(Game));
	for (Index = 0; Index < GAME_OBSTACLES; Index ++)
	{
		const Game_ObstacleTypeDef *Obstacle = &Game->Obstacles[Index];
		if (!Obstacle->Active) continue;
		for (Y = 0; Y < Obstacle->Height; Y ++)
			for (X = 0; X < Obstacle->Width; X ++)
				OLED_DrawPixel(Obstacle->X + X, Obstacle->Y + Y, 1);
	}
	for (Y = 0; Y < GAME_PLANE_SIZE; Y ++)
		for (X = 0; X < GAME_PLANE_SIZE; X ++)
			if (Game_PlanePixel(X, Y)) OLED_DrawPixel(Game->X + X, Game->Y + Y, 1);
}
