// https://gist.github.com/illusion0001/ddea726819c7621df5b75bf414ecbf3c
// https://gist.github.com/rkr35/79264c540856849a4cfa7f9edcbc493e
// Adjusted and fixed to compile on clang-cl (missing address())
// Static usage for including in multiple files
/*
Copyright (c) 2017 Artem Boldarev <artem.boldarev@gmail.com>

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files(the "Software"),
to deal in the Software without restriction, including without limitation
the rights to use, copy, modify, merge, publish, distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following conditions :

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL
THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.

*/

#pragma once

#include <windows.h>
#include <stdint.h>
#include <stdbool.h>
/*
 *https://gist.github.com/arbv/531040b8036050c5648fc55471d50352
A safer replacement for the obsolete IsBadReadPtr() and IsBadWritePtr() WinAPI functions
on top of VirtualQuery() which respects Windows guard pages. It does not use SEH
and is designed to be compatible with the above-mentioned functions.
The calls to the IsBadReadPtr() and IsBadWritePtr() can be replaced with the calls to
the is_bad_mem_ptr() as follows:
- IsBadReadPtr(...)  => is_bad_mem_ptr(false, ...)
- IsBadWritePtr(...) => is_bad_mem_ptr(true, ...)
*/
template <typename T>
static bool is_bad_mem_ptr(const bool write, const T* ptr, const size_t size)
{
    const uintptr_t min_ptr = 0x10000;
    const uintptr_t max_ptr = 0x000F000000000000;

    if (ptr == nullptr || uintptr_t(ptr) < min_ptr || uintptr_t(ptr) >= max_ptr)
    {
        return true;
    }

    DWORD mask = PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;

    if (write)
    {
        mask = PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
    }

    auto current = uintptr_t(ptr);
    const auto last = current + size;

    // So we are considering the region:
    // [ptr, ptr+size)

    while (current < last)
    {
        MEMORY_BASIC_INFORMATION mbi = {};

        if (VirtualQuery(LPCVOID(current), &mbi, sizeof(mbi)) == 0)
        {
            // We couldn't get any information on this region.
            // Let's not risk any read/write operation.
            return true;
        }

        if ((mbi.Protect & mask) == 0)
        {
            // We can't perform our desired read/write operations in this region.
            return true;
        }

        if (mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS))
        {
            // We can't access this region.
            return true;
        }

        // Let's consider the next region.
        current = uintptr_t(mbi.BaseAddress) + mbi.RegionSize;
    }

    return false;
}

template <typename T>
static bool is_bad_read_ptr(const T* ptr, const size_t size)
{
    return is_bad_mem_ptr(false, ptr, size);
}

template <typename T>
static bool is_bad_read_ptr(const T* ptr)
{
    return is_bad_read_ptr(ptr, sizeof(ptr));
}

template <typename T>
static bool is_bad_write_ptr(const T* ptr, const size_t size)
{
    return is_bad_mem_ptr(true, ptr, size);
}

template <typename T>
static bool is_bad_write_ptr(const T* ptr)
{
    return is_bad_write_ptr(ptr, sizeof(ptr));
}
