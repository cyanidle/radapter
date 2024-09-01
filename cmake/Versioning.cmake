function(get_build_identity PREFIX)
    # define in parent scope variables: PREFIX_DAY_OF_CONFIGURE
    # PREFIX_SHORT_HASH_COMMIT
    string(TIMESTAMP day_of_build "%Y-%m-%d" UTC)
    set(${PREFIX}_DAY_OF_CONFIGURE "${day_of_build}" PARENT_SCOPE)

    find_package(Git QUIET)

    if(GIT_FOUND AND EXISTS "${PROJECT_SOURCE_DIR}/.git")
        execute_process(COMMAND ${GIT_EXECUTABLE} rev-parse --short --verify HEAD
            WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
            RESULT_VARIABLE git_result
            OUTPUT_VARIABLE short_hash_commit
            OUTPUT_STRIP_TRAILING_WHITESPACE)

        if("${git_result}" STREQUAL "0")
            set(${PREFIX}_SHORT_HASH_COMMIT "${short_hash_commit}" PARENT_SCOPE)
        endif()
    endif()
endfunction()

function(get_host_info PREFIX)
    if(UNIX)
        file(STRINGS /etc/os-release content)

        foreach(var IN LISTS content)
            string(FIND "${var}" "=" eq_idx)
            string(SUBSTRING "${var}" 0 ${eq_idx} name)
            math(EXPR eq_idx "${eq_idx} + 1")
            string(SUBSTRING "${var}" ${eq_idx} -1 value)
            string(REGEX REPLACE "^\"|\"$" "" cleared_value ${value})
            set(var_name "${PREFIX}_${name}")
            set(${var_name} "${cleared_value}" PARENT_SCOPE)
            list(APPEND vars "${var_name}")
        endforeach()
    endif()

    set(${PREFIX} "${vars}" PARENT_SCOPE)
endfunction()
