#include <universal/q_shared.h>
#include "scr_compile_state.h"

scrCompilePub_t scrCompilePub;

int __cdecl Scr_ScanFile(unsigned char *buf, int max_size)
{
    char c; // [esp+3h] [ebp-5h]
    int n; // [esp+4h] [ebp-4h]

    c = 42;
    for (n = 0; n < max_size; ++n)
    {
        c = *scrCompilePub.in_ptr++;
        if (!c || c == 10)
            break;
        buf[n] = c;
    }
    if (c == 10)
    {
        buf[n++] = c;
    }
    else if (!c)
    {
        if (scrCompilePub.parseBuf)
        {
            scrCompilePub.in_ptr = scrCompilePub.parseBuf;
            scrCompilePub.parseBuf = 0;
        }
        else
        {
            --scrCompilePub.in_ptr;
        }
    }
    return n;
}

