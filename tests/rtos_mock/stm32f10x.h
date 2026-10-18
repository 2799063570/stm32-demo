#ifndef TEST_RTOS_GPIO_H
#define TEST_RTOS_GPIO_H
#include <stdint.h>
/*只模拟引脚电平，用于检查真实软件SPI驱动输出的位序；不模拟STM32时序*/
typedef struct { uint16_t Output; } GPIO_TypeDef;
extern GPIO_TypeDef Test_GPIOB;
#define GPIOB (&Test_GPIOB)
typedef enum { Bit_RESET, Bit_SET } BitAction;
typedef enum { DISABLE, ENABLE } FunctionalState;
typedef struct { uint16_t GPIO_Pin; unsigned GPIO_Mode, GPIO_Speed; } GPIO_InitTypeDef;
#define GPIO_Mode_Out_PP 1U
#define GPIO_Speed_2MHz 2U
#define GPIO_Pin_0 0x0001U
#define GPIO_Pin_1 0x0002U
#define GPIO_Pin_12 0x1000U
#define GPIO_Pin_14 0x4000U
#define GPIO_Pin_15 0x8000U
#define RCC_APB2Periph_GPIOB 8U
void RCC_APB2PeriphClockCmd(uint32_t Peripheral, FunctionalState State);
void GPIO_Init(GPIO_TypeDef *Port, GPIO_InitTypeDef *Config);
void GPIO_SetBits(GPIO_TypeDef *Port, uint16_t Pins);
void GPIO_ResetBits(GPIO_TypeDef *Port, uint16_t Pins);
void GPIO_WriteBit(GPIO_TypeDef *Port, uint16_t Pin, BitAction Value);
#endif
