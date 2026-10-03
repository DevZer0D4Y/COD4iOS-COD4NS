#include "scr_value_io.h"
#include "scr_stack_buffer.h"
#include "scr_variable_state.h"
#include "scr_compile_state.h"
#include "scr_stringlist.h"
#include "scr_vector.h"
#include <universal/memfile.h>
#include <cmath>

#ifndef IS_NAN
#define IS_NAN(x) std::isnan(x)
#endif

// Engine services supplied by scr_main/scr_variable in the full runtime.
void AddRefToObject(uint32_t id);

static unsigned int g_idHistoryIndex;
static short idHistory[16];

void __cdecl WriteByte(
    unsigned __int8 b,
    MemoryFile *memFile)
{
    MemFile_WriteData(memFile, 1, &b);
}

void __cdecl WriteShort(unsigned __int16 i, MemoryFile *memFile)
{
    MemFile_WriteData(memFile, 2, &i);
}

void __cdecl WriteString(unsigned __int16 str, MemoryFile *memFile)
{
    const char *v3; // r3

    v3 = SL_ConvertToString(str);
    MemFile_WriteCString(memFile, v3);
}

void __cdecl SafeWriteString(unsigned __int16 str, MemoryFile *memFile)
{
    unsigned int v3; // r30
    const char *v4; // r3
    _BYTE v5[8]; // [sp+50h] [-20h] BYREF

    v3 = str;
    if (str)
    {
        v5[0] = 1;
        MemFile_WriteData(memFile, 1, v5);
        v4 = SL_ConvertToString(v3);
        MemFile_WriteCString(memFile, v4);
    }
    else
    {
        v5[0] = 0;
        MemFile_WriteData(memFile, 1, v5);
    }
}

int __cdecl Scr_ReadString(MemoryFile *memFile)
{
    const char *CString; // r3
    const char *v2; // r11

    CString = MemFile_ReadCString(memFile);
    v2 = CString;
    while (*(unsigned __int8 *)v2++)
        ;
    return (unsigned __int16)SL_GetStringOfSize(CString, 0, v2 - CString, MT_TYPE_SCRIPT_STRING);
}

int __cdecl Scr_ReadOptionalString(MemoryFile *memFile)
{
    const char *CString; // r3
    const char *v4; // r11
    _BYTE v6[16]; // [sp+50h] [-20h] BYREF

    MemFile_ReadData(memFile, 1, v6);
    if (!v6[0])
        return 0;
    CString = MemFile_ReadCString(memFile);
    v4 = CString;
    while (*(unsigned __int8 *)v4++)
        ;
    return (unsigned __int16)SL_GetStringOfSize(CString, 0, v4 - CString, MT_TYPE_SCRIPT_STRING);
}

void __cdecl WriteInt(int i, MemoryFile *memFile)
{
    MemFile_WriteData(memFile, 4, &i);
}

void WriteFloat(float f, MemoryFile *memFile)
{
    iassert(!IS_NAN(f));
    MemFile_WriteData(memFile, 4, &f);
}

void __cdecl WriteVector(const float *v, MemoryFile *memFile)
{
    iassert(v);
    iassert(!IS_NAN(v[0]) && !IS_NAN(v[1]) && !IS_NAN(v[2]));
    
    WriteFloat(v[0], memFile);
    WriteFloat(v[1], memFile);
    WriteFloat(v[2], memFile);
}

const float *__cdecl Scr_ReadVec3(MemoryFile *memFile)
{
    float vec[3]; // [sp+58h] [-48h] BYREF

    vec[0] = MemFile_ReadFloat(memFile);
    vec[1] = MemFile_ReadFloat(memFile);
    vec[2] = MemFile_ReadFloat(memFile);

    nanassertvec3(vec);

    return Scr_AllocVector(vec);
}

void __cdecl WriteCodepos(const char *pos, MemoryFile *memFile)
{
    int offset = -1;
    if (pos)
    {
        iassert(Scr_IsInOpcodeMemory(pos));
        offset = static_cast<int>(pos - scrVarPub.programBuffer);
    }
    WriteInt(offset, memFile);
}

const char *__cdecl Scr_ReadCodepos(MemoryFile *memFile)
{
    int offset;
    MemFile_ReadData(memFile, sizeof(offset), reinterpret_cast<byte *>(&offset));
    if (offset == -1)
        return nullptr;
    if (offset < 0 || !scrVarPub.programBuffer || offset >= scrCompilePub.programLen)
        Com_Error(ERR_DROP, "Invalid script code offset in savegame: %d", offset);
    return scrVarPub.programBuffer + offset;
}

unsigned int Scr_CheckIdHistory(unsigned int index)
{
    unsigned int result; // r3
    unsigned int v3; // r11
    __int16 *v4; // r10
    int v5; // r9
    __int16 *v6; // r11
    unsigned int v7; // r10
    int v8; // r9

    result = 1;
    v3 = g_idHistoryIndex + 1;
    if (g_idHistoryIndex + 1 >= 0x10)
    {
    LABEL_6:
        v6 = idHistory;
        v7 = 0;
        while (1)
        {
            v8 = (unsigned __int16)*v6;
            if (index == v8 + 1)
                break;
            if (index == v8)
                return ++result;
            ++v7;
            ++v6;
            result += 2;
            if (v7 > g_idHistoryIndex)
                return 0;
        }
    }
    else
    {
        v4 = &idHistory[v3];
        while (1)
        {
            v5 = (unsigned __int16)*v4;
            if (index == v5 + 1)
                break;
            if (index == v5)
                return ++result;
            ++v3;
            ++v4;
            result += 2;
            if (v3 >= 0x10)
                goto LABEL_6;
        }
    }
    return result;
}

void WriteId(unsigned int id, unsigned int opcode, MemoryFile *memFile)
{
    iassert(id < 32768);
    iassert(scrVarPub.saveIdMap[id] || !id);
    iassert((opcode & ~7u) == 0);
    const auto savedId = scrVarPub.saveIdMap[id];
    const auto candidate = 8 * Scr_CheckIdHistory(savedId);
    const auto history = candidate < 256 ? candidate : 0;
    const auto header = static_cast<byte>(history | opcode);
    MemFile_WriteData(memFile, sizeof(header), &header);
    if (!history)
        MemFile_WriteData(memFile, sizeof(savedId), &savedId);
    idHistory[g_idHistoryIndex] = savedId;
    g_idHistoryIndex = (g_idHistoryIndex - 1) & 15;
}

unsigned int Scr_ReadId(MemoryFile *memFile, unsigned int opcode)
{
    unsigned int savedId;
    if (opcode >> 3)
    {
        const auto historyIndex = ((((opcode >> 3) + 1) >> 1) + g_idHistoryIndex) & 15;
        savedId = static_cast<std::uint16_t>(idHistory[historyIndex]) + ((opcode >> 3) & 1);
    }
    else
    {
        std::uint16_t decoded;
        MemFile_ReadData(memFile, sizeof(decoded), reinterpret_cast<byte *>(&decoded));
        savedId = decoded;
    }
    if (savedId >= 32768 || (savedId && !scrVarPub.saveIdMapRev[savedId]))
        Com_Error(ERR_DROP, "Invalid script object id in savegame: %u", savedId);
    const auto id = scrVarPub.saveIdMapRev[savedId];
    if (id)
        AddRefToObject(id);
    idHistory[g_idHistoryIndex] = savedId;
    g_idHistoryIndex = (g_idHistoryIndex - 1) & 15;
    return id;
}

void __cdecl WriteStack(const VariableStackBuffer *stackBuf, MemoryFile *memFile)
{
    iassert(stackBuf);
    // Savegame fields retain their explicit widths, independent of native pointers.
    MemFile_WriteData(memFile, sizeof(stackBuf->size), &stackBuf->size);
    WriteCodepos(stackBuf->pos, memFile);
    WriteId(stackBuf->localId, 0, memFile);
    MemFile_WriteData(memFile, sizeof(stackBuf->time), &stackBuf->time);
    for (std::size_t i = 0; i < stackBuf->size; ++i)
    {
        const auto value = Scr_ReadStackEntry(stackBuf->buf + i * SCR_STACK_ENTRY_BYTES);
        DoSaveEntryInternal(value.type, value.u, memFile);
    }
}

VariableStackBuffer *__cdecl Scr_ReadStack(MemoryFile *memFile)
{
    std::uint16_t count;
    MemFile_ReadData(memFile, sizeof(count), reinterpret_cast<unsigned char *>(&count));
    auto *stack = Scr_AllocStackBuffer(count);
    ++scrVarPub.numScriptThreads;
    stack->pos = Scr_ReadCodepos(memFile);
    std::uint8_t idOpcode;
    MemFile_ReadData(memFile, sizeof(idOpcode), &idOpcode);
    stack->localId = Scr_ReadId(memFile, idOpcode);
    MemFile_ReadData(memFile, sizeof(stack->time), &stack->time);
    for (std::size_t i = 0; i < count; ++i)
    {
        VariableValue value;
        Scr_DoLoadEntryInternal(&value, memFile);
        Scr_WriteStackEntry(stack->buf + i * SCR_STACK_ENTRY_BYTES, value);
    }
    return stack;
}

void __cdecl Scr_DoLoadEntryInternal(VariableValue *value, MemoryFile *memFile)
{
    value->u = VariableUnion();
    unsigned int v4; // r4
    int v5; // r4
    const char *v6; // r3
    _BYTE v7[4]; // [sp+50h] [-20h] BYREF
    VariableUnion v8; // [sp+54h] [-1Ch] BYREF

    MemFile_ReadData(memFile, 1, v7);
    v4 = v7[0];
    if ((v7[0] & 7) != 0)
    {
        value->type = VAR_POINTER;
        value->u.intValue = Scr_ReadId(memFile, v4);
    }
    else
    {
        v5 = v7[0] >> 3;
        value->type = (Vartype_t)v5;
        switch (v5)
        {
        case VAR_UNDEFINED:
        case VAR_PRECODEPOS:
            return;
        case VAR_STRING:
        case VAR_ISTRING:
            value->u.intValue = (unsigned __int16)Scr_ReadString(memFile);
            break;
        case VAR_VECTOR:
            value->u.vectorValue = Scr_ReadVec3(memFile);
            break;
        case VAR_FLOAT:
            value->u.floatValue = MemFile_ReadFloat(memFile);
            break;
        case VAR_INTEGER:
        case VAR_ANIMATION:
            MemFile_ReadData(memFile, 4, (unsigned char*)&v8);
            value->u = v8;
            break;
        case VAR_CODEPOS:
        case VAR_FUNCTION:
            value->u.codePosValue = Scr_ReadCodepos(memFile);
            break;
        case VAR_STACK:
            value->u.stackValue = Scr_ReadStack(memFile);
            break;
        default:
            if (!alwaysfails)
            {
                v6 = va("unknown type %i", v5);
                MyAssertHandler("c:\\trees\\cod3\\cod3src\\src\\script\\scr_readwrite.cpp", 501, 1, v6);
            }
            break;
        }
    }
}

void __cdecl DoSaveEntryInternal(unsigned int type, VariableUnion u, MemoryFile *memFile)
{
    iassert(type < 32);
    if (type == VAR_POINTER)
    {
        WriteId(u.pointerValue, 1, memFile);
        return;
    }
    const auto tag = static_cast<std::uint8_t>(8 * type);
    MemFile_WriteData(memFile, sizeof(tag), &tag);
    switch (type)
    {
    case VAR_UNDEFINED:
    case VAR_PRECODEPOS:
        return;
    case VAR_STRING:
    case VAR_ISTRING:
        MemFile_WriteCString(memFile, SL_ConvertToString(u.stringValue));
        break;
    case VAR_VECTOR:
        WriteVector(u.vectorValue, memFile);
        break;
    case VAR_FLOAT:
        WriteFloat(u.floatValue, memFile);
        break;
    case VAR_INTEGER:
    case VAR_ANIMATION:
        MemFile_WriteData(memFile, sizeof(u.intValue), &u.intValue);
        break;
    case VAR_CODEPOS:
    case VAR_FUNCTION:
        WriteCodepos(u.codePosValue, memFile);
        break;
    case VAR_STACK:
        WriteStack(u.stackValue, memFile);
        break;
    default:
        iassert(false && "unknown script save type");
        break;
    }
}

void Scr_ResetSaveIdHistory()
{
    memset(idHistory, 0, sizeof(idHistory));
    g_idHistoryIndex = 0;
}
