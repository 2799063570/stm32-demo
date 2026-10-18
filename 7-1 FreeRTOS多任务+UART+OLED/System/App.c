#include "stm32f10x.h"                  // Device header
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "App.h"
#include "DemoCommand.h"
#include "Serial.h"
#include "OLED.h"
#include "LED.h"
#include <stdio.h>
#include <string.h>

static TaskHandle_t App_SerialHandle, App_OLEDHandle, App_LEDHandle;
static QueueHandle_t App_LEDQueue;                   // 长度1：只保留最新的闪烁间隔
static SemaphoreHandle_t App_TextMutex;              // 保护Microlib格式化函数的并发调用
static volatile uint32_t App_RxCount;
static volatile uint16_t App_LEDPeriod = 500;        // 相邻两次翻转的间隔，不是完整亮灭周期
static volatile uint8_t App_LEDOn;

/*发生异常时用调试器查看这两个变量；不在故障钩子内调用打印/队列/OLED*/
const char *volatile App_FaultFile;
volatile unsigned long App_FaultLine;

/** 函数：断言、内存分配失败或栈溢出后关闭中断并保持LED点亮 */
void App_Assert(const char *File, unsigned long Line)
{
	__disable_irq();
	App_FaultFile = File;
	App_FaultLine = Line;
	LED_Set(1);
	while (1);
}

void vApplicationMallocFailedHook(void) { App_Assert("heap_4 exhausted", 0); }
void vApplicationStackOverflowHook(TaskHandle_t Task, char *Name)
{
	(void)Task;
	App_Assert(Name, 0);
}
void vApplicationIdleHook(void) { __WFI(); }         // 不阻塞空闲任务；每1ms时钟仍会唤醒CPU

/**
  * 函    数：串口任务独占DMA发送，等待通知时阻塞让出CPU
  * 参    数：Text 以零结尾且长度不超过256的字符串
  * 返 回 值：无；100ms仍未完成时中止DMA并累计驱动错误，不无限等待
  */
static void App_Send(const char *Text)
{
	TickType_t Start = xTaskGetTickCount();
	uint16_t Length = (uint16_t)strlen(Text);
	uint8_t Started;
	configASSERT(Length > 0 && Length <= 256);
	Started = Serial_SendArrayDMA((const uint8_t *)Text, Length);
	configASSERT(Started);                          // 有副作用的函数调用放在断言外
	while (Serial_GetTxBusy())
	{
		TickType_t Elapsed = xTaskGetTickCount() - Start;
		if (Elapsed >= pdMS_TO_TICKS(100)) { Serial_AbortTx(); break; }
		/*RX和TX共用通知唤醒；判断TX忙标志，不能把一次RX通知当成TX完成*/
		ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(100) - Elapsed);
	}
}

/** 函数：输出资源诊断；栈余量单位为字节，是启动以来观测到的最小余量 */
static void App_Report(void)
{
	char Text[240];
	uint16_t Period;
	uint32_t Rx;
	taskENTER_CRITICAL();
	Period = App_LEDPeriod;
	Rx = App_RxCount;
	taskEXIT_CRITICAL();
	xSemaphoreTake(App_TextMutex, portMAX_DELAY);
	snprintf(Text, sizeof(Text),
		"UP=%lums RX=%lu LED=%ums HEAP=%lu MIN=%lu STACK(U/O/L)=%lu/%lu/%luB DROP=%lu ERR=%lu\r\n",
		(unsigned long)xTaskGetTickCount(), (unsigned long)Rx, Period,
		(unsigned long)xPortGetFreeHeapSize(), (unsigned long)xPortGetMinimumEverFreeHeapSize(),
		(unsigned long)uxTaskGetStackHighWaterMark(App_SerialHandle) * sizeof(StackType_t),
		(unsigned long)uxTaskGetStackHighWaterMark(App_OLEDHandle) * sizeof(StackType_t),
		(unsigned long)uxTaskGetStackHighWaterMark(App_LEDHandle) * sizeof(StackType_t),
		(unsigned long)Serial_GetDropCount(), (unsigned long)Serial_GetErrorCount());
	xSemaphoreGive(App_TextMutex);                   // 发DMA前释放锁，不持锁等待外设
	App_Send(Text);
}

/** 函数：检查接收完整性，出现丢失/线路错误则丢弃当前命令至下一行尾 */
static void App_CheckRx(DemoCommand_State *Command, uint32_t *LastDrop, uint32_t *LastError)
{
	uint32_t Drop = Serial_GetDropCount(), Error = Serial_GetErrorCount();
	if (Drop != *LastDrop || Error != *LastError) DemoCommand_Invalidate(Command);
	*LastDrop = Drop;
	*LastError = Error;
}

/** 函数：最高应用优先级；IRQ只搬运并通知，命令解析与打印在本任务完成 */
static void App_SerialTask(void *Argument)
{
	DemoCommand_State Command;
	uint32_t LastDrop = 0, LastError = 0;
	uint16_t Period = 500;
	(void)Argument;
	DemoCommand_Init(&Command);
	Serial_SetTask(App_SerialHandle);
	Serial_Init();                                  // 此时调度器已运行，允许FromISR通知
	App_Send("FreeRTOS V11.2.0 + STM32 SPL\r\nHELP | STATUS | LED 100..2000 (CR/LF)\r\n");
	while (1)
	{
		uint8_t Byte;
		uint16_t Budget = 0;
		App_CheckRx(&Command, &LastDrop, &LastError);
		while (Budget < 64 && Serial_ReadByte(&Byte))
		{
			DemoCommand_Result Result;
			App_CheckRx(&Command, &LastDrop, &LastError);
			taskENTER_CRITICAL();
			App_RxCount ++;
			taskEXIT_CRITICAL();
			Budget ++;
			Result = DemoCommand_Push(&Command, Byte, &Period);
			if (Result == DEMO_COMMAND_HELP)
				App_Send("HELP | STATUS | LED 100..2000; LED value is toggle interval in ms\r\n");
			else if (Result == DEMO_COMMAND_STATUS) App_Report();
			else if (Result == DEMO_COMMAND_LED)
			{
				BaseType_t Sent = xQueueOverwrite(App_LEDQueue, &Period);
				configASSERT(Sent == pdPASS);
				App_Send("OK LED queued\r\n");
			}
			else if (Result == DEMO_COMMAND_INVALID) App_Send("ERR use HELP / STATUS / LED 100..2000\r\n");
		}
		/*预算耗尽可能仍有FIFO数据：延时1ms再读；否则等待下一次RX/TX通知*/
		if (Budget == 64) vTaskDelay(pdMS_TO_TICKS(1));
		else ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
	}
}

/** 函数：队列接收新间隔；等待超时才翻转LED，不使用忙等待延时 */
static void App_LEDTask(void *Argument)
{
	uint16_t Period = 500, NewPeriod;
	uint8_t On = 0;
	(void)Argument;
	while (1)
	{
		if (xQueueReceive(App_LEDQueue, &NewPeriod, pdMS_TO_TICKS(Period)) == pdPASS)
		{
			Period = NewPeriod;
			taskENTER_CRITICAL();
			App_LEDPeriod = Period;
			taskEXIT_CRITICAL();
		}
		else
		{
			On = !On;
			LED_Set(On);
			taskENTER_CRITICAL();
			App_LEDOn = On;
			taskEXIT_CRITICAL();
		}
	}
}

/** 函数：OLED显存只有本任务访问，1ms分块服务，200ms更新显示内容 */
static void App_OLEDTask(void *Argument)
{
	TickType_t LastWake = xTaskGetTickCount(), LastDraw = LastWake;
	(void)Argument;
	OLED_Init();
	xSemaphoreTake(App_TextMutex, portMAX_DELAY);
	OLED_PrintLine(1, "FreeRTOS + SPL");
	xSemaphoreGive(App_TextMutex);
	while (1)
	{
		TickType_t Now = xTaskGetTickCount();
		if ((TickType_t)(Now - LastDraw) >= pdMS_TO_TICKS(200))
		{
			uint32_t Rx;
			uint16_t Period;
			uint8_t On;
			LastDraw = Now;
			taskENTER_CRITICAL();
			Rx = App_RxCount;
			Period = App_LEDPeriod;
			On = App_LEDOn;
			taskEXIT_CRITICAL();
			xSemaphoreTake(App_TextMutex, portMAX_DELAY);
			OLED_PrintLine(2, "UP:%lus RX:%lu", (unsigned long)(Now / 1000), (unsigned long)(Rx % 100000));
			OLED_PrintLine(3, "LED:%ums %s", Period, On ? "ON" : "OFF");
			OLED_PrintLine(4, "HEAP:%luB", (unsigned long)xPortGetFreeHeapSize());
			xSemaphoreGive(App_TextMutex);
		}
		OLED_Service();
		xTaskDelayUntil(&LastWake, pdMS_TO_TICKS(1));
	}
}

/** 函数：创建固定数量对象；任务栈参数单位是32位字，不是字节 */
void App_Start(void)
{
	BaseType_t Created;
	App_LEDQueue = xQueueCreate(1, sizeof(uint16_t));
	App_TextMutex = xSemaphoreCreateMutex();
	configASSERT(App_LEDQueue && App_TextMutex);
	Created = xTaskCreate(App_SerialTask, "UART", 512, 0, 3, &App_SerialHandle);
	configASSERT(Created == pdPASS);
	Created = xTaskCreate(App_OLEDTask, "OLED", 384, 0, 2, &App_OLEDHandle);
	configASSERT(Created == pdPASS);
	Created = xTaskCreate(App_LEDTask, "LED", 128, 0, 1, &App_LEDHandle);
	configASSERT(Created == pdPASS);
	vTaskStartScheduler();
	App_Assert("scheduler start failed", 0);          // 创建空闲任务失败等情况，不静默死循环
}
