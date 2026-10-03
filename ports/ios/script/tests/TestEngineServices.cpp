// Test harness only. The campaign must link the real engine console and error
// recovery; these test adapters are never included in the shipped libraries.
#include <universal/q_shared.h>
#include <qcommon/qcommon.h>
#include <cstdarg>
#include <cstdio>
#include <stdexcept>

void Com_Printf(int, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    std::vfprintf(stderr, format, args);
    va_end(args);
}

void Com_PrintWarning(int, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    std::vfprintf(stderr, format, args);
    va_end(args);
}

void Com_Error(errorParm_t, const char *format, ...)
{
    char message[2048];
    va_list args;
    va_start(args, format);
    std::vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    throw std::runtime_error(message);
}

void CompileError(std::uint32_t sourcePos, const char *format, ...)
{
    char message[2048];
    va_list args;
    va_start(args, format);
    std::vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    throw std::runtime_error("GSC at " + std::to_string(sourcePos) + ": " + message);
}
