cmake_minimum_required(VERSION 3.16)

file(REMOVE_RECURSE "${TEST_DIR}")
file(MAKE_DIRECTORY "${TEST_DIR}/bin/plugins" "${TEST_DIR}/cwd"
    "${TEST_DIR}/env" "${TEST_DIR}/configured" "${TEST_DIR}/system")
file(COPY "${RADAPTER}" "${SDK}" DESTINATION "${TEST_DIR}/bin")
file(COPY "${CMAKE_CURRENT_LIST_DIR}/check.lua" DESTINATION "${TEST_DIR}/cwd")
get_filename_component(exe_name "${RADAPTER}" NAME)
set(exe "${TEST_DIR}/bin/${exe_name}")
set(script "${TEST_DIR}/cwd/check.lua")
set(env_path "${TEST_DIR}/env")

function(copy_plugin dir name)
    configure_file("${PLUGIN}" "${dir}/${PREFIX}${name}${SUFFIX}" COPYONLY)
endfunction()

function(check mode name resolved)
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E env "QT_PLUGIN_PATH=${env_path}"
            ${sandbox} "${exe}" "${script}" "${mode}" "${name}"
            "${TEST_DIR}/bin" "${resolved}"
        WORKING_DIRECTORY "${TEST_DIR}/cwd"
        RESULT_VARIABLE result OUTPUT_VARIABLE out ERROR_VARIABLE err TIMEOUT 5)
    if (NOT result STREQUAL "0" OR NOT out MATCHES "plugin-search check OK")
        message(FATAL_ERROR "${mode} ${name}: ${result}\n${out}\n${err}")
    endif()
    message(STATUS "PASS: ${mode} ${name}")
endfunction()

copy_plugin("${TEST_DIR}/bin" "search_beside")
copy_plugin("${TEST_DIR}/bin/plugins" "search_subdir")
copy_plugin("${TEST_DIR}/env" "search_env")
copy_plugin("${TEST_DIR}/configured" "search_configured")

check(load search_beside "${TEST_DIR}/bin/${PREFIX}search_beside${SUFFIX}")
check(load search_subdir "${TEST_DIR}/bin/plugins/${PREFIX}search_subdir${SUFFIX}")
check(load "${PREFIX}search_subdir${SUFFIX}" "${TEST_DIR}/bin/plugins/${PREFIX}search_subdir${SUFFIX}")
check(load search_env "${TEST_DIR}/env/${PREFIX}search_env${SUFFIX}")
check(load "${TEST_DIR}/bin/plugins/search_subdir"
    "${TEST_DIR}/bin/plugins/${PREFIX}search_subdir${SUFFIX}")
check(load "../bin/plugins/search_subdir"
    "${TEST_DIR}/bin/plugins/${PREFIX}search_subdir${SUFFIX}")
check(load "../bin/plugins/${PREFIX}search_subdir${SUFFIX}"
    "${TEST_DIR}/bin/plugins/${PREFIX}search_subdir${SUFFIX}")
check(missing radapter_plugin_search_missing "")
check(explicit_missing "./search_subdir" "")
check(explicit_missing "absent/search_subdir" "")
check(explicit_missing "${TEST_DIR}/cwd/search_subdir" "")

# A real library without Qt plugin metadata must retain its Qt diagnostic.
configure_file("${SDK}" "${TEST_DIR}/bin/plugins/${PREFIX}search_invalid${SUFFIX}" COPYONLY)
check(missing search_invalid "")

# qt.conf and QT_PLUGIN_PATH must still win over the new fallback directories.
copy_plugin("${TEST_DIR}/bin/plugins" "search_env")
copy_plugin("${TEST_DIR}/bin/plugins" "search_configured")
file(WRITE "${TEST_DIR}/bin/qt.conf" "[Paths]\nPlugins=../configured\n")
check(load search_configured "${TEST_DIR}/configured/${PREFIX}search_configured${SUFFIX}")
check(load search_env "${TEST_DIR}/env/${PREFIX}search_env${SUFFIX}")
file(REMOVE "${TEST_DIR}/bin/qt.conf")

# Exercise the literal system path in a private mount namespace, never /usr on
# the host. Other platforms and restricted CI still run all tests above.
find_program(BWRAP bwrap)
if (BWRAP)
    execute_process(COMMAND "${BWRAP}" --ro-bind / / -- true
        RESULT_VARIABLE sandbox_result OUTPUT_QUIET ERROR_QUIET TIMEOUT 5)
endif()
if (BWRAP AND sandbox_result STREQUAL "0")
    copy_plugin("${TEST_DIR}/system" "search_system")
    copy_plugin("${TEST_DIR}/bin/plugins" "search_system")
    set(sandbox "${BWRAP}" --ro-bind / / --tmpfs /usr/lib)
    file(GLOB system_libs "/usr/lib/*")
    foreach(lib ${system_libs})
        if (NOT lib STREQUAL "/usr/lib/radapter")
            list(APPEND sandbox --ro-bind "${lib}" "${lib}")
        endif()
    endforeach()
    list(APPEND sandbox --ro-bind "${TEST_DIR}/system" /usr/lib/radapter/plugins --)
    check(load search_system "/usr/lib/radapter/plugins/${PREFIX}search_system${SUFFIX}")
    check(load "${PREFIX}search_system${SUFFIX}"
        "/usr/lib/radapter/plugins/${PREFIX}search_system${SUFFIX}")
else()
    message(STATUS "SKIP: system-path loading requires working bubblewrap")
endif()
