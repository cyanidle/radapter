
# ============================================================================
# CPack DEB Packaging
# Must come AFTER add_subdirectory(deps) to override hiredis-leaked CPACK_* vars
# ============================================================================

set(CPACK_PACKAGE_VENDOR        "cyanidle")
set(CPACK_PACKAGE_CONTACT       "cyanidle")
set(CPACK_PACKAGE_CONTACT       "lyosha.doronin@gmail.com")
set(CPACK_PACKAGE_HOMEPAGE_URL  "https://github.com/cyanidle/radapter")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "NodeJS-style plumbing for industrial/embedded integration")
set(CPACK_PACKAGE_DESCRIPTION   "Wire together Modbus, WebSocket, Redis, SQL, Serial, CAN, and QML
GUI using short Lua scripts — with schema validation, async/await, and
hot-reload.  radapter integrates industrial devices through a pipeline of
workers and Lua functions.")

set(CPACK_RESOURCE_FILE_LICENSE  "${CMAKE_SOURCE_DIR}/LICENSE")
set(CPACK_RESOURCE_FILE_README   "${CMAKE_SOURCE_DIR}/README.md")

set(CPACK_GENERATOR              "DEB")
set(CPACK_DEBIAN_FILE_NAME       DEB-DEFAULT)
# All runtime deps are declared manually below (RADAPTER_DEB_*).  shlibdeps is
# useless here and breaks cross-builds where it can't resolve target-arch libs.
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS OFF)
set(CPACK_DEBIAN_PACKAGE_SECTION "utils")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")

# Architecture mapping
if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64|amd64")
    set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|ARM64|arm64")
    set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "arm64")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "armv7")
    set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "armhf")
else()
    set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "${CMAKE_SYSTEM_PROCESSOR}")
endif()

# Common Qt6 C++ runtime libs (both variants)
set(RADAPTER_DEB_COMMON_DEPS
    "libc6, libstdc++6, libgcc-s1, libssl3, libqt6core6, libqt6network6, libqt6serialport6, libqt6serialbus6, libqt6sql6, libqt6websockets6, libqt6serialbus6-plugins")

set(radapter_package_prefix "radapter")
if(RADAPTER_JIT)
    set(radapter_package_prefix "radapter-jit")
    if(NOT RADAPTER_JIT_STATIC)
        string(APPEND RADAPTER_DEB_COMMON_DEPS ", libluajit-5.1-2 | libluajit2-5.1-2")
    endif()
endif()

# GUI C++ runtime libs
set(RADAPTER_DEB_GUI_CPP_DEPS
    "libqt6gui6, libqt6qml6, libqt6quick6, libqt6widgets6")

# QML module runtime deps (loaded dynamically — invisible to dpkg-shlibdeps)
set(RADAPTER_DEB_GUI_QML_DEPS
    "qml6-module-qtquick, qml6-module-qtquick-controls, qml6-module-qtquick-layouts, qml6-module-qtquick-window, qml6-module-qtquick-dialogs, qml6-module-qtcharts")

if(RADAPTER_GUI)
    set(CPACK_PACKAGE_NAME "${radapter_package_prefix}-gui")
    set(CPACK_DEBIAN_PACKAGE_DEPENDS
        "${RADAPTER_DEB_COMMON_DEPS}, ${RADAPTER_DEB_GUI_CPP_DEPS}, ${RADAPTER_DEB_GUI_QML_DEPS}")
else()
    set(CPACK_PACKAGE_NAME "${radapter_package_prefix}-headless")
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "${RADAPTER_DEB_COMMON_DEPS}")
endif()

# Every variant owns /usr/bin/radapter and the same SDK path, but LuaJIT and
# Lua 5.4 plugins are not ABI-interchangeable. Do not provide the other flavor.
set(radapter_variants radapter-headless radapter-gui radapter-jit-headless radapter-jit-gui)
list(REMOVE_ITEM radapter_variants "${CPACK_PACKAGE_NAME}")
list(JOIN radapter_variants ", " CPACK_DEBIAN_PACKAGE_CONFLICTS)
set(CPACK_DEBIAN_PACKAGE_REPLACES "${CPACK_DEBIAN_PACKAGE_CONFLICTS}")
set(CPACK_DEBIAN_PACKAGE_PROVIDES "")

set(CPACK_DEBIAN_PACKAGE_NAME       "${CPACK_PACKAGE_NAME}")
set(CPACK_PACKAGE_VERSION           "${PROJECT_VERSION}")
set(CPACK_PACKAGE_FILE_NAME         "${CPACK_PACKAGE_NAME}")

# Exclude dependency headers, static archives, and other Unspecified installs
# pulled in by add_subdirectory(deps). radapter's own SDK headers are named
# radapter_runtime, so they stay in the engine package.
set(CPACK_DEB_COMPONENT_INSTALL ON)
set(CPACK_COMPONENTS_GROUPING ALL_COMPONENTS_IN_ONE)
set(CPACK_COMPONENTS_ALL radapter_runtime)

# ============================================================================
# ROS2 plugin packaging: with RADAPTER_ROS2=ON, switch to component install —
# one DEB for the engine (radapter-headless/radapter-gui) and one for the
# plugin (radapter-ros). The plugin DEB carries the ROS 2 Jazzy runtime
# dependencies, so installing it on a target pulls in (and thus verifies)
# a matching ROS 2 installation.
# ============================================================================
if(RADAPTER_ROS2 AND NOT RADAPTER_SDK_ONLY AND NOT RADAPTER_STATIC AND TARGET radapter_ros)
    install(TARGETS radapter_ros
        LIBRARY DESTINATION lib/radapter/plugins
        COMPONENT ros_plugin)
    set(CPACK_DEB_COMPONENT_INSTALL ON)
    set(CPACK_COMPONENTS_GROUPING IGNORE)
    set(CPACK_COMPONENTS_ALL radapter_runtime ros_plugin)
    set(CPACK_DEBIAN_RADAPTER_RUNTIME_PACKAGE_NAME "${CPACK_PACKAGE_NAME}")
    set(CPACK_DEBIAN_ROS_PLUGIN_PACKAGE_NAME "radapter-ros")
    # CPackDeb falls back to the engine-wide Conflicts (radapter-gui) when the
    # per-component variable is undefined — that would break the alternation below.
    set(CPACK_DEBIAN_ROS_PLUGIN_PACKAGE_CONFLICTS "")
    set(CPACK_DEBIAN_ROS_PLUGIN_PACKAGE_REPLACES "")
    # The plugin links rclcpp and the introspection typesupport directly;
    # Qt and the rest of the ROS client stack arrive transitively.
    set(CPACK_DEBIAN_ROS_PLUGIN_PACKAGE_DEPENDS
        "${radapter_package_prefix}-headless | ${radapter_package_prefix}-gui, ros-jazzy-rclcpp, ros-jazzy-rosidl-typesupport-introspection-cpp")
endif()

# Install prefix for Debian packaging
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "/usr" CACHE PATH "Install prefix for DEB packaging" FORCE)
endif()

# Disable non-DEB generators
set(CPACK_BINARY_STGZ  OFF)
set(CPACK_BINARY_TGZ   OFF)
set(CPACK_BINARY_TZ    OFF)
set(CPACK_BINARY_TBZ2  OFF)
set(CPACK_BINARY_TXZ   OFF)
set(CPACK_BINARY_DEB   ON)
set(CPACK_BINARY_RPM   OFF)
set(CPACK_BINARY_NSIS  OFF)
set(CPACK_SOURCE_TBZ2  OFF)
set(CPACK_SOURCE_TGZ   OFF)
set(CPACK_SOURCE_TXZ   OFF)
set(CPACK_SOURCE_TZ    OFF)
set(CPACK_SOURCE_ZIP   OFF)

include(CPack)

if(CPACK_DEB_COMPONENT_INSTALL)
    foreach(component IN LISTS CPACK_COMPONENTS_ALL)
        cpack_add_component(${component})
    endforeach()
endif()
