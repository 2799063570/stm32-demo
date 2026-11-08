#ifndef __JOYSTICK_H
#define __JOYSTICK_H
#include <stdint.h>
uint8_t Joystick_Init(void);
uint8_t Joystick_Read(uint16_t *X, uint16_t *Y, uint8_t *Pressed);
#endif
