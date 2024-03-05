function(include_bin name file)
    if(NOT name)
        message(FATAL_ERROR "TARGET argument required")
    endif()
    _add_incbin()
    string(MAKE_C_IDENTIFIER ${name} name)
    set(_helper_header ${CMAKE_BINARY_DIR}/_inc/_incbin_header.h)
    set(impl_dir ${CMAKE_BINARY_DIR}/_inc/_${name})
    set(impl_cpp ${impl_dir}/${name}.cpp)
    set(impl_hpp ${impl_dir}/${name}.hpp)
    if (NOT EXISTS ${_helper_header})
        file(WRITE ${_helper_header} ${incbin_h})
    endif()
    add_library(${name} STATIC)
    file(WRITE ${impl_cpp} [[
        #include "${_helper_header}"
        #include <string_view>
        INCBIN(${name}, "${file}");
        std::string_view ${name}() noexcept {
            return {g${name}Data, g${name}Size}
        }
    ]])
    file(WRITE ${impl_hpp} [[
        #ifndef ${name}_HPP
        #define ${name}_HPP
        #include <string_view>
        std::string_view ${name}() noexcept;
        #endif //${name}_HPP
    ]])
    target_sources(${name} PRIVATE ${impl_cpp})
    set_property(SOURCE ${impl_cpp} APPEND PROPERTY OBJECT_DEPENDS ${file})
    target_include_directories(${name} INTERFACE ${impl_dir})
    if(MSVC) 
        if (NOT TARGET _incbin_helper)
            file(WRITE ${CMAKE_BINARY_DIR}/_incbin_helper.c ${incbin_c})
            add_executable(_incbin_helper ${incbin_c})
        endif()
        get_property(_runtime_dir TARGET _incbin_helper PROPERTY RUNTIME_OUTPUT_DIRECTORY)
        if(_runtime_dir)
            if (CMAKE_GENERATOR MATCHES "Visual Studio")
                set(_incbin_exec ${_runtime_dir}/${CMAKE_BUILD_TYPE}/_incbin_helper)
            else()
                set(_incbin_exec ${_runtime_dir}/_incbin_helper)
            endif()
        else()
            get_property(_bindir TARGET _incbin_helper PROPERTY BINARY_DIR)
            set(_incbin_exec ${_bindir}/_incbin_helper)
        endif()
        add_custom_command(
            OUTPUT ${impl_dir}/data.c
            COMMAND ${_incbin_exec} ${file} -o ${impl_dir}/data.c
            DEPENDS ${file} _incbin_helper
            COMMENT "Including Binary File: ${file} => ${name}"
            VERBATIM
        )
        target_sources(${name} PRIVATE ${impl_dir}/data.c)
    endif()
endfunction()


#this macro creates two variables: 'incbin_h' and 'incbin_c'
macro(_add_incbin)
set(incbin_h [[/**
* @file incbin.h
* @author Dale Weiler
* @brief Utility for including binary files
*
* Facilities for including binary files into the current translation unit and
* making use from them externally in other translation units.
*/
#ifndef INCBIN_HDR
#define INCBIN_HDR
#include <limits.h>
#if   defined(__AVX512BW__) || \
    defined(__AVX512CD__) || \
    defined(__AVX512DQ__) || \
    defined(__AVX512ER__) || \
    defined(__AVX512PF__) || \
    defined(__AVX512VL__) || \
    defined(__AVX512F__)
# define INCBIN_ALIGNMENT_INDEX 6
#elif defined(__AVX__)      || \
    defined(__AVX2__)
# define INCBIN_ALIGNMENT_INDEX 5
#elif defined(__SSE__)      || \
    defined(__SSE2__)     || \
    defined(__SSE3__)     || \
    defined(__SSSE3__)    || \
    defined(__SSE4_1__)   || \
    defined(__SSE4_2__)   || \
    defined(__neon__)     || \
    defined(__ARM_NEON)   || \
    defined(__ALTIVEC__)
# define INCBIN_ALIGNMENT_INDEX 4
#elif ULONG_MAX != 0xffffffffu
# define INCBIN_ALIGNMENT_INDEX 3
# else
# define INCBIN_ALIGNMENT_INDEX 2
#endif

/* Lookup table of (1 << n) where `n' is `INCBIN_ALIGNMENT_INDEX' */
#define INCBIN_ALIGN_SHIFT_0 1
#define INCBIN_ALIGN_SHIFT_1 2
#define INCBIN_ALIGN_SHIFT_2 4
#define INCBIN_ALIGN_SHIFT_3 8
#define INCBIN_ALIGN_SHIFT_4 16
#define INCBIN_ALIGN_SHIFT_5 32
#define INCBIN_ALIGN_SHIFT_6 64

/* Actual alignment value */
#define INCBIN_ALIGNMENT \
INCBIN_CONCATENATE( \
    INCBIN_CONCATENATE(INCBIN_ALIGN_SHIFT, _), \
    INCBIN_ALIGNMENT_INDEX)

/* Stringize */
#define INCBIN_STR(X) \
#X
#define INCBIN_STRINGIZE(X) \
INCBIN_STR(X)
/* Concatenate */
#define INCBIN_CAT(X, Y) \
X ## Y
#define INCBIN_CONCATENATE(X, Y) \
INCBIN_CAT(X, Y)
/* Deferred macro expansion */
#define INCBIN_EVAL(X) \
X
#define INCBIN_INVOKE(N, ...) \
INCBIN_EVAL(N(__VA_ARGS__))
/* Variable argument count for overloading by arity */
#define INCBIN_VA_ARG_COUNTER(_1, _2, _3, N, ...) N
#define INCBIN_VA_ARGC(...) INCBIN_VA_ARG_COUNTER(__VA_ARGS__, 3, 2, 1, 0)

/* Green Hills uses a different directive for including binary data */
#if defined(__ghs__)
#  if (__ghs_asm == 2)
#    define INCBIN_MACRO ".file"
/* Or consider the ".myrawdata" entry in the ld file */
#  else
#    define INCBIN_MACRO "\tINCBIN"
#  endif
#else
#  define INCBIN_MACRO ".incbin"
#endif

#ifndef _MSC_VER
#  define INCBIN_ALIGN \
__attribute__((aligned(INCBIN_ALIGNMENT)))
#else
#  define INCBIN_ALIGN __declspec(align(INCBIN_ALIGNMENT))
#endif

#if defined(__arm__) || /* GNU C and RealView */ \
defined(__arm) || /* Diab */ \
defined(_ARM) /* ImageCraft */
#  define INCBIN_ARM
#endif

#ifdef __GNUC__
/* Utilize .balign where supported */
#  define INCBIN_ALIGN_HOST ".balign " INCBIN_STRINGIZE(INCBIN_ALIGNMENT) "\n"
#  define INCBIN_ALIGN_BYTE ".balign 1\n"
#elif defined(INCBIN_ARM)
/*
* On arm assemblers, the alignment value is calculated as (1 << n) where `n' is
* the shift count. This is the value passed to `.align'
*/
#  define INCBIN_ALIGN_HOST ".align " INCBIN_STRINGIZE(INCBIN_ALIGNMENT_INDEX) "\n"
#  define INCBIN_ALIGN_BYTE ".align 0\n"
#else
/* We assume other inline assembler's treat `.align' as `.balign' */
#  define INCBIN_ALIGN_HOST ".align " INCBIN_STRINGIZE(INCBIN_ALIGNMENT) "\n"
#  define INCBIN_ALIGN_BYTE ".align 1\n"
#endif

/* INCBIN_CONST is used by incbin.c generated files */
#if defined(__cplusplus)
#  define INCBIN_EXTERNAL extern "C"
#  define INCBIN_CONST    extern const
#else
#  define INCBIN_EXTERNAL extern
#  define INCBIN_CONST    const
#endif

/**
* @brief Optionally override the linker section into which size and data is
* emitted.
* 
* @warning If you use this facility, you might have to deal with
* platform-specific linker output section naming on your own.
*/
#if !defined(INCBIN_OUTPUT_SECTION)
#  if defined(__APPLE__)
#    define INCBIN_OUTPUT_SECTION ".const_data"
#  else
#    define INCBIN_OUTPUT_SECTION ".rodata"
#  endif
#endif

/**
* @brief Optionally override the linker section into which data is emitted.
*
* @warning If you use this facility, you might have to deal with
* platform-specific linker output section naming on your own.
*/
#if !defined(INCBIN_OUTPUT_DATA_SECTION)
#  define INCBIN_OUTPUT_DATA_SECTION INCBIN_OUTPUT_SECTION
#endif

/**
* @brief Optionally override the linker section into which size is emitted.
*
* @warning If you use this facility, you might have to deal with
* platform-specific linker output section naming on your own.
* 
* @note This is useful for Harvard architectures where program memory cannot
* be directly read from the program without special instructions. With this you
* can chose to put the size variable in RAM rather than ROM.
*/
#if !defined(INCBIN_OUTPUT_SIZE_SECTION)
#  define INCBIN_OUTPUT_SIZE_SECTION INCBIN_OUTPUT_SECTION
#endif

#if defined(__APPLE__)
#  include "TargetConditionals.h"
#  if defined(TARGET_OS_IPHONE) && !defined(INCBIN_SILENCE_BITCODE_WARNING)
#    warning "incbin is incompatible with bitcode. Using the library will break upload to App Store if you have bitcode enabled. Add `#define INCBIN_SILENCE_BITCODE_WARNING` before including this header to silence this warning."
#  endif
/* The directives are different for Apple branded compilers */
#  define INCBIN_SECTION         INCBIN_OUTPUT_SECTION "\n"
#  define INCBIN_GLOBAL(NAME)    ".globl " INCBIN_MANGLE INCBIN_STRINGIZE(INCBIN_PREFIX) #NAME "\n"
#  define INCBIN_INT             ".long "
#  define INCBIN_MANGLE          "_"
#  define INCBIN_BYTE            ".byte "
#  define INCBIN_TYPE(...)
#else
#  define INCBIN_SECTION         ".section " INCBIN_OUTPUT_SECTION "\n"
#  define INCBIN_GLOBAL(NAME)    ".global " INCBIN_STRINGIZE(INCBIN_PREFIX) #NAME "\n"
#  if defined(__ghs__)
#    define INCBIN_INT           ".word "
#  else
#    define INCBIN_INT           ".int "
#  endif
#  if defined(__USER_LABEL_PREFIX__)
#    define INCBIN_MANGLE        INCBIN_STRINGIZE(__USER_LABEL_PREFIX__)
#  else
#    define INCBIN_MANGLE        ""
#  endif
#  if defined(INCBIN_ARM)
/* On arm assemblers, `@' is used as a line comment token */
#    define INCBIN_TYPE(NAME)    ".type " INCBIN_STRINGIZE(INCBIN_PREFIX) #NAME ", %object\n"
#  elif defined(__MINGW32__) || defined(__MINGW64__)
/* Mingw doesn't support this directive either */
#    define INCBIN_TYPE(NAME)
#  else
/* It's safe to use `@' on other architectures */
#    define INCBIN_TYPE(NAME)    ".type " INCBIN_STRINGIZE(INCBIN_PREFIX) #NAME ", @object\n"
#  endif
#  define INCBIN_BYTE            ".byte "
#endif

/* List of style types used for symbol names */
#define INCBIN_STYLE_CAMEL 0
#define INCBIN_STYLE_SNAKE 1

/**
* @brief Specify the prefix to use for symbol names.
*
* @note By default this is "g".
*
* @code
* #define INCBIN_PREFIX incbin
* #include "incbin.h"
* INCBIN(Foo, "foo.txt");
*
* // Now you have the following symbols instead:
* // const unsigned char incbinFoo<data>[];
* // const unsigned char *const incbinFoo<end>;
* // const unsigned int incbinFoo<size>;
* @endcode
*/
#if !defined(INCBIN_PREFIX)
#  define INCBIN_PREFIX g
#endif

/**
* @brief Specify the style used for symbol names.
*
* Possible options are
* - INCBIN_STYLE_CAMEL "CamelCase"
* - INCBIN_STYLE_SNAKE "snake_case"
*
* @note By default this is INCBIN_STYLE_CAMEL
*
* @code
* #define INCBIN_STYLE INCBIN_STYLE_SNAKE
* #include "incbin.h"
* INCBIN(foo, "foo.txt");
*
* // Now you have the following symbols:
* // const unsigned char <prefix>foo_data[];
* // const unsigned char *const <prefix>foo_end;
* // const unsigned int <prefix>foo_size;
* @endcode
*/
#if !defined(INCBIN_STYLE)
#  define INCBIN_STYLE INCBIN_STYLE_CAMEL
#endif

/* Style lookup tables */
#define INCBIN_STYLE_0_DATA Data
#define INCBIN_STYLE_0_END End
#define INCBIN_STYLE_0_SIZE Size
#define INCBIN_STYLE_1_DATA _data
#define INCBIN_STYLE_1_END _end
#define INCBIN_STYLE_1_SIZE _size

/* Style lookup: returning identifier */
#define INCBIN_STYLE_IDENT(TYPE) \
INCBIN_CONCATENATE( \
    INCBIN_STYLE_, \
    INCBIN_CONCATENATE( \
        INCBIN_EVAL(INCBIN_STYLE), \
        INCBIN_CONCATENATE(_, TYPE)))

/* Style lookup: returning string literal */
#define INCBIN_STYLE_STRING(TYPE) \
INCBIN_STRINGIZE( \
    INCBIN_STYLE_IDENT(TYPE)) \

/* Generate the global labels by indirectly invoking the macro with our style
* type and concatenating the name against them. */
#define INCBIN_GLOBAL_LABELS(NAME, TYPE) \
INCBIN_INVOKE( \
    INCBIN_GLOBAL, \
    INCBIN_CONCATENATE( \
        NAME, \
        INCBIN_INVOKE( \
            INCBIN_STYLE_IDENT, \
            TYPE))) \
INCBIN_INVOKE( \
    INCBIN_TYPE, \
    INCBIN_CONCATENATE( \
        NAME, \
        INCBIN_INVOKE( \
            INCBIN_STYLE_IDENT, \
            TYPE)))

/**
* @brief Externally reference binary data included in another translation unit.
*
* Produces three external symbols that reference the binary data included in
* another translation unit.
*
* The symbol names are a concatenation of `INCBIN_PREFIX' before *NAME*; with
* "Data", as well as "End" and "Size" after. An example is provided below.
*
* @param TYPE Optional array type. Omitting this picks a default of `unsigned char`.
* @param NAME The name given for the binary data
*
* @code
* INCBIN_EXTERN(Foo);
*
* // Now you have the following symbols:
* // extern const unsigned char <prefix>Foo<data>[];
* // extern const unsigned char *const <prefix>Foo<end>;
* // extern const unsigned int <prefix>Foo<size>;
* @endcode
* 
* You may specify a custom optional data type as well as the first argument.
* @code
* INCBIN_EXTERN(custom_type, Foo);
* 
* // Now you have the following symbols:
* // extern const custom_type <prefix>Foo<data>[];
* // extern const custom_type *const <prefix>Foo<end>;
* // extern const unsigned int <prefix>Foo<size>;
* @endcode
*/
#define INCBIN_EXTERN(...) \
INCBIN_CONCATENATE(INCBIN_EXTERN_, INCBIN_VA_ARGC(__VA_ARGS__))(__VA_ARGS__)
#define INCBIN_EXTERN_1(NAME, ...) \
INCBIN_EXTERN_2(unsigned char, NAME)
#define INCBIN_EXTERN_2(TYPE, NAME) \
INCBIN_EXTERNAL const INCBIN_ALIGN TYPE \
    INCBIN_CONCATENATE( \
        INCBIN_CONCATENATE(INCBIN_PREFIX, NAME), \
        INCBIN_STYLE_IDENT(DATA))[]; \
INCBIN_EXTERNAL const INCBIN_ALIGN TYPE *const \
INCBIN_CONCATENATE( \
    INCBIN_CONCATENATE(INCBIN_PREFIX, NAME), \
    INCBIN_STYLE_IDENT(END)); \
INCBIN_EXTERNAL const unsigned int \
    INCBIN_CONCATENATE( \
        INCBIN_CONCATENATE(INCBIN_PREFIX, NAME), \
        INCBIN_STYLE_IDENT(SIZE))

/**
* @brief Externally reference textual data included in another translation unit.
*
* Produces three external symbols that reference the textual data included in
* another translation unit.
*
* The symbol names are a concatenation of `INCBIN_PREFIX' before *NAME*; with
* "Data", as well as "End" and "Size" after. An example is provided below.
*
* @param NAME The name given for the textual data
*
* @code
* INCBIN_EXTERN(Foo);
*
* // Now you have the following symbols:
* // extern const char <prefix>Foo<data>[];
* // extern const char *const <prefix>Foo<end>;
* // extern const unsigned int <prefix>Foo<size>;
* @endcode
*/
#define INCTXT_EXTERN(NAME) \
INCBIN_EXTERN_2(char, NAME)

/**
* @brief Include a binary file into the current translation unit.
*
* Includes a binary file into the current translation unit, producing three symbols
* for objects that encode the data and size respectively.
*
* The symbol names are a concatenation of `INCBIN_PREFIX' before *NAME*; with
* "Data", as well as "End" and "Size" after. An example is provided below.
*
* @param TYPE Optional array type. Omitting this picks a default of `unsigned char`.
* @param NAME The name to associate with this binary data (as an identifier.)
* @param FILENAME The file to include (as a string literal.)
*
* @code
* INCBIN(Icon, "icon.png");
*
* // Now you have the following symbols:
* // const unsigned char <prefix>Icon<data>[];
* // const unsigned char *const <prefix>Icon<end>;
* // const unsigned int <prefix>Icon<size>;
* @endcode
* 
* You may specify a custom optional data type as well as the first argument.
* These macros are specialized by arity.
* @code
* INCBIN(custom_type, Icon, "icon.png");
*
* // Now you have the following symbols:
* // const custom_type <prefix>Icon<data>[];
* // const custom_type *const <prefix>Icon<end>;
* // const unsigned int <prefix>Icon<size>;
* @endcode
*
* @warning This must be used in global scope
* @warning The identifiers may be different if INCBIN_STYLE is not default
*
* To externally reference the data included by this in another translation unit
* please @see INCBIN_EXTERN.
*/
#ifdef _MSC_VER
#  define INCBIN(NAME, FILENAME) \
    INCBIN_EXTERN(NAME)
#else
#  define INCBIN(...) \
    INCBIN_CONCATENATE(INCBIN_, INCBIN_VA_ARGC(__VA_ARGS__))(__VA_ARGS__)
#  if defined(__GNUC__)
#    define INCBIN_1(...) _Pragma("GCC error \"Single argument INCBIN not allowed\"")
#  elif defined(__clang__)
#    define INCBIN_1(...) _Pragma("clang error \"Single argument INCBIN not allowed\"")
#  else
#    define INCBIN_1(...) /* Cannot do anything here */
#  endif
#  define INCBIN_2(NAME, FILENAME) \
    INCBIN_3(unsigned char, NAME, FILENAME)
#  define INCBIN_3(TYPE, NAME, FILENAME) INCBIN_COMMON(TYPE, NAME, FILENAME, /* No terminator for binary data */)
#  define INCBIN_COMMON(TYPE, NAME, FILENAME, TERMINATOR) \
__asm__(INCBIN_SECTION \
        INCBIN_GLOBAL_LABELS(NAME, DATA) \
        INCBIN_ALIGN_HOST \
        INCBIN_MANGLE INCBIN_STRINGIZE(INCBIN_PREFIX) #NAME INCBIN_STYLE_STRING(DATA) ":\n" \
        INCBIN_MACRO " \"" FILENAME "\"\n" \
            TERMINATOR \
        INCBIN_GLOBAL_LABELS(NAME, END) \
        INCBIN_ALIGN_BYTE \
        INCBIN_MANGLE INCBIN_STRINGIZE(INCBIN_PREFIX) #NAME INCBIN_STYLE_STRING(END) ":\n" \
            INCBIN_BYTE "1\n" \
        INCBIN_GLOBAL_LABELS(NAME, SIZE) \
        INCBIN_ALIGN_HOST \
        INCBIN_MANGLE INCBIN_STRINGIZE(INCBIN_PREFIX) #NAME INCBIN_STYLE_STRING(SIZE) ":\n" \
            INCBIN_INT INCBIN_MANGLE INCBIN_STRINGIZE(INCBIN_PREFIX) #NAME INCBIN_STYLE_STRING(END) " - " \
                        INCBIN_MANGLE INCBIN_STRINGIZE(INCBIN_PREFIX) #NAME INCBIN_STYLE_STRING(DATA) "\n" \
        INCBIN_ALIGN_HOST \
        ".text\n" \
); \
INCBIN_EXTERN(TYPE, NAME)
#endif

/**
* @brief Include a textual file into the current translation unit.
* 
* This behaves the same as INCBIN except it produces char compatible arrays
* and implicitly adds a null-terminator byte, thus the size of data included
* by this is one byte larger than that of INCBIN.
*
* Includes a textual file into the current translation unit, producing three
* symbols for objects that encode the data and size respectively.
*
* The symbol names are a concatenation of `INCBIN_PREFIX' before *NAME*; with
* "Data", as well as "End" and "Size" after. An example is provided below.
*
* @param NAME The name to associate with this binary data (as an identifier.)
* @param FILENAME The file to include (as a string literal.)
*
* @code
* INCTXT(Readme, "readme.txt");
*
* // Now you have the following symbols:
* // const char <prefix>Readme<data>[];
* // const char *const <prefix>Readme<end>;
* // const unsigned int <prefix>Readme<size>;
* @endcode
*
* @warning This must be used in global scope
* @warning The identifiers may be different if INCBIN_STYLE is not default
*
* To externally reference the data included by this in another translation unit
* please @see INCBIN_EXTERN.
*/
#if defined(_MSC_VER)
#  define INCTXT(NAME, FILENAME) \
    INCBIN_EXTERN(NAME)
#else
#  define INCTXT(NAME, FILENAME) \
    INCBIN_COMMON(char, NAME, FILENAME, INCBIN_BYTE "0\n")
#endif

#endif]])
set(incbin_c [[#ifdef _MSC_VER
#  define _CRT_SECURE_NO_WARNINGS
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <limits.h>

#ifndef PATH_MAX
#  define PATH_MAX 260
#endif

#define SEARCH_PATHS_MAX 64
#define FILE_PATHS_MAX 1024

static int fline(char **line, size_t *n, FILE *fp) {
    int chr;
    char *pos;
    if (!line || !n || !fp)
        return -1;
    if (!*line)
        if (!(*line = (char *)malloc((*n=64))))
            return -1;
    chr = *n;
    pos = *line;
    for (;;) {
        int c = fgetc(fp);
        if (chr < 2) {
            *n += (*n > 16) ? *n : 64;
            chr = *n + *line - pos;
            if (!(*line = (char *)realloc(*line,*n)))
                return -1;
            pos = *n - chr + *line;
        }
        if (ferror(fp))
            return -1;
        if (c == EOF) {
            if (pos == *line)
                return -1;
            else
                break;
        }
        *pos++ = c;
        chr--;
        if (c == '\n')
            break;
    }
    *pos = '\0';
    return pos - *line;
}

static FILE *open_file(const char *name, const char *mode, const char (*searches)[PATH_MAX], int count) {
    int i;
    for (i = 0; i < count; i++) {
        char buffer[FILENAME_MAX + PATH_MAX];
        FILE *fp;
#ifndef _MSC_VER
        snprintf(buffer, sizeof(buffer), "%s/%s", searches[i], name);
#else
        _snprintf(buffer, sizeof(buffer), "%s/%s", searches[i], name);
#endif
        if ((fp = fopen(buffer, mode)))
            return fp;
    }
    return !count ? fopen(name, mode) : NULL;
}

static int strcicmp(const char *s1, const char *s2) {
    const unsigned char *us1 = (const unsigned char *)s1,
                        *us2 = (const unsigned char *)s2;
    while (tolower(*us1) == tolower(*us2)) {
        if (*us1++ == '\0')
            return 0;
        us2++;
    }
    return tolower(*us1) - tolower(*us2);
}

/* styles */
enum { kCamel, kSnake };
/* identifiers */
enum { kData, kEnd, kSize };

static const char *styled(int style, int ident) {
    switch (style) {
    case kCamel:
        switch (ident) {
        case kData: return "Data";
        case kEnd: return "End";
        case kSize: return "Size";
        }
        break;
    case kSnake:
        switch (ident) {
        case kData: return "_data";
        case kEnd: return "_end";
        case kSize: return "_size";
        }
        break;
    }
    return "";
}

int main(int argc, char **argv) {
    int ret = 0, i, paths, files = 0, style = kCamel;
    char outfile[FILENAME_MAX] = "data.c";
    char search_paths[SEARCH_PATHS_MAX][PATH_MAX];
    char prefix[FILENAME_MAX] = "g";
    char file_paths[FILE_PATHS_MAX][PATH_MAX];
    FILE *out = NULL;

    argc--;
    argv++;

    #define s(IDENT) styled(style, IDENT)

    if (argc == 0) {
usage:
        fprintf(stderr, "%s [-help] [-Ipath...] | <sourcefiles> | [-o output] | [-p prefix]\n", argv[-1]);
        fprintf(stderr, "   -o         - output file [default is \"data.c\"]\n");
        fprintf(stderr, "   -p         - specify a prefix for symbol names (default is \"g\")\n");
        fprintf(stderr, "   -S<style>  - specify a style for symbol generation (default is \"camelcase\")\n");
        fprintf(stderr, "   -I<path>   - specify an include path for the tool to use\n");
        fprintf(stderr, "   -help      - this\n");
        fprintf(stderr, "example:\n");
        fprintf(stderr, "   %s source.c other_source.cpp -o file.c\n", argv[-1]);
        fprintf(stderr, "styles (for -S):\n");
        fprintf(stderr, "   camelcase\n");
        fprintf(stderr, "   snakecase\n");
        return 1;
    }

    for (i = 0, paths = 0; i < argc; i++) {
        if (!strcmp(argv[i], "-o")) {
            if (i + 1 < argc) {
                strcpy(outfile, argv[i + 1]);
                memmove(argv+i+1, argv+i+2, (argc-i-2) * sizeof *argv);
                argc--;
                continue;
            }
        } else if (!strcmp(argv[i], "-p")) {
            /* supports "-p" with no prefix as well as
             * "-p -" which is another way of saying "no prefix"
             * and "-p <prefix>" for an actual prefix.
             */
            if (argv[i+1][0] == '-') {
                prefix[0] = '\0';
                /* is it just a -? */
                if (i + 1 < argc && !strcmp(argv[i+1], "-")) {
                    memmove(argv+i+1, argv+i+2, (argc-i-2) * sizeof *argv);
                    argc--;
                }
                continue;
            }
            strcpy(prefix, argv[i + 1]);
            memmove(argv+i+1, argv+i+2, (argc-i-2) * sizeof *argv);
            argc--;
            continue;
        } else if (!strncmp(argv[i], "-I", 2)) {
            char *name = argv[i] + 2; /* skip "-I"; */
            if (paths >= SEARCH_PATHS_MAX) {
                fprintf(stderr, "maximum search paths exceeded\n");
                return 1;
            }
            strcpy(search_paths[paths++], name);
            continue;
        } else if (!strncmp(argv[i], "-S", 2)) {
            char *name = argv[i] + 2; /* skip "-S"; */
            if (!strcicmp(name, "camel") || !strcicmp(name, "camelcase"))
                style = kCamel;
            else if (!strcicmp(name, "snake") || !strcicmp(name, "snakecase"))
                style = kSnake;
            else
                goto usage;
            continue;
        } else if (!strcmp(argv[i], "-help")) {
            goto usage;
        } else {
            if (files >= sizeof file_paths / sizeof *file_paths) {
                fprintf(stderr, "maximum file paths exceeded\n");
                return 1;
            }
            strcpy(file_paths[files++], argv[i]);
        }
    }

    if (!(out = fopen(outfile, "w"))) {
        fprintf(stderr, "failed to open `%s' for output\n", outfile);
        return 1;
    }

    fprintf(out, "/* File automatically generated by incbin */\n");
    /* Be sure to define the prefix if we're not using the default */
    if (strcmp(prefix, "g"))
        fprintf(out, "#define INCBIN_PREFIX %s\n", prefix);
    if (style != 0)
        fprintf(out, "#define INCBIN_STYLE INCBIN_STYLE_SNAKE\n");
    fprintf(out, "#include \"incbin.h\"\n\n");
    fprintf(out, "#ifdef __cplusplus\n");
    fprintf(out, "extern \"C\" {\n");
    fprintf(out, "#endif\n\n");

    for (i = 0; i < files; i++) {
        FILE *fp = open_file(file_paths[i], "r", search_paths, paths);
        char *line = NULL;
        size_t size = 0;
        if (!fp) {
            fprintf(stderr, "failed to open `%s' for reading\n", file_paths[i]);
            fclose(out);
            return 1;
        }
        while (fline(&line, &size, fp) != -1) {
            char *inc, *beg, *sep, *end, *name, *file;
            FILE *f;
            if (!(inc = strstr(line, "INCBIN"))) continue;
            if (!(beg = strchr(inc, '(')))       continue;
            if (!(sep = strchr(beg, ',')))       continue;
            if (!(end = strchr(sep, ')')))       continue;
            name = beg + 1;
            file = sep + 1;
            while (isspace(*name)) name++;
            while (isspace(*file)) file++;
            sep--;
            while (isspace(*sep)) sep--;
            *++sep = '\0';
            end--;
            while (isspace(*end)) end--;
            *++end = '\0';
            fprintf(out, "/* INCBIN(%s, %s); */\n", name, file);
            fprintf(out, "INCBIN_CONST INCBIN_ALIGN unsigned char %s%s%s[] = {\n    ", prefix, name, s(kData));
            *--end = '\0';
            file++;
            if (!(f = open_file(file, "rb", search_paths, paths))) {
                fprintf(stderr, "failed to include data `%s'\n", file);
                ret = 1;
                goto end;
            } else {
                long size, i;
                unsigned char *data, count = 0;
                fseek(f, 0, SEEK_END);
                size = ftell(f);
                fseek(f, 0, SEEK_SET);
                if (!(data = (unsigned char *)malloc(size))) {
                    fprintf(stderr, "out of memory\n");
                    fclose(f);
                    ret = 1;
                    goto end;
                }
                if (fread(data, size, 1, f) != 1) {
                    fprintf(stderr, "failed reading include data `%s'\n", file);
                    free(data);
                    fclose(f);
                    ret = 1;
                    goto end;
                }
                for (i = 0; i < size; i++) {
                    if (count == 12) {
                        fprintf(out, "\n    ");
                        count = 0;
                    }
                    fprintf(out, i != size - 1 ? "0x%02X, " : "0x%02X", data[i]);
                    count++;
                }
                free(data);
                fclose(f);
            }
            fprintf(out, "\n};\n");
            fprintf(out, "INCBIN_CONST INCBIN_ALIGN unsigned char *const %s%s%s = %s%s%s + sizeof(%s%s%s);\n", prefix, name, s(kEnd), prefix, name, s(kData), prefix, name, s(kData));
            fprintf(out, "INCBIN_CONST unsigned int %s%s%s = sizeof(%s%s%s);\n", prefix, name, s(kSize), prefix, name, s(kData));
        }
end:
        free(line);
        fclose(fp);
        printf("included `%s'\n", file_paths[i]);
    }

    if (ret == 0) {
        fprintf(out, "\n#ifdef __cplusplus\n");
        fprintf(out, "}\n");
        fprintf(out, "#endif\n");
        fclose(out);
        printf("generated `%s'\n", outfile);
        return 0;
    }

    fclose(out);
    remove(outfile);
    return 1;
}
]])
endmacro()
