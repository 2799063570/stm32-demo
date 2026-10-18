#include "stm32f10x.h"                  // Device header
#include "Delay.h"
#include "Serial.h"
#include "IIC.h"
#include "OLED.h"

static uint8_t MPU6050_Data[14];                    // 连续读取加速度、温度、角速度，共14字节

int main(void)
{
	uint8_t WakeUp = 0x01;
	uint8_t Phase = 0, Pending = 0;
	uint32_t NextTick = 0;
	uint32_t LastDisplay = 0;
	int16_t AX = 0, AY = 0, AZ = 0;
	uint8_t DataValid = 0;
	IIC_ResultTypeDef LastResult = IIC_BUSY;
	IIC_ResultTypeDef Result;
	/*模块初始化*/
	Delay_Init();
	Serial_Init();
	IIC_Init();
	OLED_Init();
	Serial_SendString("I2C2 DMA + EV/ER IRQ: MPU6050 address7=0x68\r\n");
	while (1)
	{
		IIC_Service();                              // 必须持续调用，负责超时和STOP确认
		if (Pending && IIC_GetResult(&Result))
		{
			Pending = 0;
			LastResult = Result;
			if (Result != IIC_OK)
			{
				Serial_Printf("IIC ERROR=%u (2=NACK,3=BUS,4=DMA,5=TIMEOUT)\r\n", Result);
				Phase = 0;
				DataValid = 0;
				NextTick = Delay_GetTick() + 1000;
			}
			else if (Phase == 0)
			{
				Phase = 1;
				NextTick = Delay_GetTick() + 100;     // 取消睡眠后留出传感器稳定时间
			}
			else
			{
				AX = (int16_t)((uint16_t)MPU6050_Data[0] << 8 | MPU6050_Data[1]);
				AY = (int16_t)((uint16_t)MPU6050_Data[2] << 8 | MPU6050_Data[3]);
				AZ = (int16_t)((uint16_t)MPU6050_Data[4] << 8 | MPU6050_Data[5]);
				DataValid = 1;
				Serial_Printf("ACC raw: X=%d Y=%d Z=%d\r\n", AX, AY, AZ);
				NextTick = Delay_GetTick() + 100;
			}
		}
		if (!Pending && (int32_t)(Delay_GetTick() - NextTick) >= 0)
		{
			Result = Phase == 0 ? IIC_WriteRegsDMA(0x68, 0x6B, &WakeUp, 1) :
				IIC_ReadRegsDMA(0x68, 0x3B, MPU6050_Data, sizeof(MPU6050_Data));
			if (Result == IIC_OK) Pending = 1;
			else
			{
				LastResult = Result;
				DataValid = 0;
				Serial_Printf("IIC START=%u; check bus levels\r\n", Result);
				NextTick = Delay_GetTick() + 1000;
			}
		}
		if ((uint32_t)(Delay_GetTick() - LastDisplay) >= 200)
		{
			LastDisplay = Delay_GetTick();
			OLED_PrintLine(1, "MPU6050 E:%u", LastResult);
			if (DataValid)
			{
				OLED_PrintLine(2, "AX:%6d", AX);
				OLED_PrintLine(3, "AY:%6d", AY);
				OLED_PrintLine(4, "AZ:%6d", AZ);
			}
			else
			{
				OLED_PrintLine(2, "NO VALID DATA");
				OLED_PrintLine(3, "CHECK I2C2 BUS");
				OLED_PrintLine(4, "PB10/11 MPU6050");
			}
		}
		OLED_Service();                             // 独立SPI总线，不占I2C2与DMA4/5
	}
}
