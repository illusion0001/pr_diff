#pragma once

#include <stdint.h>
#include <stdio.h>
#include "macro.hpp"
#include <Windows.h>

extern HWND prog_hwnd;

struct assert_data
{
    const wchar_t* expression;
    const char* message;
    const wchar_t* function;
    const wchar_t* file;
    const wchar_t* built;
    const size_t line;
};
void my_assert(const assert_data& a);
void my_assertf(const assert_data& a, const char* fmt, ...);

#define overlay_assert(expression, msg) ((void)((!!(expression)) || \
                                                (my_assert({_CRT_WIDE(#expression), msg, L"" __FUNCSIG__, _CRT_WIDE(__FILE__), _CRT_WIDE(__DATE__) " @ " __TIME__, (__LINE__)}), 0)))

#define overlay_assertf(expression, ...) ((void)((!!(expression)) || \
                                                 (my_assertf({_CRT_WIDE(#expression), 0, L"" __FUNCSIG__, _CRT_WIDE(__FILE__), _CRT_WIDE(__DATE__) " @ " __TIME__, (__LINE__)}, __VA_ARGS__), 0)))

#if defined(assert)
#undef assert
#endif

#define assert(e) overlay_assert(e, "")
#define assert_msg(e, ...) overlay_assertf(e, __VA_ARGS__)
