#ifndef __SERIAL_H
#define __SERIAL_H
#include "stm32f10x.h"
/** USART1 PA9/PA10，115200/8N1；RX DMA1 CH5，TX DMA1 CH4。 */
void Serial_Init(void);
/** 仅 main 调用，复制最多 Capacity 字节并返回实际数量；遇到错误/覆盖返回 0。 */
uint16_t Serial_ReadBlock(uint8_t *Data, uint16_t Capacity);
/** 仅 main 调用，复制 1..48 字节后异步发送；返回 1=已启动，0=忙或参数无效。 */
uint8_t Serial_SendArrayDMA(const uint8_t *Data, uint16_t Length);
/** main 每轮调用，检查并终止超过 20ms 的发送。 */
void Serial_Service(void);
/** UART/DMA/发送超时累计错误次数。 */
uint32_t Serial_GetErrorCount(void);
/** RX 环形缓冲区覆盖累计次数。 */
uint32_t Serial_GetDropCount(void);
#endif
