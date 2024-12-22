# from here: https://github.com/lefticus/cppbestpractices/blob/master/02-Use_the_Tools_Available.md

function(set_target_warnings target_name LINK_TYPE WARNING_AS_ERROR)
    set(CLANG_WARNINGS -Wall                       # reasonable and standard
                       -Wextra                     # same
                       -Wshadow                    # warn if shadow decalration
                       -Wno-gnu-zero-variadic-macro-arguments
                       -Wpedantic                  # warn on language extansions
                       -Wnon-virtual-dtor          # warn if a class with virtual functions has a non-virtual dtor
                       -Wold-style-cast            # warn for c-style cast
                       -Wcast-align                # warn for potantial perforamnce problem cast
                       -Wunused                    # warn on anything being unused
                       -Woverloaded-virtual        # warn if u overload (not override) a virtual function
                       -Wconversion                # warn on type conversion that may lose data
                       -Wsign-conversion           # warn on sign conversion
                       -Wnull-dereference          # warn if a null dereference is detected
                       -Wdouble-promotion          # warn if float is implicit promoted to double
                       -Wformat=2                  # warn on security issue around functions that format output (ie printf)
                       -Wimplicit-fallthrough)     # warn on statements that fallthrough without an explicit annotation
  
    set(GCC_WARNINGS ${CLANG_WARNINGS}
                     -Wmisleading-indentation      # warn if indent implies blocks where blocks do not exists
                     -Wduplicated-cond             # warn if if/else chain has duplicate
                     -Wduplicated-branches         # warn if if/else branches has duplicate code
                     -Wlogical-op                  # warn about logical op being used where bitwise were probably used
                     #-Wuseless-cast                # warn if u perform a cast to the same type
                 )
  
    set(MSVC_WARNINGS /W4                          # all resonable warnings
                      /w14242                      # conversion from 'type1' to 'type1', possible loss of data
                      /w14254                      # conversion from 'type1:field_bits' to 'type2:field_bits', possible loss of data
                      /w14263                      # member function does not override any base class virtual member function
                      /w14265                      # class has virtual functions, but destructor is not virtual instances of this class may not be destructed correctly
                      /w14287                      # unsigned/negative constant mismatch
                      /we4289                      # loop control variable declared in the for-loop is used outside the for-loop scope
                      /w14296                      # expression is always 'boolean_value'
                      /w14311                      # pointer truncation from 'type1' to 'type2'
                      /w14545                      # expression before comma evaluates to a function which is missing an argument list
                      /w14546                      # function call before comma missing argument list
                      /w14547                      # operator before comma has no effect; expected operator with side-effect
                      /w14549                      # operator before comma has no effect; did you intend 'operator'?
                      /w14555                      # expression has no effect; expected expression with side-effect
                      /w14619                      # pragma warning: there is no warning number 'number'
                      /w14640                      # enable warning on thread unsafe static member initialization
                      /w14826                      # conversion from 'type1' to 'type_2' is sign-extended. This may cause unexpected runtime behavior.
                      /w14905                      # wide string literal cast to 'LPSTR'
                      /w14906                      # string literal cast to 'LPWSTR'
                      /w14928)                     # illegal copy-initialization; more than one user-defined conversion has been implicitly applied
  
  if(WARNING_AS_ERROR)
    message(STATUS "Warnings treat as error.")
    list(APPEND CLANG_WARNINGS -Werror)
    list(APPEND GCC_WARNINGS   -Werror)
    list(APPEND MSVC_WARNINGS  /WX)
  endif()
  
  if(MSVC)
    set(TARGET_WARNINGS_CXX ${MSVC_WARNINGS})
  elseif(CMAKE_CXX_COMPILER_ID MATCHES ".*Clang")
    set(TARGET_WARNINGS_CXX ${CLANG_WARNINGS})
  elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(TARGET_WARNINGS_CXX ${GCC_WARNINGS})
  else()
    message(AUTHOR_WARNING "Not compiler warnings set for C/CXX compiler: ${CMAKE_CXX_COMPILER_ID}.")
  endif()
  
  set(TARGET_WARNINGS_C "${TARGET_WARNINGS_CXX}")
  
  target_compile_options(${target_name} ${LINK_TYPE} $<$<COMPILE_LANGUAGE:CXX>:${TARGET_WARNINGS_CXX}>
                                                  $<$<COMPILE_LANGUAGE:C>:${TARGET_WARNINGS_C}>)
  endfunction()
  
