#pragma once

// The subset of the D3DX9 shader API that KisakCOD's material loader names.
// It only matters for materials built from loose shader source: fastfile
// materials already carry compiled D3D9 shader bytecode. Buffers are fully
// implemented; runtime HLSL compilation and bytecode reflection report
// E_NOTIMPL on Apple platforms.

#include <d3d9.h>

#ifndef E_NOTIMPL
#define E_NOTIMPL ((HRESULT)0x80004001L)
#endif

typedef struct _D3DXMACRO
{
    LPCSTR Name;
    LPCSTR Definition;
} D3DXMACRO, *LPD3DXMACRO;

typedef struct _D3DXSEMANTIC
{
    UINT Usage;
    UINT UsageIndex;
} D3DXSEMANTIC, *LPD3DXSEMANTIC;

// Layouts of the CTAB comment block inside D3D9 shader bytecode.
typedef struct _D3DXSHADER_CONSTANTTABLE
{
    DWORD Size;
    DWORD Creator;
    DWORD Version;
    DWORD Constants;
    DWORD ConstantInfo;
    DWORD Flags;
    DWORD Target;
} D3DXSHADER_CONSTANTTABLE, *LPD3DXSHADER_CONSTANTTABLE;

typedef struct _D3DXSHADER_CONSTANTINFO
{
    DWORD Name;
    WORD RegisterSet;
    WORD RegisterIndex;
    WORD RegisterCount;
    WORD Reserved;
    DWORD TypeInfo;
    DWORD DefaultValue;
} D3DXSHADER_CONSTANTINFO, *LPD3DXSHADER_CONSTANTINFO;

struct ID3DXBuffer
{
    virtual HRESULT QueryInterface(const IID &iid, void **object) = 0;
    virtual ULONG AddRef() = 0;
    virtual ULONG Release() = 0;
    virtual LPVOID GetBufferPointer() = 0;
    virtual DWORD GetBufferSize() = 0;
};
typedef ID3DXBuffer *LPD3DXBUFFER;

struct ID3DXConstantTable
{
    virtual HRESULT QueryInterface(const IID &iid, void **object) = 0;
    virtual ULONG AddRef() = 0;
    virtual ULONG Release() = 0;
    // The table's raw CTAB bytes, as in the Windows D3DX interface.
    virtual LPVOID GetBufferPointer() = 0;
    virtual DWORD GetBufferSize() = 0;
};
typedef ID3DXConstantTable *LPD3DXCONSTANTTABLE;

struct ID3DXInclude;
typedef ID3DXInclude *LPD3DXINCLUDE;

HRESULT D3DXCreateBuffer(DWORD numBytes, LPD3DXBUFFER *buffer);
HRESULT D3DXCompileShader(LPCSTR source, UINT sourceLength, const D3DXMACRO *defines, LPD3DXINCLUDE include,
                          LPCSTR functionName, LPCSTR profile, DWORD flags, LPD3DXBUFFER *shader,
                          LPD3DXBUFFER *errorMessages, LPD3DXCONSTANTTABLE *constantTable);
HRESULT D3DXGetShaderConstantTable(const DWORD *function, LPD3DXCONSTANTTABLE *constantTable);
HRESULT D3DXGetShaderInputSemantics(const DWORD *function, D3DXSEMANTIC *semantics, UINT *count);
HRESULT D3DXGetShaderOutputSemantics(const DWORD *function, D3DXSEMANTIC *semantics, UINT *count);
