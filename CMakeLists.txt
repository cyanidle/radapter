cmake_minimum_required(VERSION 3.10)
project(radapter
    LANGUAGES CXX C ASM
    VERSION 0.0.0
    DESCRIPTION "lua-scriptable protocol zoo router"
)


option(RADAPTER_BINARY_BOOTSTRAP "Compile bootstrap.lua" ON)


include(cmake_utils/IncludeBin.cmake)

set(CMAKE_CXX_STANDARD 17)
set(BUILD_SHARED_LIBS OFF)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Qt5 REQUIRED COMPONENTS
    Core Network WebSockets SerialBus
)
set(CMAKE_AUTOMOC ON)
add_subdirectory(submodule)

add_library(lua_iface INTERFACE)
if (WIN32)
    find_package(Lua REQUIRED)
else()
    include(FindPkgConfig)
    pkg_check_modules(LUA REQUIRED lua) #liblua<version>-dev
endif()
target_include_directories(lua_iface INTERFACE ${LUA_INCLUDE_DIRS})
target_link_libraries(lua_iface INTERFACE ${LUA_LIBRARIES})
find_program(LUA_COMPILER
    NAMES luac REQUIRED
)
# TODO: Fix luac missmatch with lua

set(BOOTSTRAP_IN ${CMAKE_SOURCE_DIR}/src/bootstrap.lua)
if (RADAPTER_BINARY_BOOTSTRAP)
    set(BOOTSTRAP_OUT ${CMAKE_BINARY_DIR}/bootstrap.lua.out)
    add_custom_command(
        OUTPUT ${BOOTSTRAP_OUT}
        COMMAND ${LUA_COMPILER} -o ${BOOTSTRAP_OUT} ${BOOTSTRAP_IN}
        MAIN_DEPENDENCY ${BOOTSTRAP_IN}
        VERBATIM
        COMMENT "LUA: Compiling bootstrap => ${BOOTSTRAP_OUT}"
    )
    include_bin(compiled_bootstrap ${BOOTSTRAP_OUT})
else()
    include_bin(compiled_bootstrap ${BOOTSTRAP_IN})
endif()

file(GLOB_RECURSE SRC_FILES src/*.cpp include/*.hpp)
add_executable(radapter ${SRC_FILES})
target_include_directories(radapter PRIVATE include)
target_link_libraries(radapter PRIVATE
    fmt
    compiled_bootstrap
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
