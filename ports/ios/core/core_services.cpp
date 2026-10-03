// Engine services that the reused upstream modules call but that still live in
// Windows-only translation units (q_shared.cpp needs threads.h, mem_track.cpp
// needs Windows.h). The portable core supplies bounded, self-contained
// implementations so ARM64 modules can be built and tested in isolation; the
// complete engine keeps its own once those units are ported.

#include <universal/q_shared.h>
#include <qcommon/mem_track.h>

#include <cstdarg>
#include <cstdio>

// Upstream rotates two 1024-byte buffers held in per-thread engine storage.
// thread_local gives the same aliasing behaviour without the thread context,
// and truncation is reported by the assert handler rather than Com_Error,
// which belongs to the engine's not-yet-ported error handling.
char *QDECL va(const char *format, ...)
{
    static thread_local va_info_t info;
    char *buffer = info.va_string[info.index];
    info.index = (info.index + 1) % 2;

    va_list arguments;
    va_start(arguments, format);
    const int length = std::vsnprintf(buffer, sizeof(info.va_string[0]), format, arguments);
    va_end(arguments);

    buffer[sizeof(info.va_string[0]) - 1] = 0;
    iassert(length >= 0 && length < (int)sizeof(info.va_string[0]));
    return buffer;
}

// The upstream body is disabled behind #if 0, so the tracker records nothing.
// Matching that keeps the portable core free of the Windows memory subsystem.
void __cdecl track_static_alloc_internal(void *ptr, int size, const char *name, int type)
{
    (void)ptr;
    (void)size;
    (void)name;
    (void)type;
}
