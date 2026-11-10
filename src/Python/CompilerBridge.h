#pragma once
#include <stddef.h>

#if defined(_WIN32)
# if defined(LVUI_COMPILER_BRIDGE_BUILD)
#  define LVUI_COMPILER_EXPORT __declspec(dllexport)
# else
#  define LVUI_COMPILER_EXPORT __declspec(dllimport)
# endif
#else
# define LVUI_COMPILER_EXPORT __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
# define LVUI_COMPILER_NOEXCEPT noexcept
extern "C" {
#else
# define LVUI_COMPILER_NOEXCEPT
#endif

typedef struct lvui_compiler_bytes { const char* data; size_t size; } lvui_compiler_bytes;
typedef struct lvui_compiler_result lvui_compiler_result;
enum { LVUI_COMPILER_OK=0, LVUI_COMPILER_DIAGNOSTIC=1, LVUI_COMPILER_INVALID_ARGUMENT=2, LVUI_COMPILER_INTERNAL_ERROR=3 };
enum { LVUI_COMPILER_SOURCE=0, LVUI_COMPILER_STYLE=1, LVUI_COMPILER_HEADER=2 };

LVUI_COMPILER_EXPORT int lvui_compiler_abi_version(void) LVUI_COMPILER_NOEXCEPT;
LVUI_COMPILER_EXPORT lvui_compiler_result* lvui_compiler_compile(
    lvui_compiler_bytes source, lvui_compiler_bytes name, const lvui_compiler_bytes* includes,
    size_t include_count, const lvui_compiler_bytes* style_profile) LVUI_COMPILER_NOEXCEPT;
LVUI_COMPILER_EXPORT lvui_compiler_result* lvui_compiler_styles(
    lvui_compiler_bytes profile, lvui_compiler_bytes cpp_namespace) LVUI_COMPILER_NOEXCEPT;
LVUI_COMPILER_EXPORT int lvui_compiler_result_status(const lvui_compiler_result*) LVUI_COMPILER_NOEXCEPT;
LVUI_COMPILER_EXPORT int lvui_compiler_result_stage(const lvui_compiler_result*) LVUI_COMPILER_NOEXCEPT;
LVUI_COMPILER_EXPORT size_t lvui_compiler_result_offset(const lvui_compiler_result*) LVUI_COMPILER_NOEXCEPT;
LVUI_COMPILER_EXPORT size_t lvui_compiler_result_line(const lvui_compiler_result*) LVUI_COMPILER_NOEXCEPT;
LVUI_COMPILER_EXPORT size_t lvui_compiler_result_column(const lvui_compiler_result*) LVUI_COMPILER_NOEXCEPT;
LVUI_COMPILER_EXPORT const char* lvui_compiler_result_data(const lvui_compiler_result*) LVUI_COMPILER_NOEXCEPT;
LVUI_COMPILER_EXPORT size_t lvui_compiler_result_size(const lvui_compiler_result*) LVUI_COMPILER_NOEXCEPT;
LVUI_COMPILER_EXPORT void lvui_compiler_result_free(lvui_compiler_result*) LVUI_COMPILER_NOEXCEPT;

#ifdef __cplusplus
}
#endif
#undef LVUI_COMPILER_NOEXCEPT
