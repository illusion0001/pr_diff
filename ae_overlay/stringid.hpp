#pragma once

#include <stdint.h>

static constexpr uint64_t ToStringId64A(const char* str)
{
    uint64_t base = 0xCBF29CE484222325;
    if (*str)
    {
        do
        {
            base = 0x100000001B3 * (base ^ *str++);
        } while (*str);
    }
    return base;
}

static constexpr uint64_t ToStringId64W(const wchar_t* str)
{
    uint64_t base = 0xCBF29CE484222325;
    if (*str)
    {
        do
        {
            base = 0x100000001B3 * (base ^ *str++);
        } while (*str);
    }
    return base;
}

#define SIDA(s) ToStringId64A(s)
#define SIDW(s) ToStringId64W(s)
