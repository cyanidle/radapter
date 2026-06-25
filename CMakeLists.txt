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
set(CMAKE_C_STANDARD 11)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)
set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)

option(RADAPTER_JIT "Use LuaJIT runtime instead of PUC Rio LUA" OFF)
option(RADAPTER_JIT_STATIC "Link LuaJIT statically if JIT is enabled" OFF)
option(RADAPTER_GUI "Enable GUI capabilities" ON)
option(RADAPTER_STATIC "Compile SDK as static lib: disables plugins" OFF)
option(RADAPTER_ROS2 "Develop ROS2 plugin" OFF)

if (RADAPTER_JIT_STATIC)
    set(RADAPTER_JIT ON)
endif()

set(RADAPTER_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(RADAPTER_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

find_package(Qt5 REQUIRED COMPONENTS Core WebSockets Network SerialPort SerialBus Sql)

if (RADAPTER_GUI)
    # Widgets: --gui runs on a QApplication (QtCharts' QML module needs it). The QtCharts
    # QML plugin loads at runtime from the system qml module — no C++ link required.
    find_package(Qt5 REQUIRED COMPONENTS Gui Qml Quick Widgets)
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
configure_file(cmake/radapter_info.hpp.in ${CMAKE_BINARY_DIR}/_info/radapter_info.hpp)

file(GLOB_RECURSE RADAPTER_SRC CONFIGURE_DEPENDS src/*.cpp)
file(GLOB_RECURSE RADAPTER_HEADERS CONFIGURE_DEPENDS src/*.h src/*.hpp include/*.hpp)

if (NOT RADAPTER_GUI)
    list(FILTER RADAPTER_SRC EXCLUDE REGEX src/workers/gui/)
    list(FILTER RADAPTER_HEADERS EXCLUDE REGEX src/workers/gui/)
endif()

file(GLOB_RECURSE RADAPTER_SCRIPTS CONFIGURE_DEPENDS src/scripts/*.lua)

file(CONFIGURE OUTPUT ${CMAKE_BINARY_DIR}/_info/_embedded_scripts.h @ONLY CONTENT [[
    constexpr std::string_view _embed_path = "@CMAKE_SOURCE_DIR@/src/scripts/";
]])

function(radapter_scripts)
    set(scripts_dir ${CMAKE_CURRENT_BINARY_DIR}/_scripts)
    set(scripts_qrc ${scripts_dir}/radapter.qrc)

    file(WRITE ${scripts_qrc} "<RCC>\n")
    file(APPEND ${scripts_qrc} "<qresource prefix=\"/scripts\">\n")

    foreach(file ${ARGN})
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

    qt5_add_resources(RADAPTER_QRC ${scripts_qrc}
        OPTIONS --compress 9 --threshold 70
    )
    set(RADAPTER_QRC ${RADAPTER_QRC} PARENT_SCOPE)
endfunction()

radapter_scripts(${RADAPTER_SCRIPTS})

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

if (MSVC)
    target_compile_options(radapter-sdk PRIVATE /Zc:__cplusplus)
else()
    target_compile_options(radapter-sdk PRIVATE -Wno-old-style-cast -Wno-nested-anon-types)
endif()

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
    target_link_libraries(radapter-qt INTERFACE Qt5::Gui Qt5::Qml Qt5::Quick Qt5::Widgets)
endif()

target_link_libraries(radapter-sdk PUBLIC 
    radapter-qt
    glua
    radapter-lua
    describe
    fmt
    libcxxcanard)

if (CMAKE_BUILD_TYPE MATCHES "Debug")
    target_compile_definitions(radapter-sdk PUBLIC QT_QML_DEBUG)
endif()

install(TARGETS radapter-sdk EXPORT radapter-config)
install(DIRECTORY include/radapter DESTINATION include)
install(FILES ${CMAKE_BINARY_DIR}/_info/radapter_info.hpp DESTINATION include/radapter)

function(create_radapter_plugin name)
    set(options)
    set(args)
    set(multi_args FILES)
    cmake_parse_arguments(PARSE_ARGV 0 ARG "${options}" "${args}" "${multi_args}")

    add_library(${name} SHARED ${ARG_FILES})
    target_link_libraries(${name} PUBLIC radapter-sdk)
endfunction()

if (NOT RADAPTER_SDK_ONLY)
    add_executable(radapter ./app/main.cpp)
    target_link_libraries(radapter PRIVATE
        qctrl
        argparse
        efsw-static
        radapter-sdk)
    install(TARGETS radapter EXPORT radapter-config)

    create_radapter_plugin(radapter_test_plugin
        FILES plugins/test.cpp
    )
endif()

if (RADAPTER_ROS2)
    add_subdirectory(plugins/ros)
endif()


enable_testing()

file(GLOB RADAPTER_TESTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.lua")

foreach(test_file ${RADAPTER_TESTS})
    get_filename_component(test_name ${test_file} NAME_WE)
    set(test_argv "")
    if(test_name STREQUAL "tags")
        list(APPEND test_argv "--tags")
    endif()
    list(APPEND test_argv "${test_file}")
    add_test(
        NAME "${test_name}"
        COMMAND radapter ${test_argv}
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/tests"
    )
    # Per-test timeouts (seconds) — generous upper bound past the self-imposed deadline
    if(test_name STREQUAL "basic")
        set_tests_properties("${test_name}" PROPERTIES TIMEOUT 10)
    elseif(test_name STREQUAL "http")
        set_tests_properties("${test_name}" PROPERTIES TIMEOUT 20)
    elseif(test_name STREQUAL "local")
        set_tests_properties("${test_name}" PROPERTIES TIMEOUT 10)
    elseif(test_name STREQUAL "modbus_loopback")
        set_tests_properties("${test_name}" PROPERTIES TIMEOUT 15)
    elseif(test_name STREQUAL "process")
        set_tests_properties("${test_name}" PROPERTIES TIMEOUT 10)
    elseif(test_name STREQUAL "smoke")
        set_tests_properties("${test_name}" PROPERTIES TIMEOUT 20)
    elseif(test_name STREQUAL "stdio")
        set_tests_properties("${test_name}" PROPERTIES TIMEOUT 15)
    elseif(test_name STREQUAL "tags")
        set_tests_properties("${test_name}" PROPERTIES TIMEOUT 15)
    endif()
endforeach()

include(cmake/Packaging.cmake)