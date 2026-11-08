#ifndef __ROBOT_APP_H
#define __ROBOT_APP_H
/** 启动控制中断之前调用一次，初始化停机状态。 */
void RobotApp_Init(void);
/** main 持续调用：接收解析、IMU 采集、反馈、存活刷新；不得长时间阻塞。 */
void RobotApp_Service(void);
/** 仅由 TIM2 中断每 1ms 调用：编码器采样、控制计算、更新电机输出。 */
void RobotApp_Tick(void);
#endif
