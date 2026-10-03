#pragma once
#include <cstdint>
#include <type_traits>
#ifdef _MSC_VER
#include <intrin.h>
#endif

template<class T> inline T Sys_AtomicLoad(const volatile T *value)
{
    static_assert(std::is_integral_v<T> && sizeof(T) == 4);
#ifdef _MSC_VER
    return static_cast<T>(_InterlockedCompareExchange(
        reinterpret_cast<volatile long *>(const_cast<volatile T *>(value)), 0, 0));
#else
    return __atomic_load_n(value, __ATOMIC_SEQ_CST);
#endif
}

// The packed reference counters are 32-bit on every platform. Do not use
// Apple's 64-bit long to stand in for Win32 LONG or change their representation.
template<class T> inline T Sys_AtomicIncrement(volatile T *value)
{
    static_assert(std::is_integral_v<T> && sizeof(T) == 4);
#ifdef _MSC_VER
    return static_cast<T>(_InterlockedIncrement(reinterpret_cast<volatile long *>(value)));
#else
    return __atomic_add_fetch(value, T(1), __ATOMIC_SEQ_CST);
#endif
}

template<class T> inline T Sys_AtomicDecrement(volatile T *value)
{
    static_assert(std::is_integral_v<T> && sizeof(T) == 4);
#ifdef _MSC_VER
    return static_cast<T>(_InterlockedDecrement(reinterpret_cast<volatile long *>(value)));
#else
    return __atomic_sub_fetch(value, T(1), __ATOMIC_SEQ_CST);
#endif
}

template<class T> inline T Sys_AtomicCompareExchange(volatile T *value, T exchange, T expected)
{
    static_assert(std::is_integral_v<T> && sizeof(T) == 4);
#ifdef _MSC_VER
    return static_cast<T>(_InterlockedCompareExchange(reinterpret_cast<volatile long *>(value),
                                                      static_cast<long>(exchange), static_cast<long>(expected)));
#else
    __atomic_compare_exchange_n(value, &expected, exchange, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
    return expected;
#endif
}
