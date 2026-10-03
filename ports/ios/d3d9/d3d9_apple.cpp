#include <cstdlib>
#include <sys/stat.h>
// Direct3D 9 for Apple platforms: headless stage.
//
// The engine renders through the D3D9 interfaces. This file provides them with
// real object lifetimes and CPU-side storage for every resource, so textures,
// buffers and surfaces can be created, locked, filled and copied exactly as the
// engine expects. Nothing reaches the screen yet: draw and present calls succeed
// without output, and occlusion/event queries report completion. The Metal
// backend replaces those parts; the object model here is what it builds on.
//
// Methods the engine never calls fall back to the generated default bases,
// which report the call once and fail.

#include "generated/d3d9_default_bases.h"
#include "metal/metal_backend.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <unordered_set>
#include <vector>

bool KisakApple_GetDisplaySize(int *width, int *height);
HWND KisakApple_GetRenderWindow();

namespace gpu = kisak::metal;

void KisakD3D9_NotImplemented(const char *method)
{
    static std::mutex lock;
    static std::unordered_set<std::string> reported;
    std::lock_guard<std::mutex> guard(lock);
    if (reported.insert(method).second)
        fprintf(stderr, "D3D9 (Apple): %s is not implemented\n", method);
}

namespace {

// ---------------------------------------------------------------------------
// Format layout

struct FormatLayout
{
    UINT blockWidth;
    UINT blockHeight;
    UINT blockBytes;
};

FormatLayout LayoutFor(D3DFORMAT format)
{
    switch (static_cast<DWORD>(format))
    {
    case D3DFMT_DXT1:
        return {4, 4, 8};
    case D3DFMT_DXT2:
    case D3DFMT_DXT3:
    case D3DFMT_DXT4:
    case D3DFMT_DXT5:
        return {4, 4, 16};
    case D3DFMT_A8:
    case D3DFMT_L8:
    case D3DFMT_P8:
    case D3DFMT_A4L4:
        return {1, 1, 1};
    case D3DFMT_R5G6B5:
    case D3DFMT_X1R5G5B5:
    case D3DFMT_A1R5G5B5:
    case D3DFMT_A4R4G4B4:
    case D3DFMT_X4R4G4B4:
    case D3DFMT_A8L8:
    case D3DFMT_L16:
    case D3DFMT_V8U8:
    case D3DFMT_R16F:
    case D3DFMT_D16:
    case D3DFMT_D15S1:
        return {1, 1, 2};
    case D3DFMT_R8G8B8:
        return {1, 1, 3};
    case D3DFMT_A16B16G16R16:
    case D3DFMT_A16B16G16R16F:
    case D3DFMT_G32R32F:
        return {1, 1, 8};
    case D3DFMT_A32B32G32R32F:
        return {1, 1, 16};
    default:
        // 32-bit colour, depth and single-channel float formats.
        return {1, 1, 4};
    }
}

UINT MipDimension(UINT size, UINT level)
{
    return std::max<UINT>(size >> level, 1u);
}

UINT FullMipCount(UINT width, UINT height, UINT depth)
{
    UINT levels = 1;
    while (width > 1 || height > 1 || depth > 1)
    {
        width = std::max<UINT>(width / 2, 1u);
        height = std::max<UINT>(height / 2, 1u);
        depth = std::max<UINT>(depth / 2, 1u);
        ++levels;
    }
    return levels;
}

UINT BlocksAcross(const FormatLayout &layout, UINT width)
{
    return (std::max<UINT>(width, 1u) + layout.blockWidth - 1) / layout.blockWidth;
}

UINT BlocksDown(const FormatLayout &layout, UINT height)
{
    return (std::max<UINT>(height, 1u) + layout.blockHeight - 1) / layout.blockHeight;
}

size_t ImageBytes(D3DFORMAT format, UINT width, UINT height, UINT depth = 1)
{
    const FormatLayout layout = LayoutFor(format);
    return static_cast<size_t>(BlocksAcross(layout, width)) * BlocksDown(layout, height) * depth * layout.blockBytes;
}

INT RowPitch(D3DFORMAT format, UINT width)
{
    const FormatLayout layout = LayoutFor(format);
    return static_cast<INT>(BlocksAcross(layout, width) * layout.blockBytes);
}

// ---------------------------------------------------------------------------
// Metal backend conversions

gpu::Format GpuFormat(D3DFORMAT format)
{
    switch (static_cast<DWORD>(format))
    {
    case D3DFMT_A8R8G8B8: return gpu::Format::BGRA8;
    case D3DFMT_X8R8G8B8: return gpu::Format::BGRX8;
    case D3DFMT_A8B8G8R8: return gpu::Format::RGBA8;
    case D3DFMT_X8B8G8R8: return gpu::Format::RGBX8;
    case D3DFMT_L8: return gpu::Format::L8;
    case D3DFMT_A8: return gpu::Format::A8;
    case D3DFMT_A8L8: return gpu::Format::A8L8;
    case D3DFMT_R5G6B5: return gpu::Format::R5G6B5;
    case D3DFMT_X1R5G5B5: return gpu::Format::X1R5G5B5;
    case D3DFMT_A1R5G5B5: return gpu::Format::A1R5G5B5;
    case D3DFMT_A4R4G4B4: return gpu::Format::A4R4G4B4;
    case D3DFMT_R16F: return gpu::Format::R16F;
    case D3DFMT_R32F: return gpu::Format::R32F;
    case D3DFMT_G16R16F: return gpu::Format::RG16F;
    case D3DFMT_A16B16G16R16F: return gpu::Format::RGBA16F;
    case D3DFMT_A32B32G32R32F: return gpu::Format::RGBA32F;
    case D3DFMT_DXT1: return gpu::Format::DXT1;
    case D3DFMT_DXT3: return gpu::Format::DXT3;
    case D3DFMT_DXT5: return gpu::Format::DXT5;
    case D3DFMT_D24S8:
    case D3DFMT_D24X8:
    case D3DFMT_D24FS8:
    case D3DFMT_D24X4S4:
    case D3DFMT_D15S1: return gpu::Format::D24S8;
    case D3DFMT_D16:
    case D3DFMT_D16_LOCKABLE: return gpu::Format::D16;
    case D3DFMT_D32:
    case D3DFMT_D32F_LOCKABLE: return gpu::Format::D32F;
    default: return gpu::Format::Unknown;
    }
}

// The backend enumerations reuse Direct3D 9 numbering.
static_assert(static_cast<int>(gpu::Compare::Always) == D3DCMP_ALWAYS);
static_assert(static_cast<int>(gpu::BlendFactor::InvBlendFactor) == D3DBLEND_INVBLENDFACTOR);
static_assert(static_cast<int>(gpu::BlendOp::Max) == D3DBLENDOP_MAX);
static_assert(static_cast<int>(gpu::Cull::CounterClockwise) == D3DCULL_CCW);
static_assert(static_cast<int>(gpu::StencilOp::Decr) == D3DSTENCILOP_DECR);
static_assert(static_cast<int>(gpu::Address::MirrorOnce) == D3DTADDRESS_MIRRORONCE);
static_assert(static_cast<int>(gpu::Filter::Anisotropic) == D3DTEXF_ANISOTROPIC);
static_assert(static_cast<int>(gpu::Primitive::TriangleFan) == D3DPT_TRIANGLEFAN);
static_assert(static_cast<int>(gpu::VertexType::Unused) == D3DDECLTYPE_UNUSED);

gpu::Compare CompareFrom(DWORD v) { return v >= D3DCMP_NEVER && v <= D3DCMP_ALWAYS ? static_cast<gpu::Compare>(v) : gpu::Compare::Always; }
gpu::BlendFactor BlendFrom(DWORD v)
{
    return v >= D3DBLEND_ZERO && v <= D3DBLEND_INVBLENDFACTOR ? static_cast<gpu::BlendFactor>(v) : gpu::BlendFactor::One;
}
gpu::BlendOp BlendOpFrom(DWORD v) { return v >= D3DBLENDOP_ADD && v <= D3DBLENDOP_MAX ? static_cast<gpu::BlendOp>(v) : gpu::BlendOp::Add; }
gpu::StencilOp StencilOpFrom(DWORD v)
{
    return v >= D3DSTENCILOP_KEEP && v <= D3DSTENCILOP_DECR ? static_cast<gpu::StencilOp>(v) : gpu::StencilOp::Keep;
}
gpu::Cull CullFrom(DWORD v) { return v >= D3DCULL_NONE && v <= D3DCULL_CCW ? static_cast<gpu::Cull>(v) : gpu::Cull::CounterClockwise; }
gpu::Address AddressFrom(DWORD v)
{
    return v >= D3DTADDRESS_WRAP && v <= D3DTADDRESS_MIRRORONCE ? static_cast<gpu::Address>(v) : gpu::Address::Wrap;
}
gpu::Filter FilterFrom(DWORD v) { return v <= D3DTEXF_ANISOTROPIC ? static_cast<gpu::Filter>(v) : gpu::Filter::Linear; }
float FloatState(DWORD v)
{
    float value;
    memcpy(&value, &v, sizeof(value));
    return value;
}

template<typename T> void Assign(T *&slot, T *value)
{
    if (value)
        value->AddRef();
    if (slot)
        slot->Release();
    slot = value;
}

template<typename T> HRESULT ReturnReferenced(T *object, T **out, HRESULT whenNull)
{
    if (!out)
        return D3DERR_INVALIDCALL;
    *out = object;
    if (!object)
        return whenNull;
    object->AddRef();
    return D3D_OK;
}

D3DDISPLAYMODE CurrentDisplayMode(D3DFORMAT format)
{
    int width = 0;
    int height = 0;
    if (!KisakApple_GetDisplaySize(&width, &height))
    {
        width = 1280;
        height = 720;
    }
    D3DDISPLAYMODE mode{};
    mode.Width = static_cast<UINT>(width);
    mode.Height = static_cast<UINT>(height);
    mode.RefreshRate = 60;
    mode.Format = format;
    return mode;
}

// ---------------------------------------------------------------------------
// Capabilities
//
// R_CheckDxCaps (src/gfx_d3d/r_init.cpp) reads D3DCAPS9 through byte offsets.
// Every required bit from its s_capsCheckBits table is reported and its
// forbidden bits are not; integer limits sit inside s_capsCheckInt. Shader
// model 3.0 selects the renderer path the fastfile shaders were built for.

static_assert(offsetof(D3DCAPS9, Caps2) == 12);
static_assert(offsetof(D3DCAPS9, TextureCaps) == 60);
static_assert(offsetof(D3DCAPS9, MaxTextureWidth) == 88);
static_assert(offsetof(D3DCAPS9, StencilCaps) == 136);
static_assert(offsetof(D3DCAPS9, MaxTextureBlendStages) == 148);
static_assert(offsetof(D3DCAPS9, MaxStreams) == 188);
static_assert(offsetof(D3DCAPS9, VertexShaderVersion) == 196);
static_assert(offsetof(D3DCAPS9, PixelShaderVersion) == 204);
static_assert(offsetof(D3DCAPS9, DevCaps2) == 212);
static_assert(offsetof(D3DCAPS9, DeclTypes) == 236);
static_assert(offsetof(D3DCAPS9, StretchRectFilterCaps) == 244);

void FillCaps(D3DCAPS9 *caps)
{
    memset(caps, 0, sizeof(*caps));
    caps->DeviceType = D3DDEVTYPE_HAL;
    caps->AdapterOrdinal = 0;
    caps->Caps2 = 0x20000000 /* dynamic textures */ | 0x20000 /* fullscreen gamma */;
    caps->Caps3 = 0x20 /* alpha fullscreen flip */ | 0x100 /* copy to video memory */;
    caps->PresentationIntervals = 0x80000000 /* immediate */ | 0x1 /* one */;
    caps->DevCaps = 0x8000 | 0x10400 /* hardware T&L */ | 0x80000 /* hardware rasterization */;
    caps->PrimitiveMiscCaps = 0x2 | 0x80 | 0x800 | 0x20000 | 0x70;
    caps->RasterCaps = 0x2000000 /* slope-scale depth bias */;
    caps->ZCmpCaps = 0xFF;
    caps->SrcBlendCaps = 0x3FF;
    caps->DestBlendCaps = 0x3FF;
    caps->AlphaCmpCaps = 0xFF;
    caps->ShadeCaps = 0x0;
    // Alpha, cube maps, mipmaps, conditional non-power-of-2, perspective
    // correction. Neither "power of 2 only" (0x2) nor "square only" (0x20).
    caps->TextureCaps = 0x4 | 0x800 | 0x4000 | 0x100 | 0x1;
    caps->TextureFilterCaps = 50529024u | 0x400 /* anisotropic min */ | 0x4000000 /* anisotropic mag */;
    caps->CubeTextureFilterCaps = 50332416u;
    caps->VolumeTextureFilterCaps = 50332416u;
    caps->TextureAddressCaps = 0x4 /* clamp */ | 0x1 /* wrap */ | 0x2 /* mirror */;
    caps->VolumeTextureAddressCaps = 0x4 | 0x1 | 0x2;
    caps->MaxTextureWidth = 4096;
    caps->MaxTextureHeight = 4096;
    caps->MaxVolumeExtent = 512;
    caps->MaxTextureRepeat = 8192;
    caps->MaxTextureAspectRatio = 8192;
    caps->MaxAnisotropy = 16;
    caps->MaxVertexW = 1.0e10f;
    caps->GuardBandLeft = -32768.0f;
    caps->GuardBandTop = -32768.0f;
    caps->GuardBandRight = 32768.0f;
    caps->GuardBandBottom = 32768.0f;
    caps->StencilCaps = 0x1FF;
    caps->MaxTextureBlendStages = 8;
    caps->MaxSimultaneousTextures = 16;
    caps->MaxUserClipPlanes = 6;
    caps->MaxPointSize = 256.0f;
    caps->MaxPrimitiveCount = 0xFFFFFF;
    caps->MaxVertexIndex = 0xFFFFFF;
    caps->MaxStreams = 16;
    caps->MaxStreamStride = 255;
    caps->VertexShaderVersion = 0xFFFE0300;
    caps->MaxVertexShaderConst = 256;
    caps->PixelShaderVersion = 0xFFFF0300;
    caps->PixelShader1xMaxValue = 8.0f;
    caps->DevCaps2 = 0x1 /* stream offsets */;
    caps->NumberOfAdaptersInGroup = 1;
    caps->DeclTypes = 0x3FF;
    caps->NumSimultaneousRTs = 4;
    caps->StretchRectFilterCaps = 0x200 | 0x2000000 | 0x100 | 0x1000000;
}

class Device;

// ---------------------------------------------------------------------------
// Surfaces and volumes

class Surface final : public KisakD3D9Default_IDirect3DSurface9
{
public:
    Surface(IDirect3DDevice9 *device, UINT width, UINT height, D3DFORMAT format, DWORD usage, D3DPOOL pool,
            D3DMULTISAMPLE_TYPE multiSample, IUnknown *container)
        : m_device(device), m_container(container), m_bits(ImageBytes(format, width, height))
    {
        m_desc.Format = format;
        m_desc.Type = D3DRTYPE_SURFACE;
        m_desc.Usage = usage;
        m_desc.Pool = pool;
        m_desc.MultiSampleType = multiSample;
        m_desc.MultiSampleQuality = 0;
        m_desc.Width = width;
        m_desc.Height = height;
        // Standalone render targets, depth surfaces and back buffers own their GPU
        // texture; texture levels are attached to their container's by AttachGpu.
        if (!container && (usage & (D3DUSAGE_RENDERTARGET | D3DUSAGE_DEPTHSTENCIL)) && gpu::Available())
        {
            gpu::TextureDesc desc;
            desc.format = GpuFormat(format);
            desc.width = width;
            desc.height = height;
            desc.renderTarget = true;
            if (desc.format != gpu::Format::Unknown)
            {
                m_gpu = gpu::CreateTexture(desc);
                m_ownsGpu = true;
            }
        }
    }

    ~Surface() override
    {
        if (m_ownsGpu)
            gpu::DestroyTexture(m_gpu);
    }

    void AttachGpu(gpu::Texture *texture, UINT face, UINT level)
    {
        m_gpu = texture;
        m_face = face;
        m_level = level;
    }

    gpu::TargetBinding Binding() const { return gpu::TargetBinding{m_gpu, m_face, m_level}; }

    // Levels of a texture live and die with it, as in D3D9.
    ULONG AddRef() override { return m_container ? m_container->AddRef() : KisakD3D9Default_IDirect3DSurface9::AddRef(); }
    ULONG Release() override { return m_container ? m_container->Release() : KisakD3D9Default_IDirect3DSurface9::Release(); }

    HRESULT GetDevice(struct IDirect3DDevice9 **ppDevice) override { return ReturnReferenced(m_device, ppDevice, D3DERR_INVALIDCALL); }
    D3DRESOURCETYPE GetType() override { return D3DRTYPE_SURFACE; }

    HRESULT GetContainer(REFIID riid, void **ppContainer) override
    {
        (void)riid;
        if (!ppContainer)
            return D3DERR_INVALIDCALL;
        *ppContainer = m_container;
        if (!m_container)
            return E_NOINTERFACE;
        m_container->AddRef();
        return D3D_OK;
    }

    HRESULT GetDesc(D3DSURFACE_DESC *pDesc) override
    {
        if (!pDesc)
            return D3DERR_INVALIDCALL;
        *pDesc = m_desc;
        return D3D_OK;
    }

    HRESULT LockRect(D3DLOCKED_RECT *locked_rect, const RECT *rect, DWORD flags) override
    {
        (void)flags;
        if (!locked_rect)
            return D3DERR_INVALIDCALL;
        const FormatLayout layout = LayoutFor(m_desc.Format);
        const INT pitch = RowPitch(m_desc.Format, m_desc.Width);
        size_t offset = 0;
        if (rect)
            offset = static_cast<size_t>(rect->top / layout.blockHeight) * pitch + (rect->left / layout.blockWidth) * layout.blockBytes;
        if (offset > m_bits.size())
            return D3DERR_INVALIDCALL;
        locked_rect->Pitch = pitch;
        locked_rect->pBits = m_bits.data() + offset;
        return D3D_OK;
    }

    HRESULT UnlockRect() override
    {
        if (m_gpu && !(m_desc.Usage & (D3DUSAGE_RENDERTARGET | D3DUSAGE_DEPTHSTENCIL)))
            gpu::UploadTexture(m_gpu, m_face, m_level, m_bits.data(), m_bits.size());
        return D3D_OK;
    }

    const D3DSURFACE_DESC &Desc() const { return m_desc; }
    std::vector<uint8_t> &Bits() { return m_bits; }

private:
    IDirect3DDevice9 *m_device;
    IUnknown *m_container;
    D3DSURFACE_DESC m_desc{};
    std::vector<uint8_t> m_bits;
    gpu::Texture *m_gpu = nullptr;
    UINT m_face = 0;
    UINT m_level = 0;
    bool m_ownsGpu = false;
};

class Volume final : public KisakD3D9Default_IDirect3DVolume9
{
public:
    Volume(IDirect3DDevice9 *device, UINT width, UINT height, UINT depth, D3DFORMAT format, DWORD usage, D3DPOOL pool,
           IUnknown *container)
        : m_device(device), m_container(container), m_bits(ImageBytes(format, width, height, depth))
    {
        m_desc.Format = format;
        m_desc.Type = D3DRTYPE_VOLUME;
        m_desc.Usage = usage;
        m_desc.Pool = pool;
        m_desc.Width = width;
        m_desc.Height = height;
        m_desc.Depth = depth;
    }

    ULONG AddRef() override { return m_container->AddRef(); }
    ULONG Release() override { return m_container->Release(); }

    HRESULT GetDevice(struct IDirect3DDevice9 **ppDevice) override { return ReturnReferenced(m_device, ppDevice, D3DERR_INVALIDCALL); }

    HRESULT GetContainer(REFIID riid, void **ppContainer) override
    {
        (void)riid;
        if (!ppContainer)
            return D3DERR_INVALIDCALL;
        m_container->AddRef();
        *ppContainer = m_container;
        return D3D_OK;
    }

    HRESULT GetDesc(D3DVOLUME_DESC *pDesc) override
    {
        if (!pDesc)
            return D3DERR_INVALIDCALL;
        *pDesc = m_desc;
        return D3D_OK;
    }

    HRESULT LockBox(D3DLOCKED_BOX *locked_box, const D3DBOX *box, DWORD flags) override
    {
        (void)flags;
        if (!locked_box)
            return D3DERR_INVALIDCALL;
        const FormatLayout layout = LayoutFor(m_desc.Format);
        const INT rowPitch = RowPitch(m_desc.Format, m_desc.Width);
        const INT slicePitch = static_cast<INT>(ImageBytes(m_desc.Format, m_desc.Width, m_desc.Height));
        size_t offset = 0;
        if (box)
            offset = static_cast<size_t>(box->Front) * slicePitch + (box->Top / layout.blockHeight) * rowPitch
                   + (box->Left / layout.blockWidth) * layout.blockBytes;
        if (offset > m_bits.size())
            return D3DERR_INVALIDCALL;
        locked_box->RowPitch = rowPitch;
        locked_box->SlicePitch = slicePitch;
        locked_box->pBits = m_bits.data() + offset;
        return D3D_OK;
    }

    HRESULT UnlockBox() override
    {
        if (m_gpu)
            gpu::UploadTexture(m_gpu, 0, m_level, m_bits.data(), m_bits.size());
        return D3D_OK;
    }

    void AttachGpu(gpu::Texture *texture, UINT level)
    {
        m_gpu = texture;
        m_level = level;
    }

    // IDirect3DDevice9::UpdateTexture for volume textures: copy system-memory contents and re-upload (model lighting).
    void CopyFrom(const Volume &source)
    {
        std::memcpy(m_bits.data(), source.m_bits.data(), std::min(m_bits.size(), source.m_bits.size()));
        if (m_gpu)
            gpu::UploadTexture(m_gpu, 0, m_level, m_bits.data(), m_bits.size());
    }

private:
    IDirect3DDevice9 *m_device;
    IUnknown *m_container;
    D3DVOLUME_DESC m_desc{};
    std::vector<uint8_t> m_bits;
    gpu::Texture *m_gpu = nullptr;
    UINT m_level = 0;
};

// ---------------------------------------------------------------------------
// Textures

class Texture final : public KisakD3D9Default_IDirect3DTexture9
{
public:
    Texture(IDirect3DDevice9 *device, UINT width, UINT height, UINT levels, DWORD usage, D3DFORMAT format, D3DPOOL pool)
        : m_device(device)
    {
        const UINT count = levels ? levels : FullMipCount(width, height, 1);
        for (UINT level = 0; level < count; ++level)
            m_levels.push_back(new Surface(device, MipDimension(width, level), MipDimension(height, level), format, usage, pool,
                                           D3DMULTISAMPLE_NONE, this));
        if (gpu::Available() && GpuFormat(format) != gpu::Format::Unknown)
        {
            gpu::TextureDesc desc;
            desc.format = GpuFormat(format);
            desc.width = width;
            desc.height = height;
            desc.levels = count;
            desc.renderTarget = (usage & (D3DUSAGE_RENDERTARGET | D3DUSAGE_DEPTHSTENCIL)) != 0;
            m_gpu = gpu::CreateTexture(desc);
            for (UINT level = 0; level < count; ++level)
                m_levels[level]->AttachGpu(m_gpu, 0, level);
        }
    }

    ~Texture() override
    {
        for (Surface *level : m_levels)
            delete level;
        gpu::DestroyTexture(m_gpu);
    }

    gpu::Texture *Gpu() const { return m_gpu; }

    HRESULT GetDevice(struct IDirect3DDevice9 **ppDevice) override { return ReturnReferenced(m_device, ppDevice, D3DERR_INVALIDCALL); }
    D3DRESOURCETYPE GetType() override { return D3DRTYPE_TEXTURE; }

    DWORD SetLOD(DWORD LODNew) override
    {
        const DWORD previous = m_lod;
        m_lod = std::min<DWORD>(LODNew, static_cast<DWORD>(m_levels.size() - 1));
        return previous;
    }

    DWORD GetLOD() override { return m_lod; }
    DWORD GetLevelCount() override { return static_cast<DWORD>(m_levels.size()); }
    HRESULT SetAutoGenFilterType(D3DTEXTUREFILTERTYPE FilterType) override { m_autoGenFilter = FilterType; return D3D_OK; }
    D3DTEXTUREFILTERTYPE GetAutoGenFilterType() override { return m_autoGenFilter; }
    void GenerateMipSubLevels() override {}

    HRESULT GetLevelDesc(UINT Level, D3DSURFACE_DESC *pDesc) override
    {
        if (Level >= m_levels.size())
            return D3DERR_INVALIDCALL;
        return m_levels[Level]->GetDesc(pDesc);
    }

    HRESULT GetSurfaceLevel(UINT Level, IDirect3DSurface9 **ppSurfaceLevel) override
    {
        if (!ppSurfaceLevel || Level >= m_levels.size())
            return D3DERR_INVALIDCALL;
        m_levels[Level]->AddRef();
        *ppSurfaceLevel = m_levels[Level];
        return D3D_OK;
    }

    HRESULT LockRect(UINT level, D3DLOCKED_RECT *locked_rect, const RECT *rect, DWORD flags) override
    {
        if (level >= m_levels.size())
            return D3DERR_INVALIDCALL;
        return m_levels[level]->LockRect(locked_rect, rect, flags);
    }

    HRESULT UnlockRect(UINT Level) override { return Level < m_levels.size() ? m_levels[Level]->UnlockRect() : D3DERR_INVALIDCALL; }
    HRESULT AddDirtyRect(const RECT *dirty_rect) override { (void)dirty_rect; return D3D_OK; }

    std::vector<Surface *> &Levels() { return m_levels; }

private:
    IDirect3DDevice9 *m_device;
    std::vector<Surface *> m_levels;
    gpu::Texture *m_gpu = nullptr;
    DWORD m_lod = 0;
    D3DTEXTUREFILTERTYPE m_autoGenFilter = D3DTEXF_LINEAR;
};

class CubeTexture final : public KisakD3D9Default_IDirect3DCubeTexture9
{
public:
    CubeTexture(IDirect3DDevice9 *device, UINT edge, UINT levels, DWORD usage, D3DFORMAT format, D3DPOOL pool)
        : m_device(device)
    {
        m_levelCount = levels ? levels : FullMipCount(edge, edge, 1);
        for (UINT face = 0; face < 6; ++face)
            for (UINT level = 0; level < m_levelCount; ++level)
                m_surfaces.push_back(new Surface(device, MipDimension(edge, level), MipDimension(edge, level), format, usage, pool,
                                                 D3DMULTISAMPLE_NONE, this));
        if (gpu::Available() && GpuFormat(format) != gpu::Format::Unknown)
        {
            gpu::TextureDesc desc;
            desc.kind = gpu::TextureKind::Cube;
            desc.format = GpuFormat(format);
            desc.width = edge;
            desc.height = edge;
            desc.levels = m_levelCount;
            desc.renderTarget = (usage & D3DUSAGE_RENDERTARGET) != 0;
            m_gpu = gpu::CreateTexture(desc);
            for (UINT face = 0; face < 6; ++face)
                for (UINT level = 0; level < m_levelCount; ++level)
                    m_surfaces[face * m_levelCount + level]->AttachGpu(m_gpu, face, level);
        }
    }

    ~CubeTexture() override
    {
        for (Surface *surface : m_surfaces)
            delete surface;
        gpu::DestroyTexture(m_gpu);
    }

    gpu::Texture *Gpu() const { return m_gpu; }

    HRESULT GetDevice(struct IDirect3DDevice9 **ppDevice) override { return ReturnReferenced(m_device, ppDevice, D3DERR_INVALIDCALL); }
    D3DRESOURCETYPE GetType() override { return D3DRTYPE_CUBETEXTURE; }
    DWORD GetLevelCount() override { return m_levelCount; }

    HRESULT GetLevelDesc(UINT Level, D3DSURFACE_DESC *pDesc) override
    {
        if (Level >= m_levelCount)
            return D3DERR_INVALIDCALL;
        return m_surfaces[Level]->GetDesc(pDesc);
    }

    HRESULT GetCubeMapSurface(D3DCUBEMAP_FACES FaceType, UINT Level, IDirect3DSurface9 **ppCubeMapSurface) override
    {
        Surface *surface = Find(FaceType, Level);
        if (!surface || !ppCubeMapSurface)
            return D3DERR_INVALIDCALL;
        surface->AddRef();
        *ppCubeMapSurface = surface;
        return D3D_OK;
    }

    HRESULT LockRect(D3DCUBEMAP_FACES face, UINT level, D3DLOCKED_RECT *locked_rect, const RECT *rect, DWORD flags) override
    {
        Surface *surface = Find(face, level);
        return surface ? surface->LockRect(locked_rect, rect, flags) : D3DERR_INVALIDCALL;
    }

    HRESULT UnlockRect(D3DCUBEMAP_FACES FaceType, UINT Level) override
    {
        Surface *surface = Find(FaceType, Level);
        return surface ? surface->UnlockRect() : D3DERR_INVALIDCALL;
    }
    HRESULT AddDirtyRect(D3DCUBEMAP_FACES face, const RECT *dirty_rect) override { (void)face; (void)dirty_rect; return D3D_OK; }

private:
    Surface *Find(D3DCUBEMAP_FACES face, UINT level)
    {
        const UINT faceIndex = static_cast<UINT>(face);
        if (faceIndex >= 6 || level >= m_levelCount)
            return nullptr;
        return m_surfaces[faceIndex * m_levelCount + level];
    }

    IDirect3DDevice9 *m_device;
    UINT m_levelCount = 0;
    std::vector<Surface *> m_surfaces;
    gpu::Texture *m_gpu = nullptr;
};

class VolumeTexture final : public KisakD3D9Default_IDirect3DVolumeTexture9
{
public:
    VolumeTexture(IDirect3DDevice9 *device, UINT width, UINT height, UINT depth, UINT levels, DWORD usage, D3DFORMAT format, D3DPOOL pool)
        : m_device(device)
    {
        const UINT count = levels ? levels : FullMipCount(width, height, depth);
        for (UINT level = 0; level < count; ++level)
            m_levels.push_back(new Volume(device, MipDimension(width, level), MipDimension(height, level), MipDimension(depth, level),
                                          format, usage, pool, this));
        if (gpu::Available() && GpuFormat(format) != gpu::Format::Unknown)
        {
            gpu::TextureDesc desc;
            desc.kind = gpu::TextureKind::Volume;
            desc.format = GpuFormat(format);
            desc.width = width;
            desc.height = height;
            desc.depth = depth;
            desc.levels = count;
            m_gpu = gpu::CreateTexture(desc);
            for (UINT level = 0; level < count; ++level)
                m_levels[level]->AttachGpu(m_gpu, level);
        }
    }

    ~VolumeTexture() override
    {
        for (Volume *level : m_levels)
            delete level;
        gpu::DestroyTexture(m_gpu);
    }

    gpu::Texture *Gpu() const { return m_gpu; }

    HRESULT GetDevice(struct IDirect3DDevice9 **ppDevice) override { return ReturnReferenced(m_device, ppDevice, D3DERR_INVALIDCALL); }
    D3DRESOURCETYPE GetType() override { return D3DRTYPE_VOLUMETEXTURE; }
    DWORD GetLevelCount() override { return static_cast<DWORD>(m_levels.size()); }

    HRESULT GetLevelDesc(UINT Level, D3DVOLUME_DESC *pDesc) override
    {
        if (Level >= m_levels.size())
            return D3DERR_INVALIDCALL;
        return m_levels[Level]->GetDesc(pDesc);
    }

    HRESULT GetVolumeLevel(UINT Level, IDirect3DVolume9 **ppVolumeLevel) override
    {
        if (!ppVolumeLevel || Level >= m_levels.size())
            return D3DERR_INVALIDCALL;
        m_levels[Level]->AddRef();
        *ppVolumeLevel = m_levels[Level];
        return D3D_OK;
    }

    HRESULT LockBox(UINT level, D3DLOCKED_BOX *locked_box, const D3DBOX *box, DWORD flags) override
    {
        if (level >= m_levels.size())
            return D3DERR_INVALIDCALL;
        return m_levels[level]->LockBox(locked_box, box, flags);
    }

    HRESULT UnlockBox(UINT Level) override { return Level < m_levels.size() ? m_levels[Level]->UnlockBox() : D3DERR_INVALIDCALL; }
    HRESULT AddDirtyBox(const D3DBOX *dirty_box) override { (void)dirty_box; return D3D_OK; }
    std::vector<Volume *> &Levels() { return m_levels; }

private:
    IDirect3DDevice9 *m_device;
    std::vector<Volume *> m_levels;
    gpu::Texture *m_gpu = nullptr;
};

// ---------------------------------------------------------------------------
// Buffers, declarations, shaders, queries

class VertexBuffer final : public KisakD3D9Default_IDirect3DVertexBuffer9
{
public:
    VertexBuffer(IDirect3DDevice9 *device, UINT length, DWORD usage, DWORD fvf, D3DPOOL pool) : m_device(device), m_data(length)
    {
        m_desc.Format = D3DFMT_UNKNOWN;
        m_desc.Type = D3DRTYPE_VERTEXBUFFER;
        m_desc.Usage = usage;
        m_desc.Pool = pool;
        m_desc.Size = length;
        m_desc.FVF = fvf;
        m_gpu = gpu::CreateBuffer(length);
    }

    ~VertexBuffer() override { gpu::DestroyBuffer(m_gpu); }

    HRESULT GetDevice(struct IDirect3DDevice9 **ppDevice) override { return ReturnReferenced(m_device, ppDevice, D3DERR_INVALIDCALL); }
    D3DRESOURCETYPE GetType() override { return D3DRTYPE_VERTEXBUFFER; }

    HRESULT Lock(UINT OffsetToLock, UINT SizeToLock, void **ppbData, DWORD Flags) override
    {
        m_lockFlags = Flags;
        if (!ppbData || OffsetToLock > m_data.size())
            return D3DERR_INVALIDCALL;
        *ppbData = m_data.data() + OffsetToLock;
        m_lockOffset = OffsetToLock;
        m_lockSize = SizeToLock ? std::min<size_t>(SizeToLock, m_data.size() - OffsetToLock) : m_data.size() - OffsetToLock;
        return D3D_OK;
    }

    HRESULT Unlock() override
    {
        if (m_gpu && !(m_lockFlags & D3DLOCK_READONLY))
        {
            const auto update = (m_lockFlags & D3DLOCK_DISCARD) ? gpu::BufferUpdate::Discard
                              : (m_lockFlags & D3DLOCK_NOOVERWRITE) ? gpu::BufferUpdate::NoOverwrite
                              : gpu::BufferUpdate::Preserve;
            gpu::UploadBuffer(m_gpu, m_lockOffset, m_data.data() + m_lockOffset, m_lockSize, update);
        }
        return D3D_OK;
    }

    gpu::Buffer *Gpu() const { return m_gpu; }

    HRESULT GetDesc(D3DVERTEXBUFFER_DESC *pDesc) override
    {
        if (!pDesc)
            return D3DERR_INVALIDCALL;
        *pDesc = m_desc;
        return D3D_OK;
    }

private:
    IDirect3DDevice9 *m_device;
    D3DVERTEXBUFFER_DESC m_desc{};
    std::vector<uint8_t> m_data;
    gpu::Buffer *m_gpu = nullptr;
    size_t m_lockOffset = 0;
    size_t m_lockSize = 0;
    DWORD m_lockFlags = 0;
};

class IndexBuffer final : public KisakD3D9Default_IDirect3DIndexBuffer9
{
public:
    IndexBuffer(IDirect3DDevice9 *device, UINT length, DWORD usage, D3DFORMAT format, D3DPOOL pool) : m_device(device), m_data(length)
    {
        m_desc.Format = format;
        m_desc.Type = D3DRTYPE_INDEXBUFFER;
        m_desc.Usage = usage;
        m_desc.Pool = pool;
        m_desc.Size = length;
        m_gpu = gpu::CreateBuffer(length);
    }

    ~IndexBuffer() override { gpu::DestroyBuffer(m_gpu); }

    bool Is32() const { return m_desc.Format == D3DFMT_INDEX32; }

    HRESULT GetDevice(struct IDirect3DDevice9 **ppDevice) override { return ReturnReferenced(m_device, ppDevice, D3DERR_INVALIDCALL); }
    D3DRESOURCETYPE GetType() override { return D3DRTYPE_INDEXBUFFER; }

    HRESULT Lock(UINT OffsetToLock, UINT SizeToLock, void **ppbData, DWORD Flags) override
    {
        m_lockFlags = Flags;
        if (!ppbData || OffsetToLock > m_data.size())
            return D3DERR_INVALIDCALL;
        *ppbData = m_data.data() + OffsetToLock;
        m_lockOffset = OffsetToLock;
        m_lockSize = SizeToLock ? std::min<size_t>(SizeToLock, m_data.size() - OffsetToLock) : m_data.size() - OffsetToLock;
        return D3D_OK;
    }

    HRESULT Unlock() override
    {
        if (m_gpu && !(m_lockFlags & D3DLOCK_READONLY))
        {
            const auto update = (m_lockFlags & D3DLOCK_DISCARD) ? gpu::BufferUpdate::Discard
                              : (m_lockFlags & D3DLOCK_NOOVERWRITE) ? gpu::BufferUpdate::NoOverwrite
                              : gpu::BufferUpdate::Preserve;
            gpu::UploadBuffer(m_gpu, m_lockOffset, m_data.data() + m_lockOffset, m_lockSize, update);
        }
        return D3D_OK;
    }

    gpu::Buffer *Gpu() const { return m_gpu; }

    HRESULT GetDesc(D3DINDEXBUFFER_DESC *pDesc) override
    {
        if (!pDesc)
            return D3DERR_INVALIDCALL;
        *pDesc = m_desc;
        return D3D_OK;
    }

private:
    IDirect3DDevice9 *m_device;
    D3DINDEXBUFFER_DESC m_desc{};
    std::vector<uint8_t> m_data;
    gpu::Buffer *m_gpu = nullptr;
    size_t m_lockOffset = 0;
    size_t m_lockSize = 0;
    DWORD m_lockFlags = 0;
};

class VertexDeclaration final : public KisakD3D9Default_IDirect3DVertexDeclaration9
{
public:
    VertexDeclaration(IDirect3DDevice9 *device, const D3DVERTEXELEMENT9 *elements) : m_device(device)
    {
        // Copy through the terminating D3DDECL_END element (stream 0xFF).
        do
        {
            m_elements.push_back(*elements);
        } while ((elements++)->Stream != 0xFF);
        std::vector<gpu::VertexElement> converted;
        for (const D3DVERTEXELEMENT9 &element : m_elements)
        {
            if (element.Stream == 0xFF)
                break;
            gpu::VertexElement out;
            out.stream = element.Stream;
            out.offset = element.Offset;
            out.type = element.Type <= D3DDECLTYPE_UNUSED ? static_cast<gpu::VertexType>(element.Type) : gpu::VertexType::Unused;
            out.usage = element.Usage;
            out.usageIndex = element.UsageIndex;
            converted.push_back(out);
        }
        m_gpu = gpu::CreateVertexLayout(converted.data(), converted.size());
    }

    ~VertexDeclaration() override { gpu::DestroyVertexLayout(m_gpu); }

    gpu::VertexLayout *Gpu() const { return m_gpu; }

    HRESULT GetDevice(struct IDirect3DDevice9 **ppDevice) override { return ReturnReferenced(m_device, ppDevice, D3DERR_INVALIDCALL); }

    HRESULT GetDeclaration(D3DVERTEXELEMENT9 *elements, UINT *pNumElements) override
    {
        if (!pNumElements)
            return D3DERR_INVALIDCALL;
        if (elements)
            std::copy(m_elements.begin(), m_elements.end(), elements);
        *pNumElements = static_cast<UINT>(m_elements.size());
        return D3D_OK;
    }

private:
    IDirect3DDevice9 *m_device;
    std::vector<D3DVERTEXELEMENT9> m_elements;
    gpu::VertexLayout *m_gpu = nullptr;
};

// Shader bytecode is kept for the Metal translator.
std::vector<DWORD> CopyShaderTokens(const DWORD *byte_code)
{
    std::vector<DWORD> tokens;
    do
    {
        tokens.push_back(*byte_code);
    } while (*byte_code++ != 0x0000FFFF);
    return tokens;
}

// Development aid for the Metal shader translator: each unique shader the
// engine creates is written once to Documents/shaders for offline analysis.
static void DumpShaderTokens(const std::vector<DWORD> &tokens)
{
    if (tokens.empty() || !getenv("KISAK_DUMP_SHADERS"))
        return;
    const char *root = getenv("KISAK_INSTALL_PATH");
    if (!root)
        return;
    uint64_t hash = 1469598103934665603ull;
    for (DWORD token : tokens)
    {
        for (int shift = 0; shift < 32; shift += 8)
        {
            hash ^= (token >> shift) & 0xFF;
            hash *= 1099511628211ull;
        }
    }
    const bool pixel = (tokens[0] >> 16) == 0xFFFF;
    char dir[1024];
    snprintf(dir, sizeof(dir), "%s/shaders", root);
    mkdir(dir, 0755);
    char path[1100];
    snprintf(path, sizeof(path), "%s/%s_%016llx.bin", dir, pixel ? "ps" : "vs", static_cast<unsigned long long>(hash));
    struct stat existing;
    if (stat(path, &existing) == 0)
        return;
    if (FILE *file = fopen(path, "wb"))
    {
        fwrite(tokens.data(), sizeof(DWORD), tokens.size(), file);
        fclose(file);
    }
}

template<typename Base> class Shader final : public Base
{
public:
    Shader(IDirect3DDevice9 *device, const DWORD *byte_code) : m_device(device), m_tokens(CopyShaderTokens(byte_code))
    {
        DumpShaderTokens(m_tokens);
        m_gpu = gpu::CreateShader(m_tokens.data(), m_tokens.size());
    }

    ~Shader() override { gpu::DestroyShader(m_gpu); }

    gpu::Shader *Gpu() const { return m_gpu; }

    HRESULT GetDevice(struct IDirect3DDevice9 **ppDevice) override { return ReturnReferenced(m_device, ppDevice, D3DERR_INVALIDCALL); }

    HRESULT GetFunction(void *data, UINT *pSizeOfData) override
    {
        if (!pSizeOfData)
            return D3DERR_INVALIDCALL;
        const UINT bytes = static_cast<UINT>(m_tokens.size() * sizeof(DWORD));
        if (data)
        {
            if (*pSizeOfData < bytes)
                return D3DERR_INVALIDCALL;
            memcpy(data, m_tokens.data(), bytes);
        }
        *pSizeOfData = bytes;
        return D3D_OK;
    }

private:
    IDirect3DDevice9 *m_device;
    std::vector<DWORD> m_tokens;
    gpu::Shader *m_gpu = nullptr;
};

class Query final : public KisakD3D9Default_IDirect3DQuery9
{
public:
    Query(IDirect3DDevice9 *device, D3DQUERYTYPE type) : m_device(device), m_type(type) {}

    HRESULT GetDevice(struct IDirect3DDevice9 **ppDevice) override { return ReturnReferenced(m_device, ppDevice, D3DERR_INVALIDCALL); }
    D3DQUERYTYPE GetType() override { return m_type; }
    DWORD GetDataSize() override { return m_type == D3DQUERYTYPE_OCCLUSION ? sizeof(DWORD) : sizeof(WINBOOL); }
    HRESULT Issue(DWORD dwIssueFlags) override { (void)dwIssueFlags; return D3D_OK; }

    HRESULT GetData(void *pData, DWORD dwSize, DWORD dwGetDataFlags) override
    {
        (void)dwGetDataFlags;
        // Nothing is rasterized yet: everything is visible and every event has completed.
        if (pData && m_type == D3DQUERYTYPE_OCCLUSION && dwSize >= sizeof(DWORD))
            *static_cast<DWORD *>(pData) = 1;
        else if (pData && dwSize >= sizeof(WINBOOL))
            *static_cast<WINBOOL *>(pData) = 1;
        return D3D_OK;
    }

private:
    IDirect3DDevice9 *m_device;
    D3DQUERYTYPE m_type;
};

// ---------------------------------------------------------------------------
// Swap chain

D3DPRESENT_PARAMETERS NormalizePresentParameters(const D3DPRESENT_PARAMETERS &requested)
{
    D3DPRESENT_PARAMETERS params = requested;
    const D3DDISPLAYMODE mode = CurrentDisplayMode(D3DFMT_X8R8G8B8);
    if (!params.BackBufferWidth)
        params.BackBufferWidth = mode.Width;
    if (!params.BackBufferHeight)
        params.BackBufferHeight = mode.Height;
    if (params.BackBufferFormat == D3DFMT_UNKNOWN)
        params.BackBufferFormat = D3DFMT_X8R8G8B8;
    if (!params.BackBufferCount)
        params.BackBufferCount = 1;
    return params;
}

class SwapChain final : public KisakD3D9Default_IDirect3DSwapChain9
{
public:
    SwapChain(IDirect3DDevice9 *device, const D3DPRESENT_PARAMETERS &params)
        : m_device(device), m_params(NormalizePresentParameters(params))
    {
        m_backBuffer = new Surface(device, m_params.BackBufferWidth, m_params.BackBufferHeight, m_params.BackBufferFormat,
                                   D3DUSAGE_RENDERTARGET, D3DPOOL_DEFAULT, m_params.MultiSampleType, nullptr);
    }

    ~SwapChain() override { m_backBuffer->Release(); }

    HRESULT Present(const RECT *src_rect, const RECT *dst_rect, HWND dst_window_override, const RGNDATA *dirty_region, DWORD flags) override
    {
        (void)src_rect; (void)dst_rect; (void)dst_window_override; (void)dirty_region; (void)flags;
        gpu::Present(m_backBuffer->Binding());
        return D3D_OK;
    }

    HRESULT GetFrontBufferData(struct IDirect3DSurface9 *pDestSurface) override { (void)pDestSurface; return D3D_OK; }

    HRESULT GetBackBuffer(UINT iBackBuffer, D3DBACKBUFFER_TYPE Type, struct IDirect3DSurface9 **ppBackBuffer) override
    {
        (void)Type;
        if (iBackBuffer != 0)
            return D3DERR_INVALIDCALL;
        return ReturnReferenced<IDirect3DSurface9>(m_backBuffer, ppBackBuffer, D3DERR_INVALIDCALL);
    }

    HRESULT GetRasterStatus(D3DRASTER_STATUS *pRasterStatus) override
    {
        if (!pRasterStatus)
            return D3DERR_INVALIDCALL;
        memset(pRasterStatus, 0, sizeof(*pRasterStatus));
        return D3D_OK;
    }

    HRESULT GetDisplayMode(D3DDISPLAYMODE *pMode) override
    {
        if (!pMode)
            return D3DERR_INVALIDCALL;
        *pMode = CurrentDisplayMode(D3DFMT_X8R8G8B8);
        return D3D_OK;
    }

    HRESULT GetDevice(struct IDirect3DDevice9 **ppDevice) override { return ReturnReferenced(m_device, ppDevice, D3DERR_INVALIDCALL); }

    HRESULT GetPresentParameters(D3DPRESENT_PARAMETERS *pPresentationParameters) override
    {
        if (!pPresentationParameters)
            return D3DERR_INVALIDCALL;
        *pPresentationParameters = m_params;
        return D3D_OK;
    }

    Surface *BackBuffer() const { return m_backBuffer; }
    const D3DPRESENT_PARAMETERS &Params() const { return m_params; }

private:
    IDirect3DDevice9 *m_device;
    D3DPRESENT_PARAMETERS m_params{};
    Surface *m_backBuffer;
};

// ---------------------------------------------------------------------------
// Device

const UINT kMaxRenderTargets = 4;
const UINT kMaxSamplers = 20; // 16 pixel samplers + 4 vertex texture samplers
const UINT kMaxStreams = 16;

int SamplerSlot(DWORD stage)
{
    if (stage < 16)
        return static_cast<int>(stage);
    if (stage >= D3DVERTEXTEXTURESAMPLER0 && stage <= D3DVERTEXTEXTURESAMPLER3)
        return 16 + static_cast<int>(stage - D3DVERTEXTEXTURESAMPLER0);
    return -1;
}

void CopySurface(Surface *source, Surface *destination)
{
    if (!source || !destination || source == destination)
        return;
    const D3DSURFACE_DESC &s = source->Desc();
    const D3DSURFACE_DESC &d = destination->Desc();
    if (s.Width == d.Width && s.Height == d.Height && s.Format == d.Format)
    {
        destination->Bits() = source->Bits();
        destination->UnlockRect();
    }
}

class Device final : public KisakD3D9Default_IDirect3DDevice9
{
public:
    Device(IDirect3D9 *d3d, UINT adapter, D3DDEVTYPE deviceType, HWND focusWindow, DWORD behaviorFlags, const D3DPRESENT_PARAMETERS &params)
        : m_d3d(d3d)
    {
        m_d3d->AddRef();
        m_creation.AdapterOrdinal = adapter;
        m_creation.DeviceType = deviceType;
        m_creation.hFocusWindow = focusWindow;
        m_creation.BehaviorFlags = behaviorFlags;
        gpu::Initialize(KisakApple_GetRenderWindow());
        SetDefaultStates();
        CreateImplicitTargets(params);
    }

    ~Device() override
    {
        ReleaseBindings();
        ReleaseImplicitTargets();
        m_d3d->Release();
    }

    HRESULT TestCooperativeLevel() override { return D3D_OK; }
    // Unified memory: report a generous but finite texture budget.
    UINT GetAvailableTextureMem() override { return 1024u * 1024u * 1024u; }
    HRESULT EvictManagedResources() override { return D3D_OK; }
    HRESULT GetDirect3D(IDirect3D9 **ppD3D9) override { return ReturnReferenced(m_d3d, ppD3D9, D3DERR_INVALIDCALL); }

    HRESULT GetDeviceCaps(D3DCAPS9 *pCaps) override
    {
        if (!pCaps)
            return D3DERR_INVALIDCALL;
        FillCaps(pCaps);
        return D3D_OK;
    }

    HRESULT GetDisplayMode(UINT iSwapChain, D3DDISPLAYMODE *pMode) override
    {
        return iSwapChain == 0 ? m_swapChain->GetDisplayMode(pMode) : D3DERR_INVALIDCALL;
    }

    HRESULT GetCreationParameters(D3DDEVICE_CREATION_PARAMETERS *pParameters) override
    {
        if (!pParameters)
            return D3DERR_INVALIDCALL;
        *pParameters = m_creation;
        return D3D_OK;
    }

    HRESULT CreateAdditionalSwapChain(D3DPRESENT_PARAMETERS *pPresentationParameters, IDirect3DSwapChain9 **pSwapChain) override
    {
        if (!pPresentationParameters || !pSwapChain)
            return D3DERR_INVALIDCALL;
        *pSwapChain = new SwapChain(this, *pPresentationParameters);
        return D3D_OK;
    }

    HRESULT GetSwapChain(UINT iSwapChain, IDirect3DSwapChain9 **pSwapChain) override
    {
        if (iSwapChain != 0)
            return D3DERR_INVALIDCALL;
        return ReturnReferenced<IDirect3DSwapChain9>(m_swapChain, pSwapChain, D3DERR_INVALIDCALL);
    }

    UINT GetNumberOfSwapChains() override { return 1; }

    HRESULT Reset(D3DPRESENT_PARAMETERS *pPresentationParameters) override
    {
        if (!pPresentationParameters)
            return D3DERR_INVALIDCALL;
        ReleaseImplicitTargets();
        CreateImplicitTargets(*pPresentationParameters);
        *pPresentationParameters = m_swapChain->Params();
        return D3D_OK;
    }

    HRESULT Present(const RECT *src_rect, const RECT *dst_rect, HWND dst_window_override, const RGNDATA *dirty_region) override
    {
        return m_swapChain->Present(src_rect, dst_rect, dst_window_override, dirty_region, 0);
    }

    HRESULT GetBackBuffer(UINT iSwapChain, UINT iBackBuffer, D3DBACKBUFFER_TYPE Type, IDirect3DSurface9 **ppBackBuffer) override
    {
        return iSwapChain == 0 ? m_swapChain->GetBackBuffer(iBackBuffer, Type, ppBackBuffer) : D3DERR_INVALIDCALL;
    }

    void SetGammaRamp(UINT swapchain_idx, DWORD flags, const D3DGAMMARAMP *ramp) override
    {
        (void)swapchain_idx; (void)flags;
        if (ramp)
            m_gammaRamp = *ramp;
    }

    void GetGammaRamp(UINT iSwapChain, D3DGAMMARAMP *pRamp) override
    {
        (void)iSwapChain;
        if (pRamp)
            *pRamp = m_gammaRamp;
    }

    HRESULT CreateTexture(UINT Width, UINT Height, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DTexture9 **ppTexture,
                          HANDLE *pSharedHandle) override
    {
        (void)pSharedHandle;
        if (!ppTexture || !Width || !Height)
            return D3DERR_INVALIDCALL;
        *ppTexture = new Texture(this, Width, Height, Levels, Usage, Format, Pool);
        return D3D_OK;
    }

    HRESULT CreateVolumeTexture(UINT Width, UINT Height, UINT Depth, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool,
                                IDirect3DVolumeTexture9 **ppVolumeTexture, HANDLE *pSharedHandle) override
    {
        (void)pSharedHandle;
        if (!ppVolumeTexture || !Width || !Height || !Depth)
            return D3DERR_INVALIDCALL;
        *ppVolumeTexture = new VolumeTexture(this, Width, Height, Depth, Levels, Usage, Format, Pool);
        return D3D_OK;
    }

    HRESULT CreateCubeTexture(UINT EdgeLength, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DCubeTexture9 **ppCubeTexture,
                              HANDLE *pSharedHandle) override
    {
        (void)pSharedHandle;
        if (!ppCubeTexture || !EdgeLength)
            return D3DERR_INVALIDCALL;
        *ppCubeTexture = new CubeTexture(this, EdgeLength, Levels, Usage, Format, Pool);
        return D3D_OK;
    }

    HRESULT CreateVertexBuffer(UINT Length, DWORD Usage, DWORD FVF, D3DPOOL Pool, IDirect3DVertexBuffer9 **ppVertexBuffer, HANDLE *pSharedHandle) override
    {
        (void)pSharedHandle;
        if (!ppVertexBuffer || !Length)
            return D3DERR_INVALIDCALL;
        *ppVertexBuffer = new VertexBuffer(this, Length, Usage, FVF, Pool);
        return D3D_OK;
    }

    HRESULT CreateIndexBuffer(UINT Length, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DIndexBuffer9 **ppIndexBuffer, HANDLE *pSharedHandle) override
    {
        (void)pSharedHandle;
        if (!ppIndexBuffer || !Length)
            return D3DERR_INVALIDCALL;
        *ppIndexBuffer = new IndexBuffer(this, Length, Usage, Format, Pool);
        return D3D_OK;
    }

    HRESULT CreateRenderTarget(UINT Width, UINT Height, D3DFORMAT Format, D3DMULTISAMPLE_TYPE MultiSample, DWORD MultisampleQuality, WINBOOL Lockable,
                               IDirect3DSurface9 **ppSurface, HANDLE *pSharedHandle) override
    {
        (void)MultisampleQuality; (void)Lockable; (void)pSharedHandle;
        if (!ppSurface || !Width || !Height)
            return D3DERR_INVALIDCALL;
        *ppSurface = new Surface(this, Width, Height, Format, D3DUSAGE_RENDERTARGET, D3DPOOL_DEFAULT, MultiSample, nullptr);
        return D3D_OK;
    }

    HRESULT CreateDepthStencilSurface(UINT Width, UINT Height, D3DFORMAT Format, D3DMULTISAMPLE_TYPE MultiSample, DWORD MultisampleQuality,
                                      WINBOOL Discard, IDirect3DSurface9 **ppSurface, HANDLE *pSharedHandle) override
    {
        (void)MultisampleQuality; (void)Discard; (void)pSharedHandle;
        if (!ppSurface || !Width || !Height)
            return D3DERR_INVALIDCALL;
        *ppSurface = new Surface(this, Width, Height, Format, D3DUSAGE_DEPTHSTENCIL, D3DPOOL_DEFAULT, MultiSample, nullptr);
        return D3D_OK;
    }

    HRESULT UpdateSurface(IDirect3DSurface9 *src_surface, const RECT *src_rect, IDirect3DSurface9 *dst_surface, const POINT *dst_point) override
    {
        if (!src_rect && !dst_point)
            CopySurface(static_cast<Surface *>(src_surface), static_cast<Surface *>(dst_surface));
        return D3D_OK;
    }

    HRESULT UpdateTexture(IDirect3DBaseTexture9 *pSourceTexture, IDirect3DBaseTexture9 *pDestinationTexture) override
    {
        if (!pSourceTexture || !pDestinationTexture)
            return D3DERR_INVALIDCALL;
        if (pSourceTexture->GetType() == D3DRTYPE_TEXTURE && pDestinationTexture->GetType() == D3DRTYPE_TEXTURE)
        {
            auto &source = static_cast<Texture *>(pSourceTexture)->Levels();
            auto &destination = static_cast<Texture *>(pDestinationTexture)->Levels();
            for (size_t level = 0; level < std::min(source.size(), destination.size()); ++level)
                CopySurface(source[level], destination[level]);
        }
        else if (pSourceTexture->GetType() == D3DRTYPE_VOLUMETEXTURE && pDestinationTexture->GetType() == D3DRTYPE_VOLUMETEXTURE)
        {
            // Model lighting fills a system-memory volume and copies it here every frame; without this the GPU copy stays black.
            auto &source = static_cast<VolumeTexture *>(pSourceTexture)->Levels();
            auto &destination = static_cast<VolumeTexture *>(pDestinationTexture)->Levels();
            for (size_t level = 0; level < std::min(source.size(), destination.size()); ++level)
                destination[level]->CopyFrom(*source[level]);
        }
        return D3D_OK;
    }

    HRESULT GetRenderTargetData(IDirect3DSurface9 *pRenderTarget, IDirect3DSurface9 *pDestSurface) override
    {
        CopySurface(static_cast<Surface *>(pRenderTarget), static_cast<Surface *>(pDestSurface));
        return D3D_OK;
    }

    HRESULT GetFrontBufferData(UINT iSwapChain, IDirect3DSurface9 *pDestSurface) override
    {
        (void)iSwapChain;
        CopySurface(m_swapChain->BackBuffer(), static_cast<Surface *>(pDestSurface));
        return D3D_OK;
    }

    HRESULT StretchRect(IDirect3DSurface9 *src_surface, const RECT *src_rect, IDirect3DSurface9 *dst_surface, const RECT *dst_rect,
                        D3DTEXTUREFILTERTYPE filter) override
    {
        (void)filter;
        Surface *source = static_cast<Surface *>(src_surface);
        Surface *destination = static_cast<Surface *>(dst_surface);
        if (source && destination && source->Binding().texture && destination->Binding().texture)
            gpu::Blit(source->Binding(), destination->Binding());
        else if (!src_rect && !dst_rect)
            CopySurface(source, destination);
        return D3D_OK;
    }

    HRESULT ColorFill(IDirect3DSurface9 *surface, const RECT *rect, D3DCOLOR color) override
    {
        (void)surface; (void)rect; (void)color;
        return D3D_OK;
    }

    HRESULT CreateOffscreenPlainSurface(UINT Width, UINT Height, D3DFORMAT Format, D3DPOOL Pool, IDirect3DSurface9 **ppSurface,
                                        HANDLE *pSharedHandle) override
    {
        (void)pSharedHandle;
        if (!ppSurface || !Width || !Height)
            return D3DERR_INVALIDCALL;
        *ppSurface = new Surface(this, Width, Height, Format, 0, Pool, D3DMULTISAMPLE_NONE, nullptr);
        return D3D_OK;
    }

    HRESULT SetRenderTarget(DWORD RenderTargetIndex, IDirect3DSurface9 *pRenderTarget) override
    {
        if (RenderTargetIndex >= kMaxRenderTargets || (RenderTargetIndex == 0 && !pRenderTarget))
            return D3DERR_INVALIDCALL;
        Assign(m_renderTargets[RenderTargetIndex], pRenderTarget);
        if (RenderTargetIndex == 0)
        {
            // As in D3D9, binding render target 0 resets the viewport and scissor to cover it.
            const D3DSURFACE_DESC &desc = static_cast<Surface *>(pRenderTarget)->Desc();
            m_viewport = {0, 0, desc.Width, desc.Height, 0.0f, 1.0f};
            m_scissor = {0, 0, static_cast<LONG>(desc.Width), static_cast<LONG>(desc.Height)};
        }
        return D3D_OK;
    }

    HRESULT GetRenderTarget(DWORD RenderTargetIndex, IDirect3DSurface9 **ppRenderTarget) override
    {
        if (RenderTargetIndex >= kMaxRenderTargets)
            return D3DERR_INVALIDCALL;
        return ReturnReferenced(m_renderTargets[RenderTargetIndex], ppRenderTarget, D3DERR_NOTFOUND);
    }

    HRESULT SetDepthStencilSurface(IDirect3DSurface9 *pNewZStencil) override
    {
        Assign(m_depthStencil, pNewZStencil);
        return D3D_OK;
    }

    HRESULT GetDepthStencilSurface(IDirect3DSurface9 **ppZStencilSurface) override
    {
        return ReturnReferenced(m_depthStencil, ppZStencilSurface, D3DERR_NOTFOUND);
    }

    HRESULT BeginScene() override { return D3D_OK; }
    HRESULT EndScene() override { return D3D_OK; }

    HRESULT Clear(DWORD rect_count, const D3DRECT *rects, DWORD flags, D3DCOLOR color, float z, DWORD stencil) override
    {
        (void)rect_count; (void)rects;
        if (gpu::Available())
        {
            gpu::DrawState state;
            FillDrawState(state);
            gpu::Clear(state, (flags & D3DCLEAR_TARGET) != 0, (flags & D3DCLEAR_ZBUFFER) != 0, (flags & D3DCLEAR_STENCIL) != 0, color, z,
                       static_cast<uint8_t>(stencil));
        }
        return D3D_OK;
    }

    HRESULT SetViewport(const D3DVIEWPORT9 *viewport) override
    {
        if (!viewport)
            return D3DERR_INVALIDCALL;
        m_viewport = *viewport;
        return D3D_OK;
    }

    HRESULT GetViewport(D3DVIEWPORT9 *pViewport) override
    {
        if (!pViewport)
            return D3DERR_INVALIDCALL;
        *pViewport = m_viewport;
        return D3D_OK;
    }

    HRESULT SetRenderState(D3DRENDERSTATETYPE State, DWORD Value) override
    {
        if (static_cast<UINT>(State) >= 256)
            return D3DERR_INVALIDCALL;
        m_renderStates[State] = Value;
        return D3D_OK;
    }

    HRESULT GetRenderState(D3DRENDERSTATETYPE State, DWORD *pValue) override
    {
        if (!pValue || static_cast<UINT>(State) >= 256)
            return D3DERR_INVALIDCALL;
        *pValue = m_renderStates[State];
        return D3D_OK;
    }

    HRESULT SetTexture(DWORD Stage, IDirect3DBaseTexture9 *pTexture) override
    {
        const int slot = SamplerSlot(Stage);
        if (slot < 0)
            return D3D_OK; // displacement-map sampler: not used by this renderer
        Assign(m_textures[slot], pTexture);
        return D3D_OK;
    }

    HRESULT GetTexture(DWORD Stage, IDirect3DBaseTexture9 **ppTexture) override
    {
        const int slot = SamplerSlot(Stage);
        if (!ppTexture)
            return D3DERR_INVALIDCALL;
        if (slot < 0)
        {
            *ppTexture = nullptr;
            return D3D_OK;
        }
        return ReturnReferenced(m_textures[slot], ppTexture, D3D_OK);
    }

    HRESULT SetSamplerState(DWORD Sampler, D3DSAMPLERSTATETYPE Type, DWORD Value) override
    {
        const int slot = SamplerSlot(Sampler);
        if (slot < 0 || static_cast<UINT>(Type) >= 14)
            return D3D_OK;
        m_samplerStates[slot][Type] = Value;
        return D3D_OK;
    }

    HRESULT GetSamplerState(DWORD Sampler, D3DSAMPLERSTATETYPE Type, DWORD *pValue) override
    {
        const int slot = SamplerSlot(Sampler);
        if (!pValue || slot < 0 || static_cast<UINT>(Type) >= 14)
            return D3DERR_INVALIDCALL;
        *pValue = m_samplerStates[slot][Type];
        return D3D_OK;
    }

    HRESULT SetScissorRect(const RECT *rect) override
    {
        if (!rect)
            return D3DERR_INVALIDCALL;
        m_scissor = *rect;
        return D3D_OK;
    }

    HRESULT GetScissorRect(RECT *pRect) override
    {
        if (!pRect)
            return D3DERR_INVALIDCALL;
        *pRect = m_scissor;
        return D3D_OK;
    }

    HRESULT DrawPrimitive(D3DPRIMITIVETYPE PrimitiveType, UINT StartVertex, UINT PrimitiveCount) override
    {
        if (gpu::Available())
        {
            gpu::DrawState state;
            FillDrawState(state);
            gpu::Draw(state, static_cast<gpu::Primitive>(PrimitiveType), PrimitiveCount, false, 0, StartVertex);
        }
        return D3D_OK;
    }

    HRESULT DrawIndexedPrimitive(D3DPRIMITIVETYPE type, INT BaseVertexIndex, UINT MinVertexIndex, UINT NumVertices, UINT startIndex, UINT primCount) override
    {
        (void)MinVertexIndex; (void)NumVertices;
        if (gpu::Available())
        {
            gpu::DrawState state;
            FillDrawState(state);
            gpu::Draw(state, static_cast<gpu::Primitive>(type), primCount, true, BaseVertexIndex, startIndex);
        }
        return D3D_OK;
    }

    HRESULT DrawPrimitiveUP(D3DPRIMITIVETYPE primitive_type, UINT primitive_count, const void *data, UINT stride) override
    {
        if (gpu::Available())
        {
            gpu::DrawState state;
            FillDrawState(state);
            gpu::DrawUserPrimitives(state, static_cast<gpu::Primitive>(primitive_type), primitive_count, data, stride);
        }
        return D3D_OK;
    }

    HRESULT DrawIndexedPrimitiveUP(D3DPRIMITIVETYPE primitive_type, UINT min_vertex_idx, UINT vertex_count, UINT primitive_count,
                                   const void *index_data, D3DFORMAT index_format, const void *data, UINT stride) override
    {
        (void)primitive_type; (void)min_vertex_idx; (void)vertex_count; (void)primitive_count;
        (void)index_data; (void)index_format; (void)data; (void)stride;
        return D3D_OK;
    }

    HRESULT CreateVertexDeclaration(const D3DVERTEXELEMENT9 *elements, IDirect3DVertexDeclaration9 **declaration) override
    {
        if (!elements || !declaration)
            return D3DERR_INVALIDCALL;
        *declaration = new VertexDeclaration(this, elements);
        return D3D_OK;
    }

    HRESULT SetVertexDeclaration(IDirect3DVertexDeclaration9 *pDecl) override
    {
        Assign(m_vertexDeclaration, pDecl);
        return D3D_OK;
    }

    HRESULT GetVertexDeclaration(IDirect3DVertexDeclaration9 **ppDecl) override
    {
        return ReturnReferenced(m_vertexDeclaration, ppDecl, D3D_OK);
    }

    HRESULT SetFVF(DWORD FVF) override
    {
        m_fvf = FVF;
        return D3D_OK;
    }

    HRESULT CreateVertexShader(const DWORD *byte_code, IDirect3DVertexShader9 **shader) override
    {
        if (!byte_code || !shader)
            return D3DERR_INVALIDCALL;
        *shader = new Shader<KisakD3D9Default_IDirect3DVertexShader9>(this, byte_code);
        return D3D_OK;
    }

    HRESULT SetVertexShader(IDirect3DVertexShader9 *pShader) override
    {
        Assign(m_vertexShader, pShader);
        return D3D_OK;
    }

    HRESULT GetVertexShader(IDirect3DVertexShader9 **ppShader) override { return ReturnReferenced(m_vertexShader, ppShader, D3D_OK); }

    HRESULT SetVertexShaderConstantF(UINT reg_idx, const float *data, UINT count) override
    {
        return StoreConstants(m_vertexConstants, reg_idx, data, count);
    }

    HRESULT SetVertexShaderConstantI(UINT reg_idx, const int *data, UINT count) override { (void)reg_idx; (void)data; (void)count; return D3D_OK; }
    HRESULT SetVertexShaderConstantB(UINT reg_idx, const WINBOOL *data, UINT count) override { (void)reg_idx; (void)data; (void)count; return D3D_OK; }

    HRESULT SetStreamSource(UINT StreamNumber, IDirect3DVertexBuffer9 *pStreamData, UINT OffsetInBytes, UINT Stride) override
    {
        if (StreamNumber >= kMaxStreams)
            return D3DERR_INVALIDCALL;
        Assign(m_streams[StreamNumber].buffer, pStreamData);
        m_streams[StreamNumber].offset = OffsetInBytes;
        m_streams[StreamNumber].stride = Stride;
        return D3D_OK;
    }

    HRESULT GetStreamSource(UINT StreamNumber, IDirect3DVertexBuffer9 **ppStreamData, UINT *OffsetInBytes, UINT *pStride) override
    {
        if (StreamNumber >= kMaxStreams || !OffsetInBytes || !pStride)
            return D3DERR_INVALIDCALL;
        *OffsetInBytes = m_streams[StreamNumber].offset;
        *pStride = m_streams[StreamNumber].stride;
        return ReturnReferenced(m_streams[StreamNumber].buffer, ppStreamData, D3D_OK);
    }

    HRESULT SetStreamSourceFreq(UINT StreamNumber, UINT Divider) override { (void)StreamNumber; (void)Divider; return D3D_OK; }

    HRESULT SetIndices(IDirect3DIndexBuffer9 *pIndexData) override
    {
        Assign(m_indices, pIndexData);
        return D3D_OK;
    }

    HRESULT GetIndices(IDirect3DIndexBuffer9 **ppIndexData) override { return ReturnReferenced(m_indices, ppIndexData, D3D_OK); }

    HRESULT CreatePixelShader(const DWORD *byte_code, IDirect3DPixelShader9 **shader) override
    {
        if (!byte_code || !shader)
            return D3DERR_INVALIDCALL;
        *shader = new Shader<KisakD3D9Default_IDirect3DPixelShader9>(this, byte_code);
        return D3D_OK;
    }

    HRESULT SetPixelShader(IDirect3DPixelShader9 *pShader) override
    {
        Assign(m_pixelShader, pShader);
        return D3D_OK;
    }

    HRESULT GetPixelShader(IDirect3DPixelShader9 **ppShader) override { return ReturnReferenced(m_pixelShader, ppShader, D3D_OK); }

    HRESULT SetPixelShaderConstantF(UINT reg_idx, const float *data, UINT count) override
    {
        return StoreConstants(m_pixelConstants, reg_idx, data, count);
    }

    HRESULT SetPixelShaderConstantI(UINT reg_idx, const int *data, UINT count) override { (void)reg_idx; (void)data; (void)count; return D3D_OK; }
    HRESULT SetPixelShaderConstantB(UINT reg_idx, const WINBOOL *data, UINT count) override { (void)reg_idx; (void)data; (void)count; return D3D_OK; }

    HRESULT CreateQuery(D3DQUERYTYPE Type, IDirect3DQuery9 **ppQuery) override
    {
        if (Type != D3DQUERYTYPE_OCCLUSION && Type != D3DQUERYTYPE_EVENT)
            return D3DERR_NOTAVAILABLE;
        if (!ppQuery)
            return D3D_OK; // a null out-parameter asks whether the type is supported
        *ppQuery = new Query(this, Type);
        return D3D_OK;
    }

    HRESULT SetTransform(D3DTRANSFORMSTATETYPE state, const D3DMATRIX *matrix) override { (void)state; (void)matrix; return D3D_OK; }
    HRESULT SetClipPlane(DWORD index, const float *plane) override { (void)index; (void)plane; return D3D_OK; }
    HRESULT SetDialogBoxMode(WINBOOL bEnableDialogs) override { (void)bEnableDialogs; return D3D_OK; }
    HRESULT SetSoftwareVertexProcessing(WINBOOL bSoftware) override { (void)bSoftware; return D3D_OK; }
    HRESULT SetCursorProperties(UINT XHotSpot, UINT YHotSpot, IDirect3DSurface9 *pCursorBitmap) override
    {
        (void)XHotSpot; (void)YHotSpot; (void)pCursorBitmap;
        return D3D_OK;
    }
    WINBOOL ShowCursor(WINBOOL bShow) override { (void)bShow; return 0; }

private:
    struct StreamBinding
    {
        IDirect3DVertexBuffer9 *buffer = nullptr;
        UINT offset = 0;
        UINT stride = 0;
    };

    void SetDefaultStates()
    {
        DWORD *rs = m_renderStates;
        rs[D3DRS_ZENABLE] = D3DZB_TRUE;
        rs[D3DRS_FILLMODE] = D3DFILL_SOLID;
        rs[D3DRS_ZWRITEENABLE] = 1;
        rs[D3DRS_SRCBLEND] = D3DBLEND_ONE;
        rs[D3DRS_DESTBLEND] = D3DBLEND_ZERO;
        rs[D3DRS_CULLMODE] = D3DCULL_CCW;
        rs[D3DRS_ZFUNC] = D3DCMP_LESSEQUAL;
        rs[D3DRS_ALPHAFUNC] = D3DCMP_ALWAYS;
        rs[D3DRS_STENCILFAIL] = rs[D3DRS_STENCILZFAIL] = rs[D3DRS_STENCILPASS] = D3DSTENCILOP_KEEP;
        rs[D3DRS_STENCILFUNC] = D3DCMP_ALWAYS;
        rs[D3DRS_CCW_STENCILFAIL] = rs[D3DRS_CCW_STENCILZFAIL] = rs[D3DRS_CCW_STENCILPASS] = D3DSTENCILOP_KEEP;
        rs[D3DRS_CCW_STENCILFUNC] = D3DCMP_ALWAYS;
        rs[D3DRS_STENCILMASK] = rs[D3DRS_STENCILWRITEMASK] = 0xFFFFFFFF;
        rs[D3DRS_COLORWRITEENABLE] = 0xF;
        rs[D3DRS_BLENDOP] = rs[D3DRS_BLENDOPALPHA] = D3DBLENDOP_ADD;
        rs[D3DRS_SRCBLENDALPHA] = D3DBLEND_ONE;
        rs[D3DRS_DESTBLENDALPHA] = D3DBLEND_ZERO;
        for (auto &sampler : m_samplerStates)
        {
            sampler[D3DSAMP_ADDRESSU] = sampler[D3DSAMP_ADDRESSV] = sampler[D3DSAMP_ADDRESSW] = D3DTADDRESS_WRAP;
            sampler[D3DSAMP_MAGFILTER] = sampler[D3DSAMP_MINFILTER] = D3DTEXF_POINT;
            sampler[D3DSAMP_MIPFILTER] = D3DTEXF_NONE;
            sampler[D3DSAMP_MAXANISOTROPY] = 1;
        }
    }

    static gpu::TargetBinding SurfaceBinding(IDirect3DSurface9 *surface)
    {
        return surface ? static_cast<Surface *>(surface)->Binding() : gpu::TargetBinding{};
    }

    static gpu::Texture *GpuTexture(IDirect3DBaseTexture9 *texture)
    {
        if (!texture)
            return nullptr;
        switch (texture->GetType())
        {
        case D3DRTYPE_TEXTURE: return static_cast<Texture *>(texture)->Gpu();
        case D3DRTYPE_CUBETEXTURE: return static_cast<CubeTexture *>(texture)->Gpu();
        case D3DRTYPE_VOLUMETEXTURE: return static_cast<VolumeTexture *>(texture)->Gpu();
        default: return nullptr;
        }
    }

    void FillDrawState(gpu::DrawState &state)
    {
        for (UINT i = 0; i < kMaxRenderTargets; ++i)
            state.color[i] = SurfaceBinding(m_renderTargets[i]);
        state.depth = SurfaceBinding(m_depthStencil);
        state.vertexShader = m_vertexShader ? static_cast<Shader<KisakD3D9Default_IDirect3DVertexShader9> *>(m_vertexShader)->Gpu() : nullptr;
        state.pixelShader = m_pixelShader ? static_cast<Shader<KisakD3D9Default_IDirect3DPixelShader9> *>(m_pixelShader)->Gpu() : nullptr;
        state.layout = m_vertexDeclaration ? static_cast<VertexDeclaration *>(m_vertexDeclaration)->Gpu() : nullptr;
        for (UINT i = 0; i < kMaxStreams; ++i)
        {
            state.streams[i].buffer = m_streams[i].buffer ? static_cast<VertexBuffer *>(m_streams[i].buffer)->Gpu() : nullptr;
            state.streams[i].offset = m_streams[i].offset;
            state.streams[i].stride = m_streams[i].stride;
        }
        if (m_indices)
        {
            state.indices = static_cast<IndexBuffer *>(m_indices)->Gpu();
            state.indices32 = static_cast<IndexBuffer *>(m_indices)->Is32();
        }
        for (UINT i = 0; i < kMaxSamplers; ++i)
        {
            state.textures[i] = GpuTexture(m_textures[i]);
            const DWORD *sampler = m_samplerStates[i];
            gpu::SamplerState &out = state.samplers[i];
            out.addressU = AddressFrom(sampler[D3DSAMP_ADDRESSU]);
            out.addressV = AddressFrom(sampler[D3DSAMP_ADDRESSV]);
            out.addressW = AddressFrom(sampler[D3DSAMP_ADDRESSW]);
            out.minFilter = FilterFrom(sampler[D3DSAMP_MINFILTER]);
            out.magFilter = FilterFrom(sampler[D3DSAMP_MAGFILTER]);
            out.mipFilter = FilterFrom(sampler[D3DSAMP_MIPFILTER]);
            out.maxAnisotropy = static_cast<uint8_t>(std::clamp<DWORD>(sampler[D3DSAMP_MAXANISOTROPY], 1, 16));
            out.borderColor = sampler[D3DSAMP_BORDERCOLOR];
        }
        state.vertexConstants = m_vertexConstants.data();
        state.pixelConstants = m_pixelConstants.data();
        state.viewport = {static_cast<float>(m_viewport.X), static_cast<float>(m_viewport.Y), static_cast<float>(m_viewport.Width),
                          static_cast<float>(m_viewport.Height), m_viewport.MinZ, m_viewport.MaxZ};
        state.scissor = {m_scissor.left, m_scissor.top, m_scissor.right, m_scissor.bottom};

        const DWORD *rs = m_renderStates;
        gpu::RenderState &r = state.render;
        r.depthEnable = rs[D3DRS_ZENABLE] != D3DZB_FALSE;
        r.depthWrite = rs[D3DRS_ZWRITEENABLE] != 0;
        r.depthFunc = CompareFrom(rs[D3DRS_ZFUNC]);
        r.stencilEnable = rs[D3DRS_STENCILENABLE] != 0;
        r.twoSidedStencil = rs[D3DRS_TWOSIDEDSTENCILMODE] != 0;
        r.stencilFront = {StencilOpFrom(rs[D3DRS_STENCILFAIL]), StencilOpFrom(rs[D3DRS_STENCILZFAIL]), StencilOpFrom(rs[D3DRS_STENCILPASS]),
                          CompareFrom(rs[D3DRS_STENCILFUNC])};
        r.stencilBack = {StencilOpFrom(rs[D3DRS_CCW_STENCILFAIL]), StencilOpFrom(rs[D3DRS_CCW_STENCILZFAIL]),
                         StencilOpFrom(rs[D3DRS_CCW_STENCILPASS]), CompareFrom(rs[D3DRS_CCW_STENCILFUNC])};
        r.stencilRef = static_cast<uint8_t>(rs[D3DRS_STENCILREF]);
        r.stencilReadMask = static_cast<uint8_t>(rs[D3DRS_STENCILMASK]);
        r.stencilWriteMask = static_cast<uint8_t>(rs[D3DRS_STENCILWRITEMASK]);
        r.blendEnable = rs[D3DRS_ALPHABLENDENABLE] != 0;
        r.separateAlphaBlend = rs[D3DRS_SEPARATEALPHABLENDENABLE] != 0;
        r.srcColor = BlendFrom(rs[D3DRS_SRCBLEND]);
        r.destColor = BlendFrom(rs[D3DRS_DESTBLEND]);
        r.colorOp = BlendOpFrom(rs[D3DRS_BLENDOP]);
        r.srcAlpha = BlendFrom(rs[D3DRS_SRCBLENDALPHA]);
        r.destAlpha = BlendFrom(rs[D3DRS_DESTBLENDALPHA]);
        r.alphaOp = BlendOpFrom(rs[D3DRS_BLENDOPALPHA]);
        r.colorWriteMask = static_cast<uint8_t>(rs[D3DRS_COLORWRITEENABLE] & 0xF);
        r.alphaTest = rs[D3DRS_ALPHATESTENABLE] != 0;
        r.alphaFunc = CompareFrom(rs[D3DRS_ALPHAFUNC]);
        r.alphaRef = static_cast<uint8_t>(rs[D3DRS_ALPHAREF] & 0xFF);
        r.cull = CullFrom(rs[D3DRS_CULLMODE]);
        r.wireframe = rs[D3DRS_FILLMODE] == D3DFILL_WIREFRAME;
        r.depthBias = FloatState(rs[D3DRS_DEPTHBIAS]);
        r.slopeScaledDepthBias = FloatState(rs[D3DRS_SLOPESCALEDEPTHBIAS]);
        r.scissorTest = rs[D3DRS_SCISSORTESTENABLE] != 0;
    }

    void CreateImplicitTargets(const D3DPRESENT_PARAMETERS &params)
    {
        m_swapChain = new SwapChain(this, params);
        const D3DPRESENT_PARAMETERS &normalized = m_swapChain->Params();
        SetRenderTarget(0, m_swapChain->BackBuffer());
        if (normalized.EnableAutoDepthStencil)
        {
            IDirect3DSurface9 *depth = nullptr;
            CreateDepthStencilSurface(normalized.BackBufferWidth, normalized.BackBufferHeight, normalized.AutoDepthStencilFormat,
                                      normalized.MultiSampleType, normalized.MultiSampleQuality, 0, &depth, nullptr);
            SetDepthStencilSurface(depth);
            depth->Release();
        }
    }

    void ReleaseImplicitTargets()
    {
        for (IDirect3DSurface9 *&target : m_renderTargets)
            Assign<IDirect3DSurface9>(target, nullptr);
        Assign<IDirect3DSurface9>(m_depthStencil, nullptr);
        if (m_swapChain)
        {
            m_swapChain->Release();
            m_swapChain = nullptr;
        }
    }

    void ReleaseBindings()
    {
        for (IDirect3DBaseTexture9 *&texture : m_textures)
            Assign<IDirect3DBaseTexture9>(texture, nullptr);
        for (StreamBinding &stream : m_streams)
            Assign<IDirect3DVertexBuffer9>(stream.buffer, nullptr);
        Assign<IDirect3DIndexBuffer9>(m_indices, nullptr);
        Assign<IDirect3DVertexDeclaration9>(m_vertexDeclaration, nullptr);
        Assign<IDirect3DVertexShader9>(m_vertexShader, nullptr);
        Assign<IDirect3DPixelShader9>(m_pixelShader, nullptr);
    }

    static HRESULT StoreConstants(std::vector<float> &store, UINT reg_idx, const float *data, UINT count)
    {
        if (!data)
            return D3DERR_INVALIDCALL;
        const size_t end = (static_cast<size_t>(reg_idx) + count) * 4;
        if (end > store.size())
            return D3DERR_INVALIDCALL;
        std::copy(data, data + count * 4, store.begin() + static_cast<ptrdiff_t>(reg_idx) * 4);
        return D3D_OK;
    }

    IDirect3D9 *m_d3d;
    D3DDEVICE_CREATION_PARAMETERS m_creation{};
    SwapChain *m_swapChain = nullptr;
    IDirect3DSurface9 *m_renderTargets[kMaxRenderTargets] = {};
    IDirect3DSurface9 *m_depthStencil = nullptr;
    DWORD m_renderStates[256] = {};
    DWORD m_samplerStates[kMaxSamplers][14] = {};
    IDirect3DBaseTexture9 *m_textures[kMaxSamplers] = {};
    StreamBinding m_streams[kMaxStreams];
    IDirect3DIndexBuffer9 *m_indices = nullptr;
    IDirect3DVertexDeclaration9 *m_vertexDeclaration = nullptr;
    IDirect3DVertexShader9 *m_vertexShader = nullptr;
    IDirect3DPixelShader9 *m_pixelShader = nullptr;
    std::vector<float> m_vertexConstants = std::vector<float>(256 * 4);
    std::vector<float> m_pixelConstants = std::vector<float>(224 * 4);
    D3DVIEWPORT9 m_viewport{};
    RECT m_scissor{};
    DWORD m_fvf = 0;
    D3DGAMMARAMP m_gammaRamp{};
};

// ---------------------------------------------------------------------------
// IDirect3D9

bool IsBlockCompressed(D3DFORMAT format)
{
    switch (static_cast<DWORD>(format))
    {
    case D3DFMT_DXT1:
    case D3DFMT_DXT2:
    case D3DFMT_DXT3:
    case D3DFMT_DXT4:
    case D3DFMT_DXT5:
        return true;
    default:
        return false;
    }
}

bool IsFourCC(D3DFORMAT format)
{
    return static_cast<DWORD>(format) > 0xFF;
}

class Direct3D9 final : public KisakD3D9Default_IDirect3D9
{
public:
    UINT GetAdapterCount() override { return 1; }

    HRESULT GetAdapterIdentifier(UINT Adapter, DWORD Flags, D3DADAPTER_IDENTIFIER9 *pIdentifier) override
    {
        (void)Flags;
        if (Adapter != 0 || !pIdentifier)
            return D3DERR_INVALIDCALL;
        memset(pIdentifier, 0, sizeof(*pIdentifier));
        snprintf(pIdentifier->Driver, sizeof(pIdentifier->Driver), "Metal");
        snprintf(pIdentifier->Description, sizeof(pIdentifier->Description), "Apple GPU (Metal)");
        snprintf(pIdentifier->DeviceName, sizeof(pIdentifier->DeviceName), "\\\\.\\DISPLAY1");
        pIdentifier->VendorId = 0x106B;
        pIdentifier->WHQLLevel = 1;
        return D3D_OK;
    }

    UINT GetAdapterModeCount(UINT Adapter, D3DFORMAT Format) override
    {
        return Adapter == 0 && (Format == D3DFMT_X8R8G8B8 || Format == D3DFMT_A8R8G8B8) ? 1 : 0;
    }

    HRESULT EnumAdapterModes(UINT Adapter, D3DFORMAT Format, UINT Mode, D3DDISPLAYMODE *pMode) override
    {
        if (Adapter != 0 || Mode != 0 || !pMode || !GetAdapterModeCount(Adapter, Format))
            return D3DERR_INVALIDCALL;
        *pMode = CurrentDisplayMode(Format);
        return D3D_OK;
    }

    HRESULT GetAdapterDisplayMode(UINT Adapter, D3DDISPLAYMODE *pMode) override
    {
        if (Adapter != 0 || !pMode)
            return D3DERR_INVALIDCALL;
        *pMode = CurrentDisplayMode(D3DFMT_X8R8G8B8);
        return D3D_OK;
    }

    HRESULT CheckDeviceType(UINT iAdapter, D3DDEVTYPE DevType, D3DFORMAT DisplayFormat, D3DFORMAT BackBufferFormat, WINBOOL bWindowed) override
    {
        (void)DevType; (void)DisplayFormat; (void)BackBufferFormat; (void)bWindowed;
        return iAdapter == 0 ? D3D_OK : D3DERR_INVALIDCALL;
    }

    HRESULT CheckDeviceFormat(UINT Adapter, D3DDEVTYPE DeviceType, D3DFORMAT AdapterFormat, DWORD Usage, D3DRESOURCETYPE RType, D3DFORMAT CheckFormat) override
    {
        (void)DeviceType; (void)AdapterFormat;
        if (Adapter != 0)
            return D3DERR_INVALIDCALL;
        // No sampled depth textures (hardware shadow maps) and no vendor
        // FOURCC extensions such as transparency multisampling yet.
        if ((Usage & D3DUSAGE_DEPTHSTENCIL) && RType == D3DRTYPE_TEXTURE)
            return D3DERR_NOTAVAILABLE;
        if (IsFourCC(CheckFormat) && !IsBlockCompressed(CheckFormat))
            return D3DERR_NOTAVAILABLE;
        return D3D_OK;
    }

    HRESULT CheckDeviceMultiSampleType(UINT Adapter, D3DDEVTYPE DeviceType, D3DFORMAT SurfaceFormat, WINBOOL Windowed,
                                       D3DMULTISAMPLE_TYPE MultiSampleType, DWORD *pQualityLevels) override
    {
        (void)DeviceType; (void)SurfaceFormat; (void)Windowed;
        if (Adapter != 0)
            return D3DERR_INVALIDCALL;
        if (MultiSampleType != D3DMULTISAMPLE_NONE)
            return D3DERR_NOTAVAILABLE;
        if (pQualityLevels)
            *pQualityLevels = 1;
        return D3D_OK;
    }

    HRESULT CheckDepthStencilMatch(UINT Adapter, D3DDEVTYPE DeviceType, D3DFORMAT AdapterFormat, D3DFORMAT RenderTargetFormat,
                                   D3DFORMAT DepthStencilFormat) override
    {
        (void)DeviceType; (void)AdapterFormat; (void)RenderTargetFormat; (void)DepthStencilFormat;
        return Adapter == 0 ? D3D_OK : D3DERR_INVALIDCALL;
    }

    HRESULT CheckDeviceFormatConversion(UINT Adapter, D3DDEVTYPE DeviceType, D3DFORMAT SourceFormat, D3DFORMAT TargetFormat) override
    {
        (void)DeviceType; (void)SourceFormat; (void)TargetFormat;
        return Adapter == 0 ? D3D_OK : D3DERR_INVALIDCALL;
    }

    HRESULT GetDeviceCaps(UINT Adapter, D3DDEVTYPE DeviceType, D3DCAPS9 *pCaps) override
    {
        (void)DeviceType;
        if (Adapter != 0 || !pCaps)
            return D3DERR_INVALIDCALL;
        FillCaps(pCaps);
        return D3D_OK;
    }

    HMONITOR GetAdapterMonitor(UINT Adapter) override
    {
        (void)Adapter;
        return nullptr;
    }

    HRESULT CreateDevice(UINT Adapter, D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags,
                         D3DPRESENT_PARAMETERS *pPresentationParameters, struct IDirect3DDevice9 **ppReturnedDeviceInterface) override
    {
        if (Adapter != 0 || !pPresentationParameters || !ppReturnedDeviceInterface)
            return D3DERR_INVALIDCALL;
        auto *device = new Device(this, Adapter, DeviceType, hFocusWindow, BehaviorFlags, *pPresentationParameters);
        *ppReturnedDeviceInterface = device;
        return D3D_OK;
    }
};

} // namespace

IDirect3D9 *WINAPI Direct3DCreate9(UINT SDKVersion)
{
    (void)SDKVersion;
    return new Direct3D9();
}
