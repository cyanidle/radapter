# CMake toolchain for cross-compiling radapter + radapter_ros for aarch64.
# Used by scripts/Dockerfile.ros (cross-builder stage) and by the
# build_ros_plugin --cross arm64 script inside the DevContainer.
#
# Sysroot layout (populated by the Dockerfile):
#   /sysroot/lib               target-arch system libs
#   /sysroot/usr/lib           target-arch Qt6 + system dev libs
#   /sysroot/usr/include       target-arch headers
#   /sysroot/opt/ros/jazzy     target-arch ROS2 (libs, headers, cmake configs)

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

set(CMAKE_SYSROOT /sysroot)

set(CMAKE_C_COMPILER   aarch64-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)

set(CMAKE_FIND_ROOT_PATH /sysroot)

# Search ONLY in the sysroot for libs, headers, and cmake packages.
# Programs (like moc/rcc) are needed from the HOST — they've been
# copied into the sysroot tree so they're findable, but we still
# want host tools to be usable, so PROGRAM is set to NEVER (search
# host first) and the sysroot copies handle the case where a
# cross-compiled dependency's cmake config hardcodes a tool path.
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Qt6 and ROS2 are under non-standard prefixes inside the sysroot.
# find_package re-roots each entry against CMAKE_FIND_ROOT_PATH, so
# /opt/ros/jazzy resolves to /sysroot/opt/ros/jazzy at search time.
list(APPEND CMAKE_PREFIX_PATH /opt/ros/jazzy)
