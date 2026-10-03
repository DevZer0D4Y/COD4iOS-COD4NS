#pragma once
#include "scr_variable.h"

#include <xanim/xanim.h>

#include "scr_debugger.h"
#include <bgame/bg_local.h>

enum $3FAD84344DD9017EDEA6C2E0F6A382A4 : __int32
{
    SCR_SYS_GAME = 0x1,
};

#include "scr_opcode.h"

struct Scr_StringNode_s // sizeof=0x8
{
    const char *text;
    Scr_StringNode_s *next;
};
static_assert(sizeof(void *) != 4 || sizeof(Scr_StringNode_s) == 0x8);

struct function_stack_t // sizeof=0x14
{                                       // ...
    const char *pos;                    // ...
    uint32_t localId;               // ...
    uint32_t localVarCount;         // ...
    VariableValue *top;                 // ...
    VariableValue *startTop;            // ...
};
static_assert(sizeof(void *) != 4 || sizeof(function_stack_t) == 0x14);

struct function_frame_t // sizeof=0x18
{                                       // ...
    function_stack_t fs;                // ...
    Vartype_t topType;
};
static_assert(sizeof(void *) != 4 || sizeof(function_frame_t) == 0x18);

struct scrVmPub_t // sizeof=0x4328
{                                       // ...
    uint32_t* localVars;            // ...
    VariableValue* maxstack;            // ...
    int function_count;                 // ...
    function_frame_t* function_frame;   // ...
    VariableValue* top;                 // ...
    bool debugCode;                     // ...
    bool abort_on_error;                // ...
    bool terminal_error;                // ...
    // padding byte
    uint32_t inparamcount;          // ...
    uint32_t outparamcount;         // ...
    uint32_t breakpointOutparamcount; // ...
    bool showError;                     // ...
    // padding byte
    // padding byte
    // padding byte
    function_frame_t function_frame_start[32]; // ...
    VariableValue stack[2048];          // ...
};
static_assert(sizeof(void *) != 4 || sizeof(scrVmPub_t) == 0x4328);

struct FuncDebugData // sizeof=0x10
{                                       // ...
    int breakpointCount;                // ...
    const char *name;                   // ...
    int prof;                           // ...
    int usage;                          // ...
};
static_assert(sizeof(void *) != 4 || sizeof(FuncDebugData) == 0x10);

struct scrVmDebugPub_t // sizeof=0x24210
{                                       // ...
    FuncDebugData func_table[1024];     // ...
    int checkBreakon;                   // ...
    int profileEnable[32768];           // ...
    int builtInTime;                    // ...
    const char *jumpbackHistory[128];   // ...
    int jumpbackHistoryIndex;           // ...
    int dummy;
};
static_assert(sizeof(void *) != 4 || sizeof(scrVmDebugPub_t) == 0x24210);

struct scrVmGlob_t // sizeof=0x2028
{                                       // ...
    VariableValue eval_stack[2];        // ...
    const char *dialog_error_message;   // ...
    int loading;                        // ...
    int starttime;                      // ...
    uint32_t localVarsStack[2048];  // ...
    bool recordPlace;                   // ...
    // padding byte
    // padding byte
    // padding byte
    char *lastFileName;                 // ...
    int lastLine;                       // ...
};
static_assert(sizeof(void *) != 4 || sizeof(scrVmGlob_t) == 0x2028);

void Scr_Error(const char* error);
void Scr_ErrorWithDialogMessage(const char *error, const char *dialog_error);

void __cdecl SCR_Init();
void GScr_GetAnimLength();
void __cdecl Scr_ErrorOnDefaultAsset(XAssetType type, const char* assetName);
void(__cdecl* __cdecl Scr_GetFunction(const char** pName, int* type))();
uint32_t Scr_GetFunc(uint32_t index);
void Scr_SetRecordScriptPlace(int on);
void Scr_GetLastScriptPlace(int *line, const char **filename);
struct XAnim_s *Scr_GetAnimTree(uint32_t index);
void(__cdecl *__cdecl Scr_GetMethod(const char **pName, int *type))(scr_entref_t);
void(__cdecl *__cdecl BuiltIn_GetMethod(const char **pName, int *type))(scr_entref_t);
void __cdecl GScr_AddVector(const float* vVec);
void __cdecl GScr_Shutdown();
void __cdecl GScr_SetDynamicEntityField(gentity_s* ent, uint32_t index);
void __cdecl Scr_InitFromChildBlocks(struct scr_block_s** childBlocks, int childCount, struct scr_block_s* block);
Scr_StringNode_s* __cdecl Scr_GetStringList(const char* filename, char** pBuf);
void __cdecl Scr_SetSelectionComp(struct UI_Component *comp);
void __cdecl Scr_InitDebuggerSystem();
void Scr_InitBreakpoints();
void __cdecl Scr_ShutdownDebuggerSystem(int restart);
void __cdecl Scr_ShutdownRemoteClient(int restart);
int __cdecl Scr_GetFunctionHandle(const char* filename, const char* name);
int __cdecl Scr_GetStringUsage();
void __cdecl Scr_ShutdownGameStrings();
void __cdecl TRACK_scr_vm();
void __cdecl Scr_ClearErrorMessage();
void __cdecl Scr_Init();
const dvar_s* Scr_VM_Init();
void __cdecl Scr_Settings(int developer, int developer_script, int abort_on_error);
void __cdecl Scr_Shutdown();
void VM_Shutdown();
void __cdecl Scr_SetLoading(int bLoading);
uint32_t __cdecl Scr_GetNumScriptThreads();
void __cdecl Scr_ClearOutParams();
char* __cdecl Scr_GetReturnPos(uint32_t* localId);
char* __cdecl Scr_GetNextCodepos(VariableValue* top, const char* pos, int opcode, int mode, uint32_t* localId);
void __cdecl VM_CancelNotify(uint32_t notifyListOwnerId, uint32_t startLocalId);
void __cdecl VM_CancelNotifyInternal(
    uint32_t notifyListOwnerId,
    uint32_t startLocalId,
    uint32_t notifyListId,
    uint32_t notifyNameListId,
    uint32_t stringValue);
bool __cdecl Scr_IsEndonThread(uint32_t localId);
uint32_t __cdecl Scr_GetWaittillThreadStackId(uint32_t localId, uint32_t startLocalId);
const char* __cdecl Scr_GetThreadPos(uint32_t localId);
const char* __cdecl Scr_GetStackThreadPos(uint32_t endLocalId, VariableStackBuffer* stackValue, bool killThread);
const char* __cdecl Scr_GetRunningThreadPos(uint32_t localId);
uint32_t __cdecl Scr_GetWaitThreadStackId(uint32_t localId, uint32_t startLocalId);
void __cdecl Scr_NotifyNum(
    uint32_t entnum,
    uint32_t classnum,
    uint32_t stringValue,
    uint32_t paramcount);
void __cdecl VM_Notify(uint32_t notifyListOwnerId, uint32_t stringValue, VariableValue* top);
void __cdecl Scr_TerminateThread(uint32_t localId);
void __cdecl Scr_TerminateRunningThread(uint32_t localId);
void __cdecl Scr_TerminateWaitThread(uint32_t localId, uint32_t startLocalId);
void __cdecl VM_TerminateStack(uint32_t endLocalId, uint32_t startLocalId, VariableStackBuffer* stackValue);
void __cdecl Scr_TerminateWaittillThread(uint32_t localId, uint32_t startLocalId);
void __cdecl Scr_CancelNotifyList(uint32_t notifyListOwnerId);
void __cdecl VM_TrimStack(uint32_t startLocalId, VariableStackBuffer* stackValue, bool fromEndon);
void __cdecl Scr_CancelWaittill(uint32_t startLocalId);
uint16_t __cdecl Scr_ExecThread(int handle, uint32_t paramcount);
uint32_t __cdecl VM_Execute(uint32_t localId, const char* pos, uint32_t paramcount);
//uint32_t __cdecl VM_Execute_0();
uint32_t __cdecl GetDummyObject();
uint32_t __cdecl GetDummyFieldValue();
void VM_PrintJumpHistory();
VariableStackBuffer* __cdecl VM_ArchiveStack();
uint16_t __cdecl Scr_ExecEntThreadNum(
    uint32_t entnum,
    uint32_t classnum,
    int handle,
    uint32_t paramcount);
void __cdecl Scr_AddExecThread(int handle, uint32_t paramcount);
void __cdecl Scr_FreeThread(uint16_t handle);
void __cdecl Scr_ExecCode(const char* pos, uint32_t localId);
void __cdecl Scr_InitSystem(int sys);
void __cdecl Scr_ShutdownSystem(uint8_t sys, int bComplete);
void __cdecl VM_TerminateTime(uint32_t timeId);
BOOL __cdecl Scr_IsSystemActive(); // LWSS: Note this has a "system" argument, however it's not used and optimized out in some builds
int __cdecl Scr_GetInt(uint32_t index);
scr_anim_s __cdecl Scr_GetAnim(uint32_t index, XAnimTree_s* tree);
BOOL Scr_ErrorInternal();
float __cdecl Scr_GetFloat(uint32_t index);
uint32_t __cdecl Scr_GetConstString(uint32_t index);
uint32_t __cdecl Scr_GetConstLowercaseString(uint32_t index);
const char* __cdecl Scr_GetString(uint32_t index);
uint32_t __cdecl Scr_GetConstStringIncludeNull(uint32_t index);
const char* __cdecl Scr_GetDebugString(uint32_t index);
uint32_t __cdecl Scr_GetConstIString(uint32_t index);
const char* __cdecl Scr_GetIString(uint32_t index);
void __cdecl Scr_GetVector(uint32_t index, float* vectorValue);
scr_entref_t __cdecl Scr_GetEntityRef(uint32_t index);
uint32_t __cdecl Scr_GetObject(uint32_t index);
int __cdecl Scr_GetType(uint32_t index);
const char* __cdecl Scr_GetTypeName(uint32_t index);
uint32_t __cdecl Scr_GetPointerType(uint32_t index);
uint32_t __cdecl Scr_GetNumParam();
void __cdecl Scr_AddBool(uint32_t value);
void IncInParam();
void __cdecl Scr_AddInt(int value);
void __cdecl Scr_AddFloat(float value);
void __cdecl Scr_AddAnim(scr_anim_s value);
void __cdecl Scr_AddUndefined();
void __cdecl Scr_AddObject(uint32_t id);
void __cdecl Scr_AddEntityNum(uint32_t entnum, uint32_t classnum);
void __cdecl Scr_AddStruct();
void __cdecl Scr_AddString(const char* value);
void __cdecl Scr_AddIString(const char* value);
void __cdecl Scr_AddConstString(uint32_t value);
void __cdecl Scr_AddVector(const float* value);
void __cdecl Scr_MakeArray();
void __cdecl Scr_AddArray();
void __cdecl Scr_AddArrayStringIndexed(uint32_t stringValue);
void __cdecl Scr_Error(const char* error);
void __cdecl Scr_SetErrorMessage(const char* error);
void __cdecl Scr_TerminalError(const char* error);
void __cdecl Scr_NeverTerminalError(const char* error);
void __cdecl Scr_ParamError(uint32_t index, const char* error);
void __cdecl Scr_ObjectError(const char* error);
char __cdecl SetEntityFieldValue(uint32_t classnum, int entnum, int offset, VariableValue* value);
VariableValue __cdecl GetEntityFieldValue(uint32_t classnum, int entnum, int offset);
void __cdecl Scr_SetStructField(uint32_t structId, uint32_t index);
void __cdecl Scr_SetDynamicEntityField(uint32_t entnum, uint32_t classnum, uint32_t index);
void __cdecl Scr_IncTime();
void __cdecl Scr_RunCurrentThreads();
void VM_SetTime();
void __cdecl VM_Resume(uint32_t timeId);
void __cdecl VM_UnarchiveStack(uint32_t startLocalId, VariableStackBuffer* stackValue);
void VM_UnarchiveStack2(uint32_t startLocalId, function_stack_t *stack, VariableStackBuffer *stackValue);
int __cdecl Scr_AddLocalVars(uint32_t localId);
void __cdecl Scr_ResetTimeout();
BOOL __cdecl Scr_IsStackClear();
void __cdecl Scr_StackClear();
void __cdecl Scr_ProfileUpdate();
void __cdecl Scr_ProfileBuiltinUpdate();
void __cdecl Scr_DoProfile(float minTime);
void __cdecl Scr_DoProfileBuiltin(float minTime);
char __cdecl Scr_PrintProfileBuiltinTimes(float minTime);
int __cdecl Scr_BuiltinCompare(_DWORD* a, _DWORD* b);

void Scr_DecTime();

void Scr_AddExecEntThreadNum(int entnum, uint32_t classnum, int handle, uint32_t paramcount);

extern scrVmPub_t scrVmPub;
extern scrVmDebugPub_t scrVmDebugPub;

extern const dvar_s *logScriptTimes;