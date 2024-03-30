cmake_minimum_required(VERSION 3.10)
project(radapter
    LANGUAGES CXX C ASM
    VERSION 0.0.0
    DESCRIPTION "lua-scriptable protocol zoo router"
)

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

function(add_lua_file target file)
    set(OUT ${CMAKE_BINARY_DIR}/${target}.lua.out)
    add_custom_command(
        OUTPUT ${OUT}
        COMMAND ${LUA_COMPILER} -o ${OUT} ${file}
        MAIN_DEPENDENCY ${IN}
        VERBATIM
        COMMENT "${LUA_COMPILER}: ${file} => ${OUT}"
    )
    include_bin(${target} ${OUT})
endfunction()

add_lua_file(compiled_bootstrap ${CMAKE_SOURCE_DIR}/src/lua/bootstrap.lua)
add_lua_file(compiled_mobdebug ${CMAKE_SOURCE_DIR}/submodule/MobDebug/src/mobdebug.lua)

file(GLOB_RECURSE SRC_FILES src/*.cpp include/*.hpp)
add_executable(radapter ${SRC_FILES})
target_include_directories(radapter PRIVATE include)
target_link_libraries(radapter PRIVATE
    fmt
    compiled_bootstrap
    compiled_mobdebug
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
