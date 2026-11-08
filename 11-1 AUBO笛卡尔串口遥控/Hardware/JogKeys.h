#ifndef __JOG_KEYS_H
#define __JOG_KEYS_H
#include <stdint.h>
/* 只读取摇杆自带PA2按键，不占用PA3～PA6。按键采集独立于ADC。 */
void JogKeys_Init(void);
uint8_t JogKeys_Read(void);
#endif
