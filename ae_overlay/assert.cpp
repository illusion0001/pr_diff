#include <stdio.h>
#include <stdarg.h>
#include "assert.hpp"
#include "macro.hpp"

HWND prog_hwnd = 0;

#define ASSERT_MAX_BUF 512

void my_assert(const assert_data& a)
{
    wchar_t msg[(128 + ASSERT_MAX_BUF) + 1];
    MyRtlSecureZeroMemory(msg, sizeof(msg));
    {
        const bool has_msg = a.message && a.message[0] != 0;
        _snwprintf_s(msg, _countof_1(msg), L"%s%hs%s", has_msg ? L"Message: " : L"", has_msg ? a.message : "", has_msg ? L"\n" : L"");
    }
    wchar_t buf[(8192 + _countof(msg)) + 1];
    MyRtlSecureZeroMemory(buf, sizeof(buf));
    _snwprintf_s(buf, _countof_1(buf),
                 L"Assertion failed:\n"
                 "Expression: %s\n"
                 "%s"
                 "Function: %s\n"
                 "File: %s\n"
                 "Line: %d\n"
                 "Built: %s\n\n"
                 "This is only a warning\n"
                 "You may press Okay to continue execution to save your project but may encounter issues.",
                 a.expression,
                 msg,
                 a.function,
                 a.file,
                 a.line,
                 a.built);
    wprintf_s(L"%s\n", buf);
    OutputDebugStringW(buf);
    OutputDebugStringW(L"\n");
    if (IsDebuggerPresent())
    {
        DebugBreak();
    }
    else
    {
        MessageBoxW(prog_hwnd, buf, L"Overlay Assert", MB_ICONWARNING);
    }
}

void my_assertf(const assert_data& a, const char* fmt, ...)
{
    va_list ap = 0;
    char buf[ASSERT_MAX_BUF + 1];
    MyRtlSecureZeroMemory(buf, sizeof(buf));
    va_start(ap, fmt);
    _vsnprintf_s(buf, _countof_1(buf), fmt, ap);
    va_end(ap);
    assert_data& temp_a = (assert_data&)a;
    temp_a.message = buf;
    my_assert(temp_a);
}

void assert_test()
{
    assert_msg(false, "checking %d", 1234);
    assert(false);
}
