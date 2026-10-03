#include <d3dx9shader.h>

#include <atomic>
#include <cstdlib>
#include <cstring>

namespace {

class HeapBuffer final : public ID3DXBuffer
{
public:
    explicit HeapBuffer(DWORD size) : m_data(static_cast<unsigned char *>(std::calloc(size ? size : 1, 1))), m_size(size) {}
    ~HeapBuffer() { std::free(m_data); }

    HRESULT QueryInterface(const IID &, void **object) override
    {
        if (object)
            *object = nullptr;
        return E_NOINTERFACE;
    }
    ULONG AddRef() override { return ++m_refs; }
    ULONG Release() override
    {
        const ULONG refs = --m_refs;
        if (!refs)
            delete this;
        return refs;
    }
    LPVOID GetBufferPointer() override { return m_data; }
    DWORD GetBufferSize() override { return m_size; }
    bool Valid() const { return m_data != nullptr; }

private:
    unsigned char *m_data;
    DWORD m_size;
    std::atomic<ULONG> m_refs{1};
};

HRESULT MakeMessage(const char *text, LPD3DXBUFFER *buffer)
{
    if (!buffer)
        return S_OK;
    const DWORD length = static_cast<DWORD>(std::strlen(text) + 1);
    if (FAILED(D3DXCreateBuffer(length, buffer)))
        return E_OUTOFMEMORY;
    std::memcpy((*buffer)->GetBufferPointer(), text, length);
    return S_OK;
}

} // namespace

HRESULT D3DXCreateBuffer(DWORD numBytes, LPD3DXBUFFER *buffer)
{
    if (!buffer)
        return E_POINTER;
    auto *created = new HeapBuffer(numBytes);
    if (!created->Valid())
    {
        created->Release();
        *buffer = nullptr;
        return E_OUTOFMEMORY;
    }
    *buffer = created;
    return S_OK;
}

HRESULT D3DXCompileShader(LPCSTR, UINT, const D3DXMACRO *, LPD3DXINCLUDE, LPCSTR, LPCSTR, DWORD,
                          LPD3DXBUFFER *shader, LPD3DXBUFFER *errorMessages, LPD3DXCONSTANTTABLE *constantTable)
{
    if (shader)
        *shader = nullptr;
    if (constantTable)
        *constantTable = nullptr;
    MakeMessage("HLSL compilation is not available on this platform; use shaders from fastfiles.", errorMessages);
    return E_NOTIMPL;
}

HRESULT D3DXGetShaderConstantTable(const DWORD *, LPD3DXCONSTANTTABLE *constantTable)
{
    if (constantTable)
        *constantTable = nullptr;
    return E_NOTIMPL;
}

HRESULT D3DXGetShaderInputSemantics(const DWORD *, D3DXSEMANTIC *, UINT *count)
{
    if (count)
        *count = 0;
    return E_NOTIMPL;
}

HRESULT D3DXGetShaderOutputSemantics(const DWORD *, D3DXSEMANTIC *, UINT *count)
{
    if (count)
        *count = 0;
    return E_NOTIMPL;
}
