cmake_minimum_required(VERSION 3.10)
project(radapter
    LANGUAGES CXX C ASM
    VERSION 0.0.0
    DESCRIPTION "protocol zoo router with lua"
)

set(CMAKE_CXX_STANDARD 17)
set(BUILD_SHARED_LIBS OFF)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Qt5 REQUIRED COMPONENTS
    Core Network WebSockets SerialBus
)
set(CMAKE_AUTOMOC ON)
add_subdirectory(submodule)

find_program(LUA_COMPILER
    NAMES luac5.4 REQUIRED
)

set(BOOTSTRAP_OUT ${CMAKE_BINARY_DIR}/bootstrap.lua.out)
set(BOOTSTRAP_IN ${CMAKE_SOURCE_DIR}/src/bootstrap.lua)
add_custom_command(
    OUTPUT ${BOOTSTRAP_OUT}
    COMMAND ${LUA_COMPILER} -o ${BOOTSTRAP_OUT} ${BOOTSTRAP_IN}
    MAIN_DEPENDENCY ${BOOTSTRAP_IN}
    VERBATIM
    COMMENT "LUA: Compiling bootstrap"
)
add_custom_target(compile_bootstrap DEPENDS ${BOOTSTRAP_OUT})

file(GLOB_RECURSE SRC_FILES src/*.cpp include/*.hpp)
add_executable(radapter ${SRC_FILES})
set_property(SOURCE src/main.cpp APPEND PROPERTY OBJECT_DEPENDS ${BOOTSTRAP_IN})
add_dependencies(radapter compile_bootstrap)
target_include_directories(radapter PRIVATE include)
target_compile_definitions(radapter PRIVATE BOOTSTRAP_PATH="${BOOTSTRAP_OUT}")
target_link_libraries(radapter PRIVATE
    fmt
    incbin
    argparse
    lua_iface
    rapid
    hiredis
    hiredis_ssl
    describe
    utilcpp
    Qt5::Core
    Qt5::Network
    Qt5::WebSockets
    Qt5::SerialBus
)

target_precompile_headers(radapter PRIVATE
    <QObject>
    <string>
    <cstring>
    <string_view>
    <vector>
)
