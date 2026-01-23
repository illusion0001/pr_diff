#pragma once

#define __my_countof(_Array) (sizeof(_Array) / sizeof(_Array[0]))
#if !defined(_countof)
// Just making sure
#define _countof __my_countof
#endif
#define _countof_1(x) (__my_countof(x) - 1)

// `-Wmicrosoft-string-literal-from-predefined` ignored in cmake
// this way, we have *full* printout of this without any formatting.
// btw, `__FUNCSIG__` can be a string literal. thanks microsoft

#define nstr(x) #x
#define xstr(x) nstr(x)

#define DEBUG_LINE \
    "[" __FILE__ ":" __FUNCSIG__ ":" xstr(__LINE__) "]"
#define verbose_printf(...) printf_s(DEBUG_LINE " " __VA_ARGS__)
#define verbose_wprintf(...) wprintf_s(L"" DEBUG_LINE " " __VA_ARGS__)
#define print_this_var(var, fmt) verbose_printf(#var " " fmt "\n", var)

#define new_log(obj, new_obj) verbose_printf("New opreator called. allocating object %s\n", #obj), obj = new new_obj
#define delete_log(obj) verbose_printf("Delete opreator called. deleting object %s\n", #obj), delete obj, obj = 0

#if defined(_DEBUG)
#define debug_printf(...) verbose_printf("[DEBUG]: " __VA_ARGS__)
#define debug_wprintf(...) verbose_wprintf(L"[DEBUG]: " __VA_ARGS__)
#else
static int dummyf(const char* f, ...)
{
    (void)f;
    return 0;
}
static int wdummyf(const wchar_t* f, ...)
{
    (void)f;
    return 0;
}
// to prevent unused functions and variables that is only used for debug prints
#define debug_printf(...) dummyf(DEBUG_LINE __VA_ARGS__)
#define debug_wprintf(...) wdummyf(DEBUG_LINE __VA_ARGS__)
#endif

#define ENUM_STR(e) \
    case e:         \
        return #e

// a copy of `RtlSecureZeroMemory` but not inlined. to produce cleaner assembly.
static __attribute__((noinline)) void* MyRtlSecureZeroMemory(void* ptr, const unsigned long long cnt)
{
    volatile char* vptr = (volatile char*)ptr;
#if defined(_M_AMD64) && !defined(_M_ARM64EC)
    // actually, clang-cl seems to replace it to memset
    // but fwiw, just no inlining of small `{0}`
    // i guess if cnt is < 257, then it will be inlined
    // ref:
    // <https://groups.google.com/g/llvm-dev/c/Nfm9qfb69w0/m/IMRpfJiLgYAJ> + <https://github.com/llvm/llvm-project/issues/63232#issuecomment-2940875834>
    // (<https://godbolt.org/z/Pr7rTrja4>)
    // and my copy:
    // <https://godbolt.org/z/r369h3cPo>
    void __stosb(
        unsigned char* Destination,
        unsigned char Data,
        unsigned long long Count);
    __stosb((unsigned char*)vptr, 0, cnt);
#else
    while (cnt)
    {
#if !defined(_M_CEE) && (defined(_M_ARM) || defined(_M_ARM64) || defined(_M_ARM64EC))
        __iso_volatile_store8(vptr, 0);
#else
        *vptr = 0;
#endif
        vptr++;
        cnt--;
    }
#endif  // _M_AMD64 && !defined(_M_ARM64EC)
    return ptr;
}

#if !defined(AEFX_CLR_STRUCT)
#define AEFX_CLR_STRUCT(STRUCT) MyRtlSecureZeroMemory(&(STRUCT), sizeof(STRUCT))
#endif

#if !defined(ERR)
#define ERR(FUNC)                                       \
    do                                                  \
    {                                                   \
        if (!err)                                       \
        {                                               \
            err = (FUNC);                               \
            if (err)                                    \
            {                                           \
                verbose_printf(#FUNC " err %d\n", err); \
            }                                           \
        }                                               \
    } while (0)
#endif

#if !defined(ERR2)
#define ERR2(FUNC)                                        \
    do                                                    \
    {                                                     \
        if (((err2 = (FUNC)) != A_Err_NONE) && !err)      \
        {                                                 \
            err = err2;                                   \
            if (err2)                                     \
            {                                             \
                verbose_printf(#FUNC " err2 %d\n", err2); \
            }                                             \
        }                                                 \
    } while (0)
#endif
