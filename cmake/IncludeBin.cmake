function(include_bin name file)
    if(NOT name)
        message(FATAL_ERROR "TARGET argument required")
    endif()
    string(MAKE_C_IDENTIFIER ${name} name)
    set(impl_dir ${CMAKE_BINARY_DIR}/_inc/_${name})
    set(impl_cpp ${impl_dir}/${name}.cpp)
    set(impl_hpp ${impl_dir}/${name}.hpp)
    set(data g${name}Data)
    set(dataEnd g${name}DataEnd)
    if (MSVC)
        set(impl_s ${impl_dir}/${name}.c)
        file(WRITE ${impl_s} "") #kostyl for autogen
        add_custom_command(
            OUTPUT ${impl_s}
            DEPENDS ${file}
            COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/IncludeBin.cmake
                -D_INC_SELF_RUN_IN=${file}
                -D_INC_SELF_RUN_OUT=${impl_s}
                -D_INC_SELF_NAME=${name}
            COMMENT "Converting to bin: ${file} => ${impl_s}"
            VERBATIM
        )
    else()
        set(impl_s ${impl_dir}/${name}.s)
        file(WRITE ${impl_s} "
            .global ${data}
            .type ${data}, @object
            .global ${dataEnd}
            .type ${dataEnd}, @object
            
            .section .rodata
            .balign 64
            ${data}:
                .incbin \"${file}\"
                .balign 1
            ${dataEnd}:
                .byte 0
        ")
    endif()
    file(WRITE ${impl_cpp} "
        #include <string_view>
        extern \"C\" const char* ${data};
        extern \"C\" const char* ${dataEnd};
        std::string_view ${name}() noexcept {
            return {${data}, size_t(${dataEnd} - ${data})};
        }
        ")
    file(WRITE ${impl_hpp} "
        #ifndef ${name}_HPP
        #define ${name}_HPP
        #include <string_view>
        std::string_view ${name}() noexcept;
        #endif //${name}_HPP
    ")
    add_library(${name} STATIC ${impl_cpp} ${impl_s})
    set_property(SOURCE ${impl_s} PROPERTY SKIP_AUTOMOC ON)
    target_include_directories(${name} INTERFACE ${impl_dir})
endfunction()

function(_self_run in out name)
    file(READ ${in} src HEX)
    string(REGEX MATCHALL "([A-Fa-f0-9][A-Fa-f0-9])" src ${src})
    list(LENGTH src len)
    if (NOT len)
        message(FATAL_ERROR "Cannot include empty file")
    endif()
    list(JOIN src ", 0x" src)
    if (src)
        set(src 0x${src})
    endif()
    file(WRITE ${out} "
        const char g${name}Data[${len}] = {${src}};
        const char* g${name}DataEnd = ${data} + ${len};
    ")
    return()
endfunction()

if (_INC_SELF_RUN_IN AND _INC_SELF_RUN_OUT AND _INC_SELF_NAME)
    _self_run(${_INC_SELF_RUN_IN} ${_INC_SELF_RUN_OUT} ${_INC_SELF_NAME})
endif()
