#ifndef __CONFIG_H
#define __CONFIG_H

/*编码器参数：以下是示例值，使用前按实际电机修改；TI12模式已经四倍频*/
#define ENCODER_PPR             11.0f               // 电机轴每相每圈原始脉冲数，不是四倍频后的数
#define MOTOR_GEAR_RATIO        30.0f               // 电机轴转数/输出轴转数
#define ENCODER_COUNTS_PER_REV  (ENCODER_PPR * 4.0f * MOTOR_GEAR_RATIO)
#define ENCODER_DIRECTION       1                   // 正向PWM时反馈应为正，否则改成-1或交换AB相

/*控制周期、目标与输出约束*/
#define CONTROL_PERIOD_MS       10                  // TIM2每10ms执行一次控制，即100Hz
#define CONTROL_MAX_RPM         300                 // 输出轴RPM命令范围：-300~300
#define CONTROL_RAMP_RPM_S      100.0f              // 每秒最多改变100RPM，限制目标阶跃
#define CONTROL_FILTER_ALPHA    0.25f               // 测速一阶滤波系数，范围(0,1]
#define CONTROL_PWM_LIMIT       600.0f              // 输出限幅±600/1000，占空比不超过60%
#define CONTROL_COMMAND_TIMEOUT 1000                // 超过1s无有效SPEED命令则停机并锁定故障

/*以下增益仅供演示，需要结合电机辨识和实测整定；输出单位为PWM千分比*/
#define PID_KP                  1.5f                // PWM/RPM
#define PID_KI                  3.0f                // PWM/(RPM*s)
#define PID_KD                  0.0f                // PWM*s/RPM；默认先关闭微分
#define PID_KV                  1.5f                // 速度前馈，PWM/RPM
#define PID_KS                  40.0f               // 静摩擦前馈，PWM千分比
#define PID_D_FILTER            0.2f                // 测量微分低通系数

/*反馈异常保护：可按低速分辨率和实际机械负载调整*/
#define CONTROL_STALL_PWM       350.0f
#define CONTROL_STALL_RPM       5.0f
#define CONTROL_STALL_TIME_MS   500
#define CONTROL_REVERSE_TIME_MS 200

#if CONTROL_PERIOD_MS != 10
#error This example uses a fixed 10ms TIM2 configuration; update Timer.c together.
#endif
#if ENCODER_DIRECTION != 1 && ENCODER_DIRECTION != -1
#error ENCODER_DIRECTION must be 1 or -1
#endif
#endif
