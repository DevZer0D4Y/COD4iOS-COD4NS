#pragma once
#include <cstdint>

#define SCR_FUNC_TABLE_SIZE 1024

struct scrCompilePub_t
{
    int value_count;
    int far_function_count;
    uint32_t loadedscripts;
    uint32_t scripts;
    uint32_t builtinFunc;
    uint32_t builtinMeth;
    short canonicalStrings[65536];
    const char *in_ptr;
    const char *parseBuf;
    bool script_loading;
    bool allowedBreakpoint;
    int developer_statement;
    unsigned char *opcodePos;
    uint32_t programLen;
    int func_table_size;
    intptr_t func_table[SCR_FUNC_TABLE_SIZE];
};

extern scrCompilePub_t scrCompilePub;
int Scr_ScanFile(unsigned char *buffer, int maxSize);
void CompileError(uint32_t sourcePos, const char *message, ...);
