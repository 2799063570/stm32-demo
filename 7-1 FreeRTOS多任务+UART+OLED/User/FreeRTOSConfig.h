#ifndef __FREERTOS_CONFIG_H
#define __FREERTOS_CONFIG_H

/*外设仍使用SPL；这里只配置FreeRTOS内核，不引入HAL或CMSIS-RTOS封装*/
#define configUSE_PREEMPTION                     1
#define configUSE_TIME_SLICING                   1
#define configCPU_CLOCK_HZ                      72000000UL
#define configTICK_RATE_HZ                      1000U
#define configTICK_TYPE_WIDTH_IN_BITS           TICK_TYPE_WIDTH_32_BITS
#define configMAX_PRIORITIES                    5
#define configMINIMAL_STACK_SIZE                128U
#define configMAX_TASK_NAME_LEN                 12
#define configTOTAL_HEAP_SIZE                   (10U * 1024U)
#define configSUPPORT_DYNAMIC_ALLOCATION        1
#define configSUPPORT_STATIC_ALLOCATION         0
#define configUSE_MUTEXES                       1
#define configUSE_RECURSIVE_MUTEXES             0
#define configUSE_COUNTING_SEMAPHORES            0
#define configUSE_TASK_NOTIFICATIONS             1
#define configUSE_TIMERS                        0
#define configUSE_CO_ROUTINES                   0
#define configUSE_IDLE_HOOK                     1
#define configUSE_TICK_HOOK                     0
#define configUSE_TICKLESS_IDLE                  0
#define configCHECK_FOR_STACK_OVERFLOW          2
#define configUSE_MALLOC_FAILED_HOOK            1
#define configUSE_TRACE_FACILITY                0
#define configUSE_STATS_FORMATTING_FUNCTIONS    0
#define INCLUDE_vTaskDelay                      1
#define INCLUDE_xTaskDelayUntil                 1
#define INCLUDE_vTaskSuspend                    1
#define INCLUDE_uxTaskGetStackHighWaterMark      1

/*F103实现4位优先级：全部分给抢占优先级，数值越小，硬件优先级越高*/
#define configPRIO_BITS                         4
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY 15
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY 5
/*RVDS的内联汇编也使用该常量，采用无U后缀的直接数值*/
#define configKERNEL_INTERRUPT_PRIORITY         240  // 15 << 4
#define configMAX_SYSCALL_INTERRUPT_PRIORITY    80   // 5 << 4

/*直接把官方移植层的三个异常入口绑定到启动文件中的向量名*/
#define vPortSVCHandler                         SVC_Handler
#define xPortPendSVHandler                      PendSV_Handler
#define xPortSysTickHandler                     SysTick_Handler

void App_Assert(const char *File, unsigned long Line);
#define configASSERT(Expression) do { if (!(Expression)) App_Assert(__FILE__, __LINE__); } while (0)
#endif
