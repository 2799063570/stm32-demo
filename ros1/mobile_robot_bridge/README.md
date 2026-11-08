# ROS1双轮底盘桥

配合 `12-1 ROS移动机器人下层控制器`。脚本只负责串口协议、差速运动学、原始IMU/轮状态发布及明确使能，不依赖AUBO包。

在ROS1 Python3环境中把本目录放入catkin工作空间的src，安装rosdep依赖并编译：

```bash
cd ~/catkin_ws
rosdep install --from-paths src --ignore-src -r -y
catkin_make
source devel/setup.bash
rosrun mobile_robot_bridge mobile_robot_node.py _port:=/dev/ttyUSB0 _wheel_radius:=0.05 _wheel_track:=0.30 _counts_rev:=1320
```

源码通过Git传到Linux后需 `chmod +x scripts/*.py`，或使用catkin安装空间的脚本。设备串口权限由运行用户配置。

- 订阅 `cmd_vel`（Twist），线速度m/s、角速度rad/s。左右RPM按半径/轮距转换，超200RPM时等比例限幅；参数必须与实物一致。
- 订阅 `emergency_stop`（Bool），true锁定主机急停；false不解除。服务 `~clear` 清除主机急停，并排队一条零目标CLEAR；其成功响应表示已排队，**需观察状态0确认MCU已接受**。
- `~enable`（SetBool）true要求最近100ms内反馈STOP、最近150ms内收到零cmd_vel且无急停/待发CLEAR；false在下一发送周期关闭输出。
- 发布 `controller/state`（UInt8，0..5）、`wheel_states`（JointState，rad及rad/s）、`imu/data_raw`（Imu）。不发布虚构orientation，协方差首项-1表示无姿态估计。坐标轴尚未做安装旋转；先确认imu_link实际安装方向。
- 串口50Hz发送，反馈100Hz。cmd_vel超过150ms未更新或反馈超过100ms未到，主机取消使能并发送禁用帧，需重新授权。串口断开/写入错误退出进程，由MCU200ms超时减速处理；退出尝试发一次急停，不能保证链路故障时送达。
- 使用主机monotonic计时，不受 `/use_sim_time` 停滞影响；消息stamp取接收时的ROS时间，未做MCU时钟同步。MCU重启会重置编码器累计值，需重启桥接以建立新的轮位置基准。

抬轮调试示例，在另一个终端持续发零速度，清除后确认状态0再使能：

```bash
rostopic pub -r 20 /cmd_vel geometry_msgs/Twist '{}'
rosservice call /mobile_robot_bridge/clear '{}'
rostopic echo /controller/state
rosservice call /mobile_robot_bridge/enable 'data: true'
```

之后由单一控制源持续发布cmd_vel；更换控制源前停止零速度发布器，避免多个发布者争抢。默认ROS节点名为mobile_robot_bridge，命名空间改变时相应调整服务和话题路径。

该包的协议部分已有电脑交叉验证；当前工作环境没有ROS运行时，catkin构建、真实串口与ROS话题联调需在ROS机器上执行。若使用ROS2，保持48字节协议，另写rclpy/rclcpp适配层即可。
