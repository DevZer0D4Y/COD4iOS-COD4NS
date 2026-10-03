#pragma once
// Boundaries supplied by the VM, source debugger and animation system.
// This header keeps the generic variable database independent of game/render types.
#include "scr_variable.h"
void Scr_Error(const char *error);
void Scr_TerminalError(const char *error);
void Scr_CancelNotifyList(uint32_t id);
void VM_CancelNotify(uint32_t ownerId, uint32_t startLocalId);
void VM_TerminateStack(uint32_t endLocalId, uint32_t startLocalId, VariableStackBuffer *stack);
char SetEntityFieldValue(uint32_t classnum, int entnum, int offset, VariableValue *value);
VariableValue GetEntityFieldValue(uint32_t classnum, int entnum, int offset);
void Scr_MakeArray();
void Scr_AddConstString(uint32_t value);
void Scr_AddInt(int value);
void Scr_AddArray();
void Scr_UpdateDebugger();
struct XAnim_s;
XAnim_s *Scr_GetAnims(uint32_t index);
char *XAnimGetAnimDebugName(const XAnim_s *anims, uint32_t index);
