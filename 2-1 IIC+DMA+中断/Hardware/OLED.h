#ifndef __OLED_H
#define __OLED_H
#include <stdint.h>
void OLED_Init(void);
void OLED_Service(void);
uint8_t OLED_GetReady(void);
uint32_t OLED_GetErrorCount(void);
void OLED_Clear(void);
void OLED_ShowChar(uint8_t Line, uint8_t Column, char Char);
void OLED_ShowString(uint8_t Line, uint8_t Column, const char *String);
void OLED_PrintLine(uint8_t Line, const char *Format, ...);
void OLED_DrawPixel(int16_t X, int16_t Y, uint8_t On);
void OLED_DrawLine(int16_t X1, int16_t Y1, int16_t X2, int16_t Y2, uint8_t On);
void OLED_DrawBar(uint8_t X, uint8_t Y, uint8_t Width, uint8_t Height, int32_t Value, int32_t Limit);
const uint8_t *OLED_GetBuffer(void);
#endif
