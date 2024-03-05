set(DISABLE_TESTS ON CACHE BOOL "override hiredis")
set(ENABLE_SSL ON CACHE BOOL "override hiredis")
set(BUILD_SHARED_LIBS OFF CACHE BOOL "override hiredis")
add_subdirectory(hiredis EXCLUDE_FROM_ALL)

add_subdirectory(backward-cpp EXCLUDE_FROM_ALL)

add_subdirectory(fmt EXCLUDE_FROM_ALL)
add_subdirectory(argparse EXCLUDE_FROM_ALL)

add_library(rapid INTERFACE)
target_include_directories(rapid INTERFACE rapidjson/include)

add_subdirectory(describe)
add_subdirectory(utilcpp)
