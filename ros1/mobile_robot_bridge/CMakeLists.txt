cmake_minimum_required(VERSION 3.0.2)
project(mobile_robot_bridge)
find_package(catkin REQUIRED)
catkin_package()
catkin_install_python(PROGRAMS scripts/mobile_robot_node.py scripts/robot_protocol.py
  DESTINATION ${CATKIN_PACKAGE_BIN_DESTINATION})
