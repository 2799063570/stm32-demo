#include "GameView.h"
#include "OLED.h"
#include "GameConfig.h"

/** 函数：开始页显示当前ADC和实际送入游戏的方向，不需要串口排查方向问题。
  * DX正=右、DY正=下；推上应得到负DY，推下应得到正DY。
  * 仅在回中校准成功后覆盖标题页第2/3行，游戏区域不受影响。 */
void GameView_DrawStick(const StickInput_TypeDef *Stick, uint16_t RawX, uint16_t RawY)
{
	OLED_PrintLine(2, "X:%4u Y:%4u", (unsigned)RawX, (unsigned)RawY);
	OLED_PrintLine(3, "DX%+d DY%+d C%4u", (int)Stick->DX, (int)Stick->DY, (unsigned)Stick->CenterY);
}

/** 函数：复现时直接看原始采样、纵向命令和飞机位置，避免离开游戏后异常消失。
  * 例Y2048 D+0 P51：Y为PA1读数，D为实际DY，P为飞机左上角Y坐标。
  * 本函数只覆盖计分栏；坐标、碰撞、障碍和计分逻辑均不修改。 */
void GameView_DrawInputDebug(const Game_TypeDef *Game, const StickInput_TypeDef *Stick, uint16_t RawY)
{
	OLED_PrintLine(1, "Y%4u D%+d P%02d", (unsigned)RawY, (int)Stick->DY, (int)Game->Y);
}

void GameView_Draw(const Game_TypeDef *Game, uint8_t Calibrated, uint8_t ADCValid)
{
	uint8_t Index, X, Y;
	OLED_Clear();
	if (!ADCValid || !Calibrated)
	{
		OLED_PrintLine(1, GAME_BUILD_TAG);
		OLED_PrintLine(2, ADCValid ? "CENTER STICK" : "ADC ERROR");
		OLED_PrintLine(3, ADCValid ? "RELEASE TO CAL" : "CHECK / RESET");
		OLED_PrintLine(4, "X:PA0 Y:PA1");
		return;
	}
	if (Game->State == GAME_TITLE)
	{
		OLED_PrintLine(1, GAME_BUILD_TAG);
		OLED_PrintLine(2, "STICK TO MOVE");
		OLED_PrintLine(3, "FLY UP / DODGE");
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
		{
			int16_t ScreenY = Obstacle->Y + Y;
			/* 刚生成的障碍有一部分在计分栏内，必须额外裁剪。
			 * OLED_DrawPixel只裁屏幕边界，不会保护顶部文字区域。 */
			if (ScreenY < GAME_TOP || ScreenY >= GAME_HEIGHT) continue;
			for (X = 0; X < Obstacle->Width; X ++)
				OLED_DrawPixel(Obstacle->X + X, ScreenY, 1);
		}
	}
	for (Y = 0; Y < GAME_PLANE_SIZE; Y ++)
		for (X = 0; X < GAME_PLANE_SIZE; X ++)
			if (Game_PlanePixel(X, Y)) OLED_DrawPixel(Game->X + X, Game->Y + Y, 1);
}
