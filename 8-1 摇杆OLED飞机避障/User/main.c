#include "stm32f10x.h"
#include "Delay.h"
#include "OLED.h"
#include "Joystick.h"
#include "StickInput.h"
#include "Game.h"
#include "GameView.h"
#include "GameConfig.h"

int main(void)
{
	static Game_TypeDef Game;
	static StickInput_TypeDef Stick;
	uint32_t LastSample, LastFrame;
	uint16_t X = 2048, Y = 2048;
	uint8_t Pressed, ADCReady, ADCValid;
	Delay_Init();
	OLED_Init();
	ADCReady = Joystick_Init();
	ADCValid = ADCReady;
	StickInput_Init(&Stick);
	Game_Init(&Game, 1);
	LastSample = LastFrame = Delay_GetTick();
	GameView_Draw(&Game, 0, ADCValid);
	while (1)
	{
		uint32_t Now = Delay_GetTick();
		if ((uint32_t)(Now - LastSample) >= STICK_SAMPLE_MS)
		{
			LastSample = Now;
			ADCValid = ADCReady && Joystick_Read(&X, &Y, &Pressed);
			if (ADCValid) StickInput_Feed(&Stick, X, Y, Pressed);
			else
			{
				/*采样失败即暂停并重新校准，禁止使用旧方向继续飞行。*/
				if (Game.State == GAME_PLAY) Game.State = GAME_PAUSE;
				StickInput_Init(&Stick);
			}
		}
		if ((uint32_t)(Now - LastFrame) >= GAME_FRAME_MS)
		{
			uint8_t Event = StickInput_TakePress(&Stick);
			LastFrame = Now;                          // 过载时放慢游戏，不突发追赶跳过画面
			if (Stick.Ready && ADCValid)
			{
				if (Game.State == GAME_TITLE) Game.Random = Now ^ ((uint32_t)X << 16) ^ Y ^ 0x13579BDFu;
				if (Game.Random == 0) Game.Random = 1;
				Game_Step(&Game, Stick.DX, Stick.DY, Event);
			}
			GameView_Draw(&Game, Stick.Ready, ADCValid);
		}
		OLED_Service();                             // 空闲时间连续分块刷新，不等待Delay_ms(40)
	}
}
