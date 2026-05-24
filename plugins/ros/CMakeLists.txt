cmake_minimum_required(VERSION 3.16)
project(radapter_ros C CXX)

add_subdirectory(../../. radapter EXCLUDE_FROM_ALL)

create_radapter_plugin(radapter_ros
    FILES ./plugin.cpp
)