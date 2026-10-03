#pragma once
#include <universal/memfile.h>

#ifndef KISAK_SP 
#error This file is for SinglePlayer only 
#endif
#include "scr_variable.h"
#include "scr_value_io.h"
#include <server/server.h> // SaveImmediate

int __cdecl Scr_DoLoadEntry(VariableValue *value, bool isArray, MemoryFile *memFile);
void __cdecl AddSaveObjectInternal(unsigned int parentId);
unsigned int __cdecl Scr_ConvertThreadFromLoad(unsigned __int16 handle);
void __cdecl Scr_DoLoadObjectInfo(unsigned __int16 parentId, MemoryFile *memFile);
void __cdecl Scr_ReadGameEntry(MemoryFile *memFile);
void __cdecl Scr_SaveShutdown(bool savegame);
void __cdecl Scr_LoadPre(int sys, MemoryFile *memFile);
void __cdecl Scr_LoadShutdown();
void __cdecl Scr_SaveSource(MemoryFile *memFile);
void __cdecl SaveMemory_SaveWriteImmediate(const void *buffer, unsigned int len, SaveImmediate *save);
void __cdecl Scr_SaveSourceImmediate(SaveImmediate *save);
void __cdecl Scr_LoadSource(MemoryFile *memFile, void *fileHandle);
void __cdecl Scr_SkipSource(MemoryFile *memFile, void *fileHandle);
void __cdecl AddSaveStackInternal(const VariableStackBuffer *stackBuf);
void __cdecl AddSaveEntryInternal(unsigned int type, VariableUnion u);
// local variable allocation has failed, the output may be wrong!
void __cdecl DoSaveEntry(VariableValue *value, VariableValue *name, bool isArray, MemoryFile *memFile);
void __cdecl AddSaveObjectChildren(unsigned int parentId);
void __cdecl AddSaveObject(unsigned int parentId);
void __cdecl DoSaveObjectInfo(unsigned int parentId, MemoryFile *memFile);
int __cdecl Scr_ConvertThreadToSave(unsigned __int16 handle);
void __cdecl WriteGameEntry(MemoryFile *memFile);
void __cdecl Scr_SavePost(MemoryFile *memFile);
void __cdecl AddSaveStack(const VariableStackBuffer *stackBuf);
void __cdecl AddSaveEntry(unsigned int type, VariableUnion u);
void __cdecl Scr_SavePre(int sys);
