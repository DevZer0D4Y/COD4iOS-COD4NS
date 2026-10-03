#include <cstdarg>
#include <cstdio>
#include <cstdlib>

// The complete engine supplies its own handler. The isolated portable library
// has no console subsystem, so assertion failures report to stderr and abort.
void MyAssertHandler(const char *filename, int line, int, const char *format, ...)
{
    std::fprintf(stderr, "%s:%d: KisakCOD core assertion: ", filename, line);
    va_list arguments;
    va_start(arguments, format);
    std::vfprintf(stderr, format, arguments);
    va_end(arguments);
    std::fputc('\n', stderr);
    std::abort();
}
