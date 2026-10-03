#pragma once

#include <cstdint>
struct HunkUser;

struct scrVarPub_t // sizeof=0x2007C
{
    char* fieldBuffer;
    uint16_t canonicalStrCount;
    bool developer;
    bool developer_script;
    bool evaluate;
    const char* error_message;
    int error_index;
    uint32_t time;
    uint32_t timeArrayId;
    uint32_t pauseArrayId;
    uint32_t levelId;
    uint32_t gameId;
    uint32_t animId;
    uint32_t freeEntList;
    uint32_t tempVariable;
    bool bInited;
    uint16_t savecount;
    uint32_t checksum;
    uint32_t entId;
    uint32_t entFieldName;
    HunkUser* programHunkUser;
    const char* programBuffer;
    const char* endScriptBuffer;
    uint16_t saveIdMap[32768];
    uint16_t saveIdMapRev[32768];
    bool bScriptProfile;
    float scriptProfileMinTime;
    bool bScriptProfileBuiltin;
    float scriptProfileBuiltinMinTime;
    uint32_t numScriptThreads;
    uint32_t numScriptValues;
    uint32_t numScriptObjects;
    const char* varUsagePos;
    int ext_threadcount;
    int totalObjectRefCount;
    volatile uint32_t totalVectorRefCount;
};
static_assert(sizeof(scrVarPub_t) == (sizeof(void *) == 8 ? 0x200A0 : 0x2007C));

extern scrVarPub_t scrVarPub;
bool Scr_IsInOpcodeMemory(const char *pos);
