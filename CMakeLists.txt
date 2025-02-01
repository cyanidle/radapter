cmake_minimum_required(VERSION 3.16)

project(radapter
    LANGUAGES C CXX
    VERSION 3.0
)

include(cmake/CPM.cmake)
include(cmake/warnings.cmake)
include(cmake/Versioning.cmake)

set(BUILD_SHARED_LIBS OFF)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(RADAPTER_JIT "Use LuaJIT runtime instead of PUC Rio LUA" OFF)
option(RADAPTER_JIT_STATIC "Link LuaJIT statically if JIT is enabled" OFF)
option(RADAPTER_GUI "Enable GUI capabilities" ON)
option(RADAPTER_STATIC "Compile SDK as static lib: disables plugins" OFF)
# RADAPTER_EXTRA_SCRIPTS may be a list of extra embedded scripts directories

set(RADAPTER_VERSION_MAJOR ${CMAKE_PROJECT_VERSION_MAJOR})
set(RADAPTER_VERSION_MINOR ${CMAKE_PROJECT_VERSION_MINOR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

find_package(Qt5 REQUIRED COMPONENTS Core WebSockets Network SerialPort SerialBus Sql)

if (RADAPTER_GUI)
    find_package(Qt5 REQUIRED COMPONENTS Gui Qml Quick)
endif()

add_subdirectory(deps)

set(CMAKE_AUTOMOC ON)

add_library(qctrl STATIC deps/qctrlsignals/qctrlsignalhandler.cpp)
target_include_directories(qctrl PUBLIC deps/qctrlsignals)
if (UNIX)
    target_sources(qctrl PRIVATE deps/qctrlsignals/qctrlsignalhandler_unix.cpp)
elseif(WIN32)
    target_sources(qctrl PRIVATE deps/qctrlsignals/qctrlsignalhandler_win.cpp)
endif()
target_link_libraries(qctrl PRIVATE Qt5::Core)
target_compile_definitions(qctrl PUBLIC Q_CTRL_SIGNALS_EXPORT=)

get_build_identity(RADAPTER)
configure_file(include/radapter_info.hpp.in ${CMAKE_BINARY_DIR}/_info/radapter_info.hpp)

file(GLOB_RECURSE RADAPTER_SRC CONFIGURE_DEPENDS src/*.cpp)
file(GLOB_RECURSE RADAPTER_HEADERS CONFIGURE_DEPENDS src/*.h src/*.hpp include/*.hpp)

if (NOT RADAPTER_GUI)
    list(FILTER RADAPTER_SRC EXCLUDE REGEX src/workers/gui/)
    list(FILTER RADAPTER_HEADERS EXCLUDE REGEX src/workers/gui/)
endif()

list(TRANSFORM RADAPTER_EXTRA_SCRIPTS APPEND /*.lua)
file(GLOB_RECURSE RADAPTER_SCRIPTS CONFIGURE_DEPENDS src/scripts/*.lua ${RADAPTER_EXTRA_SCRIPTS})

function(radapter_scripts scripts)
    set(scripts_dir ${CMAKE_CURRENT_BINARY_DIR}/_scripts)
    set(scripts_qrc ${scripts_dir}/radapter.qrc)

    file(WRITE ${scripts_qrc} "<RCC>\n")
    file(APPEND ${scripts_qrc} "<qresource prefix=\"/scripts\">\n")

    foreach(file ${${scripts}})
        get_filename_component(name ${file} NAME)
        message(STATUS "Embedding file => scripts/${name}")
        if (RADAPTER_JIT OR CMAKE_CROSSCOMPILING) #no precompile for JIT or Cross
            file(COPY ${file} DESTINATION ${scripts_dir})
        else()
            set(out ${scripts_dir}/${name})
            add_custom_command(
                OUTPUT ${out}
                DEPENDS radapter-luac ${file}
                COMMAND $<TARGET_FILE:radapter-luac>
                ARGS -o ${out} ${file}
                COMMENT "Precompiling => ${out}"
            )
        endif()
        file(APPEND ${scripts_qrc} "<file>${name}</file>\n")
    endforeach()

    file(APPEND ${scripts_qrc} "</qresource>\n")
    file(APPEND ${scripts_qrc} "</RCC>\n")

    qt5_add_resources(RADAPTER_QRC ${scripts_qrc})
    set(RADAPTER_QRC ${RADAPTER_QRC} PARENT_SCOPE)
endfunction()

radapter_scripts(RADAPTER_SCRIPTS)

set(RADAPTER_ALL_SRC ${RADAPTER_SRC} ${RADAPTER_TOPLEVEL_SRC} ${RADAPTER_HEADERS} ${RADAPTER_QRC})

if (RADAPTER_STATIC)
    add_library(radapter-sdk STATIC ${RADAPTER_ALL_SRC})
else()
    add_library(radapter-sdk SHARED ${RADAPTER_ALL_SRC})
endif()
set_target_warnings(radapter-sdk PRIVATE OFF)
target_compile_definitions(radapter-sdk PRIVATE RADAPTER_API=Q_DECL_EXPORT)
target_compile_definitions(radapter-sdk INTERFACE RADAPTER_API=Q_DECL_IMPORT)
target_include_directories(radapter-sdk PUBLIC include ${CMAKE_BINARY_DIR}/_info)
target_include_directories(radapter-sdk PRIVATE src)
target_link_system_libraries(radapter-sdk PRIVATE hiredis hiredis_ssl)

target_link_libraries(radapter-sdk PRIVATE
    rpcxx-future
    rpcxx-json
    radapter-luafs
    radapter-luasock)

add_library(radapter-qt INTERFACE)

target_link_libraries(radapter-qt INTERFACE
    Qt5::Core Qt5::SerialPort Qt5::SerialBus
    Qt5::Sql Qt5::WebSockets Qt5::Network
)
if (RADAPTER_GUI)
    target_link_libraries(radapter-qt INTERFACE Qt5::Gui Qt5::Qml Qt5::Quick)
endif()

target_link_libraries(radapter-sdk PUBLIC 
    radapter-qt
    glua
    radapter-lua
    describe
    fmt)

if (CMAKE_BUILD_TYPE MATCHES "Debug")
    target_compile_definitions(radapter-sdk PUBLIC QT_QML_DEBUG)
endif()

add_executable(radapter ./app/main.cpp)
target_link_libraries(radapter PRIVATE
    qctrl
    argparse
    efsw-static
    radapter-sdk)

function(create_radapter_plugin name)
    set(options)
    set(args)
    set(multi_args FILES)
    cmake_parse_arguments(PARSE_ARGV 0 ARG "${options}" "${args}" "${multi_args}")

    add_library(${name} SHARED ${ARG_FILES})
    target_link_libraries(${name} PUBLIC radapter-sdk)
endfunction()


create_radapter_plugin(radapter_test_plugin
    FILES plugins/test.cpp
)


install(TARGETS radapter radapter-sdk EXPORT radapter-config)
