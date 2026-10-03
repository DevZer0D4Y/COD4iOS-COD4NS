#pragma once


#include <universal/com_memory.h>
#include "scr_variable.h"
#include "scr_variable_state.h"

static const char *var_typename[] =
{
    "undefined",
    "object",
    "string",
    "localized string",
    "vector",
    "float",
    "int",
    "codepos",
    "precodepos",
    "function",
    "stack",
    "animation",
    "developer codepos",
    "include codepos",
    "thread",
    "thread",
    "thread",
    "thread",
    "struct",
    "removed entity",
    "entity",
    "array",
    "removed thread",
};

struct PrecacheEntry // sizeof=0x8
{                                       // ...
    uint16_t filename;
    bool include;
    // padding byte
    uint32_t sourcePos;
};
static_assert(sizeof(void *) != 4 || sizeof(PrecacheEntry) == 0x8);

extern scrVarPub_t scrVarPub;
extern scrVarDebugPub_t scrVarDebugPubBuf;

bool Scr_IsInOpcodeMemory(char const* pos);
bool Scr_IsIdentifier(char const* token);

int Scr_GetFunctionHandle(char const*, char const*);
uint32_t SL_TransferToCanonicalString(uint32_t);
uint32_t SL_GetCanonicalString(char const*);
void Scr_BeginLoadScriptsRemote(void);
void Scr_BeginLoadAnimTrees(int);
int Scr_ScanFile(unsigned char*, int);
uint32_t Scr_LoadScriptInternal(char const*, struct PrecacheEntry*, int);
uint32_t Scr_LoadScript(char const*);
void Scr_PostCompileScripts(void);
void Scr_EndLoadScripts(void);
void Scr_PrecacheAnimTrees(void* (__cdecl*)(int), int);
void Scr_EndLoadAnimTrees(void);
void Scr_FreeScripts(unsigned char);
void Scr_BeginLoadScripts(void);


//int marker_scr_main      83043248     scr_main.obj
//int Scr_IsInScriptMemory(char const*);

extern scrVarDebugPub_t *scrVarDebugPub;