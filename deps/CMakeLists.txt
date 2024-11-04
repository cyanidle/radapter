CPMAddPackage("gh:cyanidle/describe@2.0")
CPMAddPackage("gh:p-ranav/argparse@3.1")
CPMAddPackage("gh:cyanidle/glua@1.1")

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
  add_library(radapter-lua INTERFACE)
  target_include_system_directories(radapter-lua INTERFACE ${LUA_JIT_INCLUDE_DIRS})
  target_link_libraries(radapter-lua INTERFACE ${LUA_JIT_STATIC_LIBRARIES})
  target_link_directories(radapter-lua INTERFACE ${LUA_JIT_LIBRARY_DIRS})
  target_compile_definitions(radapter-lua INTERFACE RADAPTER_JIT)
else()
  CPMAddPackage(NAME lua
    URL https://www.lua.org/ftp/lua-5.4.7.tar.gz
    URL_HASH SHA256=9fbf5e28ef86c69858f6d3d34eccc32e911c1a28b4120ff3e84aaa70cfbf1e30
    VERSION 5.4.7
    DOWNLOAD_ONLY YES
  )
  set(lua_src_dir ${lua_SOURCE_DIR})
  file(GLOB LUA_SRC_FILES CONFIGURE_DEPENDS ${lua_src_dir}/src/*.c)
  list(REMOVE_ITEM LUA_SRC_FILES "${lua_src_dir}/src/lua.c" "${lua_src_dir}/src/luac.c")
  add_library(radapter-lua STATIC ${LUA_SRC_FILES})
  target_include_system_directories(radapter-lua PUBLIC ${lua_src_dir}/src)
  target_compile_definitions(radapter-lua PRIVATE LUA_USE_DLOPEN)
  if (WIN32)
      CPMAddPackage("gh:dlfcn-win32/dlfcn-win32@1.4.1")
  endif()
  target_link_libraries(radapter-lua PUBLIC dl)
  if (UNIX)
      target_compile_definitions(radapter-lua PRIVATE LUA_USE_POSIX)
  endif()
  add_executable(radapter-luac EXCLUDE_FROM_ALL ${lua_SOURCE_DIR}/src/luac.c)
  target_link_libraries(radapter-luac PRIVATE radapter-lua m)
endif()


CPMAddPackage(NAME luasocket
  GITHUB_REPOSITORY lunarmodules/luasocket
  VERSION 3.1.0
  GIT_TAG v3.1.0
  GIT_SHALLOW YES
  DOWNLOAD_ONLY YES
)

set(LUASOCK_SRC
  src/luasocket.c
  src/timeout.c
  src/buffer.c
  src/io.c
  src/auxiliar.c
  src/options.c
  src/inet.c
  src/except.c
  src/select.c
  src/tcp.c
  src/udp.c
  src/compat.c
)
if (UNIX)
  list(APPEND LUASOCK_SRC src/usocket.c)
endif()
if (WIN32)
  list(APPEND LUASOCK_SRC src/wsocket.c)
endif()
list(TRANSFORM LUASOCK_SRC PREPEND ${luasocket_SOURCE_DIR}/)
add_library(radapter-luasock STATIC ${LUASOCK_SRC})
target_include_directories(radapter-luasock PRIVATE ${luasocket_SOURCE_DIR}/src)
target_link_libraries(radapter-luasock PRIVATE radapter-lua)
target_compile_definitions(radapter-luasock PRIVATE LUASOCKET_DEBUG)
if (WIN32)
    target_link_libraries(radapter-luasock PUBLIC ws2_32)
endif()
