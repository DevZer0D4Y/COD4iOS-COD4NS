#pragma once

// Force-included into every engine translation unit on Apple targets.
// Supplies the handful of MSVC CRT and Win32 names the decompiled engine uses
// without platform guards. Anything with real behaviour (threads, files,
// windows, input) is implemented in ports/ios/platform, not faked here.

#if defined(__APPLE__)

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>

// DXVK-native Win32 base types (HWND, DWORD, LONG as int32_t, ...).
#include <windows_base.h>
#define KISAK_NATIVE_WINDOWS_BASE 1

typedef unsigned char byte;
typedef unsigned char Byte; // zconf.h skips this when TARGET_OS_MAC is defined

// Windows declares handles as pointers to distinct tag structs under STRICT;
// the decompiled engine spells some of those tags directly.
struct HWND__;
struct HINSTANCE__;

typedef uintptr_t WPARAM;
typedef intptr_t LPARAM;
typedef intptr_t LRESULT;

typedef struct _OSVERSIONINFOA {
    DWORD dwOSVersionInfoSize;
    DWORD dwMajorVersion;
    DWORD dwMinorVersion;
    DWORD dwBuildNumber;
    DWORD dwPlatformId;
    CHAR szCSDVersion[128];
} OSVERSIONINFOA, OSVERSIONINFO;

#define _iobuf __sFILE
#define _isnan(x) isnan(x)
#define _finite(x) isfinite(x)
#define _vsnprintf vsnprintf
#define _snprintf snprintf
#define _stricmp strcasecmp
#define _strnicmp strncasecmp
#define _strdup strdup
typedef int64_t __time64_t;
typedef POINT tagPOINT;
typedef RECT tagRECT;
typedef LARGE_INTEGER _LARGE_INTEGER;

#define PF_NON_TEMPORAL_LEVEL_ALL 0
#define PreFetchCacheLine(level, address) __builtin_prefetch(address)

#ifdef __cplusplus
extern "C++" {
std::uint64_t Sys_ReadRawTimer();
}
#define __rdtsc() Sys_ReadRawTimer()

// Win32 Interlocked* take LONG*, but the decompiled callers pass whatever
// 32-bit integer or pointer type the field happens to have.
template<typename T> inline T InterlockedIncrement(T *v) { return __sync_add_and_fetch(v, 1); }
template<typename T> inline T InterlockedDecrement(T *v) { return __sync_sub_and_fetch(v, 1); }
template<typename T, typename U> inline T InterlockedExchangeAdd(T *v, U add)
{
    return __sync_fetch_and_add(v, static_cast<T>(add));
}
template<typename T, typename U> inline T InterlockedExchange(T *dst, U value)
{
    return __sync_lock_test_and_set(dst, static_cast<T>(value));
}
template<typename T, typename U, typename V> inline T InterlockedCompareExchange(T *dst, U exchange, V comparand)
{
    return __sync_val_compare_and_swap(dst, static_cast<T>(comparand), static_cast<T>(exchange));
}
template<typename P, typename V> inline P InterlockedExchangePointer(P *dst, V value)
{
    return __sync_lock_test_and_set(dst, static_cast<P>(value));
}
#define _InterlockedIncrement InterlockedIncrement
// MSVC secure CRT names used by the decompiled sources.
#define sscanf_s sscanf
// Other MSVC-only CRT spellings used by the decompiled sources.
#define _TRUNCATE ((size_t)-1)
#define _putenv putenv
#define _strlwr I_strlwr
// _vsnprintf_s(buffer, size, _TRUNCATE, fmt, args) truncates like vsnprintf does.
#define _vsnprintf_s(buffer, size, count, fmt, args) vsnprintf((buffer), (size), (fmt), (args))
#define _InterlockedDecrement InterlockedDecrement
#define _InterlockedExchangeAdd InterlockedExchangeAdd
#define _InterlockedExchange InterlockedExchange
#define _InterlockedCompareExchange InterlockedCompareExchange
#define _InterlockedExchangePointer InterlockedExchangePointer

#define sprintf_s snprintf

inline __time64_t _time64(__time64_t *out)
{
    const __time64_t now = static_cast<__time64_t>(time(nullptr));
    if (out)
        *out = now;
    return now;
}

inline struct tm *_localtime64(const __time64_t *value)
{
    const time_t converted = static_cast<time_t>(*value);
    return localtime(&converted);
}

inline char *_ctime64(const __time64_t *value)
{
    const time_t converted = static_cast<time_t>(*value);
    return ctime(&converted);
}

inline int fopen_s(FILE **file, const char *name, const char *mode)
{
    *file = fopen(name, mode);
    return *file ? 0 : errno;
}

inline int strcpy_s(char *dst, size_t size, const char *src)
{
    if (!dst || !size)
        return EINVAL;
    const size_t len = strlen(src);
    if (len >= size) {
        dst[0] = 0;
        return ERANGE;
    }
    memcpy(dst, src, len + 1);
    return 0;
}

template<size_t N> inline int strcpy_s(char (&dst)[N], const char *src)
{
    return strcpy_s(dst, N, src);
}

inline char *_itoa(int value, char *buffer, int radix)
{
    if (radix == 10) {
        snprintf(buffer, 33, "%d", value);
        return buffer;
    }
    static const char digits[] = "0123456789abcdefghijklmnopqrstuvwxyz";
    char tmp[33];
    unsigned int v = static_cast<unsigned int>(value);
    int i = 0;
    do {
        tmp[i++] = digits[v % static_cast<unsigned int>(radix)];
        v /= static_cast<unsigned int>(radix);
    } while (v);
    for (int j = 0; j < i; ++j)
        buffer[j] = tmp[i - 1 - j];
    buffer[i] = 0;
    return buffer;
}

// MMX state reset; arm64 has no MMX state to clear.
inline void _m_empty() {}

inline void Sleep(DWORD ms) { usleep(static_cast<useconds_t>(ms) * 1000); }

inline DWORD timeGetTime()
{
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<DWORD>(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

inline BOOL QueryPerformanceFrequency(LARGE_INTEGER *freq)
{
    freq->QuadPart = 1000000000;
    return 1;
}

inline BOOL QueryPerformanceCounter(LARGE_INTEGER *count)
{
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    count->QuadPart = static_cast<int64_t>(ts.tv_sec) * 1000000000 + ts.tv_nsec;
    return 1;
}
#endif

#ifdef __cplusplus
#include "kisak_win32_file_api.h"
#endif

#endif // __APPLE__
