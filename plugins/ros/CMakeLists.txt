cmake_minimum_required(VERSION 3.16)
project(radapter_ros C CXX)

find_package(rclcpp REQUIRED)

create_radapter_plugin(radapter_ros
    FILES ./ros_plugin.cpp
)

target_link_libraries(radapter_ros PRIVATE rclcpp::rclcpp)