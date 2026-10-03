#include <universal/q_shared.h>
#include "com_pack.h"
#include <qcommon/qcommon.h>

// KISAKTODO: Move more stuff into here. The Bgra/4byte stuff probably belongs in here.

PackedUnitVec __cdecl Vec3PackUnitVec(const float *unitVec)
{
    float v2; // [esp+0h] [ebp-8Ch]
    float v3; // [esp+4h] [ebp-88h]
    float v4; // [esp+40h] [ebp-4Ch]
    float v5; // [esp+44h] [ebp-48h]
    PackedUnitVec out; // [esp+58h] [ebp-34h]
    // The union carries the same four bytes as the original uint8_t[4] but
    // reads them back as a word without an aliasing or alignment violation.
    PackedUnitVec testEncoding; // [esp+5Ch] [ebp-30h]
    float decodeScale; // [esp+60h] [ebp-2Ch]
    float encodeScale; // [esp+64h] [ebp-28h]
    float normalized[3]; // [esp+68h] [ebp-24h] BYREF
    float bestLenError; // [esp+74h] [ebp-18h]
    float bestDirError; // [esp+78h] [ebp-14h]
    float lenError; // [esp+7Ch] [ebp-10h]
    float decoded[3]; // [esp+80h] [ebp-Ch] BYREF

    Vec3NormalizeTo(unitVec, normalized);
    out.packed = 0;
    bestDirError = FLT_MAX;
    bestLenError = FLT_MAX;
    testEncoding.array[3] = 0;
    do
    {
        encodeScale = 32385.0 / ((double)testEncoding.array[3] - -192.0);
        testEncoding.array[0] = (int)(normalized[0] * encodeScale + 127.5);
        testEncoding.array[1] = (int)(normalized[1] * encodeScale + 127.5);
        testEncoding.array[2] = (int)(normalized[2] * encodeScale + 127.5);
        decodeScale = ((double)testEncoding.array[3] - -192.0) / 32385.0;
        decoded[0] = ((double)testEncoding.array[0] - 127.0) * decodeScale;
        decoded[1] = ((double)testEncoding.array[1] - 127.0) * decodeScale;
        decoded[2] = ((double)testEncoding.array[2] - 127.0) * decodeScale;
        v5 = Vec3Normalize(decoded) - 1.0;
        v3 = fabs(v5);
        lenError = v3;
        if (v3 < 0.001000000047497451)
        {
            v4 = Vec3Dot(decoded, normalized) - 1.0f;
            v2 = fabs(v4);
            if (v2 < (double)bestDirError || v2 == bestDirError && lenError < (double)bestLenError)
            {
                bestDirError = v2;
                bestLenError = lenError;
                out.packed = testEncoding.packed;
                if (lenError + v2 == 0.0)
                    return testEncoding;
            }
        }
        ++testEncoding.array[3];
    } while (testEncoding.array[3]);

    iassert(out.packed != 0);

    return out;
}

// 16-bit pack: low 14 bits = clamped fixed-point magnitude, high 2 bits = float exponent bits 30-31.
static uint16_t PackTexCoordHalf(float coord)
{
    uint32_t bits = COERCE_UNSIGNED_INT(coord);
    int low14 = (int)((2 * bits) ^ 0x80000000) >> 14;
    low14 = CLAMP(low14, -16384, 0x3FFF);
    return (uint16_t)((low14 & 0x3FFF) | ((bits >> 16) & 0xC000));
}

PackedTexCoords __cdecl Vec2PackTexCoords(const float *in)
{
    uint16_t u = PackTexCoordHalf(in[0]);
    uint16_t v = PackTexCoordHalf(in[1]);
    return v | (u << 16);
}

void __cdecl Byte4PackVertexColor(const float *from, uint8_t *to)
{
    to[2] = CLAMP(SnapFloatToInt(from[0] * 255.0f), 0, 255);
    to[1] = CLAMP(SnapFloatToInt(from[1] * 255.0f), 0, 255);
    to[0] = CLAMP(SnapFloatToInt(from[2] * 255.0f), 0, 255);
    to[3] = CLAMP(SnapFloatToInt(from[3] * 255.0f), 0, 255);
}

void __cdecl Byte4PackRgba(const float *from, uint8_t *to)
{
    to[0] = CLAMP(SnapFloatToInt(from[0] * 255.0f), 0, 255);
    to[1] = CLAMP(SnapFloatToInt(from[1] * 255.0f), 0, 255);
    to[2] = CLAMP(SnapFloatToInt(from[2] * 255.0f), 0, 255);
    to[3] = CLAMP(SnapFloatToInt(from[3] * 255.0f), 0, 255);
}

void __cdecl Byte4UnpackRgba(const uint8_t *from, float *to)
{
    to[0] = (float)((double)from[0] * 0.003921568859368563);
    to[1] = (float)((double)from[1] * 0.003921568859368563);
    to[2] = (float)((double)from[2] * 0.003921568859368563);
    to[3] = (float)((double)from[3] * 0.003921568859368563);
}

// Vertex colours are stored BGRA. Written byte by byte: the original stored one
// uint32_t through a uint8_t*, which is misaligned for three of four vertex
// colours and also depended on the host being little endian.
void __cdecl Byte4CopyRgbaToVertexColor(const uint8_t *rgbaFrom, uint8_t *nativeTo)
{
    nativeTo[0] = rgbaFrom[2];
    nativeTo[1] = rgbaFrom[1];
    nativeTo[2] = rgbaFrom[0];
    nativeTo[3] = rgbaFrom[3];
}

void __cdecl Byte4CopyBgraToVertexColor(const uint8_t *rgbaFrom, uint8_t *nativeTo)
{
    nativeTo[0] = rgbaFrom[0];
    nativeTo[1] = rgbaFrom[1];
    nativeTo[2] = rgbaFrom[2];
    nativeTo[3] = rgbaFrom[3];
}

void __cdecl Vec3UnpackUnitVec(PackedUnitVec in, float *out)
{
    float decodeScale; // [esp+10h] [ebp-4h]

    decodeScale = (in.array[3] - -192.0f) / 32385.0f;

    out[0] = (in.array[0] - 127.0f) * decodeScale;
    out[1] = (in.array[1] - 127.0f) * decodeScale;
    out[2] = (in.array[2] - 127.0f) * decodeScale;
}

// Inverse of PackTexCoordHalf. Kept on unsigned values: the original promoted a
// 16-bit half to int, where `half << 16` overflows the sign bit.
static float UnpackTexCoordHalf(uint32_t half)
{
    if (half == 0)
        return 0.0f;
    const uint32_t shifted = half << 14;
    const uint32_t magnitude =
        ((((shifted & 0xFFFC000u) - (~shifted & 0x10000000u)) ^ 0x80000000u) >> 1);
    return COERCE_FLOAT(((half << 16) & 0x80000000u) | magnitude);
}

void __cdecl Vec2UnpackTexCoords(PackedTexCoords in, float *out)
{
    out[0] = UnpackTexCoordHalf(in.packed >> 16);
    out[1] = UnpackTexCoordHalf(in.packed & 0xFFFFu);
}