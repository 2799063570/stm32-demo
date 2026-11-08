/**
 * MPU6050 采集：前台依次配置寄存器，再按周期异步读取六轴原始值。
 * 读写完成由 IIC_GetResult() 通知；采集失败标无效，1 秒后重试。
 */
#include "IMU.h"
#include "IIC.h"
#include "Delay.h"
#include "RobotConfig.h"
#include <string.h>
static IMU_DataTypeDef IMU_Data;
/* Buffer 含加速度 6 字节、温度 2 字节、角速度 6 字节；Phase 为配置索引。 */
static uint8_t IMU_Buffer[14], IMU_Phase, IMU_Pending;
static uint32_t IMU_Next, IMU_Start;
/* 对应 PWR_MGMT_1、CONFIG、GYRO_CONFIG、ACCEL_CONFIG、SMPLRT_DIV。 */
static const uint8_t IMU_Registers[] = {0x6B, 0x1A, 0x1B, 0x1C, 0x19};
static const uint8_t IMU_Values[] = {1, 3, 0, 0, 9}; /* PLL、DLPF44Hz、量程、100Hz采样 */

/** 初始化异步 I2C 驱动和本地采样状态；实际 MPU6050 配置由 Service 分步完成。 */
void IMU_Init(void)
{
	IIC_Init();
	IMU_Phase = IMU_Pending = 0;
	memset(&IMU_Data, 0, sizeof(IMU_Data));
	IMU_Next = Delay_GetTick();
}

/** 函数：前台非阻塞初始化与DMA采集，错误重试，不在控制中断等待I2C。 */
void IMU_Service(void)
{
	IIC_ResultTypeDef Result;
	uint32_t Now = Delay_GetTick();
	uint8_t Axis;
	IIC_Service();
	/* 先消费上次事务结果，再决定发起下一次配置或采样。 */
	if (IMU_Pending && IIC_GetResult(&Result))
	{
		IMU_Pending = 0;
		if (Result != IIC_OK)
		{
			IMU_Data.Errors++;
			IMU_Data.Valid = 0;
			IMU_Phase = 0;
			IIC_Init();
			IMU_Next = Now + 1000;
		}
		else if (IMU_Phase < sizeof(IMU_Registers))
		{
			IMU_Phase++;
			IMU_Next = Now + (IMU_Phase == 1 ? 100 : 1);
		}
		else
		{
			for (Axis = 0; Axis < 6; Axis++)
			{
				uint8_t Offset = Axis < 3 ? Axis * 2 : (Axis * 2 + 2); /* 跳过温度 */
				IMU_Data.Raw[Axis] = (int16_t)(((uint16_t)IMU_Buffer[Offset] << 8) | IMU_Buffer[Offset + 1]);
			}
			IMU_Data.Tick = IMU_Start;
			IMU_Data.Valid = 1;
			IMU_Next = IMU_Start + ROBOT_IMU_MS;
		}
	}
	/* 超过 100ms 未更新的样本无效；Tick 使用采样事务启动时间。 */
	if (IMU_Data.Valid && (uint32_t)(Now - IMU_Data.Tick) > 100)
		IMU_Data.Valid = 0;
	if (!IMU_Pending && (int32_t)(Now - IMU_Next) >= 0)
	{
		IMU_Start = Now;
		Result = IMU_Phase < sizeof(IMU_Registers)
		             ? IIC_WriteRegsDMA(0x68, IMU_Registers[IMU_Phase], &IMU_Values[IMU_Phase], 1)
		             : IIC_ReadRegsDMA(0x68, 0x3B, IMU_Buffer, sizeof(IMU_Buffer));
		if (Result == IIC_OK)
			IMU_Pending = 1;
		else
		{
			IMU_Data.Valid = 0;
			IMU_Data.Errors++;
			IMU_Phase = 0;
			IMU_Next = Now + 1000;
		}
	}
}

/** 复制最近一次数据及有效标志；与 IMU_Service() 同在 main 中调用。 */
void IMU_Get(IMU_DataTypeDef *Data)
{
	*Data = IMU_Data;
}
