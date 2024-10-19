CPMAddPackage("gh:cyanidle/describe@1.3")
CPMAddPackage("gh:cyanidle/glua@1.0")
CPMAddPackage("gh:p-ranav/argparse@3.1")

CPMAddPackage(NAME fmt
    GITHUB_REPOSITORY fmtlib/fmt
    GIT_SHALLOW YES
    GIT_TAG 11.0.2
)

CPMAddPackage(NAME hiredis
  GITHUB_REPOSITORY redis/hiredis
  GIT_SHALLOW YES
  VERSION 1.2.5
  GIT_TAG e0f4820
  OPTIONS
    "DISABLE_TESTS ON"
    "ENABLE_SSL ON"
    "BUILD_SHARED_LIBS OFF"
  PATCHES
    ./hiredis-min_cmake.patch
    ./hiredis-cert_null.patch
)

include(../cmake/SystemLink.cmake)

if (RADAPTER_JIT) #libluajit2-5.1-dev
  include(FindPkgConfig)
  pkg_check_modules(LUA_JIT luajit)
  if (NOT LUA_JIT_FOUND)
    message(FATAL_ERROR "Could not find luajit using pkgconfig")
  endif()
  add_library(radapter_lua INTERFACE)
  target_include_system_directories(radapter_lua INTERFACE ${LUA_JIT_INCLUDE_DIRS})
  target_link_libraries(radapter_lua INTERFACE ${LUA_JIT_STATIC_LIBRARIES})
  target_link_directories(radapter_lua INTERFACE ${LUA_JIT_LIBRARY_DIRS})
  target_compile_definitions(radapter_lua INTERFACE RADAPTER_JIT)
else()
  CPMAddPackage(NAME lua
    URL https://www.lua.org/ftp/lua-5.4.6.tar.gz
    URL_HASH SHA256=7d5ea1b9cb6aa0b59ca3dde1c6adcb57ef83a1ba8e5432c0ecd06bf439b3ad88 
    VERSION 5.4.6
    DOWNLOAD_ONLY YES
  )
  set(lua_src_dir ${lua_SOURCE_DIR})
  file(GLOB LUA_SRC_FILES CONFIGURE_DEPENDS ${lua_src_dir}/src/*.c)
  list(REMOVE_ITEM LUA_SRC_FILES "${lua_src_dir}/src/lua.c" "${lua_src_dir}/src/luac.c")
  add_library(radapter_lua STATIC ${LUA_SRC_FILES})
  target_include_system_directories(radapter_lua PUBLIC ${lua_src_dir}/src)
  if (UNIX)
      target_compile_definitions(radapter_lua PRIVATE LUA_USE_POSIX)
  endif()
endif()

