#ifndef TEST_STM32_H
#define TEST_STM32_H
#include <stdint.h>
/*仅供电脑测试Control.c使用，不冒充硬件外设仿真。*/
static inline uint32_t __get_PRIMASK(void) { return 0; }
static inline void __disable_irq(void) { }
static inline void __set_PRIMASK(uint32_t Value) { (void)Value; }
#endif
