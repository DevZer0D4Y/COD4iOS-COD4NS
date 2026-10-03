#ifndef KISAK_SP 
#error This file is for SinglePlayer only 
#endif

#include <universal/q_shared.h>
#include "scr_readwrite.h"
#include "scr_main.h"
#include "scr_memorytree.h"
#include "scr_vm.h"
#include "scr_parsetree.h"
#include <game/savedevice.h>
#include <universal/com_files.h>  // FS_Write for SaveMemory_SaveWriteImmediate

int __cdecl Scr_DoLoadEntry(VariableValue *value, bool isArray, MemoryFile *memFile)
{

    int result;
    unsigned __int8 byte0;
    unsigned __int8 byte1;
    unsigned __int8 byte2;
    unsigned int header = 0;
    unsigned __int16 header2 = 0;
    int header4 = 0;

    Scr_DoLoadEntryInternal(value, memFile);
    if (isArray)
    {
        MemFile_ReadData(memFile, 1, (unsigned char *)&header);
        unsigned int tag = header & 0xFF;
        switch (tag & 7)
        {
        case 0:
            result = 0x800000;
            break;
        case 1:
        {
            unsigned int byteRead = 0;
            MemFile_ReadData(memFile, 1, (unsigned char *)&byteRead);
            result = (int)(__int8)byteRead + 0x800000;
            break;
        }
        case 2:
            header2 = 0;
            MemFile_ReadData(memFile, 2, (unsigned char *)&header2);
            result = (int)(__int16)header2 + 0x800000;
            break;
        case 3:
            header4 = 0;
            MemFile_ReadData(memFile, 4, (unsigned char *)&header4);
            result = header4 + 0x800000;
            break;
        case 4:
            result = (unsigned __int16)Scr_ReadString(memFile);
            break;
        case 5:
            result = Scr_ReadId(memFile, tag) + 0x10000;
            break;
        default:
            if (!alwaysfails)
                MyAssertHandler("c:\\trees\\cod3\\cod3src\\src\\script\\scr_readwrite.cpp", 539, 1, "bad case");
            result = 0;
            break;
        }
    }
    else
    {

        MemFile_ReadData(memFile, 1, &byte0);
        MemFile_ReadData(memFile, 1, &byte1);
        MemFile_ReadData(memFile, 1, &byte2);
        return (((unsigned int)byte2) << 16) | (((unsigned int)byte1) << 8) | byte0;
    }
    return result;
}

void __cdecl AddSaveObjectInternal(unsigned int parentId)
{
    if (parentId)
    {
        if (!scrVarPub.saveIdMap[parentId])
        {
            scrVarPub.saveIdMap[parentId] = ++scrVarPub.savecount;
            *(unsigned __int16 *)((char *)scrVarPub.saveIdMapRev + __ROL4__(scrVarPub.savecount, 1)) = parentId;
        }
    }
}

unsigned int __cdecl Scr_ConvertThreadFromLoad(unsigned __int16 handle)
{
    int v2; // r30
    unsigned int v3; // r30

    if (!handle)
        return 0;
    v2 = handle;
    if (!scrVarPub.saveIdMapRev[v2])
        MyAssertHandler(
            "c:\\trees\\cod3\\cod3src\\src\\script\\scr_readwrite.cpp",
            746,
            0,
            "%s",
            "scrVarPub.saveIdMapRev[handle]");
    v3 = scrVarPub.saveIdMapRev[v2];
    ++scrVarPub.ext_threadcount;
    AddRefToObject(v3);
    if (scrVarDebugPub)
        ++scrVarDebugPub->extRefCount[v3];
    return v3;
}

void __cdecl Scr_DoLoadObjectInfo(unsigned __int16 parentId, MemoryFile *memFile)
{


    unsigned int v2;
    VariableValueInternal *parentValue;
    int v5;
    bool v12;
    int v13;
    unsigned int v14;
    VariableValueInternal *entryValue;
    int v18;
    int type;
    int v20;
    unsigned int header;       // 1-byte read scratch
    unsigned int header4;      // 4-byte read scratch
    unsigned __int16 header2;  // 2-byte read scratch
    VariableValue value;

    v2 = parentId;
    parentValue = &scrVarGlob.variableList[parentId + 1];

    iassert((parentValue->w.status & VAR_STAT_MASK) == VAR_STAT_EXTERNAL);
    iassert(IsObject(parentValue));

    header = 0;
    MemFile_ReadData(memFile, 1, (unsigned char *)&header);
    unsigned int headerByte = header & 0xFF;

    switch (headerByte & 7)
    {
    case 1:
        v5 = VAR_THREAD;
        parentValue->u.o.u.size = Scr_ReadId(memFile, headerByte);
        break;
    case 2:
        v5 = VAR_NOTIFY_THREAD;
        parentValue->u.o.u.entnum = Scr_ReadId(memFile, headerByte);
        iassert(!(parentValue->w.notifyName & VAR_NAME_HIGH_MASK));
        parentValue->w.type |= (Scr_ReadOptionalString(memFile) << 8) & 0xFFFF00;
        break;
    case 3:
        v5 = VAR_TIME_THREAD;
        parentValue->u.o.u.entnum = Scr_ReadId(memFile, headerByte);
        iassert(!(parentValue->w.waitTime & VAR_NAME_HIGH_MASK));
        header4 = 0;
        MemFile_ReadData(memFile, 4, (unsigned char *)&header4);
        parentValue->w.type |= header4 << 8;
        break;
    case 4:
    {
        v5 = VAR_CHILD_THREAD;
        parentValue->u.o.u.size = Scr_ReadId(memFile, headerByte);
        unsigned int header_b = 0;
        MemFile_ReadData(memFile, 1, (unsigned char *)&header_b);
        unsigned int byte2 = header_b & 0xFF;
        iassert(!(parentValue->w.parentLocalId & VAR_NAME_HIGH_MASK));
        parentValue->w.type |= Scr_ReadId(memFile, byte2) << 8;
        break;
    }
    case 5:
        v5 = VAR_DEAD_ENTITY;
        parentValue->u.o.u.size = Scr_ReadId(memFile, headerByte);
        break;
    default:
        v5 = headerByte >> 3;
        if (v5 == VAR_ENTITY)
        {
            header2 = 0;
            MemFile_ReadData(memFile, 2, (unsigned char *)&header2);
            parentValue->u.o.u.size = header2;
            if (parentValue->w.classnum & VAR_NAME_HIGH_MASK)
                MyAssertHandler(
                    "c:\\trees\\cod3\\cod3src\\src\\script\\scr_readwrite.cpp",
                    827,
                    0,
                    "%s",
                    "!(parentValue->w.classnum & VAR_NAME_HIGH_MASK)");
            header2 = 0;
            MemFile_ReadData(memFile, 2, (unsigned char *)&header2);
            parentValue->w.type |= ((int)(__int16)header2) << 8;
        }
        else if (v5 == VAR_ARRAY)
        {
            parentValue->u.o.u.size = 0;
        }
        break;
    }
    if ((v5 & ~VAR_MASK) != 0)
        MyAssertHandler("c:\\trees\\cod3\\cod3src\\src\\script\\scr_readwrite.cpp", 837, 0, "%s", "!(type & ~VAR_MASK)");
    v12 = v5 == VAR_ARRAY;
    parentValue->w.type = parentValue->w.type & 0xFFFFFFE0 | v5;

    header2 = 0;
    MemFile_ReadData(memFile, 2, (unsigned char *)&header2);
    if (header2)
    {
        v13 = header2;
        do
        {
            v14 = Scr_DoLoadEntry(&value, v12, memFile);
            entryValue = &scrVarGlob.variableList[GetVariable(v2, v14) + VARIABLELIST_CHILD_BEGIN];
            if (v12)
            {
                VariableValue val = Scr_GetArrayIndexValue(v14);
                RemoveRefToValue(val.type, val.u);
            }
            v18 = entryValue->w.status & 0x60;
            if (!v18 || v18 == 96)
                MyAssertHandler(
                    "c:\\trees\\cod3\\cod3src\\src\\script\\scr_readwrite.cpp",
                    856,
                    0,
                    "%s",
                    "(entryValue->w.status & VAR_STAT_MASK) != VAR_STAT_FREE && (entryValue->w.status & VAR_STAT_MASK) != VAR_STAT_EXTERNAL");
            iassert((entryValue->w.type & VAR_MASK) == VAR_UNDEFINED);
            type = value.type;
            iassert(!(value.type & ~VAR_MASK));
            iassert(!(entryValue->w.type & VAR_MASK));
            --v13;
            v20 = type | entryValue->w.type;
            entryValue->u.u = value.u;
            entryValue->w.type = v20;
        } while (v13);
    }
}

void __cdecl Scr_ReadGameEntry(MemoryFile *memFile)
{
    int type; // r28
    unsigned int gameId; // r11
    VariableUnion v4; // r8
    VariableValue v5; // [sp+50h] [-30h] BYREF

    if (scrVarPub.gameId)
        MyAssertHandler("c:\\trees\\cod3\\cod3src\\src\\script\\scr_readwrite.cpp", 910, 0, "%s", "!scrVarPub.gameId");
    scrVarPub.gameId = AllocValue();
    Scr_DoLoadEntryInternal(&v5, memFile);
    type = v5.type;
    if ((v5.type & ~VAR_MASK) != 0)
        MyAssertHandler(
            "c:\\trees\\cod3\\cod3src\\src\\script\\scr_readwrite.cpp",
            915,
            0,
            "%s",
            "!(tempValue.type & ~VAR_MASK)");
    gameId = scrVarPub.gameId;
    v4.intValue = (int)v5.u.intValue;
    scrVarGlob.variableList[gameId + VARIABLELIST_CHILD_BEGIN].w.type |= type;
    scrVarGlob.variableList[gameId + VARIABLELIST_CHILD_BEGIN].u.u = v4;
}

static void Scr_RemoveElementValue(Scr_WatchElement_s *element)
{
    Scr_WatchElement_s *i; // r31

    if (element->valueDefined)
    {
        element->valueDefined = 0;
        RemoveRefToValue(element->value.type, element->value.u);
    }
    for (i = element->childHead; i; i = i->next)
        Scr_RemoveElementValue(i);
}

void Scr_RemoveElementValues()
{
    Scr_WatchElement_s *i; // r30
    Scr_WatchElement_s *j; // r31

    for (i = scrDebuggerGlob.scriptWatch.elementHead; i; i = i->next)
    {
        if (i->valueDefined)
        {
            i->valueDefined = 0;
            RemoveRefToValue(i->value.type, i->value.u);
        }
        for (j = i->childHead; j; j = j->next)
            Scr_RemoveElementValue(j);
        if (!i->breakpoint)
        {
            if (!i->expr.exprHead)
                MyAssertHandler("c:\\trees\\cod3\\cod3src\\src\\script\\scr_debugger.cpp", 8570, 0, "%s", "expr->exprHead");
            Scr_ClearDebugExpr(i->expr.exprHead);
        }
    }
}

static void Scr_AddDebuggerRefs()
{
    if (scrVarPub.developer)
    {
        if (scrVmPub.function_count)
            MyAssertHandler(
                "c:\\trees\\cod3\\cod3src\\src\\script\\scr_debugger.cpp",
                8605,
                0,
                "%s",
                "!scrVmPub.function_count");
        scrDebuggerGlob.scriptWatch.localId = 0;
        iassert(!scrVarPub.evaluate);
        scrVarPub.evaluate = 1;
        //Scr_ScriptWatch::Evaluate(&scrDebuggerGlob.scriptWatch);
        scrDebuggerGlob.scriptWatch.Evaluate();
        //Scr_ScriptWatch::UpdateBreakpoints(&scrDebuggerGlob.scriptWatch, 1);
        scrDebuggerGlob.scriptWatch.UpdateBreakpoints(true);
        iassert(scrVarPub.evaluate);
        scrVarPub.evaluate = 0;
    }
}

static void Scr_RemoveDebuggerRefs()
{
    if (scrVarPub.developer)
    {
        if (!Scr_IsStackClear())
            MyAssertHandler("c:\\trees\\cod3\\cod3src\\src\\script\\scr_debugger.cpp", 8582, 0, "%s", "Scr_IsStackClear()");
        if (scrVmPub.function_count)
            MyAssertHandler(
                "c:\\trees\\cod3\\cod3src\\src\\script\\scr_debugger.cpp",
                8584,
                0,
                "%s",
                "!scrVmPub.function_count");
        scrDebuggerGlob.scriptWatch.localId = 0;
        if (scrVarPub.evaluate)
            MyAssertHandler("c:\\trees\\cod3\\cod3src\\src\\script\\scr_debugger.cpp", 8587, 0, "%s", "!scrVarPub.evaluate");
        scrVarPub.evaluate = 1;
        //Scr_ScriptWatch::UpdateBreakpoints(&scrDebuggerGlob.scriptWatch, 0);
        scrDebuggerGlob.scriptWatch.UpdateBreakpoints(false);
        Scr_RemoveElementValues();
        if (!scrVarPub.evaluate)
            MyAssertHandler("c:\\trees\\cod3\\cod3src\\src\\script\\scr_debugger.cpp", 8593, 0, "%s", "scrVarPub.evaluate");
        scrVarPub.evaluate = 0;
        if (!Scr_IsStackClear())
            MyAssertHandler("c:\\trees\\cod3\\cod3src\\src\\script\\scr_debugger.cpp", 8596, 0, "%s", "Scr_IsStackClear()");
    }
}

void __cdecl Scr_SaveShutdown(bool savegame)
{
    char v2; // r20
    unsigned __int16 *v3; // r25
    int v4; // r30
    VariableValueInternal_w *p_w; // r27
    int v6; // r26
    const char *v7; // r31

    v2 = 0;
    if (scrVarDebugPub)
    {
        v3 = &scrVarPub.saveIdMap[1];
        v4 = 1;
        v6 = 0x7FFF;
        do
        {
            // Was a VariableValueInternal_w walk (p_w += 4, entry at p_w[-2]) sized for 16-byte 32-bit entries.
            VariableValueInternal *shutdownEntry = &scrVarGlob.variableList[v4 + 1];
            p_w = &shutdownEntry->w;
            if ((p_w->type & 0x60) != 0)
            {
                if (!IsObject(shutdownEntry))
                    MyAssertHandler(
                        "c:\\trees\\cod3\\cod3src\\src\\script\\scr_readwrite.cpp",
                        968,
                        0,
                        "%s",
                        "IsObject( entryValue )");
                v7 = scrVarDebugPub->varUsage[v4 + 1];
                if (!v7)
                    MyAssertHandler("c:\\trees\\cod3\\cod3src\\src\\script\\scr_readwrite.cpp", 970, 0, "%s", "pos");
                if (!*v3 && (p_w->type & VAR_MASK) != VAR_ARRAY)
                {
                    if (!v2)
                    {
                        v2 = 1;
                        Com_Printf(CON_CHANNEL_PARSERSCRIPT, "\n****script variable cyclic leak*****\n");
                    }
                    Scr_PrintPrevCodePos(CON_CHANNEL_PARSERSCRIPT, (char*)v7, 0);
                }
            }
            --v6;
            ++v4;
            ++v3;
        } while (v6);
    }
    Scr_AddDebuggerRefs();
    if (v2)
    {
        Com_Printf(CON_CHANNEL_PARSERSCRIPT, "************************************\n");
        if (savegame)
            Com_Error(ERR_DROP, "Script variable leak due to cyclic usage (see console for details)");
        else
            Com_Printf(CON_CHANNEL_PARSERSCRIPT, "script variable leak due to cyclic usage\n");
    }
}

void __cdecl Scr_LoadPre(int sys, MemoryFile *memFile)
{
    unsigned __int16 savecount; // r11
    unsigned int v4; // r30
    unsigned __int16 *v5; // r29
    unsigned int v6; // r3
    unsigned int v7; // r29
    unsigned __int16 *v8; // r30
    unsigned int v9; // r30
    unsigned int Id; // r3
    unsigned int v11; // r30
    unsigned int v12; // r3
    unsigned int v13; // r30
    unsigned int v14; // r3
    unsigned int v15; // r30
    unsigned int v16; // r3
    unsigned int v17; // r30
    unsigned int v18; // r3
    int v19; // r29
    unsigned __int16 *p_entArrayId; // r31
    unsigned __int16 v21; // r10
    scrVarDebugPub_t *v22; // r11
    unsigned __int8 v23[96]; // [sp+50h] [-60h] BYREF

    if (sys != 1)
        MyAssertHandler("c:\\trees\\cod3\\cod3src\\src\\script\\scr_readwrite.cpp", 1008, 0, "%s", "sys == SCR_SYS_GAME");
    if (scrVarPub.varUsagePos)
        MyAssertHandler("c:\\trees\\cod3\\cod3src\\src\\script\\scr_readwrite.cpp", 1011, 0, "%s", "!scrVarPub.varUsagePos");
    scrVarPub.varUsagePos = "<save game variable>";
    memset(scrVmDebugPub.profileEnable, 0, sizeof(scrVmDebugPub.profileEnable));
    MemFile_ReadData(memFile, 4, v23);
    scrVarPub.time = *(unsigned int *)v23;
    MemFile_ReadData(memFile, 2, v23);
    scrVarPub.savecount = *(_WORD *)v23;
    Com_Memset(scrVarPub.saveIdMap, 0, 0x10000);
    Com_Memset(scrVarPub.saveIdMapRev, 0, 0x10000);
    Scr_ResetSaveIdHistory();
    savecount = scrVarPub.savecount;
    v4 = 1;
    if (scrVarPub.savecount)
    {
        v5 = &scrVarPub.saveIdMapRev[1];
        do
        {
            v6 = AllocObject();
            *v5++ = v6;
            scrVarPub.saveIdMap[v6] = v4++;
            savecount = scrVarPub.savecount;
        } while (v4 <= scrVarPub.savecount);
    }
    v7 = 1;
    if (savecount)
    {
        v8 = &scrVarPub.saveIdMapRev[1];
        do
        {
            Scr_DoLoadObjectInfo(*v8, memFile);
            ++v7;
            ++v8;
        } while (v7 <= scrVarPub.savecount);
    }
    Scr_ReadGameEntry(memFile);
    MemFile_ReadData(memFile, 1, v23);
    v9 = v23[0];
    if (scrVarPub.levelId)
        MyAssertHandler("c:\\trees\\cod3\\cod3src\\src\\script\\scr_readwrite.cpp", 1042, 0, "%s", "!scrVarPub.levelId");
    Id = Scr_ReadId(memFile, v9);
    scrVarPub.levelId = Id;
    if (scrVarDebugPub)
        ++scrVarDebugPub->extRefCount[Id];
    MemFile_ReadData(memFile, 1, v23);
    v11 = v23[0];
    if (scrVarPub.animId)
        MyAssertHandler("c:\\trees\\cod3\\cod3src\\src\\script\\scr_readwrite.cpp", 1050, 0, "%s", "!scrVarPub.animId");
    v12 = Scr_ReadId(memFile, v11);
    scrVarPub.animId = v12;
    if (scrVarDebugPub)
        ++scrVarDebugPub->extRefCount[v12];
    MemFile_ReadData(memFile, 1, v23);
    v13 = v23[0];
    if (scrVarPub.timeArrayId)
        MyAssertHandler("c:\\trees\\cod3\\cod3src\\src\\script\\scr_readwrite.cpp", 1058, 0, "%s", "!scrVarPub.timeArrayId");
    v14 = Scr_ReadId(memFile, v13);
    scrVarPub.timeArrayId = v14;
    if (scrVarDebugPub)
        ++scrVarDebugPub->extRefCount[v14];
    MemFile_ReadData(memFile, 1, v23);
    v15 = v23[0];
    if (scrVarPub.pauseArrayId)
        MyAssertHandler(
            "c:\\trees\\cod3\\cod3src\\src\\script\\scr_readwrite.cpp",
            1066,
            0,
            "%s",
            "!scrVarPub.pauseArrayId");
    v16 = Scr_ReadId(memFile, v15);
    scrVarPub.pauseArrayId = v16;
    if (scrVarDebugPub)
        ++scrVarDebugPub->extRefCount[v16];
    MemFile_ReadData(memFile, 1, v23);
    v17 = v23[0];
    if (scrVarPub.freeEntList)
        MyAssertHandler("c:\\trees\\cod3\\cod3src\\src\\script\\scr_readwrite.cpp", 1074, 0, "%s", "!scrVarPub.freeEntList");
    v18 = Scr_ReadId(memFile, v17);
    scrVarPub.freeEntList = v18;
    if (scrVarDebugPub)
        ++scrVarDebugPub->extRefCount[v18];
    for (auto &scriptClass : g_classMap)
    {
        p_entArrayId = &scriptClass.entArrayId;
        if (GetArraySize(*p_entArrayId))
            MyAssertHandler(
                "c:\\trees\\cod3\\cod3src\\src\\script\\scr_readwrite.cpp",
                1083,
                0,
                "%s",
                "!GetArraySize( g_classMap[classnum].entArrayId )");
        if (scrVarDebugPub)
            --scrVarDebugPub->extRefCount[*p_entArrayId];
        RemoveRefToObject(*p_entArrayId);
        MemFile_ReadData(memFile, 1, v23);
        v21 = Scr_ReadId(memFile, v23[0]);
        v22 = scrVarDebugPub;
        *p_entArrayId = v21;
        if (v22)
            ++v22->extRefCount[v21];
    }
}

static void Scr_AddDebugExprValueRefCount(unsigned __int16 *refCount, sval_u *val)
{
    if (val->type == 81)
        ++refCount[val[1].type];
}

static void Scr_AddDebugExprRefCount(unsigned __int16 *refCount, sval_u *debugExprHead)
{
    sval_u *i; // r31

    // The next link is the whole first sval_u slot; reading it through the 4-byte type field truncated the pointer.
    for (i = debugExprHead; i; i = i->node)
        Scr_AddDebugExprValueRefCount(refCount, i + 1);
}

static void Scr_AddDebugRefCountChildren(Scr_WatchElement_s *element, unsigned __int16 *refCount)
{
    Scr_WatchElement_s *i; // r31

    if (element->valueDefined && element->value.type == VAR_POINTER)
        ++refCount[element->value.u.intValue];
    for (i = element->childHead; i; i = i->next)
        Scr_AddDebugRefCountChildren(i, refCount);
}

static void Scr_AddDebugRefCount(unsigned __int16 *refCount)
{
    Scr_WatchElement_s *i; // r31

    for (i = scrDebuggerGlob.scriptWatch.elementHead; i; i = i->next)
    {
        if (!i->breakpoint)
        {
            if (!i->expr.exprHead)
                MyAssertHandler("c:\\trees\\cod3\\cod3src\\src\\script\\scr_debugger.cpp", 7231, 0, "%s", "expr->exprHead");
            Scr_AddDebugExprRefCount(refCount, (sval_u*)i->expr.exprHead);
        }
        Scr_AddDebugRefCountChildren(i, refCount);
    }
}

static bool Scr_IsVariableBreakpoint(unsigned int id)
{
    Scr_WatchElementDoubleNode_t **variableBreakpoints; // r11

    variableBreakpoints = scrDebuggerGlob.variableBreakpoints;
    if (!scrDebuggerGlob.variableBreakpoints)
    {
        MyAssertHandler(
            "c:\\trees\\cod3\\cod3src\\src\\script\\scr_debugger.cpp",
            7193,
            0,
            "%s",
            "scrDebuggerGlob.variableBreakpoints");
        variableBreakpoints = scrDebuggerGlob.variableBreakpoints;
    }
    return variableBreakpoints[id] != 0;
}

static void CheckReferenceRange(unsigned int begin, unsigned int end)
{
    if ((int)(end - begin) <= 1)
        return;

    for (unsigned int parentType = 1; (int)parentType < (int)(end - begin); ++parentType)
    {
        VariableValueInternal *entry = &scrVarGlob.variableList[begin + parentType];
        unsigned int status = entry->w.status;
        if ((status & 0x60) == 0)
            continue;

        switch (status & VAR_MASK)
        {
        case VAR_POINTER:
            ++scrVarDebugPub->refCount[entry->u.u.intValue];
            break;

        case VAR_STACK:
        {
            VariableStackBuffer *sb = entry->u.u.stackValue;
            if (!sb->localId)
                MyAssertHandler(
                    "c:\\trees\\cod3\\cod3src\\src\\script\\scr_variable.cpp",
                    271,
                    0,
                    "%s",
                    "entryValue->u.u.stackValue->localId");
            ++scrVarDebugPub->refCount[sb->localId];
            unsigned __int8 *p = (unsigned __int8 *)&sb->buf[0];
            unsigned int count = sb->size;
            while (count)
            {
                unsigned int entryType = *p;
                const auto entryVal = Scr_ReadStackValue(p + 1).pointerValue;
                p += SCR_STACK_ENTRY_BYTES;
                --count;
                if (entryType == VAR_POINTER)
                    ++scrVarDebugPub->refCount[entryVal];
            }
            break;
        }

        case VAR_THREAD:
        case VAR_NOTIFY_THREAD:
        case VAR_TIME_THREAD:
        case VAR_DEAD_ENTITY:
            ++scrVarDebugPub->refCount[entry->u.o.u.size];
            break;

        case VAR_CHILD_THREAD:
            ++scrVarDebugPub->refCount[GetParentLocalId(parentType)];
            ++scrVarDebugPub->refCount[entry->u.o.u.size];
            break;

        case VAR_ARRAY:
            for (unsigned int i = FindFirstSibling(parentType); i; i = FindNextSibling(i))
            {
                VariableValueInternal *child = &scrVarGlob.variableList[i + VARIABLELIST_CHILD_BEGIN];
                if (IsObject(child))
                    MyAssertHandler(
                        "c:\\trees\\cod3\\cod3src\\src\\script\\scr_variable.cpp",
                        261,
                        0,
                        "%s",
                        "!IsObject( entryValue2 )");

                VariableValue val = Scr_GetArrayIndexValue(child->w.status >> 8);
                if (val.type == VAR_POINTER)
                    ++scrVarDebugPub->refCount[val.u.intValue];
            }
            break;

        default:
            break;
        }
    }
}

static int CheckReferences()
{
    int v0; // r11
    unsigned int i; // r31
    int v2; // r30
    int v3; // r9
    unsigned int v4; // r7
    VariableValueInternal_w *j; // r11

    if (!scrVarDebugPub || scrStringDebugGlob && scrStringDebugGlob->ignoreLeaks)
        return 1;
    memcpy(scrVarDebugPub->refCount, scrVarDebugPub->extRefCount, sizeof(scrVarDebugPub->refCount));
    Scr_AddDebugRefCount(scrVarDebugPub->refCount);
    CheckReferenceRange(1u, 0x8001u);
    CheckReferenceRange(0x8002u, 0x18000u);
    if (scrVarPub.developer)
    {
        for (uint32_t id = 1; id < VARIABLELIST_PARENT_SIZE; ++id)
            if (Scr_IsVariableBreakpoint(id))
                ++scrVarDebugPub->refCount[id];
    }
    for (uint32_t id = 1; id < VARIABLELIST_PARENT_SIZE; ++id)
    {
        const auto &entry = scrVarGlob.variableList[id + VARIABLELIST_PARENT_BEGIN];
        if ((entry.w.status & 0x60) != 0 && (entry.w.type & VAR_MASK) >= VAR_THREAD)
        {
            const uint16_t references = scrVarDebugPub->refCount[id];
            if (!references || references != static_cast<uint16_t>(entry.u.o.refCount + 1))
                return 0;
        }
    }
    return 1;
}

void __cdecl Scr_LoadShutdown()
{
    unsigned int v0; // r30
    unsigned __int16 *v1; // r31

    v0 = 1;
    if (scrVarPub.savecount)
    {
        v1 = &scrVarPub.saveIdMapRev[1];
        do
        {
            RemoveRefToObject(*v1);
            ++v0;
            ++v1;
        } while (v0 <= scrVarPub.savecount);
    }
    Scr_InitDebuggerSystem();
    scrVarPub.varUsagePos = 0;
    if (!CheckReferences())
        MyAssertHandler("c:\\trees\\cod3\\cod3src\\src\\script\\scr_readwrite.cpp", 1115, 0, "%s", "CheckReferences()");
}



void __cdecl Scr_SaveSource(MemoryFile *memFile)
{
    MemFile_WriteData(memFile, 1, &scrVarPub.developer);
    MemFile_WriteData(memFile, 1, &scrVarPub.developer_script);
}

void __cdecl SaveMemory_SaveWriteImmediate(const void *buffer, unsigned int len, SaveImmediate *save)
{
    if (!save || !save->f || !len)
        return;
    const int handle = (int)(intptr_t)save->f;
    // Latch a failed metadata write. The caller retains the real handle for
    // closing, and must not commit a checkpoint with truncated script/demo data.
    if (!buffer || FS_Write((const char *)buffer, len, handle) != len)
        save->f = nullptr;
}

void __cdecl Scr_SaveSourceImmediate(SaveImmediate *save)
{
    if (!scrVarPub.developer)
        return;

    // Was a bool* walked in 44-byte steps (sizeof(SourceBufferInfo) in the 32-bit build). With
    // 64-bit pointers in that struct the stride is wrong, so the count written here disagreed
    // with the entries written below - and the loader then read garbage lengths.
    int countOut = 0;
    for (unsigned int index = 0; index < scrParserPub.sourceBufferLookupLen; ++index)
        if (scrParserPub.sourceBufferLookup[index].archive)
            ++countOut;
    SaveMemory_SaveWriteImmediate(&countOut, 4u, save);

    unsigned int i = 0;
    if (scrParserPub.sourceBufferLookupLen)
    {
        int idx = 0;
        do
        {
            SourceBufferInfo *info = &scrParserPub.sourceBufferLookup[idx];
            if (scrParserPub.sourceBufferLookup[idx].archive)
            {
                SaveMemory_SaveWriteImmediate(&info->len, 4u, save);
                int len = info->len;
                if (len > 0)
                    SaveMemory_SaveWriteImmediate(info->sourceBuf, (unsigned int)len, save);
            }
            ++i;
            ++idx;
        } while (i < scrParserPub.sourceBufferLookupLen);
    }
}

void __cdecl Scr_LoadSource(MemoryFile *memFile, void *fileHandle)
{
    SaveSourceBufferInfo *saveSourceBufferLookup; // r3
    signed int v5; // r25
    int v6; // r27
    SaveSourceBufferInfo *v7; // r31
    int *p_len; // r30
    int len; // r3
    char *v10; // r3
    int v11; // r4
    char v12; // [sp+50h] [-50h] BYREF

    if (scrParserGlob.saveSourceBufferLookup)
        MyAssertHandler(
            "c:\\trees\\cod3\\cod3src\\src\\script\\scr_readwrite.cpp",
            1236,
            0,
            "%s",
            "!scrParserGlob.saveSourceBufferLookup");
    MemFile_ReadData(memFile, 1, (unsigned char*)&v12);
    MemFile_ReadData(memFile, 1, (unsigned char*)&scrVarPub.developer_script);
    if (v12)
    {
        const int bytesRead = ReadFromDevice(
            &scrParserGlob.saveSourceBufferLookupLen, 4, fileHandle);
        if (bytesRead != 4 || scrParserGlob.saveSourceBufferLookupLen == 0)
        {
            scrParserGlob.saveSourceBufferLookupLen = 0;
            scrParserGlob.saveSourceBufferLookup = 0;
            return;
        }
        // sizeof, not 8: SaveSourceBufferInfo holds a pointer, so the entries are wider than the
        // 32-bit original. Allocating the old size overran the array and fed garbage lengths back.
        saveSourceBufferLookup = (SaveSourceBufferInfo *)Hunk_AllocDebugMem(
            sizeof(SaveSourceBufferInfo) * scrParserGlob.saveSourceBufferLookupLen,
            "Scr_LoadSource");
        scrParserGlob.saveSourceBufferLookup = saveSourceBufferLookup;
        v5 = scrParserGlob.saveSourceBufferLookupLen - 1;
        if ((signed int)(scrParserGlob.saveSourceBufferLookupLen - 1) >= 0)
        {
            v6 = v5;
            while (1)
            {
                v7 = &saveSourceBufferLookup[v6];
                p_len = &saveSourceBufferLookup[v6].len;
                ReadFromDevice(p_len, 4, fileHandle);
                len = v7->len;
                if (len <= 0)
                {
                    v7->sourceBuf = 0;
                }
                else
                {
                    v10 = (char *)Hunk_AllocDebugMem(len, "Scr_LoadSource");
                    v11 = *p_len;
                    v7->sourceBuf = v10;
                    ReadFromDevice(v10, v11, fileHandle);
                }
                --v5;
                --v6;
                if (v5 < 0)
                    break;
                saveSourceBufferLookup = scrParserGlob.saveSourceBufferLookup;
            }
        }
    }
}

void __cdecl Scr_SkipSource(MemoryFile *memFile, void *fileHandle)
{
    int i; // r30
    _BYTE v5[4]; // [sp+50h] [-30h] BYREF
    int v6; // [sp+54h] [-2Ch] BYREF
    int v7[4]; // [sp+58h] [-28h] BYREF

    MemFile_ReadData(memFile, 1, v5);
    MemFile_ReadData(memFile, 1, (unsigned char*)&scrVarPub.developer_script);
    if (v5[0])
    {
        if (fileHandle)
        {
            v6 = 0;
            const int bytesRead = ReadFromDevice(&v6, 4, fileHandle);
            if (bytesRead != 4 || v6 <= 0)
                return;
            for (i = v6 - 1; i >= 0; --i)
            {
                ReadFromDevice(v7, 4, fileHandle);
                if (v7[0] > 0)
                    ReadFromDevice(0, v7[0], fileHandle);
            }
        }
    }
}

void __cdecl AddSaveStackInternal(const VariableStackBuffer *stackBuf)
{
    AddSaveEntryInternal(VAR_POINTER, VariableUnion(static_cast<int>(stackBuf->localId)));
    for (std::size_t i = 0; i < stackBuf->size; ++i)
    {
        const auto value = Scr_ReadStackEntry(stackBuf->buf + i * SCR_STACK_ENTRY_BYTES);
        AddSaveEntryInternal(value.type, value.u);
    }
}

void __cdecl AddSaveEntryInternal(unsigned int type, VariableUnion u)
{
    if (type == VAR_POINTER)
    {
        const auto id = u.pointerValue;
        if (id && !scrVarPub.saveIdMap[id])
        {
            scrVarPub.saveIdMap[id] = ++scrVarPub.savecount;
            scrVarPub.saveIdMapRev[scrVarPub.savecount] = id;
        }
    }
    else if (type == VAR_STACK)
    {
        AddSaveStackInternal(u.stackValue);
    }
}

// local variable allocation has failed, the output may be wrong!
void __cdecl DoSaveEntry(VariableValue *value, VariableValue *name, bool isArray, MemoryFile *memFile)
{
    unsigned int UsedSize; // r3
    unsigned int v9; // r3
    unsigned int v10; // r3
    unsigned int v11; // r4
    unsigned int v12; // r3
    unsigned int v13; // r3
    unsigned int v14; // r3
    VariableValue *ArrayIndexValue; // r3 OVERLAPPED
    unsigned int v16; // r3
    int v17; // r30
    unsigned int v18; // r3
    unsigned int v19; // r3
    unsigned int v20; // r3
    unsigned int v21; // r3
    unsigned int v22; // r3
    const char *v23; // r3
    unsigned int v24; // r3
    unsigned int v25; // r3
    unsigned int v26; // r3
    unsigned int v27[2]; // [sp+50h] [-40h] BYREF
    __int64 v28; // [sp+58h] [-38h]

    if (!value)
        MyAssertHandler("c:\\trees\\cod3\\cod3src\\src\\script\\scr_readwrite.cpp", 391, 0, "%s", "value");
    UsedSize = MemFile_GetUsedSize(memFile);
    //ProfMem_Begin("DoSaveEntry", UsedSize);
    v9 = MemFile_GetUsedSize(memFile);
    //ProfMem_Begin("DoSaveEntryInternal", v9);
    DoSaveEntryInternal(value->type, value->u, memFile);
    v10 = MemFile_GetUsedSize(memFile);
    //ProfMem_End(v10);
    if (!isArray)
    {
        v12 = MemFile_GetUsedSize(memFile);
        //ProfMem_Begin("non-array", v12);
        if (((unsigned int)name & 0xFF000000) != 0)
            MyAssertHandler(
                "c:\\trees\\cod3\\cod3src\\src\\script\\scr_readwrite.cpp",
                402,
                0,
                "%s\n\t(name) = %i",
                "(!(name & 0xFF000000))",
                name);
        v27[0] = (v27[0] & 0xFFFFFF00) | ((uint8_t)name);
        MemFile_WriteData(memFile, 1, v27);
        v27[0] = (v27[0] & 0xFFFFFF00) | (((uint32_t)name >> 8) & 0xFF);
        MemFile_WriteData(memFile, 1, v27);
        v27[0] = (v27[0] & 0xFFFFFF00) | (((uint32_t)name >> 16) & 0xFF);
        MemFile_WriteData(memFile, 1, v27);
        v13 = MemFile_GetUsedSize(memFile);
        //ProfMem_End(v13);
        v14 = MemFile_GetUsedSize(memFile);
        //ProfMem_End(v14);
        return;
    }
    VariableValue arrVal = Scr_GetArrayIndexValue((unsigned int)name);
    if (arrVal.type == VAR_POINTER)
    {
        WriteId(arrVal.u.intValue, 5u, memFile);
        return;
    }
    if (arrVal.type == VAR_STRING)
    {
        v27[0] = (v27[0] & 0xFFFFFF00) | 4;
        MemFile_WriteData(memFile, 1, v27);
        v23 = SL_ConvertToString((unsigned int)arrVal.u.intValue & 0xFFFF);
        MemFile_WriteCString(memFile, v23);
        return;
    }
    if (arrVal.type != VAR_INTEGER)
    {
        if (!alwaysfails)
            MyAssertHandler("c:\\trees\\cod3\\cod3src\\src\\script\\scr_readwrite.cpp", 449, 1, "bad case");
        return;
    }
    v17 = arrVal.u.intValue;
    if (v17)
    {
        if (v17 < -128 || v17 >= 128)
        {
            if (v17 < -32768 || v17 >= 0x8000)
            {
                v27[0] = (v27[0] & 0xFFFFFF00) | 3;
                MemFile_WriteData(memFile, 1, v27);
                v27[0] = v17;
                MemFile_WriteData(memFile, 4, v27);
            }
            else
            {
                v27[0] = (v27[0] & 0xFFFFFF00) | 2;
                MemFile_WriteData(memFile, 1, v27);
                v27[0] = (v27[0] & 0xFFFF0000) | ((uint32_t)v17 & 0xFFFF);
                MemFile_WriteData(memFile, 2, v27);
            }
        }
        else
        {
            v27[0] = (v27[0] & 0xFFFFFF00) | 1;
            MemFile_WriteData(memFile, 1, v27);
            v27[0] = (v27[0] & 0xFFFFFF00) | ((uint32_t)v17 & 0xFF);
            MemFile_WriteData(memFile, 1, v27);
        }
    }
    else
    {
        v27[0] = v27[0] & 0xFFFFFF00;
        MemFile_WriteData(memFile, 1, v27);
    }
}

void __cdecl AddSaveObjectChildren(unsigned int parentId)
{
    VariableValueInternal *parentValue; // r23
    int parentType; // r24
    unsigned int i; // r29
    VariableValueInternal *entryValue; // r30
    unsigned int v6; // r4
    int v7; // r2
    VariableValueInternal_u u; // r3
    int v9; // r11
    VariableValueInternal_w w; // r11
    int v11; // r9
    int size; // r9

    parentValue = &scrVarGlob.variableList[parentId + 1];

    iassert((parentValue->w.status & VAR_STAT_MASK) != VAR_STAT_FREE);
    iassert(IsObject(parentValue));
    parentType = parentValue->w.type & 0x1F;
    for (i = FindLastSibling(parentId); i; i = FindPrevSibling(i))
    {
        entryValue = &scrVarGlob.variableList[VARIABLELIST_CHILD_BEGIN + scrVarGlob.variableList[i + VARIABLELIST_CHILD_BEGIN].hash.id];
        iassert(!IsObject(entryValue));
        if (parentType == VAR_ARRAY)
        {
            VariableValue arrVal = Scr_GetArrayIndexValue((unsigned int)entryValue->w.status >> 8);
            if (arrVal.type == VAR_POINTER
                && arrVal.u.intValue
                && !scrVarPub.saveIdMap[arrVal.u.intValue])
            {
                scrVarPub.saveIdMap[arrVal.u.intValue] = ++scrVarPub.savecount;
                scrVarPub.saveIdMapRev[scrVarPub.savecount] = (unsigned __int16)arrVal.u.intValue;
            }
        }
        u = entryValue->u;
        v9 = entryValue->w.type & 0x1F;
        if (v9 == VAR_POINTER)
        {
            if (u.u.intValue && !scrVarPub.saveIdMap[u.u.intValue])
            {
                scrVarPub.saveIdMap[u.u.intValue] = ++scrVarPub.savecount;
                scrVarPub.saveIdMapRev[scrVarPub.savecount] = (unsigned __int16)u.u.intValue;
            }
        }
        else if (v9 == VAR_STACK)
        {
            AddSaveStackInternal(u.u.stackValue);
        }
    }
    switch (parentType)
    {
    case VAR_THREAD:
    case VAR_NOTIFY_THREAD:
    case VAR_TIME_THREAD:
    case VAR_DEAD_ENTITY:
        goto LABEL_24;
    case VAR_CHILD_THREAD:
        w = parentValue->w;
        v11 = (unsigned __int16)((unsigned int)w.status >> 8);
        if ((unsigned __int16)((unsigned int)w.status >> 8) && !scrVarPub.saveIdMap[v11])
        {
            scrVarPub.saveIdMap[v11] = ++scrVarPub.savecount;
            *(unsigned __int16 *)((char *)scrVarPub.saveIdMapRev + __ROL4__(scrVarPub.savecount, 1)) = v11;
        }
    LABEL_24:
        size = parentValue->u.o.u.size;
        if (parentValue->u.o.u.size)
        {
            if (!scrVarPub.saveIdMap[size])
            {
                scrVarPub.saveIdMap[size] = ++scrVarPub.savecount;
                *(unsigned __int16 *)((char *)scrVarPub.saveIdMapRev + __ROL4__(scrVarPub.savecount, 1)) = size;
            }
        }
        break;
    default:
        return;
    }
}

void __cdecl AddSaveObject(unsigned int parentId)
{
    unsigned __int16 savecount; // r11
    int v2; // r29
    unsigned __int16 *v3; // r30

    savecount = scrVarPub.savecount;
    v2 = scrVarPub.savecount;
    if (parentId && !scrVarPub.saveIdMap[parentId])
    {
        ++scrVarPub.savecount;
        scrVarPub.saveIdMap[parentId] = v2 + 1;
        *(unsigned __int16 *)((char *)scrVarPub.saveIdMapRev + __ROL4__(scrVarPub.savecount, 1)) = parentId;
        savecount = scrVarPub.savecount;
    }
    if (v2 < savecount)
    {
        v3 = &scrVarPub.saveIdMapRev[v2];
        do
        {
            ++v3;
            ++v2;
            AddSaveObjectChildren(*v3);
        } while (v2 < scrVarPub.savecount);
    }
}

void __cdecl DoSaveObjectInfo(unsigned int parentId, MemoryFile *memFile)
{
    VariableValueInternal *v4; // r31
    int v5; // r30
    unsigned int UsedSize; // r3
    unsigned int v7; // r3
    int v8; // r4
    bool v9; // r26
    unsigned int v10; // r3
    __int16 v11; // r31
    unsigned int i; // r3
    unsigned int j; // r30
    VariableValueInternal *v14; // r31
    int v15; // r11
    VariableValueInternal_w w; // r11
    unsigned int v17; // r3
    VariableValue v18[12]; // [sp+50h] [-60h] BYREF

    v4 = &scrVarGlob.variableList[parentId + 1];
    if ((v4->w.status & 0x60) != 0x60)
        MyAssertHandler(
            "c:\\trees\\cod3\\cod3src\\src\\script\\scr_readwrite.cpp",
            655,
            0,
            "%s",
            "(parentValue->w.status & VAR_STAT_MASK) == VAR_STAT_EXTERNAL");
    if (!IsObject(v4))
        MyAssertHandler("c:\\trees\\cod3\\cod3src\\src\\script\\scr_readwrite.cpp", 656, 0, "%s", "IsObject( parentValue )");
    v5 = v4->w.type & 0x1F;
    switch (v5)
    {
    case VAR_THREAD:
        WriteId(v4->u.o.u.size, 1u, memFile);
        goto LABEL_14;
    case VAR_NOTIFY_THREAD:
        UsedSize = MemFile_GetUsedSize(memFile);
        //ProfMem_Begin("VAR_NOTIFY_THREAD", UsedSize);
        WriteId(v4->u.o.u.size, 2u, memFile);
        SafeWriteString((unsigned int)v4->w.status >> 8, memFile);
        v7 = MemFile_GetUsedSize(memFile);
        //ProfMem_End(v7);
        goto LABEL_14;
    case VAR_TIME_THREAD:
        WriteId(v4->u.o.u.size, 3u, memFile);
        v8 = 4;
        v18[0].u.intValue = (unsigned int)v4->w.status >> 8;
        goto LABEL_13;
    case VAR_CHILD_THREAD:
        WriteId(v4->u.o.u.size, 4u, memFile);
        WriteId((unsigned __int16)((unsigned int)v4->w.status >> 8), 0, memFile);
        goto LABEL_14;
    case VAR_DEAD_ENTITY:
        WriteId(v4->u.o.u.size, 5u, memFile);
        goto LABEL_14;
    case VAR_ENTITY:
        v18[0].u.intValue = (v18[0].u.intValue & 0xFFFFFF00) | (uint8_t)(-96);
        MemFile_WriteData(memFile, 1, v18);
        v18[0].u.intValue = (v18[0].u.intValue & 0xFFFF0000) | (v4->u.o.u.size & 0xFFFF);
        MemFile_WriteData(memFile, 2, v18);
        v8 = 2;
        v18[0].u.intValue = (v18[0].u.intValue & 0xFFFF0000) | (((unsigned int)v4->w.status >> 8) & 0xFFFF);
        goto LABEL_13;
    default:
        v8 = 1;
        v18[0].u.intValue = (v18[0].u.intValue & 0xFFFFFF00) | ((uint32_t)(8 * v5) & 0xFF);
    LABEL_13:
        MemFile_WriteData(memFile, v8, v18);
    LABEL_14:
        v9 = v5 == VAR_ARRAY;
        v10 = MemFile_GetUsedSize(memFile);
        //ProfMem_Begin("children", v10);
        v11 = 0;
        for (i = FindFirstSibling(parentId); i; i = FindNextSibling(i))
            ++v11;
        v18[0].u.intValue = (v18[0].u.intValue & 0xFFFF0000) | ((uint32_t)v11 & 0xFFFF);
        MemFile_WriteData(memFile, 2, v18);
        for (j = FindLastSibling(parentId); j; j = FindPrevSibling(j))
        {
            v14 = &scrVarGlob.variableList[VARIABLELIST_CHILD_BEGIN + scrVarGlob.variableList[j + VARIABLELIST_CHILD_BEGIN].hash.id];
            v15 = v14->w.status & 0x60;
            if (!v15 || v15 == 96)
                MyAssertHandler(
                    "c:\\trees\\cod3\\cod3src\\src\\script\\scr_readwrite.cpp",
                    715,
                    0,
                    "%s",
                    "(entryValue->w.status & VAR_STAT_MASK) != VAR_STAT_FREE && (entryValue->w.status & VAR_STAT_MASK) != VAR_STAT_EXTERNAL");
            if (IsObject(v14))
                MyAssertHandler(
                    "c:\\trees\\cod3\\cod3src\\src\\script\\scr_readwrite.cpp",
                    716,
                    0,
                    "%s",
                    "!IsObject( entryValue )");
            w = v14->w;
            // Copy the whole value union: intValue is only the low 4 bytes of vector/codepos/stack pointers on 64-bit.
            v18[0].u = v14->u.u;
            v18[0].type = (Vartype_t)(w.type & 0x1F);
            DoSaveEntry(v18, (VariableValue *)((unsigned int)w.status >> 8), v9, memFile);
        }
        v17 = MemFile_GetUsedSize(memFile);
        //ProfMem_End(v17);
        return;
    }
}

int __cdecl Scr_ConvertThreadToSave(unsigned __int16 handle)
{
    int v1; // r31
    int v3; // r31

    v1 = handle;
    if (!handle)
        return 0;
    AddSaveObject(handle);
    v3 = v1;
    if (!scrVarPub.saveIdMap[v3])
        MyAssertHandler(
            "c:\\trees\\cod3\\cod3src\\src\\script\\scr_readwrite.cpp",
            736,
            0,
            "%s",
            "scrVarPub.saveIdMap[handle]");
    return scrVarPub.saveIdMap[v3];
}

void __cdecl WriteGameEntry(MemoryFile *memFile)
{
    DoSaveEntryInternal(
        scrVarGlob.variableList[scrVarPub.gameId + VARIABLELIST_CHILD_BEGIN].w.type & 0x1F,
        scrVarGlob.variableList[scrVarPub.gameId + VARIABLELIST_CHILD_BEGIN].u.u,
        memFile);
}

void __cdecl Scr_SavePost(MemoryFile *memFile)
{
    unsigned int UsedSize; // r3
    unsigned int v3; // r29
    unsigned __int16 *v4; // r28
    unsigned int v5; // r3
    int v6; // r30
    unsigned __int16 *p_entArrayId; // r29
    unsigned int v8[12]; // [sp+50h] [-30h] BYREF

    Scr_ResetSaveIdHistory();
    v8[0] = scrVarPub.time;
    MemFile_WriteData(memFile, 4, v8);
    v8[0] = (v8[0] & 0xFFFF0000) | ((uint32_t)scrVarPub.savecount & 0xFFFF);
    MemFile_WriteData(memFile, 2, v8);
    UsedSize = MemFile_GetUsedSize(memFile);
    //ProfMem_Begin("DoSaveObjectInfo", UsedSize);
    v3 = 1;
    if (scrVarPub.savecount)
    {
        v4 = &scrVarPub.saveIdMapRev[1];
        do
        {
            DoSaveObjectInfo(*v4, memFile);
            ++v3;
            ++v4;
        } while (v3 <= scrVarPub.savecount);
    }
    v5 = MemFile_GetUsedSize(memFile);
    //ProfMem_End(v5);
    DoSaveEntryInternal(
        scrVarGlob.variableList[scrVarPub.gameId + VARIABLELIST_CHILD_BEGIN].w.type & VAR_MASK,
        scrVarGlob.variableList[scrVarPub.gameId + VARIABLELIST_CHILD_BEGIN].u.u,
        memFile);
    WriteId(scrVarPub.levelId, 0, memFile);
    WriteId(scrVarPub.animId, 0, memFile);
    WriteId(scrVarPub.timeArrayId, 0, memFile);
    WriteId(scrVarPub.pauseArrayId, 0, memFile);
    WriteId(scrVarPub.freeEntList, 0, memFile);
    for (const auto &scriptClass : g_classMap)
        WriteId(scriptClass.entArrayId, 0, memFile);
}

void __cdecl AddSaveStack(const VariableStackBuffer *stackBuf)
{
    AddSaveObject(stackBuf->localId);
    for (std::size_t i = 0; i < stackBuf->size; ++i)
    {
        const auto value = Scr_ReadStackEntry(stackBuf->buf + i * SCR_STACK_ENTRY_BYTES);
        AddSaveEntry(value.type, value.u);
    }
}

void __cdecl AddSaveEntry(unsigned int type, VariableUnion u)
{
    if (type == VAR_POINTER)
        AddSaveObject(u.pointerValue);
    else if (type == VAR_STACK)
        AddSaveStack(u.stackValue);
}

void __cdecl Scr_SavePre(int sys)
{
    int v2; // r30
    unsigned __int16 *p_entArrayId; // r29
    VariableValueInternal *v4; // r11
    const VariableStackBuffer *stackValue; // r3
    int v6; // r11

    if (!scrVarPub.timeArrayId)
        MyAssertHandler("c:\\trees\\cod3\\cod3src\\src\\script\\scr_readwrite.cpp", 871, 0, "%s", "scrVarPub.timeArrayId");
    if (!CheckReferences())
        MyAssertHandler("c:\\trees\\cod3\\cod3src\\src\\script\\scr_readwrite.cpp", 874, 0, "%s", "CheckReferences()");
    if (sys != 1)
        MyAssertHandler("c:\\trees\\cod3\\cod3src\\src\\script\\scr_readwrite.cpp", 876, 0, "%s", "sys == SCR_SYS_GAME");
    Scr_RemoveDebuggerRefs();
    Com_Memset(scrVarPub.saveIdMap, 0, 0x10000);
    Com_Memset(scrVarPub.saveIdMapRev, 0, 0x10000);
    scrVarPub.savecount = 0;
    AddSaveObject(scrVarPub.levelId);
    AddSaveObject(scrVarPub.animId);
    AddSaveObject(scrVarPub.timeArrayId);
    AddSaveObject(scrVarPub.pauseArrayId);
    AddSaveObject(scrVarPub.freeEntList);
    for (const auto &scriptClass : g_classMap)
        AddSaveObject(scriptClass.entArrayId);
    v4 = &scrVarGlob.variableList[scrVarPub.gameId + VARIABLELIST_CHILD_BEGIN];
    stackValue = v4->u.u.stackValue;
    v6 = v4->w.type & VAR_MASK;
    if (v6 == VAR_POINTER)
    {
        AddSaveObject(v4->u.u.pointerValue);
    }
    else if (v6 == VAR_STACK)
    {
        AddSaveStack(stackValue);
    }
}
