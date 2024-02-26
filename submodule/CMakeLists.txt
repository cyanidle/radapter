set(DISABLE_TESTS ON CACHE BOOL "override hiredis")
set(ENABLE_SSL ON CACHE BOOL "override hiredis")
set(BUILD_SHARED_LIBS OFF CACHE BOOL "override hiredis")
add_subdirectory(hiredis EXCLUDE_FROM_ALL)

add_subdirectory(backward-cpp EXCLUDE_FROM_ALL)

cmake_policy(SET CMP0079 NEW)
include(FindPkgConfig)
pkg_check_modules(LUA REQUIRED lua-5.4) #liblua<version>-dev
add_library(lua_iface INTERFACE)
target_include_directories(lua_iface INTERFACE ${LUA_INCLUDE_DIRS})
target_link_libraries(lua_iface INTERFACE ${LUA_LIBRARIES})

add_subdirectory(fmt EXCLUDE_FROM_ALL)
add_subdirectory(argparse EXCLUDE_FROM_ALL)

add_library(incbin INTERFACE)
target_include_directories(incbin INTERFACE incbin)

add_library(rapid INTERFACE)
target_include_directories(rapid INTERFACE rapidjson/include)

add_subdirectory(describe)
add_subdirectory(utilcpp)
