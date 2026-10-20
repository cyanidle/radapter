cmake_minimum_required(VERSION 3.16)

function(check_exit expression expected)
    execute_process(
        COMMAND "${RADAPTER}" -e "${expression}"
        RESULT_VARIABLE result OUTPUT_QUIET ERROR_QUIET TIMEOUT 30)
    if (NOT result STREQUAL "${expected}")
        message(FATAL_ERROR "${expression}: expected exit ${expected}, got ${result}")
    endif()
    message(STATUS "PASS: ${expression} -> ${result}")
endfunction()

check_exit("os.exit(0)" "0")
check_exit("os.exit(3)" "3")
check_exit("os.exit(7)" "7")
check_exit("os.exit(true)" "0")
check_exit("os.exit(false)" "1")
