#include "stm32f10x.h"                  // Device header
#include "Delay.h"
#include "Serial.h"
#include "CAN.h"
#include "Config.h"
#include "OLED.h"

int main(void)
{
	uint32_t LastSend = 0;
	uint8_t Sequence = 0, Expected = 0, Waiting = 0;
	uint32_t RxStart = 0;
	uint32_t LastDisplay = 0, TxCount = 0, RxCount = 0, ErrorCount = 0;
	CanRxMsg Message;
	uint8_t CANReady;
	/*模块初始化*/
	Delay_Init();
	Serial_Init();
	CANReady = MyCAN_Init();
	OLED_Init();                                    // CAN先重映射，OLED最后设置SWJ，确保PB3释放且保留SWD
	if (!CANReady)
	{
		Serial_SendString("CAN INIT FAIL\r\n");
		OLED_PrintLine(1, "CAN INIT FAIL");
		while (1) OLED_Service();
	}
	Serial_SendString(CAN_LOOPBACK ? "CAN 500k: internal loopback\r\n" : "CAN 500k: normal bus, transceiver + ACK peer required\r\n");
	while (1)
	{
		uint8_t Result;
		MyCAN_Service();
		Result = MyCAN_GetTxResult();
		if (Result)
		{
			if (Result == 1) TxCount ++; else ErrorCount ++;
			Serial_SendString(Result == 1 ? "TX IRQ: OK\r\n" : "TX IRQ/TIMEOUT: FAIL\r\n");
		}
		if ((uint32_t)(Delay_GetTick() - LastSend) >= 1000)
		{
			uint8_t Data[3] = {Sequence, 0xA5, 0x5A};
			LastSend = Delay_GetTick();
			if (MyCAN_Send(0x123, Data, 3))
			{
				Expected = Sequence ++;
				Waiting = CAN_LOOPBACK;
				RxStart = Delay_GetTick();
			}
		}
		/*限制每轮处理帧数，持续CAN流量不能饿死显示与发送超时检查*/
		for (uint8_t Processed = 0; Processed < 3 && MyCAN_Receive(&Message); Processed ++)
		{
			uint8_t Index;
			RxCount ++;
			Serial_Printf("RX IRQ: ID=%08lX DLC=%u %s ",
				(unsigned long)(Message.IDE == CAN_Id_Standard ? Message.StdId : Message.ExtId),
				Message.DLC, Message.RTR == CAN_RTR_Data ? "DATA" : "REMOTE");
			if (Message.RTR == CAN_RTR_Data)
				for (Index = 0; Index < Message.DLC && Index < 8; Index ++) Serial_Printf("%02X ", Message.Data[Index]);
			if (CAN_LOOPBACK)
			{
				uint8_t Match = Waiting && Message.IDE == CAN_Id_Standard && Message.StdId == 0x123 &&
					Message.RTR == CAN_RTR_Data && Message.DLC == 3 && Message.Data[0] == Expected &&
					Message.Data[1] == 0xA5 && Message.Data[2] == 0x5A;
				Serial_SendString(Match ? "PASS" : "FAIL");
				if (!Match) ErrorCount ++;
				Waiting = 0;
			}
			Serial_Printf(" DROP=%lu OVERRUN=%lu\r\n", (unsigned long)MyCAN_GetDropCount(), (unsigned long)MyCAN_GetOverrunCount());
		}
		if (Waiting && (uint32_t)(Delay_GetTick() - RxStart) >= 100)
		{
			Waiting = 0;
			ErrorCount ++;
			Serial_SendString("LOOPBACK RX TIMEOUT\r\n");
		}
		if ((uint32_t)(Delay_GetTick() - LastDisplay) >= 200)
		{
			LastDisplay = Delay_GetTick();
			OLED_PrintLine(1, CAN_LOOPBACK ? "CAN 500K LOOP" : "CAN 500K NORMAL");
			OLED_PrintLine(2, "TX:%lu", (unsigned long)TxCount);
			OLED_PrintLine(3, "RX:%lu", (unsigned long)RxCount);
			OLED_PrintLine(4, "E%lu D%lu", (unsigned long)(ErrorCount % 100000),
				(unsigned long)((MyCAN_GetDropCount() + MyCAN_GetOverrunCount()) % 100000));
		}
		OLED_Service();
	}
}
