#ifndef __IMU_H
#define __IMU_H
#include <stdint.h>

typedef struct
{
	int16_t Raw[6];  /* AX AY AZ GX GY GZ；默认±2g、±250deg/s */
	uint32_t Tick;   /* 采样事务启动时的系统毫秒计数 */
	uint32_t Errors; /* 采集/启动失败累计数 */
	uint8_t Valid;   /* 1=有效；失败或样本超过 100ms 则为 0 */
} IMU_DataTypeDef;

/** 仅初始化本地状态；main 持续调用 Service 才会完成 MPU6050 配置。 */
void IMU_Init(void);
/** 仅 main 调用：非阻塞推进配置/采样，失败 1 秒后重试。 */
void IMU_Service(void);
/** 仅 main 调用：复制最新样本，使用前检查 Valid；Tick 为采样启动时的系统毫秒数。 */
void IMU_Get(IMU_DataTypeDef *Data);
#endif
