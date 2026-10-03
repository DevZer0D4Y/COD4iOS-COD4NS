#include <universal/q_shared.h>
#include "scr_vector.h"
#include "scr_stringlist.h"
#include "scr_variable_state.h"
#include <qcommon/atomic_ops.h>
#include <qcommon/critical_sections.h>

float const* Scr_AllocVector(float const* v)
{
	float* result;

	result = Scr_AllocVector();
	
	result[0] = v[0];
	result[1] = v[1];
	result[2] = v[2];

	return result;
}

void  AddRefToVector(float const* vectorValue)
{
	SysScopedCriticalSection lock(CRITSECT_SCRIPT_STRING);
	if (!*((_BYTE*)vectorValue - 1))
	{
		Sys_AtomicIncrement(&scrVarPub.totalVectorRefCount);
		if (scrStringDebugGlob)
		{
			iassert(Sys_AtomicLoad(&scrStringDebugGlob->refCount[((char*)vectorValue - 4 - scrMemTreePub.mt_buffer) / MT_NODE_SIZE]) >= 0);
			Sys_AtomicIncrement(&scrStringDebugGlob->refCount[((char*)(vectorValue - 1) - scrMemTreePub.mt_buffer) / MT_NODE_SIZE]);
		}
		((unsigned short*)vectorValue)[-2]++;
		iassert(((unsigned short*)vectorValue)[-2]);
	}
}

void  RemoveRefToVector(float const* vectorValue)
{
	SysScopedCriticalSection lock(CRITSECT_SCRIPT_STRING);
	if (!*((_BYTE*)vectorValue - 1))
	{
		Sys_AtomicDecrement(&scrVarPub.totalVectorRefCount);
		if (scrStringDebugGlob)
		{
			iassert(Sys_AtomicLoad(&scrStringDebugGlob->refCount[((char*)vectorValue - 4 - scrMemTreePub.mt_buffer) / MT_NODE_SIZE]) >= 0);
			Sys_AtomicDecrement(&scrStringDebugGlob->refCount[((char*)(vectorValue - 1) - scrMemTreePub.mt_buffer) / MT_NODE_SIZE]);
		}
		if (*((_WORD*)vectorValue - 2))
			--*((_WORD*)vectorValue - 2);
		else
			MT_Free((_BYTE*)vectorValue - 4, 16);
	}
}

float* Scr_AllocVector(void)
{
	RefVector* vec; // eax
	float* result; // [esp+4h] [ebp-4h]
	
	vec = (RefVector *)MT_Alloc(sizeof(RefVector), MT_TYPE_VECTOR);
	result = &vec->vec[0];
	vec->head = 0;

	Sys_AtomicIncrement(&scrVarPub.totalVectorRefCount);
	if (scrStringDebugGlob)
		Sys_AtomicIncrement(&scrStringDebugGlob->refCount[((char*)(result - 1) - scrMemTreePub.mt_buffer) / MT_NODE_SIZE]);

	return result;
}
