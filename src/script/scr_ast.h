#pragma once
#include "scr_yacc.h"
#include <type_traits>

// Native parser values. Script strings and source positions remain 32-bit IDs.
struct debugger_sval_s // sizeof=0x4
{
    debugger_sval_s *next;
};
static_assert(sizeof(debugger_sval_s) == sizeof(void *));

struct scr_localVar_t // sizeof=0x8
{                                       // ...
    uint32_t name;                  // ...
    uint32_t sourcePos;             // ...
};
static_assert(sizeof(void *) != 4 || sizeof(scr_localVar_t) == 0x8);

#define LOCAL_VAR_STACK_SIZE 64
#define MAX_SWITCH_CASES 1024

struct scr_block_s // sizeof=0x218
{
    int abortLevel;
    int localVarsCreateCount;
    int localVarsPublicCount;
    int localVarsCount;
    uint8_t localVarsInitBits[8];
    scr_localVar_t localVars[LOCAL_VAR_STACK_SIZE];
};
static_assert(sizeof(void *) != 4 || sizeof(scr_block_s) == 0x218);

union sval_u
{
    sval_u() : node(nullptr) {}
    sval_u(int value) : node(nullptr) { intValue = value; }
    sval_u(const sval_u &) = default;
    sval_u &operator=(const sval_u &) = default;
    Enum_t type;
    uint32_t stringValue;
    uint32_t idValue;
    float floatValue;
    int intValue;
    sval_u *node;
    uint32_t sourcePosValue;
    const char *codePosValue;
    const char *debugString;
    scr_block_s *block;
};
static_assert(sizeof(sval_u) == sizeof(void *));
static_assert(std::is_trivially_copyable_v<sval_u>);

struct ScriptExpression_t // sizeof=0xC
{                                       // ...
    sval_u parseData;                   // ...
    int breakonExpr;                    // ...
    debugger_sval_s *exprHead;          // ...
};
static_assert(sizeof(ScriptExpression_t) == (sizeof(void *) == 8 ? 24 : 12));

