cmake_minimum_required(VERSION 3.16)
project(radapter_ros C CXX)

find_package(rpcxx REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rosidl_typesupport_introspection_cpp REQUIRED)

create_radapter_plugin(radapter_ros
    FILES ./ros_plugin.cpp
)

target_link_libraries(radapter_ros PRIVATE
    rpcxx::rpcxx
    rclcpp::rclcpp
    rosidl_typesupport_introspection_cpp::rosidl_typesupport_introspection_cpp
)