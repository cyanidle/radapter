# CMake toolchain for cross-compiling radapter for aarch64.
# Used by scripts/Dockerfile.headless and scripts/Dockerfile.gui
# (cross-builder stages).
#
# Sysroot layout (populated by the Dockerfile):
#   /sysroot/lib               target-arch system libs
#   /sysroot/usr/lib           target-arch Qt6 + system dev libs
#   /sysroot/usr/bin           target-arch tools
#   /sysroot/usr/include       target-arch headers

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

set(CMAKE_SYSROOT /sysroot)

set(CMAKE_C_COMPILER   aarch64-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)

set(CMAKE_FIND_ROOT_PATH /sysroot)

# Search ONLY in the sysroot for libs, headers, and cmake packages.
# Programs (like moc/rcc) are needed from the HOST — they've been
# copied into the sysroot tree so they're findable, but we still
# want host tools to be usable, so PROGRAM is set to NEVER.
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
