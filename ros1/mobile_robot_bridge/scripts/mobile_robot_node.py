#!/usr/bin/env python3
"""ROS1 adapter: explicit arming, fresh cmd_vel required, never auto-rearm."""
import math
import struct
import threading
import time

import rospy
import serial
from geometry_msgs.msg import Twist
from sensor_msgs.msg import Imu, JointState
from std_msgs.msg import Bool, UInt8
from std_srvs.srv import SetBool, SetBoolResponse, Trigger, TriggerResponse
from robot_protocol import command, Parser


class Bridge:
    def __init__(self):
        self.radius = rospy.get_param('~wheel_radius', 0.05)
        self.track = rospy.get_param('~wheel_track', 0.30)
        self.cpr = rospy.get_param('~counts_rev', 1320.0)
        if not all(math.isfinite(x) and x > 0 for x in (self.radius, self.track, self.cpr)):
            raise ValueError('wheel geometry/counts must be finite and positive')
        self.port = serial.Serial(rospy.get_param('~port', '/dev/ttyUSB0'), 115200,
                                  timeout=0, write_timeout=0.02)
        self.lock = threading.Lock()
        self.armed = self.estop = self.clear_pending = False
        self.left = self.right = 0
        self.cmd_time = self.rx_time = float('-inf')
        self.state = 255
        self.sequence = 0
        self.parser = Parser()
        self.last_counts = None
        self.position = [0.0, 0.0]
        self.imu_pub = rospy.Publisher('imu/data_raw', Imu, queue_size=10)
        self.joint_pub = rospy.Publisher('wheel_states', JointState, queue_size=10)
        self.state_pub = rospy.Publisher('controller/state', UInt8, queue_size=10)
        rospy.Subscriber('cmd_vel', Twist, self.velocity, queue_size=1)
        rospy.Subscriber('emergency_stop', Bool, self.emergency, queue_size=1)
        rospy.Service('~enable', SetBool, self.enable)
        rospy.Service('~clear', Trigger, self.clear)

    def velocity(self, msg):
        v, w = msg.linear.x, msg.angular.z
        with self.lock:
            if not (math.isfinite(v) and math.isfinite(w)):
                self.armed = False
                self.left = self.right = 0
                self.cmd_time = float('-inf')
                return
            factor = 60 / (2 * math.pi * self.radius)
            left, right = (v - w * self.track / 2) * factor, (v + w * self.track / 2) * factor
            if not (math.isfinite(left) and math.isfinite(right)):
                self.armed = False
                return
            scale = max(1.0, abs(left) / 200, abs(right) / 200)
            self.left, self.right = round(left / scale), round(right / scale)
            self.cmd_time = time.monotonic()

    def emergency(self, msg):
        if msg.data:
            with self.lock:
                self.estop = True
                self.armed = False

    def enable(self, request):
        with self.lock:
            if not request.data:
                self.armed = False
                return SetBoolResponse(True, 'Disabled; next command switches output off')
            now = time.monotonic()
            if (self.estop or self.clear_pending or self.state != 0 or now - self.rx_time > 0.1
                    or now - self.cmd_time > 0.15 or self.left or self.right):
                return SetBoolResponse(False, 'Need fresh STOP feedback, zero cmd_vel, and cleared emergency')
            self.armed = True
            return SetBoolResponse(True, 'Armed; cmd_vel must remain fresh')

    def clear(self, _request):
        with self.lock:
            self.armed = self.estop = False
            self.clear_pending = True
        return TriggerResponse(True, 'CLEAR queued; verify controller/state=0 before enabling')

    def feedback(self, f):
        now = time.monotonic()
        state = f[5]
        with self.lock:
            self.state = state
            self.rx_time = now
            if state >= 2:
                self.armed = False
        stamp = rospy.Time.now()  # 接收时间；MCU tick位于帧8..11，尚未做时钟同步。
        self.state_pub.publish(UInt8(state))
        counts = struct.unpack_from('<II', f, 12)
        if self.last_counts is None:
            self.last_counts = counts
        for i in range(2):
            delta = ((counts[i] - self.last_counts[i] + 0x80000000) & 0xffffffff) - 0x80000000
            self.position[i] += delta * 2 * math.pi / self.cpr
        self.last_counts = counts
        joint = JointState()
        joint.header.stamp = stamp
        joint.name = ['left_wheel_joint', 'right_wheel_joint']
        joint.position = self.position[:]
        joint.velocity = [x * 2 * math.pi / 60 for x in struct.unpack_from('<hh', f, 20)]
        self.joint_pub.publish(joint)
        if f[38]:
            values = struct.unpack_from('<6h', f, 24)
            imu = Imu()
            imu.header.stamp = stamp
            imu.header.frame_id = rospy.get_param('~imu_frame', 'imu_link')
            imu.orientation_covariance[0] = -1  # 原始六轴数据，没有姿态估计。
            imu.linear_acceleration.x, imu.linear_acceleration.y, imu.linear_acceleration.z = [
                x * 9.80665 / 16384 for x in values[:3]]
            imu.angular_velocity.x, imu.angular_velocity.y, imu.angular_velocity.z = [
                x * math.pi / (180 * 131) for x in values[3:]]
            self.imu_pub.publish(imu)

    def run(self):
        try:
            while not rospy.is_shutdown():
                for f in self.parser.feed(self.port.read(512)):
                    self.feedback(f)
                with self.lock:
                    now = time.monotonic()
                    if now - self.rx_time > 0.1 or now - self.cmd_time > 0.15:
                        self.armed = False
                    flags = 2 if self.estop else (4 if self.clear_pending else (1 if self.armed else 0))
                    left, right = (self.left, self.right) if flags == 1 else (0, 0)
                    self.clear_pending = False
                data = command(self.sequence, left, right, flags)
                if self.port.write(data) != len(data):
                    raise serial.SerialTimeoutException('partial command write')
                self.sequence = (self.sequence + 1) & 65535
                time.sleep(0.02)  # 50Hz，独立于/use_sim_time。
        finally:
            try:
                self.port.write(command(self.sequence, flags=2))
            except serial.SerialException:
                pass
            self.port.close()


if __name__ == '__main__':
    rospy.init_node('mobile_robot_bridge')
    try:
        Bridge().run()
    except (serial.SerialException, ValueError) as error:
        rospy.logfatal('%s; bridge stopped, MCU timeout remains active', error)
