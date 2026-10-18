#ifndef __OLED_SPI_H
#define __OLED_SPI_H
#include <stdint.h>
void OLED_SPI_Init(void);
void OLED_SPI_SetReset(uint8_t Level);
uint8_t OLED_SPI_Write(uint8_t Control, const uint8_t *Data, uint8_t Length);
#endif
