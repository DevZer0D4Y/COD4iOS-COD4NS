#pragma once
#include <cstdint>

// Engine contexts shared by the platform and renderer. Values are ABI-stable.
#ifdef KISAK_MP
 enum ThreadContext_t : std::int32_t // Not a real struct, used for forced usage of this enum 
 {                                       // ...
     THREAD_CONTEXT_MAIN         = 0x0,
     THREAD_CONTEXT_BACKEND      = 0x1,
     THREAD_CONTEXT_WORKER0      = 0x2,
     THREAD_CONTEXT_WORKER1      = 0x3,
     THREAD_CONTEXT_TRACE_COUNT  = 0x4,
     THREAD_CONTEXT_TRACE_LAST   = 0x3,
     THREAD_CONTEXT_CINEMATIC    = 0x4,
     THREAD_CONTEXT_TITLE_SERVER = 0x5,
     THREAD_CONTEXT_DATABASE     = 0x6,
     THREAD_CONTEXT_COUNT        = 0x7,
 };
#elif KISAK_SP
enum ThreadContext_t : std::int32_t
{
    THREAD_CONTEXT_MAIN = 0x0,
    THREAD_CONTEXT_BACKEND = 0x1,
    THREAD_CONTEXT_WORKER0 = 0x2,
    THREAD_CONTEXT_WORKER1 = 0x3,
    THREAD_CONTEXT_WORKER2 = 0x4,
    THREAD_CONTEXT_SERVER = 0x5,
    THREAD_CONTEXT_TRACE_COUNT = 0x6,
    THREAD_CONTEXT_TRACE_LAST = 0x5,
    THREAD_CONTEXT_CINEMATIC = 0x6,
    THREAD_CONTEXT_TITLE_SERVER = 0x7,
    THREAD_CONTEXT_DATABASE = 0x8,
    THREAD_CONTEXT_STREAM = 0x9,
    THREAD_CONTEXT_SNDSTREAMPACKETCALLBACK = 10,
    THREAD_CONTEXT_SERVER_DEMO = 11,
    THREAD_CONTEXT_COUNT = 12,
};
#elif defined(KISAK_RADIANT)
enum ThreadContext_t : std::int32_t {
    THREAD_CONTEXT_MAIN     = 0x0,
    THREAD_CONTEXT_BACKEND  = 0x1,
    THREAD_CONTEXT_DATABASE = 0x2,
    THREAD_CONTEXT_COUNT    = 0x3,
};
#endif

