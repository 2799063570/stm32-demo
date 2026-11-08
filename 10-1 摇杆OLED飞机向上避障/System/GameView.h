#ifndef __GAME_VIEW_H
#define __GAME_VIEW_H
#include "Game.h"
#include "StickInput.h"
void GameView_Draw(const Game_TypeDef *Game, uint8_t Calibrated, uint8_t ADCValid);
void GameView_DrawStick(const StickInput_TypeDef *Stick, uint16_t RawX, uint16_t RawY);
void GameView_DrawInputDebug(const Game_TypeDef *Game, const StickInput_TypeDef *Stick, uint16_t RawY);
#endif
