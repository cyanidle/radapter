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

option(RADAPTER_JIT "Use LuaJIT runtime instead of PUC Rio LUA" OFF)
# RADAPTER_EXTRA_SCRIPTS may be a list of extra embedded scripts directories

set(RADAPTER_VERSION_MAJOR ${CMAKE_PROJECT_VERSION_MAJOR})
set(RADAPTER_VERSION_MINOR ${CMAKE_PROJECT_VERSION_MINOR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

find_package(Qt5 REQUIRED COMPONENTS Core WebSockets Network SerialPort SerialBus Sql)
add_subdirectory(deps)

set(CMAKE_AUTOMOC ON)

add_library(qctrl STATIC deps/qctrlsignals/qctrlsignalhandler.cpp)
target_include_directories(qctrl PUBLIC deps/qctrlsignals)
if (UNIX)
    target_sources(qctrl PRIVATE deps/qctrlsignals/qctrlsignalhandler_unix.cpp)
elseif(WIN32)
    target_sources(qctrl PRIVATE deps/qctrlsignals/qctrlsignalhandler_win.cpp)
endif()
target_link_libraries(qctrl PRIVATE Qt::Core)
target_compile_definitions(qctrl PUBLIC Q_CTRL_SIGNALS_EXPORT=)

get_build_identity(RADAPTER)
configure_file(include/radapter_info.hpp.in ${CMAKE_BINARY_DIR}/_info/radapter_info.hpp)

file(GLOB_RECURSE RADAPTER_SRC CONFIGURE_DEPENDS src/*.cpp)
file(GLOB_RECURSE RADAPTER_HPP CONFIGURE_DEPENDS src/*.h src/*.hpp include/*.hpp)
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

add_library(radapter-sdk STATIC
    ${RADAPTER_SRC}
    ${RADAPTER_HPP}
    ${RADAPTER_QRC}
)
set_target_warnings(radapter-sdk PRIVATE OFF)
target_compile_definitions(radapter-sdk PUBLIC RADAPTER_API=)
target_include_directories(radapter-sdk PUBLIC include ${CMAKE_BINARY_DIR}/_info)
target_include_directories(radapter-sdk PRIVATE src)
target_link_system_libraries(radapter-sdk PRIVATE hiredis hiredis_ssl)

target_link_libraries(radapter-sdk PRIVATE
    rpcxx-future
    radapter-luasock)

target_link_libraries(radapter-sdk PUBLIC 
    Qt::Core
    Qt::SerialPort
    Qt::SerialBus
    Qt::Sql
    Qt::WebSockets
    Qt::Network
    radapter-lua
    glua
    describe
    fmt)

add_executable(radapter ./app/main.cpp)
target_link_libraries(radapter PRIVATE
    qctrl
    argparse
    radapter-sdk)
