#pragma once

#include <universal/q_shared.h>
#include "scr_value_types.h"
struct MemoryFile;

void __cdecl WriteByte(unsigned __int8 b, MemoryFile *memFile);
void __cdecl WriteShort(unsigned __int16 i, MemoryFile *memFile);
void __cdecl WriteString(unsigned __int16 str, MemoryFile *memFile);
void __cdecl SafeWriteString(unsigned __int16 str, MemoryFile *memFile);
int __cdecl Scr_ReadString(MemoryFile *memFile);
int __cdecl Scr_ReadOptionalString(MemoryFile *memFile);
void __cdecl WriteInt(int i, MemoryFile *memFile);
void WriteFloat(float f, MemoryFile *memFile);
void __cdecl WriteVector(const float *v, MemoryFile *memFile);
const float *__cdecl Scr_ReadVec3(MemoryFile *memFile);
void __cdecl WriteCodepos(const char *pos, MemoryFile *memFile);
const char *__cdecl Scr_ReadCodepos(MemoryFile *memFile);
unsigned int __cdecl Scr_CheckIdHistory(unsigned int index);
void __cdecl WriteId(unsigned int id, unsigned int opcode, MemoryFile *memFile);
unsigned int __cdecl Scr_ReadId(MemoryFile *memFile, unsigned int opcode);
void __cdecl WriteStack(const VariableStackBuffer *stackBuf, MemoryFile *memFile);
VariableStackBuffer *__cdecl Scr_ReadStack(MemoryFile *memFile);
void __cdecl Scr_DoLoadEntryInternal(VariableValue *value, MemoryFile *memFile);
void __cdecl DoSaveEntryInternal(unsigned int type, VariableUnion u, MemoryFile *memFile);
void Scr_ResetSaveIdHistory();
