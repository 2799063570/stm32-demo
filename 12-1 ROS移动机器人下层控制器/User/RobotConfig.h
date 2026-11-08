#ifndef __ROBOT_CONFIG_H
#define __ROBOT_CONFIG_H
/* STM32F103C8 / 72MHz；参数仅为台架起点，先标定编码器方向和减速比。 */

/* 编码器标定与目标速度限制。 */
#define ROBOT_COUNTS_REV 1320.0f /* 输出轴一圈四倍频计数 */
#define ROBOT_ENCODER_LEFT 1     /* 左编码器极性，仅取 +1 或 -1 */
#define ROBOT_ENCODER_RIGHT 1    /* 右编码器极性，仅取 +1 或 -1 */
#define ROBOT_MAX_RPM 200        /* 允许接纳的目标速度绝对值上限，RPM */

/* 超时与前台调度周期；控制 Tick 本身固定为 1ms。 */
#define ROBOT_COMMAND_MS 200u      /* 无新有效使能命令超过此时间后开始减速，ms */
#define ROBOT_DECEL_MS 600u        /* 进入减速后最多继续闭环的时间，ms；随后锁定停机 */
#define ROBOT_MAIN_TIMEOUT_MS 100u /* main 未刷新存活时间的故障阈值，ms */
#define ROBOT_FEEDBACK_MS 10u      /* 状态/编码器/IMU 上行反馈周期，ms */
#define ROBOT_IMU_MS 10u           /* 正常 IMU 采样启动间隔，ms */

/* PID + 前馈 + 斜坡参数；仅为台架起点，需结合实物调参。 */
#define ROBOT_KP 3.0f          /* 比例增益：速度误差 -> PWM 千分比 */
#define ROBOT_KI 8.0f          /* 积分增益，控制器内部乘 dt=0.001s */
#define ROBOT_KD 0.0f          /* 微分增益，控制器内部除 dt=0.001s */
#define ROBOT_FF 3.0f          /* 速度前馈：斜坡目标 RPM -> PWM 千分比 */
#define ROBOT_RAMP_RPM_MS 0.6f /* 每个 1ms 控制周期允许的目标变化量，RPM */
#endif
