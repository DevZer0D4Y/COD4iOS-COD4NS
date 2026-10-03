#pragma once

#include "scr_debugger.h"

#define MAX_PRECACHE_ENTRIES 1024

enum : __int32
{
    SOURCE_TYPE_NONE = 0,
    SOURCE_TYPE_BREAKPOINT = 0x1,
    SOURCE_TYPE_CALL = 0x2,
    SOURCE_TYPE_THREAD_START = 0x4,
    SOURCE_TYPE_BUILTIN_CALL = 0x8,
    SOURCE_TYPE_NOTIFY = 0x10,
};
enum : __int32
{
    SCR_DEV_NO = 0x0,
    SCR_DEV_YES = 0x1,
    SCR_DEV_IGNORE = 0x2,
    SCR_DEV_EVALUATE = 0x3,
};

enum : __int32
{
    SCR_ABORT_NONE = 0x0,
    SCR_ABORT_CONTINUE = 0x1,
    SCR_ABORT_BREAK = 0x2,
    SCR_ABORT_RETURN = 0x3,
    SCR_ABORT_MAX = 0x3,
};

struct CaseStatementInfo // sizeof=0x10
{
    uint32_t name;
    const char *codePos;
    uint32_t sourcePos;
    CaseStatementInfo *next;
};
static_assert(sizeof(void *) != 4 || sizeof(CaseStatementInfo) == 0x10);

struct BreakStatementInfo // sizeof=0xC
{
    char *codePos;
    const char *nextCodePos;
    BreakStatementInfo *next;
};
static_assert(sizeof(void *) != 4 || sizeof(BreakStatementInfo) == 0xC);

struct ContinueStatementInfo // sizeof=0xC
{
    char *codePos;
    const char *nextCodePos;
    ContinueStatementInfo *next;
};
static_assert(sizeof(void *) != 4 || sizeof(ContinueStatementInfo) == 0xC);

struct VariableCompileValue // sizeof=0xC
{                                       // ...
    VariableValue value;                // ...
    sval_u sourcePos;
};
static_assert(sizeof(void *) != 4 || sizeof(VariableCompileValue) == 0xC);

#define VALUE_STACK_SIZE 32

struct scrCompileGlob_t // sizeof=0x1D8
{                                       // ...
    uint8_t *codePos;           // ...
    uint8_t *prevOpcodePos;     // ...
    uint32_t fileId;                // ...
    uint32_t threadId;              // ...
    int cumulOffset;                    // ...
    int maxOffset;                      // ...
    int maxCallOffset;                  // ...
    bool bConstRefCount;                // ...
    bool in_developer_thread;           // ...
    // padding byte
    // padding byte
    uint32_t developer_thread_sourcePos; // ...
    bool firstThread[2];                // ...
    // padding byte
    // padding byte
    CaseStatementInfo *currentCaseStatement; // ...
    bool bCanBreak;                     // ...
    // padding byte
    // padding byte
    // padding byte
    BreakStatementInfo *currentBreakStatement; // ...
    bool bCanContinue;                  // ...
    // padding byte
    // padding byte
    // padding byte
    ContinueStatementInfo *currentContinueStatement; // ...
    scr_block_s **breakChildBlocks;     // ...
    int *breakChildCount;               // ...
    scr_block_s *breakBlock;            // ...
    scr_block_s **continueChildBlocks;  // ...
    int *continueChildCount;            // ...
    bool forceNotCreate;                // ...
    // padding byte
    // padding byte
    // padding byte
    struct PrecacheEntry *precachescriptList;  // ...
    VariableCompileValue value_start[VALUE_STACK_SIZE]; // ...
};
static_assert(sizeof(void *) != 4 || sizeof(scrCompileGlob_t) == 0x1D8);

#include "scr_compile_state.h"

void __cdecl Scr_CompileStatement(sval_u parseData);
void __cdecl ScriptCompile(
    sval_u val,
    uint32_t fileId,
    uint32_t scriptId,
    struct PrecacheEntry *entries,
    int entriesCount);

extern scrCompilePub_t scrCompilePub;
extern scrCompileGlob_t scrCompileGlob;