#pragma once

// Direct3D 9 shader model 3 bytecode to Metal Shading Language.
//
// COD4 ships its HLSL precompiled as ps_3_0/vs_3_0 bytecode inside the
// fastfiles. This translator turns one such program into a Metal vertex or
// fragment function and reports what the Metal backend needs to bind it:
// input/output semantics, samplers and the float constant range.
//
// The generated functions follow Direct3D 9 conventions the backend relies on:
// float constants live in one buffer indexed by register, `def` constants are
// baked into the source, alpha test runs in the fragment function (Metal has no
// fixed-function alpha test), and vertex positions get the Direct3D 9 half-pixel
// offset. It depends only on the standard library so it can be validated offline.

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace kisak::metal {

// Direct3D 9 declaration usages (D3DDECLUSAGE_*).
enum : uint8_t
{
    kUsagePosition = 0,
    kUsageBlendWeight = 1,
    kUsageBlendIndices = 2,
    kUsageNormal = 3,
    kUsagePointSize = 4,
    kUsageTexCoord = 5,
    kUsageTangent = 6,
    kUsageBinormal = 7,
    kUsageTessFactor = 8,
    kUsagePositionT = 9,
    kUsageColor = 10,
    kUsageFog = 11,
    kUsageDepth = 12,
    kUsageSample = 13,
};

// Sampler texture types from `dcl_2d`, `dcl_cube` and `dcl_volume`.
enum : uint8_t
{
    kTextureUnknown = 0,
    kTexture2D = 2,
    kTextureCube = 3,
    kTextureVolume = 4,
};

struct ShaderSemantic
{
    uint8_t usage = 0;
    uint8_t index = 0;
    uint16_t reg = 0;
};

struct ShaderSampler
{
    uint16_t reg = 0;
    uint8_t type = kTextureUnknown;
};

struct TranslateOptions
{
    // Samplers (by register) bound to depth textures. Direct3D 9 drivers return
    // a hardware shadow comparison when a depth texture is sampled; these
    // samplers become `depth2d` with `sample_compare` against coord.z.
    uint32_t depthSamplerMask = 0;
    // Varyings the paired vertex shader writes (pixel shaders only). Bits 0-7
    // texcoord0-7, 8-9 color0-1, 10 fog, 11 normal, 12 tangent, 13 binormal.
    // Inputs outside the mask read as zero instead of failing pipeline creation.
    uint32_t availableVaryings = 0xFFFFFFFF;
    // Vertex inputs (bits by AttributeSlot) fed by unnormalized integer vertex formats. Metal cannot convert those to
    // float attributes the way Direct3D 9 does, so they are declared as uint4/int4 and converted in the shader.
    uint32_t unsignedIntInputMask = 0;
    uint32_t signedIntInputMask = 0;
};

struct TranslatedShader
{
    bool pixel = false;
    std::string source;
    std::vector<ShaderSemantic> inputs;
    std::vector<ShaderSemantic> outputs;
    std::vector<ShaderSampler> samplers;
    uint32_t constantCount = 0; // highest float constant register read, plus one
    std::string error;
};

// Resource slots shared with the Metal backend.
inline constexpr uint32_t kVertexConstantBuffer = 30;
inline constexpr uint32_t kVertexParamsBuffer = 29;
inline constexpr uint32_t kFragmentConstantBuffer = 0;
inline constexpr uint32_t kFragmentParamsBuffer = 1;
inline constexpr const char *kShaderEntryPoint = "kisak_main";

// Layouts of the parameter buffers, matching the generated MSL structs.
struct VertexParams
{
    float halfPixel[2]; // added to clip-space xy, scaled by w
};

struct FragmentParams
{
    int32_t alphaFunc; // D3DCMPFUNC, 0 when alpha test is disabled
    float alphaRef;    // 0..1
};

// Metal vertex attribute index for a declaration usage, or -1 when unmapped.
int AttributeSlot(uint8_t usage, uint8_t index);

// Translates one shader. `tokens` must hold the whole program through the end
// token. Returns false with `out.error` set when the program uses a feature the
// translator does not handle.
bool TranslateShader(const uint32_t *tokens, size_t tokenCount, const TranslateOptions &options, TranslatedShader &out);

} // namespace kisak::metal
