#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#include <TargetConditionals.h>
#if TARGET_OS_IPHONE
#import <UIKit/UIKit.h>
#endif

#include "metal_backend.h"
#include "dx9_msl_translator.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace kisak::metal {

// ---------------------------------------------------------------------------
// Objects

struct Texture
{
    TextureDesc desc;
    id<MTLTexture> texture;     // storage and render target
    id<MTLTexture> sampleView;  // storage with the D3D9 channel mapping (L8, X8, ...)
    MTLPixelFormat pixelFormat = MTLPixelFormatInvalid;
    bool depth = false;
    bool stencil = false;
    bool decodeDxt = false;
    bool convert16 = false;
};

struct Buffer
{
    id<MTLBuffer> buffer;
    size_t length = 0;
    // Encoded draws retain this allocation. A later CPU upload must not change
    // the bytes those draws will read when the command buffer reaches the GPU.
    bool referencedByDraw = false;
};

struct Shader
{
    std::vector<uint32_t> tokens;
    TranslatedShader base;
    bool valid = false;
    uint32_t varyingMask = 0; // vertex shaders: outputs a pixel shader can read
    std::unordered_map<std::string, id<MTLFunction>> functions; // keyed by translate options
    std::unordered_set<std::string> failed;
};

struct VertexLayout
{
    std::vector<VertexElement> elements;
};

namespace {

// Bits of TranslateOptions::availableVaryings.
int VaryingBit(uint8_t usage, uint8_t index)
{
    switch (usage)
    {
    case kUsageTexCoord: return index < 8 ? index : -1;
    case kUsageColor: return index < 2 ? 8 + index : -1;
    case kUsageFog: return index == 0 ? 10 : -1;
    case kUsageNormal: return index == 0 ? 11 : -1;
    case kUsageTangent: return index == 0 ? 12 : -1;
    case kUsageBinormal: return index == 0 ? 13 : -1;
    default: return -1;
    }
}

void LogOnce(const std::string &message)
{
    static std::mutex lock;
    static std::unordered_set<std::string> seen;
    std::lock_guard<std::mutex> guard(lock);
    if (seen.insert(message).second)
        fprintf(stderr, "Metal: %s\n", message.c_str());
}

// ---------------------------------------------------------------------------
// Format handling

bool IsDxt(Format format) { return format == Format::DXT1 || format == Format::DXT3 || format == Format::DXT5; }
bool Is16Bit(Format format)
{
    return format == Format::R5G6B5 || format == Format::X1R5G5B5 || format == Format::A1R5G5B5 || format == Format::A4R4G4B4;
}

MTLPixelFormat StorageFormat(Format format, bool bcSupported)
{
    switch (format)
    {
    case Format::BGRA8:
    case Format::BGRX8: return MTLPixelFormatBGRA8Unorm;
    case Format::RGBA8:
    case Format::RGBX8: return MTLPixelFormatRGBA8Unorm;
    case Format::L8: return MTLPixelFormatR8Unorm;
    case Format::A8: return MTLPixelFormatA8Unorm;
    case Format::A8L8: return MTLPixelFormatRG8Unorm;
    case Format::R5G6B5:
    case Format::X1R5G5B5:
    case Format::A1R5G5B5:
    case Format::A4R4G4B4: return MTLPixelFormatBGRA8Unorm;
    case Format::R16F: return MTLPixelFormatR16Float;
    case Format::R32F: return MTLPixelFormatR32Float;
    case Format::RG16F: return MTLPixelFormatRG16Float;
    case Format::RGBA16F: return MTLPixelFormatRGBA16Float;
    case Format::RGBA32F: return MTLPixelFormatRGBA32Float;
    case Format::DXT1: return bcSupported ? MTLPixelFormatBC1_RGBA : MTLPixelFormatRGBA8Unorm;
    case Format::DXT3: return bcSupported ? MTLPixelFormatBC2_RGBA : MTLPixelFormatRGBA8Unorm;
    case Format::DXT5: return bcSupported ? MTLPixelFormatBC3_RGBA : MTLPixelFormatRGBA8Unorm;
    case Format::D24S8: return MTLPixelFormatDepth32Float_Stencil8;
    case Format::D16:
    case Format::D32F: return MTLPixelFormatDepth32Float;
    default: return MTLPixelFormatInvalid;
    }
}

bool SampleSwizzle(Format format, MTLTextureSwizzleChannels &swizzle)
{
    switch (format)
    {
    case Format::BGRX8:
    case Format::RGBX8:
    case Format::R5G6B5:
    case Format::X1R5G5B5:
        swizzle = MTLTextureSwizzleChannelsMake(MTLTextureSwizzleRed, MTLTextureSwizzleGreen, MTLTextureSwizzleBlue, MTLTextureSwizzleOne);
        return format == Format::BGRX8 || format == Format::RGBX8;
    case Format::L8:
        swizzle = MTLTextureSwizzleChannelsMake(MTLTextureSwizzleRed, MTLTextureSwizzleRed, MTLTextureSwizzleRed, MTLTextureSwizzleOne);
        return true;
    case Format::A8L8:
        swizzle = MTLTextureSwizzleChannelsMake(MTLTextureSwizzleRed, MTLTextureSwizzleRed, MTLTextureSwizzleRed, MTLTextureSwizzleGreen);
        return true;
    case Format::R16F:
    case Format::R32F:
        swizzle = MTLTextureSwizzleChannelsMake(MTLTextureSwizzleRed, MTLTextureSwizzleOne, MTLTextureSwizzleOne, MTLTextureSwizzleOne);
        return true;
    case Format::RG16F:
        swizzle = MTLTextureSwizzleChannelsMake(MTLTextureSwizzleRed, MTLTextureSwizzleGreen, MTLTextureSwizzleOne, MTLTextureSwizzleOne);
        return true;
    default:
        return false;
    }
}

void ExpandRgb565(uint16_t c, uint8_t *rgba)
{
    const int r = (c >> 11) & 31, g = (c >> 5) & 63, b = c & 31;
    rgba[0] = static_cast<uint8_t>((r << 3) | (r >> 2));
    rgba[1] = static_cast<uint8_t>((g << 2) | (g >> 4));
    rgba[2] = static_cast<uint8_t>((b << 3) | (b >> 2));
    rgba[3] = 255;
}

void DecodeColorBlock(const uint8_t *block, bool allowTransparent, uint8_t texels[16][4])
{
    const uint16_t c0 = static_cast<uint16_t>(block[0] | (block[1] << 8));
    const uint16_t c1 = static_cast<uint16_t>(block[2] | (block[3] << 8));
    uint8_t palette[4][4];
    ExpandRgb565(c0, palette[0]);
    ExpandRgb565(c1, palette[1]);
    if (c0 > c1 || !allowTransparent)
    {
        for (int k = 0; k < 3; ++k)
        {
            palette[2][k] = static_cast<uint8_t>((2 * palette[0][k] + palette[1][k]) / 3);
            palette[3][k] = static_cast<uint8_t>((palette[0][k] + 2 * palette[1][k]) / 3);
        }
        palette[2][3] = palette[3][3] = 255;
    }
    else
    {
        for (int k = 0; k < 3; ++k)
            palette[2][k] = static_cast<uint8_t>((palette[0][k] + palette[1][k]) / 2);
        palette[2][3] = 255;
        palette[3][0] = palette[3][1] = palette[3][2] = palette[3][3] = 0;
    }
    const uint32_t indices = block[4] | (block[5] << 8) | (block[6] << 16) | (static_cast<uint32_t>(block[7]) << 24);
    for (int i = 0; i < 16; ++i)
        memcpy(texels[i], palette[(indices >> (2 * i)) & 3], 4);
}

// Decodes a DXT1/3/5 image to RGBA8.
std::vector<uint8_t> DecodeDxt(Format format, const uint8_t *data, size_t size, uint32_t width, uint32_t height)
{
    const uint32_t blocksX = (width + 3) / 4;
    const uint32_t blocksY = (height + 3) / 4;
    const size_t blockBytes = format == Format::DXT1 ? 8 : 16;
    std::vector<uint8_t> out(static_cast<size_t>(width) * height * 4);
    if (size < static_cast<size_t>(blocksX) * blocksY * blockBytes)
        return out;
    uint8_t texels[16][4];
    for (uint32_t by = 0; by < blocksY; ++by)
    {
        for (uint32_t bx = 0; bx < blocksX; ++bx)
        {
            const uint8_t *block = data + (static_cast<size_t>(by) * blocksX + bx) * blockBytes;
            if (format == Format::DXT1)
            {
                DecodeColorBlock(block, true, texels);
            }
            else if (format == Format::DXT3)
            {
                DecodeColorBlock(block + 8, false, texels);
                for (int i = 0; i < 16; ++i)
                    texels[i][3] = static_cast<uint8_t>(((block[i / 2] >> ((i & 1) * 4)) & 0xF) * 17);
            }
            else
            {
                DecodeColorBlock(block + 8, false, texels);
                const uint8_t a0 = block[0], a1 = block[1];
                uint8_t alphas[8] = {a0, a1};
                if (a0 > a1)
                    for (int i = 1; i < 7; ++i)
                        alphas[i + 1] = static_cast<uint8_t>(((7 - i) * a0 + i * a1) / 7);
                else
                {
                    for (int i = 1; i < 5; ++i)
                        alphas[i + 1] = static_cast<uint8_t>(((5 - i) * a0 + i * a1) / 5);
                    alphas[6] = 0;
                    alphas[7] = 255;
                }
                uint64_t bits = 0;
                for (int i = 0; i < 6; ++i)
                    bits |= static_cast<uint64_t>(block[2 + i]) << (8 * i);
                for (int i = 0; i < 16; ++i)
                    texels[i][3] = alphas[(bits >> (3 * i)) & 7];
            }
            for (uint32_t ty = 0; ty < 4; ++ty)
            {
                const uint32_t y = by * 4 + ty;
                if (y >= height)
                    break;
                for (uint32_t tx = 0; tx < 4; ++tx)
                {
                    const uint32_t x = bx * 4 + tx;
                    if (x >= width)
                        break;
                    memcpy(&out[(static_cast<size_t>(y) * width + x) * 4], texels[ty * 4 + tx], 4);
                }
            }
        }
    }
    return out;
}

std::vector<uint8_t> Convert16Bit(Format format, const uint8_t *data, size_t size, uint32_t width, uint32_t height, uint32_t depth)
{
    const size_t count = static_cast<size_t>(width) * height * depth;
    std::vector<uint8_t> out(count * 4);
    if (size < count * 2)
        return out;
    for (size_t i = 0; i < count; ++i)
    {
        const uint16_t v = static_cast<uint16_t>(data[i * 2] | (data[i * 2 + 1] << 8));
        uint8_t r, g, b, a = 255;
        switch (format)
        {
        case Format::R5G6B5:
            r = static_cast<uint8_t>(((v >> 11) & 31) * 255 / 31);
            g = static_cast<uint8_t>(((v >> 5) & 63) * 255 / 63);
            b = static_cast<uint8_t>((v & 31) * 255 / 31);
            break;
        case Format::A4R4G4B4:
            a = static_cast<uint8_t>(((v >> 12) & 15) * 17);
            r = static_cast<uint8_t>(((v >> 8) & 15) * 17);
            g = static_cast<uint8_t>(((v >> 4) & 15) * 17);
            b = static_cast<uint8_t>((v & 15) * 17);
            break;
        default:
            if (format == Format::A1R5G5B5)
                a = (v & 0x8000) ? 255 : 0;
            r = static_cast<uint8_t>(((v >> 10) & 31) * 255 / 31);
            g = static_cast<uint8_t>(((v >> 5) & 31) * 255 / 31);
            b = static_cast<uint8_t>((v & 31) * 255 / 31);
            break;
        }
        out[i * 4] = b;
        out[i * 4 + 1] = g;
        out[i * 4 + 2] = r;
        out[i * 4 + 3] = a;
    }
    return out;
}

size_t BytesPerPixel(MTLPixelFormat format)
{
    switch (format)
    {
    case MTLPixelFormatR8Unorm:
    case MTLPixelFormatA8Unorm: return 1;
    case MTLPixelFormatRG8Unorm:
    case MTLPixelFormatR16Float: return 2;
    case MTLPixelFormatRG16Float:
    case MTLPixelFormatR32Float:
    case MTLPixelFormatBGRA8Unorm:
    case MTLPixelFormatRGBA8Unorm: return 4;
    case MTLPixelFormatRGBA16Float: return 8;
    case MTLPixelFormatRGBA32Float: return 16;
    default: return 4;
    }
}

// ---------------------------------------------------------------------------
// State conversion

MTLCompareFunction CompareFunction(Compare compare)
{
    // D3DCMPFUNC and MTLCompareFunction list the same comparisons in the same order.
    const int value = static_cast<int>(compare);
    return value >= 1 && value <= 8 ? static_cast<MTLCompareFunction>(value - 1) : MTLCompareFunctionAlways;
}

MTLBlendFactor BlendFactorFor(BlendFactor factor)
{
    switch (factor)
    {
    case BlendFactor::Zero: return MTLBlendFactorZero;
    case BlendFactor::One: return MTLBlendFactorOne;
    case BlendFactor::SrcColor: return MTLBlendFactorSourceColor;
    case BlendFactor::InvSrcColor: return MTLBlendFactorOneMinusSourceColor;
    case BlendFactor::SrcAlpha: return MTLBlendFactorSourceAlpha;
    case BlendFactor::InvSrcAlpha: return MTLBlendFactorOneMinusSourceAlpha;
    case BlendFactor::DestAlpha: return MTLBlendFactorDestinationAlpha;
    case BlendFactor::InvDestAlpha: return MTLBlendFactorOneMinusDestinationAlpha;
    case BlendFactor::DestColor: return MTLBlendFactorDestinationColor;
    case BlendFactor::InvDestColor: return MTLBlendFactorOneMinusDestinationColor;
    case BlendFactor::SrcAlphaSat: return MTLBlendFactorSourceAlphaSaturated;
    case BlendFactor::BlendFactor: return MTLBlendFactorBlendColor;
    case BlendFactor::InvBlendFactor: return MTLBlendFactorOneMinusBlendColor;
    default: return MTLBlendFactorOne;
    }
}

MTLBlendOperation BlendOperationFor(BlendOp op)
{
    switch (op)
    {
    case BlendOp::Subtract: return MTLBlendOperationSubtract;
    case BlendOp::RevSubtract: return MTLBlendOperationReverseSubtract;
    case BlendOp::Min: return MTLBlendOperationMin;
    case BlendOp::Max: return MTLBlendOperationMax;
    default: return MTLBlendOperationAdd;
    }
}

MTLStencilOperation StencilOperationFor(StencilOp op)
{
    switch (op)
    {
    case StencilOp::Zero: return MTLStencilOperationZero;
    case StencilOp::Replace: return MTLStencilOperationReplace;
    case StencilOp::IncrSat: return MTLStencilOperationIncrementClamp;
    case StencilOp::DecrSat: return MTLStencilOperationDecrementClamp;
    case StencilOp::Invert: return MTLStencilOperationInvert;
    case StencilOp::Incr: return MTLStencilOperationIncrementWrap;
    case StencilOp::Decr: return MTLStencilOperationDecrementWrap;
    default: return MTLStencilOperationKeep;
    }
}

MTLSamplerAddressMode AddressModeFor(Address address)
{
    switch (address)
    {
    case Address::Mirror: return MTLSamplerAddressModeMirrorRepeat;
    case Address::Clamp: return MTLSamplerAddressModeClampToEdge;
    case Address::Border: return MTLSamplerAddressModeClampToBorderColor;
    case Address::MirrorOnce: return MTLSamplerAddressModeMirrorClampToEdge;
    default: return MTLSamplerAddressModeRepeat;
    }
}

MTLVertexFormat VertexFormatFor(VertexType type)
{
    switch (type)
    {
    case VertexType::Float1: return MTLVertexFormatFloat;
    case VertexType::Float2: return MTLVertexFormatFloat2;
    case VertexType::Float3: return MTLVertexFormatFloat3;
    case VertexType::Float4: return MTLVertexFormatFloat4;
    case VertexType::Color: return MTLVertexFormatUChar4Normalized_BGRA;
    case VertexType::UByte4: return MTLVertexFormatUChar4;
    case VertexType::Short2: return MTLVertexFormatShort2;
    case VertexType::Short4: return MTLVertexFormatShort4;
    case VertexType::UByte4N: return MTLVertexFormatUChar4Normalized;
    case VertexType::Short2N: return MTLVertexFormatShort2Normalized;
    case VertexType::Short4N: return MTLVertexFormatShort4Normalized;
    case VertexType::UShort2N: return MTLVertexFormatUShort2Normalized;
    case VertexType::UShort4N: return MTLVertexFormatUShort4Normalized;
    case VertexType::UDec3: return MTLVertexFormatUInt1010102Normalized;
    case VertexType::Dec3N: return MTLVertexFormatInt1010102Normalized;
    case VertexType::Float16_2: return MTLVertexFormatHalf2;
    case VertexType::Float16_4: return MTLVertexFormatHalf4;
    default: return MTLVertexFormatInvalid;
    }
}

uint32_t VertexCount(Primitive primitive, uint32_t primitives)
{
    switch (primitive)
    {
    case Primitive::PointList: return primitives;
    case Primitive::LineList: return primitives * 2;
    case Primitive::LineStrip: return primitives + 1;
    case Primitive::TriangleList: return primitives * 3;
    case Primitive::TriangleStrip:
    case Primitive::TriangleFan: return primitives + 2;
    default: return 0;
    }
}

MTLPrimitiveType PrimitiveTypeFor(Primitive primitive)
{
    switch (primitive)
    {
    case Primitive::PointList: return MTLPrimitiveTypePoint;
    case Primitive::LineList: return MTLPrimitiveTypeLine;
    case Primitive::LineStrip: return MTLPrimitiveTypeLineStrip;
    case Primitive::TriangleStrip: return MTLPrimitiveTypeTriangleStrip;
    default: return MTLPrimitiveTypeTriangle;
    }
}

uint32_t LevelSize(uint32_t size, uint32_t level) { return std::max<uint32_t>(size >> level, 1u); }

// ---------------------------------------------------------------------------
// Context

constexpr uint32_t kDefaultAttributeBuffer = 27;
constexpr size_t kFrameArenaSize = 16 * 1024 * 1024;
constexpr int kInflightFrames = 3;

struct Allocation
{
    id<MTLBuffer> buffer;
    size_t offset = 0;
    void *pointer = nullptr;
};

class FrameArena
{
public:
    Allocation Allocate(id<MTLDevice> device, size_t size)
    {
        // Constant-buffer offsets require 256-byte alignment on the simulator's
        // Metal device. The arena also holds vertices and indices, so round
        // every allocation to keep subsequent shader constants aligned.
        constexpr size_t alignment = 256;
        size = (size + alignment - 1) & ~(alignment - 1);
        auto &frame = m_frames[m_frame];
        while (frame.current < frame.buffers.size())
        {
            id<MTLBuffer> buffer = frame.buffers[frame.current];
            if (frame.used + size <= buffer.length)
            {
                Allocation allocation{buffer, frame.used, static_cast<uint8_t *>(buffer.contents) + frame.used};
                frame.used += size;
                return allocation;
            }
            ++frame.current;
            frame.used = 0;
        }
        id<MTLBuffer> buffer = [device newBufferWithLength:std::max(size, kFrameArenaSize) options:MTLResourceStorageModeShared];
        frame.buffers.push_back(buffer);
        frame.used = size;
        return Allocation{buffer, 0, buffer.contents};
    }

    void NextFrame()
    {
        m_frame = (m_frame + 1) % kInflightFrames;
        m_frames[m_frame].current = 0;
        m_frames[m_frame].used = 0;
    }

private:
    struct Frame
    {
        std::vector<id<MTLBuffer>> buffers;
        size_t current = 0;
        size_t used = 0;
    };
    Frame m_frames[kInflightFrames];
    int m_frame = 0;
};

struct Context
{
    id<MTLDevice> device;
    id<MTLCommandQueue> queue;
    CAMetalLayer *layer = nil;
    bool bcSupported = false;
    dispatch_semaphore_t inflight;

    id<MTLCommandBuffer> commandBuffer;
    id<MTLRenderCommandEncoder> encoder;
    TargetBinding passColor[kMaxColorTargets];
    TargetBinding passDepth;
    uint32_t passWidth = 0;
    uint32_t passHeight = 0;

    id<MTLLibrary> blitLibrary;
    std::unordered_map<NSUInteger, id<MTLRenderPipelineState>> blitPipelines;
    id<MTLSamplerState> blitSampler;
    id<MTLBuffer> defaultAttributes;
    id<MTLTexture> dummy2D;
    id<MTLTexture> dummyCube;
    id<MTLTexture> dummyVolume;
    id<MTLTexture> dummyDepth;

    std::unordered_map<std::string, id<MTLRenderPipelineState>> pipelines;
    std::unordered_set<std::string> failedPipelines;
    std::unordered_map<uint64_t, id<MTLDepthStencilState>> depthStates;
    std::unordered_map<uint64_t, id<MTLSamplerState>> samplers;
    FrameArena arena;
    std::recursive_mutex lock;
};

Context *g = nullptr;

id<MTLTexture> MakeDummy(id<MTLDevice> device, MTLTextureType type)
{
    MTLTextureDescriptor *desc = [MTLTextureDescriptor new];
    desc.textureType = type;
    desc.width = 1;
    desc.height = 1;
    desc.depth = 1;
    if (type == MTLTextureTypeCube)
        desc.arrayLength = 1;
    desc.pixelFormat = MTLPixelFormatRGBA8Unorm;
    desc.usage = MTLTextureUsageShaderRead;
    desc.storageMode = MTLStorageModeShared;
    id<MTLTexture> texture = [device newTextureWithDescriptor:desc];
    const uint8_t black[4] = {0, 0, 0, 255};
    const int slices = type == MTLTextureTypeCube ? 6 : 1;
    for (int slice = 0; slice < slices; ++slice)
        [texture replaceRegion:MTLRegionMake3D(0, 0, 0, 1, 1, 1) mipmapLevel:0 slice:static_cast<NSUInteger>(slice) withBytes:black
                   bytesPerRow:4 bytesPerImage:4];
    return texture;
}

id<MTLCommandBuffer> CommandBuffer()
{
    if (!g->commandBuffer)
    {
        dispatch_semaphore_wait(g->inflight, DISPATCH_TIME_FOREVER);
        g->commandBuffer = [g->queue commandBuffer];
        dispatch_semaphore_t inflight = g->inflight;
        [g->commandBuffer addCompletedHandler:^(id<MTLCommandBuffer>) {
          dispatch_semaphore_signal(inflight);
        }];
    }
    return g->commandBuffer;
}

void EndPass()
{
    if (g->encoder)
    {
        [g->encoder endEncoding];
        g->encoder = nil;
    }
}

bool SameBinding(const TargetBinding &a, const TargetBinding &b)
{
    return a.texture == b.texture && a.face == b.face && a.level == b.level;
}

bool SamePass(const DrawState &state)
{
    if (!g->encoder)
        return false;
    for (uint32_t i = 0; i < kMaxColorTargets; ++i)
        if (!SameBinding(state.color[i], g->passColor[i]))
            return false;
    return SameBinding(state.depth, g->passDepth);
}

bool BeginPass(const DrawState &state, bool clearColor, bool clearDepth, bool clearStencil, MTLClearColor color, double depth, uint32_t stencil)
{
    EndPass();
    MTLRenderPassDescriptor *pass = [MTLRenderPassDescriptor renderPassDescriptor];
    uint32_t width = 0, height = 0;
    bool any = false;
    for (uint32_t i = 0; i < kMaxColorTargets; ++i)
    {
        const TargetBinding &binding = state.color[i];
        g->passColor[i] = binding;
        if (!binding.texture || !binding.texture->texture || binding.texture->depth)
            continue;
        MTLRenderPassColorAttachmentDescriptor *attachment = pass.colorAttachments[i];
        attachment.texture = binding.texture->texture;
        attachment.level = binding.level;
        attachment.slice = binding.texture->desc.kind == TextureKind::Cube ? binding.face : 0;
        attachment.loadAction = clearColor ? MTLLoadActionClear : MTLLoadActionLoad;
        attachment.clearColor = color;
        attachment.storeAction = MTLStoreActionStore;
        width = LevelSize(binding.texture->desc.width, binding.level);
        height = LevelSize(binding.texture->desc.height, binding.level);
        any = true;
    }
    g->passDepth = state.depth;
    if (state.depth.texture && state.depth.texture->texture && state.depth.texture->depth)
    {
        Texture *texture = state.depth.texture;
        pass.depthAttachment.texture = texture->texture;
        pass.depthAttachment.loadAction = clearDepth ? MTLLoadActionClear : MTLLoadActionLoad;
        pass.depthAttachment.clearDepth = depth;
        pass.depthAttachment.storeAction = MTLStoreActionStore;
        if (texture->stencil)
        {
            pass.stencilAttachment.texture = texture->texture;
            pass.stencilAttachment.loadAction = clearStencil ? MTLLoadActionClear : MTLLoadActionLoad;
            pass.stencilAttachment.clearStencil = stencil;
            pass.stencilAttachment.storeAction = MTLStoreActionStore;
        }
        if (!any)
        {
            width = texture->desc.width;
            height = texture->desc.height;
        }
        any = true;
    }
    if (!any)
        return false;
    g->passWidth = width;
    g->passHeight = height;
    g->encoder = [CommandBuffer() renderCommandEncoderWithDescriptor:pass];
    return g->encoder != nil;
}

id<MTLFunction> ShaderFunction(Shader *shader, uint32_t depthSamplerMask, uint32_t availableVaryings, uint32_t unsignedIntInputMask = 0,
                               uint32_t signedIntInputMask = 0)
{
    if (!shader || !shader->valid)
        return nil;
    if (!shader->base.pixel)
        availableVaryings = 0xFFFFFFFF;
    else
        unsignedIntInputMask = signedIntInputMask = 0;
    const uint32_t keyFields[4] = {depthSamplerMask, availableVaryings, unsignedIntInputMask, signedIntInputMask};
    const std::string key(reinterpret_cast<const char *>(keyFields), sizeof(keyFields));
    auto found = shader->functions.find(key);
    if (found != shader->functions.end())
        return found->second;
    if (shader->failed.count(key))
        return nil;

    TranslateOptions options;
    options.depthSamplerMask = depthSamplerMask;
    options.availableVaryings = availableVaryings;
    options.unsignedIntInputMask = unsignedIntInputMask;
    options.signedIntInputMask = signedIntInputMask;
    TranslatedShader translated;
    if (!TranslateShader(shader->tokens.data(), shader->tokens.size(), options, translated))
    {
        LogOnce("shader translation failed: " + translated.error);
        shader->failed.insert(key);
        return nil;
    }
    {
        // Diagnostic: dump a few model vertex shaders (packed-vertex inputs) to Documents for inspection.
        static int mslDumps = 0;
        if (!shader->base.pixel && unsignedIntInputMask && mslDumps < 6)
        {
            char dumpPath[512];
            snprintf(dumpPath, sizeof(dumpPath), "%s/Documents/msl_vs_%d.metal", getenv("HOME") ? getenv("HOME") : ".", mslDumps);
            if (FILE *dump = fopen(dumpPath, "w"))
            {
                fprintf(dump, "// unsignedIntInputMask 0x%x signedIntInputMask 0x%x\n", unsignedIntInputMask, signedIntInputMask);
                fputs(translated.source.c_str(), dump);
                fclose(dump);
                ++mslDumps;
            }
        }
    }
    NSError *error = nil;
    MTLCompileOptions *compile = [MTLCompileOptions new];
    compile.languageVersion = MTLLanguageVersion2_4;
    id<MTLLibrary> library = [g->device newLibraryWithSource:[NSString stringWithUTF8String:translated.source.c_str()] options:compile error:&error];
    id<MTLFunction> function = [library newFunctionWithName:@"kisak_main"];
    if (!function)
    {
        LogOnce(std::string("shader compile failed: ") + (error ? error.localizedDescription.UTF8String : "no entry point"));
        shader->failed.insert(key);
        return nil;
    }
    shader->functions[key] = function;
    return function;
}

MTLPixelFormat AttachmentFormat(const TargetBinding &binding)
{
    return binding.texture && binding.texture->texture ? binding.texture->pixelFormat : MTLPixelFormatInvalid;
}

id<MTLRenderPipelineState> PipelineFor(const DrawState &state, id<MTLFunction> vertexFunction, id<MTLFunction> fragmentFunction,
                                       uint32_t vsDepthMask, uint32_t psDepthMask, uint32_t varyings)
{
    const RenderState &rs = state.render;
    struct Key
    {
        Shader *vs;
        Shader *ps;
        VertexLayout *layout;
        uint32_t strides[kMaxStreams];
        NSUInteger color[kMaxColorTargets];
        NSUInteger depth;
        uint8_t blend[9];
        uint32_t vsDepthMask;
        uint32_t psDepthMask;
        uint32_t varyings;
    } key;
    memset(&key, 0, sizeof(key));
    key.vs = state.vertexShader;
    key.ps = state.pixelShader;
    key.layout = state.layout;
    for (uint32_t i = 0; i < kMaxStreams; ++i)
        key.strides[i] = state.streams[i].stride;
    for (uint32_t i = 0; i < kMaxColorTargets; ++i)
        key.color[i] = state.color[i].texture && !state.color[i].texture->depth ? AttachmentFormat(state.color[i]) : MTLPixelFormatInvalid;
    key.depth = state.depth.texture && state.depth.texture->depth ? AttachmentFormat(state.depth) : MTLPixelFormatInvalid;
    key.blend[0] = rs.blendEnable;
    key.blend[1] = static_cast<uint8_t>(rs.srcColor);
    key.blend[2] = static_cast<uint8_t>(rs.destColor);
    key.blend[3] = static_cast<uint8_t>(rs.colorOp);
    key.blend[4] = static_cast<uint8_t>(rs.separateAlphaBlend ? rs.srcAlpha : rs.srcColor);
    key.blend[5] = static_cast<uint8_t>(rs.separateAlphaBlend ? rs.destAlpha : rs.destColor);
    key.blend[6] = static_cast<uint8_t>(rs.separateAlphaBlend ? rs.alphaOp : rs.colorOp);
    key.blend[7] = rs.colorWriteMask;
    key.blend[8] = 0;
    key.vsDepthMask = vsDepthMask;
    key.psDepthMask = psDepthMask;
    key.varyings = varyings;
    const std::string keyBytes(reinterpret_cast<const char *>(&key), sizeof(key));
    auto found = g->pipelines.find(keyBytes);
    if (found != g->pipelines.end())
        return found->second;
    if (g->failedPipelines.count(keyBytes))
        return nil;

    MTLRenderPipelineDescriptor *desc = [MTLRenderPipelineDescriptor new];
    desc.vertexFunction = vertexFunction;
    desc.fragmentFunction = fragmentFunction;

    MTLVertexDescriptor *vertex = [MTLVertexDescriptor vertexDescriptor];
    bool needsDefaults = false;
    for (const ShaderSemantic &input : state.vertexShader->base.inputs)
    {
        const int slot = AttributeSlot(input.usage, input.index);
        if (slot < 0)
            continue;
        const VertexElement *element = nullptr;
        if (state.layout)
        {
            for (const VertexElement &candidate : state.layout->elements)
            {
                const bool positionMatch = input.usage == kUsagePosition && candidate.usage == kUsagePositionT;
                if ((candidate.usage == input.usage || positionMatch) && candidate.usageIndex == input.index)
                {
                    element = &candidate;
                    break;
                }
            }
        }
        MTLVertexAttributeDescriptor *attribute = vertex.attributes[static_cast<NSUInteger>(slot)];
        if (element && element->stream < kMaxStreams && state.streams[element->stream].stride
            && VertexFormatFor(element->type) != MTLVertexFormatInvalid)
        {
            attribute.format = VertexFormatFor(element->type);
            attribute.offset = element->offset;
            attribute.bufferIndex = element->stream;
            vertex.layouts[element->stream].stride = state.streams[element->stream].stride;
            vertex.layouts[element->stream].stepFunction = MTLVertexStepFunctionPerVertex;
        }
        else
        {
            attribute.format = MTLVertexFormatFloat4;
            attribute.offset = 0;
            attribute.bufferIndex = kDefaultAttributeBuffer;
            needsDefaults = true;
        }
    }
    if (needsDefaults)
    {
        vertex.layouts[kDefaultAttributeBuffer].stride = 16;
        vertex.layouts[kDefaultAttributeBuffer].stepFunction = MTLVertexStepFunctionConstant;
        vertex.layouts[kDefaultAttributeBuffer].stepRate = 0;
    }
    desc.vertexDescriptor = vertex;

    MTLColorWriteMask writeMask = MTLColorWriteMaskNone;
    if (rs.colorWriteMask & 1) writeMask |= MTLColorWriteMaskRed;
    if (rs.colorWriteMask & 2) writeMask |= MTLColorWriteMaskGreen;
    if (rs.colorWriteMask & 4) writeMask |= MTLColorWriteMaskBlue;
    if (rs.colorWriteMask & 8) writeMask |= MTLColorWriteMaskAlpha;
    for (uint32_t i = 0; i < kMaxColorTargets; ++i)
    {
        if (key.color[i] == MTLPixelFormatInvalid)
            continue;
        MTLRenderPipelineColorAttachmentDescriptor *attachment = desc.colorAttachments[i];
        attachment.pixelFormat = static_cast<MTLPixelFormat>(key.color[i]);
        attachment.writeMask = writeMask;
        attachment.blendingEnabled = rs.blendEnable;
        attachment.sourceRGBBlendFactor = BlendFactorFor(rs.srcColor);
        attachment.destinationRGBBlendFactor = BlendFactorFor(rs.destColor);
        attachment.rgbBlendOperation = BlendOperationFor(rs.colorOp);
        attachment.sourceAlphaBlendFactor = BlendFactorFor(rs.separateAlphaBlend ? rs.srcAlpha : rs.srcColor);
        attachment.destinationAlphaBlendFactor = BlendFactorFor(rs.separateAlphaBlend ? rs.destAlpha : rs.destColor);
        attachment.alphaBlendOperation = BlendOperationFor(rs.separateAlphaBlend ? rs.alphaOp : rs.colorOp);
    }
    if (key.depth != MTLPixelFormatInvalid)
    {
        desc.depthAttachmentPixelFormat = static_cast<MTLPixelFormat>(key.depth);
        if (state.depth.texture->stencil)
            desc.stencilAttachmentPixelFormat = static_cast<MTLPixelFormat>(key.depth);
    }

    NSError *error = nil;
    id<MTLRenderPipelineState> pipeline = [g->device newRenderPipelineStateWithDescriptor:desc error:&error];
    if (!pipeline)
    {
        LogOnce(std::string("pipeline creation failed: ") + (error ? error.localizedDescription.UTF8String : "unknown"));
        g->failedPipelines.insert(keyBytes);
        return nil;
    }
    g->pipelines[keyBytes] = pipeline;
    return pipeline;
}

id<MTLDepthStencilState> DepthStencilFor(const RenderState &rs, bool hasDepth, bool hasStencil)
{
    auto packFace = [](const StencilFace &face) {
        return static_cast<uint64_t>(face.fail) | (static_cast<uint64_t>(face.depthFail) << 4) | (static_cast<uint64_t>(face.pass) << 8)
             | (static_cast<uint64_t>(face.func) << 12);
    };
    const bool depthOn = hasDepth && rs.depthEnable;
    const bool stencilOn = hasStencil && rs.stencilEnable;
    uint64_t key = (depthOn ? 1u : 0u) | ((depthOn && rs.depthWrite) ? 2u : 0u) | (static_cast<uint64_t>(rs.depthFunc) << 2);
    if (stencilOn)
    {
        key |= 1ull << 6;
        key |= packFace(rs.stencilFront) << 8;
        key |= packFace(rs.twoSidedStencil ? rs.stencilBack : rs.stencilFront) << 24;
        key |= static_cast<uint64_t>(rs.stencilReadMask) << 40;
        key |= static_cast<uint64_t>(rs.stencilWriteMask) << 48;
    }
    auto found = g->depthStates.find(key);
    if (found != g->depthStates.end())
        return found->second;

    MTLDepthStencilDescriptor *desc = [MTLDepthStencilDescriptor new];
    desc.depthCompareFunction = depthOn ? CompareFunction(rs.depthFunc) : MTLCompareFunctionAlways;
    desc.depthWriteEnabled = depthOn && rs.depthWrite;
    if (stencilOn)
    {
        auto makeFace = [&](const StencilFace &face) {
            MTLStencilDescriptor *stencil = [MTLStencilDescriptor new];
            stencil.stencilFailureOperation = StencilOperationFor(face.fail);
            stencil.depthFailureOperation = StencilOperationFor(face.depthFail);
            stencil.depthStencilPassOperation = StencilOperationFor(face.pass);
            stencil.stencilCompareFunction = CompareFunction(face.func);
            stencil.readMask = rs.stencilReadMask;
            stencil.writeMask = rs.stencilWriteMask;
            return stencil;
        };
        desc.frontFaceStencil = makeFace(rs.stencilFront);
        desc.backFaceStencil = makeFace(rs.twoSidedStencil ? rs.stencilBack : rs.stencilFront);
    }
    id<MTLDepthStencilState> state = [g->device newDepthStencilStateWithDescriptor:desc];
    g->depthStates[key] = state;
    return state;
}

id<MTLSamplerState> SamplerFor(const SamplerState &s, bool compare)
{
    const uint64_t key = static_cast<uint64_t>(s.addressU) | (static_cast<uint64_t>(s.addressV) << 4) | (static_cast<uint64_t>(s.addressW) << 8)
                       | (static_cast<uint64_t>(s.minFilter) << 12) | (static_cast<uint64_t>(s.magFilter) << 16)
                       | (static_cast<uint64_t>(s.mipFilter) << 20) | (static_cast<uint64_t>(s.maxAnisotropy) << 24)
                       | (static_cast<uint64_t>(compare) << 32) | ((s.addressU == Address::Border || s.addressV == Address::Border) ? static_cast<uint64_t>(s.borderColor >> 24) << 40 : 0);
    auto found = g->samplers.find(key);
    if (found != g->samplers.end())
        return found->second;
    MTLSamplerDescriptor *desc = [MTLSamplerDescriptor new];
    auto filter = [](Filter f) { return f == Filter::Point || f == Filter::None ? MTLSamplerMinMagFilterNearest : MTLSamplerMinMagFilterLinear; };
    desc.minFilter = filter(s.minFilter);
    desc.magFilter = filter(s.magFilter);
    desc.mipFilter = s.mipFilter == Filter::None ? MTLSamplerMipFilterNotMipmapped
                   : s.mipFilter == Filter::Point ? MTLSamplerMipFilterNearest : MTLSamplerMipFilterLinear;
    if (s.minFilter == Filter::Anisotropic || s.magFilter == Filter::Anisotropic)
        desc.maxAnisotropy = std::clamp<NSUInteger>(s.maxAnisotropy, 1, 16);
    desc.sAddressMode = AddressModeFor(s.addressU);
    desc.tAddressMode = AddressModeFor(s.addressV);
    desc.rAddressMode = AddressModeFor(s.addressW);
    desc.borderColor = (s.borderColor >> 24) == 0 ? MTLSamplerBorderColorTransparentBlack
                     : (s.borderColor & 0xFFFFFF) == 0xFFFFFF ? MTLSamplerBorderColorOpaqueWhite : MTLSamplerBorderColorOpaqueBlack;
    if (compare)
        desc.compareFunction = MTLCompareFunctionLessEqual;
    id<MTLSamplerState> sampler = [g->device newSamplerStateWithDescriptor:desc];
    g->samplers[key] = sampler;
    return sampler;
}

id<MTLTexture> DummyFor(uint8_t type, bool depth)
{
    if (depth)
        return g->dummyDepth;
    if (type == kTextureCube)
        return g->dummyCube;
    if (type == kTextureVolume)
        return g->dummyVolume;
    return g->dummy2D;
}

bool TextureMatches(Texture *texture, uint8_t type)
{
    if (!texture || !texture->texture)
        return false;
    switch (type)
    {
    case kTextureCube: return texture->desc.kind == TextureKind::Cube;
    case kTextureVolume: return texture->desc.kind == TextureKind::Volume;
    default: return texture->desc.kind == TextureKind::Texture2D;
    }
}

uint32_t DepthSamplerMask(const Shader *shader, const DrawState &state, uint32_t firstSlot)
{
    uint32_t mask = 0;
    if (!shader)
        return 0;
    for (const ShaderSampler &sampler : shader->base.samplers)
    {
        const uint32_t slot = firstSlot + sampler.reg;
        if (slot < kMaxSamplers && state.textures[slot] && state.textures[slot]->depth && sampler.type != kTextureCube && sampler.type != kTextureVolume)
            mask |= 1u << sampler.reg;
    }
    return mask;
}

void BindTextures(const Shader *shader, const DrawState &state, uint32_t firstSlot, uint32_t depthMask, bool fragment)
{
    for (const ShaderSampler &sampler : shader->base.samplers)
    {
        const uint32_t slot = firstSlot + sampler.reg;
        if (slot >= kMaxSamplers)
            continue;
        Texture *texture = state.textures[slot];
        const bool depth = (depthMask >> sampler.reg) & 1;
        id<MTLTexture> bound = depth ? (texture ? texture->texture : nil)
                             : (TextureMatches(texture, sampler.type) ? texture->sampleView : nil);
        if (!bound)
            bound = DummyFor(sampler.type, depth);
        id<MTLSamplerState> samplerState = SamplerFor(state.samplers[slot], depth);
        if (fragment)
        {
            [g->encoder setFragmentTexture:bound atIndex:sampler.reg];
            [g->encoder setFragmentSamplerState:samplerState atIndex:sampler.reg];
        }
        else
        {
            [g->encoder setVertexTexture:bound atIndex:sampler.reg];
            [g->encoder setVertexSamplerState:samplerState atIndex:sampler.reg];
        }
    }
}

bool PrepareDraw(const DrawState &state)
{
    Shader *vs = state.vertexShader;
    Shader *ps = state.pixelShader;
    if (!vs || !ps || !vs->valid || !ps->valid)
        return false;

    const uint32_t vsDepthMask = DepthSamplerMask(vs, state, kPixelSamplers);
    const uint32_t psDepthMask = DepthSamplerMask(ps, state, 0);
    // Unnormalized integer vertex elements (e.g. packed texcoords/normals as UBYTE4) need integer shader attributes.
    uint32_t unsignedIntInputs = 0;
    uint32_t signedIntInputs = 0;
    for (const ShaderSemantic &input : vs->base.inputs)
    {
        const int slot = AttributeSlot(input.usage, input.index);
        if (slot < 0 || !state.layout)
            continue;
        for (const VertexElement &candidate : state.layout->elements)
        {
            const bool positionMatch = input.usage == kUsagePosition && candidate.usage == kUsagePositionT;
            if ((candidate.usage == input.usage || positionMatch) && candidate.usageIndex == input.index)
            {
                if (candidate.stream < kMaxStreams && state.streams[candidate.stream].stride)
                {
                    if (candidate.type == VertexType::UByte4)
                        unsignedIntInputs |= 1u << slot;
                    else if (candidate.type == VertexType::Short2 || candidate.type == VertexType::Short4)
                        signedIntInputs |= 1u << slot;
                }
                break;
            }
        }
    }
    id<MTLFunction> vertexFunction = ShaderFunction(vs, vsDepthMask, 0xFFFFFFFF, unsignedIntInputs, signedIntInputs);
    id<MTLFunction> fragmentFunction = ShaderFunction(ps, psDepthMask, vs->varyingMask);
    if (!vertexFunction || !fragmentFunction)
        return false;

    if (!SamePass(state) && !BeginPass(state, false, false, false, MTLClearColorMake(0, 0, 0, 1), 1.0, 0))
        return false;

    id<MTLRenderPipelineState> pipeline = PipelineFor(state, vertexFunction, fragmentFunction, vsDepthMask, psDepthMask, vs->varyingMask);
    if (!pipeline)
        return false;

    const RenderState &rs = state.render;
    id<MTLRenderCommandEncoder> encoder = g->encoder;
    [encoder setRenderPipelineState:pipeline];
    const bool hasDepth = state.depth.texture && state.depth.texture->depth;
    [encoder setDepthStencilState:DepthStencilFor(rs, hasDepth, hasDepth && state.depth.texture->stencil)];
    [encoder setStencilReferenceValue:rs.stencilRef];
    // Direct3D 9 treats clockwise triangles as front-facing.
    [encoder setFrontFacingWinding:MTLWindingClockwise];
    [encoder setCullMode:rs.cull == Cull::CounterClockwise ? MTLCullModeBack : rs.cull == Cull::Clockwise ? MTLCullModeFront : MTLCullModeNone];
    [encoder setTriangleFillMode:rs.wireframe ? MTLTriangleFillModeLines : MTLTriangleFillModeFill];
    [encoder setDepthBias:rs.depthBias slopeScale:rs.slopeScaledDepthBias clamp:0.0f];

    const double targetWidth = g->passWidth;
    const double targetHeight = g->passHeight;
    MTLViewport viewport;
    viewport.originX = std::clamp<double>(state.viewport.x, 0.0, targetWidth);
    viewport.originY = std::clamp<double>(state.viewport.y, 0.0, targetHeight);
    viewport.width = std::clamp<double>(state.viewport.width, 0.0, targetWidth - viewport.originX);
    viewport.height = std::clamp<double>(state.viewport.height, 0.0, targetHeight - viewport.originY);
    viewport.znear = state.viewport.minZ;
    viewport.zfar = state.viewport.maxZ;
    if (viewport.width <= 0.0 || viewport.height <= 0.0)
        return false;
    [encoder setViewport:viewport];

    MTLScissorRect scissor = {0, 0, g->passWidth, g->passHeight};
    if (rs.scissorTest)
    {
        const int32_t left = std::clamp<int32_t>(state.scissor.left, 0, static_cast<int32_t>(g->passWidth));
        const int32_t top = std::clamp<int32_t>(state.scissor.top, 0, static_cast<int32_t>(g->passHeight));
        const int32_t right = std::clamp<int32_t>(state.scissor.right, left, static_cast<int32_t>(g->passWidth));
        const int32_t bottom = std::clamp<int32_t>(state.scissor.bottom, top, static_cast<int32_t>(g->passHeight));
        if (right <= left || bottom <= top)
            return false;
        scissor = {static_cast<NSUInteger>(left), static_cast<NSUInteger>(top), static_cast<NSUInteger>(right - left), static_cast<NSUInteger>(bottom - top)};
    }
    [encoder setScissorRect:scissor];

    for (uint32_t stream = 0; stream < kMaxStreams; ++stream)
        if (state.streams[stream].buffer && state.streams[stream].buffer->buffer)
        {
            state.streams[stream].buffer->referencedByDraw = true;
            [encoder setVertexBuffer:state.streams[stream].buffer->buffer offset:state.streams[stream].offset atIndex:stream];
        }
    [encoder setVertexBuffer:g->defaultAttributes offset:0 atIndex:kDefaultAttributeBuffer];

    const uint32_t vsConstants = std::max<uint32_t>(vs->base.constantCount, 1);
    Allocation vc = g->arena.Allocate(g->device, vsConstants * 16);
    if (state.vertexConstants)
        memcpy(vc.pointer, state.vertexConstants, std::min<uint32_t>(vsConstants, 256) * 16);
    [encoder setVertexBuffer:vc.buffer offset:vc.offset atIndex:kVertexConstantBuffer];

    const uint32_t psConstants = std::max<uint32_t>(ps->base.constantCount, 1);
    Allocation pc = g->arena.Allocate(g->device, psConstants * 16);
    if (state.pixelConstants)
        memcpy(pc.pointer, state.pixelConstants, std::min<uint32_t>(psConstants, 224) * 16);
    [encoder setFragmentBuffer:pc.buffer offset:pc.offset atIndex:kFragmentConstantBuffer];

    VertexParams vertexParams;
    vertexParams.halfPixel[0] = viewport.width > 0 ? static_cast<float>(-1.0 / viewport.width) : 0.0f;
    vertexParams.halfPixel[1] = viewport.height > 0 ? static_cast<float>(1.0 / viewport.height) : 0.0f;
    [encoder setVertexBytes:&vertexParams length:sizeof(vertexParams) atIndex:kVertexParamsBuffer];

    FragmentParams fragmentParams;
    fragmentParams.alphaFunc = rs.alphaTest ? static_cast<int32_t>(rs.alphaFunc) : 0;
    fragmentParams.alphaRef = rs.alphaRef / 255.0f;
    [encoder setFragmentBytes:&fragmentParams length:sizeof(fragmentParams) atIndex:kFragmentParamsBuffer];

    BindTextures(vs, state, kPixelSamplers, vsDepthMask, false);
    BindTextures(ps, state, 0, psDepthMask, true);
    return true;
}

std::string EnvFlag(const char *name)
{
    const char *value = getenv(name);
    return value ? value : "";
}

} // namespace

// ---------------------------------------------------------------------------
// Public interface

bool Initialize(void *view)
{
    if (g)
        return true;
    if (!view)
        return false;
    id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    if (!device)
        return false;

    __block CAMetalLayer *layer = nil;
    auto configure = ^{
#if TARGET_OS_IPHONE
      UIView *uiView = (__bridge UIView *)view;
      if ([uiView.layer isKindOfClass:CAMetalLayer.class])
          layer = (CAMetalLayer *)uiView.layer;
#endif
      if (layer)
      {
          layer.device = device;
          layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
          layer.framebufferOnly = YES;
      }
    };
    if (NSThread.isMainThread)
        configure();
    else
        dispatch_sync(dispatch_get_main_queue(), configure);
    if (!layer)
        return false;

    auto *context = new Context();
    context->device = device;
    context->queue = [device newCommandQueue];
    context->layer = layer;
    context->inflight = dispatch_semaphore_create(kInflightFrames);
    if (@available(iOS 16.4, macOS 11.0, *))
        context->bcSupported = device.supportsBCTextureCompression && EnvFlag("KISAK_DECODE_DXT") != "1";

    static const char *blitSource = R"(
#include <metal_stdlib>
using namespace metal;
struct BlitVertex { float4 position [[position]]; float2 uv; };
vertex BlitVertex blit_vs(uint id [[vertex_id]])
{
    const float2 uv = float2((id << 1) & 2, id & 2);
    BlitVertex out;
    out.position = float4(uv * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
    out.uv = uv;
    return out;
}
fragment float4 blit_fs(BlitVertex in [[stage_in]], texture2d<float> source [[texture(0)]], sampler smp [[sampler(0)]])
{
    return source.sample(smp, in.uv);
}
)";
    NSError *error = nil;
    context->blitLibrary = [device newLibraryWithSource:[NSString stringWithUTF8String:blitSource] options:nil error:&error];
    if (!context->blitLibrary)
    {
        fprintf(stderr, "Metal: blit shader failed: %s\n", error.localizedDescription.UTF8String);
        delete context;
        return false;
    }
    MTLSamplerDescriptor *blitSampler = [MTLSamplerDescriptor new];
    blitSampler.minFilter = MTLSamplerMinMagFilterLinear;
    blitSampler.magFilter = MTLSamplerMinMagFilterLinear;
    context->blitSampler = [device newSamplerStateWithDescriptor:blitSampler];

    const float defaults[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    context->defaultAttributes = [device newBufferWithBytes:defaults length:sizeof(defaults) options:MTLResourceStorageModeShared];
    context->dummy2D = MakeDummy(device, MTLTextureType2D);
    context->dummyCube = MakeDummy(device, MTLTextureTypeCube);
    context->dummyVolume = MakeDummy(device, MTLTextureType3D);
    MTLTextureDescriptor *depthDesc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatDepth32Float width:1 height:1 mipmapped:NO];
    depthDesc.usage = MTLTextureUsageShaderRead;
    depthDesc.storageMode = MTLStorageModePrivate;
    context->dummyDepth = [device newTextureWithDescriptor:depthDesc];

    g = context;
    fprintf(stderr, "Metal: backend ready on %s (BC textures %s)\n", device.name.UTF8String, context->bcSupported ? "native" : "decoded");
    return true;
}

bool Available()
{
    return g != nullptr;
}

Texture *CreateTexture(const TextureDesc &desc)
{
    if (!g)
        return nullptr;
    @autoreleasepool
    {
        auto *texture = new Texture();
        texture->desc = desc;
        texture->pixelFormat = StorageFormat(desc.format, g->bcSupported);
        if (texture->pixelFormat == MTLPixelFormatInvalid)
        {
            LogOnce("unsupported texture format " + std::to_string(static_cast<int>(desc.format)));
            return texture;
        }
        texture->depth = desc.format == Format::D24S8 || desc.format == Format::D16 || desc.format == Format::D32F;
        texture->stencil = desc.format == Format::D24S8;
        texture->decodeDxt = IsDxt(desc.format) && !g->bcSupported;
        texture->convert16 = Is16Bit(desc.format);

        MTLTextureDescriptor *d = [MTLTextureDescriptor new];
        d.textureType = desc.kind == TextureKind::Cube ? MTLTextureTypeCube : desc.kind == TextureKind::Volume ? MTLTextureType3D : MTLTextureType2D;
        d.pixelFormat = texture->pixelFormat;
        d.width = std::max<uint32_t>(desc.width, 1);
        d.height = desc.kind == TextureKind::Cube ? d.width : std::max<uint32_t>(desc.height, 1);
        d.depth = desc.kind == TextureKind::Volume ? std::max<uint32_t>(desc.depth, 1) : 1;
        d.mipmapLevelCount = std::max<uint32_t>(desc.levels, 1);
        d.usage = MTLTextureUsageShaderRead | (desc.renderTarget || texture->depth ? MTLTextureUsageRenderTarget : 0);
        d.storageMode = texture->depth ? MTLStorageModePrivate : MTLStorageModeShared;
        texture->texture = [g->device newTextureWithDescriptor:d];
        if (!texture->texture)
        {
            LogOnce("texture creation failed");
            return texture;
        }
        MTLTextureSwizzleChannels swizzle;
        if (SampleSwizzle(desc.format, swizzle))
            texture->sampleView = [texture->texture newTextureViewWithPixelFormat:texture->pixelFormat textureType:d.textureType
                                                                           levels:NSMakeRange(0, d.mipmapLevelCount)
                                                                           slices:NSMakeRange(0, desc.kind == TextureKind::Cube ? 6 : 1)
                                                                          swizzle:swizzle];
        else
            texture->sampleView = texture->texture;
        return texture;
    }
}

void DestroyTexture(Texture *texture)
{
    delete texture;
}

void UploadTexture(Texture *texture, uint32_t face, uint32_t level, const uint8_t *bits, size_t size)
{
    if (!g || !texture || !texture->texture || texture->depth || !bits)
        return;
    if (level >= texture->texture.mipmapLevelCount)
        return;
    @autoreleasepool
    {
        const TextureDesc &desc = texture->desc;
        const uint32_t width = LevelSize(desc.width, level);
        const uint32_t height = desc.kind == TextureKind::Cube ? width : LevelSize(desc.height, level);
        const uint32_t depth = desc.kind == TextureKind::Volume ? LevelSize(desc.depth, level) : 1;
        const NSUInteger slice = desc.kind == TextureKind::Cube ? face : 0;

        std::vector<uint8_t> converted;
        const uint8_t *data = bits;
        NSUInteger bytesPerRow = 0;
        if (texture->decodeDxt)
        {
            if (depth != 1)
            {
                LogOnce("DXT volume textures are not supported");
                return;
            }
            converted = DecodeDxt(desc.format, bits, size, width, height);
            data = converted.data();
            bytesPerRow = width * 4;
        }
        else if (IsDxt(desc.format))
        {
            bytesPerRow = ((width + 3) / 4) * (desc.format == Format::DXT1 ? 8 : 16);
        }
        else if (texture->convert16)
        {
            converted = Convert16Bit(desc.format, bits, size, width, height, depth);
            data = converted.data();
            bytesPerRow = width * 4;
        }
        else
        {
            const size_t bpp = BytesPerPixel(texture->pixelFormat);
            if (size < static_cast<size_t>(width) * height * depth * bpp)
                return;
            bytesPerRow = width * bpp;
        }
        const NSUInteger rows = IsDxt(desc.format) && !texture->decodeDxt ? (height + 3) / 4 : height;
        [texture->texture replaceRegion:MTLRegionMake3D(0, 0, 0, width, height, depth) mipmapLevel:level slice:slice withBytes:data
                            bytesPerRow:bytesPerRow bytesPerImage:bytesPerRow * rows];
    }
}

Buffer *CreateBuffer(size_t length)
{
    if (!g || !length)
        return nullptr;
    auto *buffer = new Buffer();
    buffer->buffer = [g->device newBufferWithLength:length options:MTLResourceStorageModeShared];
    buffer->length = length;
    return buffer;
}

void DestroyBuffer(Buffer *buffer)
{
    delete buffer;
}

void UploadBuffer(Buffer *buffer, size_t offset, const uint8_t *data, size_t size, BufferUpdate update)
{
    if (!g || !buffer || !buffer->buffer || !data || offset >= buffer->length)
        return;
    std::lock_guard<std::recursive_mutex> guard(g->lock);
    size = std::min(size, buffer->length - offset);
    // NOOVERWRITE promises that this range is not used by pending draws. The
    // engine appends hundreds of small batches to its dynamic buffers; copying
    // the whole allocation for each append can consume gigabytes per frame.
    // DISCARD starts a new allocation while encoded draws retain the old one.
    if (buffer->referencedByDraw && update != BufferUpdate::NoOverwrite)
    {
        id<MTLBuffer> replacement = [g->device newBufferWithLength:buffer->length options:MTLResourceStorageModeShared];
        if (!replacement)
        {
            LogOnce("could not allocate buffer storage for a pending draw");
            return;
        }
        if (update != BufferUpdate::Discard && (offset != 0 || size != buffer->length))
            memcpy(replacement.contents, buffer->buffer.contents, buffer->length);
        buffer->buffer = replacement;
        buffer->referencedByDraw = false;
    }
    memcpy(static_cast<uint8_t *>(buffer->buffer.contents) + offset, data, size);
}

Shader *CreateShader(const uint32_t *tokens, size_t count)
{
    if (!g || !tokens || !count)
        return nullptr;
    auto *shader = new Shader();
    shader->tokens.assign(tokens, tokens + count);
    shader->valid = TranslateShader(tokens, count, {}, shader->base);
    if (!shader->valid)
    {
        LogOnce("shader rejected: " + shader->base.error);
        return shader;
    }
    if (!shader->base.pixel)
    {
        for (const ShaderSemantic &output : shader->base.outputs)
        {
            const int bit = VaryingBit(output.usage, output.index);
            if (bit >= 0)
                shader->varyingMask |= 1u << bit;
        }
    }
    return shader;
}

void DestroyShader(Shader *shader)
{
    if (!g || !shader)
    {
        delete shader;
        return;
    }
    std::lock_guard<std::recursive_mutex> guard(g->lock);
    delete shader;
}

VertexLayout *CreateVertexLayout(const VertexElement *elements, size_t count)
{
    if (!g)
        return nullptr;
    auto *layout = new VertexLayout();
    layout->elements.assign(elements, elements + count);
    return layout;
}

void DestroyVertexLayout(VertexLayout *layout)
{
    delete layout;
}

void Draw(const DrawState &state, Primitive primitive, uint32_t primitiveCount, bool indexed, int32_t baseVertex, uint32_t start)
{
    if (!g || !primitiveCount)
        return;
    std::lock_guard<std::recursive_mutex> guard(g->lock);
    @autoreleasepool
    {
        if (!PrepareDraw(state))
            return;
        const uint32_t count = VertexCount(primitive, primitiveCount);
        if (indexed)
        {
            if (!state.indices || !state.indices->buffer)
                return;
            const size_t indexSize = state.indices32 ? 4 : 2;
            if (start > state.indices->length / indexSize || count > state.indices->length / indexSize - start)
            {
                LogOnce("indexed draw exceeds its index buffer");
                return;
            }
            state.indices->referencedByDraw = true;
            if (primitive == Primitive::TriangleFan)
            {
                LogOnce("indexed triangle fans are not supported");
                return;
            }
            [g->encoder drawIndexedPrimitives:PrimitiveTypeFor(primitive) indexCount:count
                                    indexType:state.indices32 ? MTLIndexTypeUInt32 : MTLIndexTypeUInt16
                                  indexBuffer:state.indices->buffer indexBufferOffset:start * indexSize instanceCount:1
                                   baseVertex:baseVertex baseInstance:0];
        }
        else if (primitive == Primitive::TriangleFan)
        {
            Allocation indices = g->arena.Allocate(g->device, primitiveCount * 3 * 4);
            auto *out = static_cast<uint32_t *>(indices.pointer);
            for (uint32_t i = 0; i < primitiveCount; ++i)
            {
                out[i * 3] = start;
                out[i * 3 + 1] = start + i + 1;
                out[i * 3 + 2] = start + i + 2;
            }
            [g->encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:primitiveCount * 3 indexType:MTLIndexTypeUInt32
                                  indexBuffer:indices.buffer indexBufferOffset:indices.offset];
        }
        else
        {
            [g->encoder drawPrimitives:PrimitiveTypeFor(primitive) vertexStart:start vertexCount:count];
        }
    }
}

void DrawUserPrimitives(const DrawState &state, Primitive primitive, uint32_t primitiveCount, const void *vertices, uint32_t stride)
{
    if (!g || !primitiveCount || !vertices || !stride)
        return;
    std::lock_guard<std::recursive_mutex> guard(g->lock);
    @autoreleasepool
    {
        const uint32_t count = VertexCount(primitive, primitiveCount);
        Allocation data = g->arena.Allocate(g->device, static_cast<size_t>(count) * stride);
        memcpy(data.pointer, vertices, static_cast<size_t>(count) * stride);
        Buffer temporary;
        temporary.buffer = data.buffer;
        temporary.length = data.buffer.length;
        DrawState userState = state;
        for (StreamBinding &stream : userState.streams)
            stream = StreamBinding{};
        userState.streams[0] = StreamBinding{&temporary, static_cast<uint32_t>(data.offset), stride};
        Draw(userState, primitive, primitiveCount, false, 0, 0);
    }
}

void Clear(const DrawState &state, bool color, bool depth, bool stencil, uint32_t argb, float z, uint8_t stencilValue)
{
    if (!g || (!color && !depth && !stencil))
        return;
    std::lock_guard<std::recursive_mutex> guard(g->lock);
    @autoreleasepool
    {
        const MTLClearColor clearColor = MTLClearColorMake(((argb >> 16) & 0xFF) / 255.0, ((argb >> 8) & 0xFF) / 255.0, (argb & 0xFF) / 255.0,
                                                           ((argb >> 24) & 0xFF) / 255.0);
        BeginPass(state, color, depth, stencil, clearColor, z, stencilValue);
    }
}

void Blit(const TargetBinding &source, const TargetBinding &destination)
{
    if (!g || !source.texture || !destination.texture || !source.texture->texture || !destination.texture->texture)
        return;
    if (source.texture->depth || destination.texture->depth || source.texture->desc.kind != TextureKind::Texture2D)
        return;
    std::lock_guard<std::recursive_mutex> guard(g->lock);
    @autoreleasepool
    {
        EndPass();
        const NSUInteger format = destination.texture->pixelFormat;
        id<MTLRenderPipelineState> pipeline = g->blitPipelines[format];
        if (!pipeline)
        {
            MTLRenderPipelineDescriptor *desc = [MTLRenderPipelineDescriptor new];
            desc.vertexFunction = [g->blitLibrary newFunctionWithName:@"blit_vs"];
            desc.fragmentFunction = [g->blitLibrary newFunctionWithName:@"blit_fs"];
            desc.colorAttachments[0].pixelFormat = static_cast<MTLPixelFormat>(format);
            pipeline = [g->device newRenderPipelineStateWithDescriptor:desc error:nil];
            if (!pipeline)
                return;
            g->blitPipelines[format] = pipeline;
        }
        MTLRenderPassDescriptor *pass = [MTLRenderPassDescriptor renderPassDescriptor];
        pass.colorAttachments[0].texture = destination.texture->texture;
        pass.colorAttachments[0].level = destination.level;
        pass.colorAttachments[0].slice = destination.texture->desc.kind == TextureKind::Cube ? destination.face : 0;
        pass.colorAttachments[0].loadAction = MTLLoadActionDontCare;
        pass.colorAttachments[0].storeAction = MTLStoreActionStore;
        id<MTLRenderCommandEncoder> encoder = [CommandBuffer() renderCommandEncoderWithDescriptor:pass];
        [encoder setRenderPipelineState:pipeline];
        [encoder setFragmentTexture:source.texture->sampleView atIndex:0];
        [encoder setFragmentSamplerState:g->blitSampler atIndex:0];
        [encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
        [encoder endEncoding];
    }
}

void Present(const TargetBinding &backBuffer)
{
    if (!g)
        return;
    std::lock_guard<std::recursive_mutex> guard(g->lock);
    @autoreleasepool
    {
        EndPass();
        id<MTLCommandBuffer> commandBuffer = CommandBuffer();
        id<CAMetalDrawable> drawable = [g->layer nextDrawable];
        if (drawable && backBuffer.texture && backBuffer.texture->texture)
        {
            const NSUInteger format = g->layer.pixelFormat;
            id<MTLRenderPipelineState> pipeline = g->blitPipelines[format];
            if (!pipeline)
            {
                MTLRenderPipelineDescriptor *desc = [MTLRenderPipelineDescriptor new];
                desc.vertexFunction = [g->blitLibrary newFunctionWithName:@"blit_vs"];
                desc.fragmentFunction = [g->blitLibrary newFunctionWithName:@"blit_fs"];
                desc.colorAttachments[0].pixelFormat = static_cast<MTLPixelFormat>(format);
                pipeline = [g->device newRenderPipelineStateWithDescriptor:desc error:nil];
                g->blitPipelines[format] = pipeline;
            }
            MTLRenderPassDescriptor *pass = [MTLRenderPassDescriptor renderPassDescriptor];
            pass.colorAttachments[0].texture = drawable.texture;
            pass.colorAttachments[0].loadAction = MTLLoadActionDontCare;
            pass.colorAttachments[0].storeAction = MTLStoreActionStore;
            id<MTLRenderCommandEncoder> encoder = [commandBuffer renderCommandEncoderWithDescriptor:pass];
            if (pipeline)
            {
                [encoder setRenderPipelineState:pipeline];
                [encoder setFragmentTexture:backBuffer.texture->texture atIndex:0];
                [encoder setFragmentSamplerState:g->blitSampler atIndex:0];
                [encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
            }
            [encoder endEncoding];
            [commandBuffer presentDrawable:drawable];
        }
        [commandBuffer commit];
        g->commandBuffer = nil;
        g->arena.NextFrame();
    }
}

} // namespace kisak::metal
