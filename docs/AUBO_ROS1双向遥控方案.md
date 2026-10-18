# STM32 摇杆 OLED 与 AUBO ROS1 双向遥控方案

本文件是整体接入设计，依据2026-10-18读取的本机 `D:/files/code/wheeltec_robot/wheeltec_robot/aubo` 源码整理。当前 [第九项固件](../9-1%20串口ROS双向遥控/说明.md) 和 [ROS Noetic C++功能包](../ros1/aubo_remote_bridge/README.md) 已提供串口输入/反馈及Gazebo HOME/ZERO命名位置验证场景，协议见 [串口遥控协议v2](串口遥控协议.md)。Gazebo动态联调尚未实跑；未连接或控制真机，连续末端点动、诊断回传和无线接入仍是后续设计。

## 已核对的工程接口

| 项目 | 现有配置 | 源码依据 |
|---|---|---|
| 机器人 | AUBO i5，六关节 | aubo_ros_control/config/controllers.yaml |
| 规划组 | aubo_i5 | aubo_moveit_config/config/aubo_i5.srdf |
| 末端坐标系 | tcp_link，相对base_link | 同上 |
| 原始关节反馈 | /aubo_i5/joint_states | aubo_ros_control/launch/aubo_real_bringup.launch |
| MoveIt/TF合并状态 | /joint_states | 同上 |
| 轨迹动作接口 | /aubo_i5/aubo_i5_controller/follow_joint_trajectory | aubo_moveit_config/config/arm_controllers.yaml |
| 底层控制器 | position_controllers/JointTrajectoryController | aubo_ros_control/config/controllers.yaml |

关节依次为 shoulder_joint、upperArm_joint、foreArm_joint、wrist1_joint、wrist2_joint、wrist3_joint。反馈解码必须按名称匹配，不能假定JointState数组顺序固定。

现有 `aubo_moveit_config/launch/joystick_control.launch` 使用 `/dev/input/js0`、joy_node及moveit_joy.py，是Linux输入设备路径。HC-05串口和ESP8266网络数据需要自己的接收桥，不会自动成为该输入设备。

现有 `aubo_visual_servo_node` 是本项目自定义视觉位置伺服，输入 `/visual_servo/target_pose`，不等同于标准MoveIt Servo。手动遥控需要单独控制适配层；不能直接假定已有TwistStamped/JointJog订阅口，也不能把遥控位置直接注入视觉目标并保留原有搜索行为。

硬件驱动内部读取急停/碰撞诊断并停止输出，但尚需增加明确的ROS状态发布接口，才能将这些故障可靠反馈到OLED。关节反馈正常不等于已确认机械臂安全就绪。

## 通信选择

| 方案 | 遥控器侧 | ROS电脑侧 | 前提 |
|---|---|---|---|
| HC-05 | STM32 UART连接HC-05 | 经典蓝牙SPP/RFCOMM字节流 | 电脑有兼容的经典蓝牙适配器，已配对并建立串口连接 |
| ESP8266 | STM32 UART连接ESP-AT模块 | TCP服务器接收和返回同一连接上的字节流 | 模块与电脑IP可达，端口允许访问，AT固件及接线已核实 |

短距离单遥控器原型建议先用HC-05，前提是主机蓝牙可用；需要接入已有机器人局域网时选ESP8266。首轮协议验证可以直接用USB-TTL，随后替换传输层。两条链路沿用相同的应用协议。

ESP8266初版可使用TCP单连接透传，模块作为客户端、电脑作为服务器。TCP会分段或合并数据，蓝牙串口也只提供字节流，因此都必须按帧解析，不能把一次read当作完整指令。TCP保持连接及模块STATE引脚都不能替代应用层心跳。

供电按实际模块型号和底板核对：HC-05裸模块与带稳压底板的VCC要求可能不同；ESP8266需要稳定且具备足够瞬态供电能力的3.3V电源。UART保持与STM32兼容的3.3V逻辑，不将5V信号直接送入MCU。

## 软件分层

```text
STM32: ADC摇杆/按键 -> 输入状态 -> 协议帧 -> UART -> HC-05或ESP8266
                            ^
                            | 反馈解码 -> OLED

PC: SerialTransport / TcpTransport
                 <-> 协议解析与会话管理
                 <-> aubo_remote_bridge（ROS1通信节点）
                       -> /remote/joy、/remote/link_state（拟新增）
                       -> 手动控制适配层 -> 已有机械臂执行链
                       <- /aubo_i5/joint_states、TF、控制器反馈/结果
                       <- 需要补充的机械臂诊断状态
```

上述 `/remote/*` 是拟新增接口，不是现有工程已存在的话题。通信节点负责双向协议与新鲜度；控制节点负责坐标映射、使能、速度和关节约束、控制权及停止处理。STM32不运行ROS，也不承担逆运动学。

连续点动需要选择并验证兼容的伺服/轨迹输出方案。现有JointTrajectoryController可用于有明确起止的轨迹动作；不能对每一帧摇杆输入都执行一次阻塞式MoveIt规划。手动、视觉伺服、分拣只能有一个运动命令源拥有控制权。SDK视觉伺服与ros_control硬件节点不能同时接管控制柜。

## 后续应用协议扩展草案

本节HELLO/ACK、连续点动模式和运动使能属于后续扩展，不是当前v2字段；当前POLL/INPUT/FEEDBACK已携带HOME/ZERO/CANCEL请求及状态。

帧结构拟采用：帧头、版本、类型、会话标识、序号、长度、有效载荷、CRC。明确字节序、数值单位及最大帧长；解析器支持分帧、粘帧、丢字节后的恢复。

| 消息 | 方向 | 内容 |
|---|---|---|
| HELLO / HELLO_ACK | 双向 | 协议版本、会话建立及能力 |
| POLL / INPUT | 电脑->遥控器->电脑 | 携带当前请求标识的最新摇杆、按键、模式、使能；电脑按本机单调时钟限制往返时间 |
| REQUEST / ACK | 双向 | 模式切换等离散请求，返回接受/拒绝及原因；使用请求ID去重 |
| FEEDBACK | 电脑->遥控器 | 实际关节、末端坐标、状态有效标记、控制模式、故障及执行结果 |

仅有递增序号和接收时刻不足以识别TCP积压的连续旧帧，因此运动输入应与当前轮询/有效期关联；过期请求的响应即使刚收到也不恢复运动。不直接比较两端未同步的时钟。接收缓存有界，连续输入只取最新有效状态；离散动作不能因重传重复执行。

反馈分别标明链路在线、机械臂反馈有效、请求已接受、动作已完成。缺少或过期的反馈显示UNKNOWN/STALE，不用零值或缓存值冒充实时状态。

## 遥控操作和显示

- 摇杆X/Y控制当前选定的两轴；通过模式键切换XY、XZ或单关节模式。
- 建议增加独立使能键，按住才允许点动；模式切换在停止状态完成。
- 摇杆原有按键可用于菜单确认；不将其同时安排为长按使能和点击切换，以免操作冲突。
- OLED轮换显示连接状态、模式、使能、真实关节角或末端位置、动作反馈与故障。
- 末端位置由电脑根据实际关节和机器人模型计算TF，明确base_link/tcp_link以及米/毫米等单位。

## 分阶段实现与验证

1. 通信与只读反馈：遥控输入在ROS中可观察，机械臂状态可返回OLED；不启动运动输出。可以使用现有aubo_state.launch验证关节反馈，但它不提供完整的安全就绪确认。
2. Gazebo手动点动：先选择关节，再验证XY/XZ映射、使能释放、控制权和停止流程。无线超时使用电脑单调时钟，不依赖可能暂停的仿真时间。
3. 真机适配：验证驱动停止/取消路径、控制柜已有缓存的处理、状态发布、重连后需重新使能及独立硬件急停。应用超时阈值不能等同于机械臂实际停止时间。

当前串口阶段不自动启动机械臂，不把通信桥直接接到真机运动接口。已确认电脑为ROS Noetic/Ubuntu 20.04/Python 3；物理串口、Noetic/Catkin运行、无线模块和主机网络，以及Gazebo/真机运行状态尚未验证。

## 模块参考

- [汇承HC-05用户手册](https://www.hc01.com/downloads/HC-05.pdf)：工作角色、串口和AT命令。不同固件和底板以实物资料为准。
- [Espressif ESP8266 ESP-AT TCP/IP示例](https://docs.espressif.com/projects/esp-at/en/release-v2.2.0.0_esp8266/AT_Command_Examples/TCP-IP_AT_Examples.html)：TCP客户端、UDP和UART-WiFi透传。
