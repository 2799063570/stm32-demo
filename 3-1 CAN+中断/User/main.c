#include "stm32f10x.h"                  // Device header
#include "Delay.h"
#include "Serial.h"
#include "CAN.h"
#include "Config.h"

int main(void)
{
	uint32_t LastSend = 0;
	uint8_t Sequence = 0, Expected = 0, Waiting = 0;
	uint32_t RxStart = 0;
	CanRxMsg Message;
	/*模块初始化*/
	Delay_Init();
	Serial_Init();
	if (!MyCAN_Init()) { Serial_SendString("CAN INIT FAIL\r\n"); while (1); }
	Serial_SendString(CAN_LOOPBACK ? "CAN 500k: internal loopback\r\n" : "CAN 500k: normal bus, transceiver + ACK peer required\r\n");
	while (1)
	{
		uint8_t Result;
		MyCAN_Service();
		Result = MyCAN_GetTxResult();
		if (Result) Serial_SendString(Result == 1 ? "TX IRQ: OK\r\n" : "TX IRQ/TIMEOUT: FAIL\r\n");
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
		while (MyCAN_Receive(&Message))
		{
			uint8_t Index;
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
				Waiting = 0;
			}
			Serial_Printf(" DROP=%lu OVERRUN=%lu\r\n", (unsigned long)MyCAN_GetDropCount(), (unsigned long)MyCAN_GetOverrunCount());
		}
		if (Waiting && (uint32_t)(Delay_GetTick() - RxStart) >= 100)
		{
			Waiting = 0;
			Serial_SendString("LOOPBACK RX TIMEOUT\r\n");
		}
	}
}
