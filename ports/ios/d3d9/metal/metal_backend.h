#pragma once

// Metal backend for the Apple Direct3D 9 layer (ports/ios/d3d9/d3d9_apple.cpp).
//
// The D3D9 layer keeps the object model the engine expects; this backend owns
// the GPU side. Its interface uses plain C++ types only, because the engine's
// Windows prelude and the Objective-C headers cannot share a translation unit.
// Enumerations follow Direct3D 9 numbering so the D3D9 layer converts with a
// cast checked by static_assert.
//
// Every function is a no-op when Initialize() has not succeeded (headless runs).

#include <cstddef>
#include <cstdint>

namespace kisak::metal {

enum class Format : uint8_t
{
    Unknown,
    BGRA8,     // D3DFMT_A8R8G8B8
    BGRX8,     // D3DFMT_X8R8G8B8 (alpha reads as one)
    RGBA8,     // D3DFMT_A8B8G8R8
    RGBX8,     // D3DFMT_X8B8G8R8
    L8,
    A8,
    A8L8,
    R5G6B5,
    X1R5G5B5,
    A1R5G5B5,
    A4R4G4B4,
    R16F,
    R32F,
    RG16F,     // D3DFMT_G16R16F
    RGBA16F,   // D3DFMT_A16B16G16R16F
    RGBA32F,   // D3DFMT_A32B32G32R32F
    DXT1,
    DXT3,
    DXT5,
    D24S8,     // all depth formats with stencil
    D16,
    D32F,
};

enum class TextureKind : uint8_t
{
    Texture2D,
    Cube,
    Volume,
};

struct TextureDesc
{
    TextureKind kind = TextureKind::Texture2D;
    Format format = Format::Unknown;
    uint32_t width = 1;
    uint32_t height = 1;
    uint32_t depth = 1;
    uint32_t levels = 1;
    bool renderTarget = false;
};

// D3DCMPFUNC numbering.
enum class Compare : uint8_t { Never = 1, Less, Equal, LessEqual, Greater, NotEqual, GreaterEqual, Always };
// D3DBLEND numbering.
enum class BlendFactor : uint8_t
{
    Zero = 1, One, SrcColor, InvSrcColor, SrcAlpha, InvSrcAlpha, DestAlpha, InvDestAlpha, DestColor, InvDestColor,
    SrcAlphaSat, BothSrcAlpha, BothInvSrcAlpha, BlendFactor, InvBlendFactor,
};
// D3DBLENDOP numbering.
enum class BlendOp : uint8_t { Add = 1, Subtract, RevSubtract, Min, Max };
// D3DCULL numbering: the winding that is culled.
enum class Cull : uint8_t { None = 1, Clockwise, CounterClockwise };
// D3DSTENCILOP numbering.
enum class StencilOp : uint8_t { Keep = 1, Zero, Replace, IncrSat, DecrSat, Invert, Incr, Decr };
// D3DTEXTUREADDRESS numbering.
enum class Address : uint8_t { Wrap = 1, Mirror, Clamp, Border, MirrorOnce };
// D3DTEXTUREFILTERTYPE numbering.
enum class Filter : uint8_t { None = 0, Point, Linear, Anisotropic };
// D3DPRIMITIVETYPE numbering.
enum class Primitive : uint8_t { PointList = 1, LineList, LineStrip, TriangleList, TriangleStrip, TriangleFan };
// D3DDECLTYPE numbering.
enum class VertexType : uint8_t
{
    Float1 = 0, Float2, Float3, Float4, Color, UByte4, Short2, Short4, UByte4N, Short2N, Short4N, UShort2N, UShort4N,
    UDec3, Dec3N, Float16_2, Float16_4, Unused,
};

struct Texture;
struct Buffer;
struct Shader;
struct VertexLayout;

struct VertexElement
{
    uint16_t stream = 0;
    uint16_t offset = 0;
    VertexType type = VertexType::Unused;
    uint8_t usage = 0;
    uint8_t usageIndex = 0;
};

struct StencilFace
{
    StencilOp fail = StencilOp::Keep;
    StencilOp depthFail = StencilOp::Keep;
    StencilOp pass = StencilOp::Keep;
    Compare func = Compare::Always;
};

struct RenderState
{
    bool depthEnable = true;
    bool depthWrite = true;
    Compare depthFunc = Compare::LessEqual;

    bool stencilEnable = false;
    bool twoSidedStencil = false;
    StencilFace stencilFront;
    StencilFace stencilBack;
    uint8_t stencilRef = 0;
    uint8_t stencilReadMask = 0xFF;
    uint8_t stencilWriteMask = 0xFF;

    bool blendEnable = false;
    bool separateAlphaBlend = false;
    BlendFactor srcColor = BlendFactor::One;
    BlendFactor destColor = BlendFactor::Zero;
    BlendOp colorOp = BlendOp::Add;
    BlendFactor srcAlpha = BlendFactor::One;
    BlendFactor destAlpha = BlendFactor::Zero;
    BlendOp alphaOp = BlendOp::Add;
    uint8_t colorWriteMask = 0xF; // D3DCOLORWRITEENABLE bits: red 1, green 2, blue 4, alpha 8

    bool alphaTest = false;
    Compare alphaFunc = Compare::Always;
    uint8_t alphaRef = 0;

    Cull cull = Cull::CounterClockwise;
    bool wireframe = false;
    float depthBias = 0.0f;
    float slopeScaledDepthBias = 0.0f;
    bool scissorTest = false;
};

struct SamplerState
{
    Address addressU = Address::Wrap;
    Address addressV = Address::Wrap;
    Address addressW = Address::Wrap;
    Filter minFilter = Filter::Point;
    Filter magFilter = Filter::Point;
    Filter mipFilter = Filter::None;
    uint8_t maxAnisotropy = 1;
    uint32_t borderColor = 0; // D3DCOLOR
};

struct Viewport
{
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    float minZ = 0.0f;
    float maxZ = 1.0f;
};

struct Rect
{
    int32_t left = 0;
    int32_t top = 0;
    int32_t right = 0;
    int32_t bottom = 0;
};

// A render target or depth surface: one level (and cube face) of a texture.
struct TargetBinding
{
    Texture *texture = nullptr;
    uint32_t face = 0;
    uint32_t level = 0;
};

struct StreamBinding
{
    Buffer *buffer = nullptr;
    uint32_t offset = 0;
    uint32_t stride = 0;
};

inline constexpr uint32_t kMaxColorTargets = 4;
inline constexpr uint32_t kMaxStreams = 16;
inline constexpr uint32_t kPixelSamplers = 16;
inline constexpr uint32_t kMaxSamplers = 20; // 16 pixel samplers, then 4 vertex texture samplers

struct DrawState
{
    TargetBinding color[kMaxColorTargets];
    TargetBinding depth;
    Shader *vertexShader = nullptr;
    Shader *pixelShader = nullptr;
    VertexLayout *layout = nullptr;
    StreamBinding streams[kMaxStreams];
    Buffer *indices = nullptr;
    bool indices32 = false;
    Texture *textures[kMaxSamplers] = {};
    SamplerState samplers[kMaxSamplers];
    const float *vertexConstants = nullptr; // 256 float4 registers
    const float *pixelConstants = nullptr;  // 224 float4 registers
    Viewport viewport;
    Rect scissor;
    RenderState render;
};

// view: the UIView whose layer is a CAMetalLayer. Returns false when Metal is
// unavailable or view is null; the backend then stays headless.
bool Initialize(void *view);
bool Available();

Texture *CreateTexture(const TextureDesc &desc);
void DestroyTexture(Texture *texture);
// bits: one level (and face) in Direct3D 9 layout, as returned by LockRect/LockBox.
void UploadTexture(Texture *texture, uint32_t face, uint32_t level, const uint8_t *bits, size_t size);

Buffer *CreateBuffer(size_t length);
void DestroyBuffer(Buffer *buffer);
enum class BufferUpdate { Preserve, Discard, NoOverwrite };
void UploadBuffer(Buffer *buffer, size_t offset, const uint8_t *data, size_t size, BufferUpdate update = BufferUpdate::Preserve);

// tokens: shader model 3 bytecode through the end token.
Shader *CreateShader(const uint32_t *tokens, size_t count);
void DestroyShader(Shader *shader);

VertexLayout *CreateVertexLayout(const VertexElement *elements, size_t count);
void DestroyVertexLayout(VertexLayout *layout);

void Draw(const DrawState &state, Primitive primitive, uint32_t primitiveCount, bool indexed, int32_t baseVertex, uint32_t start);
void DrawUserPrimitives(const DrawState &state, Primitive primitive, uint32_t primitiveCount, const void *vertices, uint32_t stride);
void Clear(const DrawState &state, bool color, bool depth, bool stencil, uint32_t argb, float z, uint8_t stencilValue);
// StretchRect between two GPU surfaces (whole surfaces, filtered).
void Blit(const TargetBinding &source, const TargetBinding &destination);
void Present(const TargetBinding &backBuffer);

} // namespace kisak::metal
