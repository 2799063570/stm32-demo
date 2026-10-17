#ifndef __MYCAN_H
#define __MYCAN_H
#include "stm32f10x.h"
uint8_t MyCAN_Init(void);
uint8_t MyCAN_Send(uint16_t ID, const uint8_t *Data, uint8_t Length);
uint8_t MyCAN_Receive(CanRxMsg *Message);
uint8_t MyCAN_GetTxResult(void);
void MyCAN_Service(void);
uint32_t MyCAN_GetDropCount(void);
uint32_t MyCAN_GetOverrunCount(void);
#endif
