#include "kisak_core.h"
#include "kisak_huffman_data.h"
#include <qcommon/huffman.h>
#include <qcommon/md4.h>
#include <qcommon/com_pack.h>
#include <qcommon/qcommon.h>
#include <universal/base64.h>
#include <universal/com_math.h>
#include <universal/q_shared.h>
#include <xanim/dobj.h> // DObjAnimMat, for the quaternion matrix builders

#include <cmath>

#include <algorithm>
#include <array>
#include <climits>
#include <cstdio>
#include <cstring>
#include <numeric>
#include <string>
#include <vector>

namespace {
int checks = 0;
int failures = 0;

void check(bool condition, const char *description)
{
    ++checks;
    if (!condition)
    {
        ++failures;
        std::fprintf(stderr, "FAIL: %s\n", description);
    }
}

std::string hex(const uint8_t digest[16])
{
    constexpr char digits[] = "0123456789abcdef";
    std::string result;
    for (size_t i = 0; i < 16; ++i)
    {
        result.push_back(digits[digest[i] >> 4]);
        result.push_back(digits[digest[i] & 15]);
    }
    return result;
}

void digestTests()
{
    // RFC 1320, appendix A.5: all seven published MD4 test vectors.
    const std::pair<const char *, const char *> vectors[] = {
        {"", "31d6cfe0d16ae931b73c59d7e0c089c0"},
        {"a", "bde52cb31de33e46245e05fbdbd6fb24"},
        {"abc", "a448017aaf21d8525fc10ae87aa6729d"},
        {"message digest", "d9130a8164549fe818874806e1c7014b"},
        {"abcdefghijklmnopqrstuvwxyz", "d79e1c308aa5bbcdeea8ed63df412da9"},
        {"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789",
         "043f8582f241db351ce627e153e7f0e4"},
        {"12345678901234567890123456789012345678901234567890123456789012345678901234567890",
         "e33b4ddc9c38f2199c3e7b164fcc0536"}
    };
    for (const auto &[input, expected] : vectors)
    {
        const size_t length = std::strlen(input);
        // Offset both input and output to catch ARM alignment/aliasing problems.
        std::vector<uint8_t> unaligned(length + 2, 0xcd);
        std::memcpy(unaligned.data() + 1, input, length);
        std::array<uint8_t, 18> output;
        output.fill(0xcd);
        check(KisakCore_MD4(unaligned.data() + 1, length, output.data() + 1), "MD4 call succeeds");
        check(hex(output.data() + 1) == expected, "RFC 1320 digest, unaligned buffers");
        check(output.front() == 0xcd && output.back() == 0xcd, "MD4 output bounds");
        for (size_t split = 0; split <= length; ++split)
        {
            MD4_CTX context;
            MD4Init(&context);
            MD4Update(&context, unaligned.data() + 1, static_cast<uint32_t>(split));
            MD4Update(&context, unaligned.data() + 1 + split, static_cast<uint32_t>(length - split));
            MD4Final(output.data() + 1, &context);
            check(hex(output.data() + 1) == expected, "MD4 every incremental split");
            const std::array<uint8_t, sizeof(MD4_CTX)> zero{};
            check(std::memcmp(&context, zero.data(), zero.size()) == 0, "MD4 final clears context");
        }
    }
    uint8_t digest[16];
    check(KisakCore_MD4(nullptr, 0, digest), "MD4 accepts empty null input");
    check(hex(digest) == vectors[0].second, "MD4 empty null input value");
    check(!KisakCore_MD4(nullptr, 1, digest), "MD4 rejects missing nonempty input");
    check(!KisakCore_MD4(nullptr, 0, nullptr), "MD4 rejects missing output");
    std::vector<uint8_t> million(1'000'000, 'a');
    KisakCore_MD4(million.data(), million.size(), digest);
    check(hex(digest) == "bbce80cc6bb65e5c6745e30d4eeca9a4", "MD4 million a multiblock vector");

    std::vector<uint8_t> input(4097);
    for (size_t i = 0; i < input.size(); ++i)
        input[i] = static_cast<uint8_t>(i);
    uint8_t expected[16];
    KisakCore_MD4(input.data() + 1, input.size() - 1, expected);
    for (const size_t chunk : {1, 63, 64, 65, 255, 1024})
    {
        MD4_CTX context;
        MD4Init(&context);
        for (size_t i = 1; i < input.size(); i += chunk)
            MD4Update(&context, input.data() + i, static_cast<uint32_t>(std::min(chunk, input.size() - i)));
        MD4Final(digest, &context);
        check(std::memcmp(digest, expected, 16) == 0, "binary MD4 block boundaries");
    }
    Com_BlockChecksum128Cat(input.data() + 1, 63, input.data() + 64, 4033, digest);
    check(std::memcmp(digest, expected, 16) == 0, "game concatenated checksum");

    const int key = static_cast<int>(0x89abcdefu);
    std::vector<uint8_t> keyed = {0xef, 0xcd, 0xab, 0x89};
    keyed.insert(keyed.end(), input.begin() + 1, input.end());
    KisakCore_MD4(keyed.data(), keyed.size(), expected);
    Com_BlockChecksum128(input.data() + 1, 4096, key, digest);
    check(std::memcmp(digest, expected, 16) == 0, "game keyed checksum little-endian order");
}

void roundTrip(huff_t &huff, const std::vector<uint8_t> &input, int initialOffset)
{
    int requiredBits = initialOffset;
    for (const auto value : input)
        requiredBits += Huff_bitCount(&huff, value);
    std::vector<uint8_t> storage(static_cast<size_t>((requiredBits + 7) / 8) + 2, 0);
    storage.front() = 0x5a;
    storage.back() = 0xa5;
    uint8_t *encoded = storage.data() + 1;
    const auto prefix = static_cast<uint8_t>((1 << (initialOffset & 7)) - 1);
    encoded[initialOffset / 8] = prefix;
    int cursor = initialOffset;
    std::vector<int> starts;
    for (const auto value : input)
    {
        starts.push_back(cursor);
        Huff_offsetTransmit(&huff, value, encoded, &cursor);
    }
    check(cursor == requiredBits, "Huffman predicted bit count");
    check((encoded[initialOffset / 8] & prefix) == prefix, "Huffman preserves preceding bits");
    check(storage.front() == 0x5a && storage.back() == 0xa5, "Huffman encoding bounds");
    cursor = initialOffset;
    for (const auto value : input)
    {
        int decoded = -1;
        check(Huff_offsetReceive(huff.tree, &decoded, encoded, &cursor, requiredBits), "Huffman decode succeeds");
        check(decoded == value, "Huffman round trip byte equality");
    }
    check(cursor == requiredBits, "Huffman consumes exact encoded bit count");
    for (size_t i = 0; i < std::min<size_t>(input.size(), 256); ++i)
    {
        const int end = i + 1 < starts.size() ? starts[i + 1] : requiredBits;
        for (int limit = starts[i]; limit < end; ++limit)
        {
            int decoded = -1;
            cursor = starts[i];
            check(!Huff_offsetReceive(huff.tree, &decoded, encoded, &cursor, limit), "Huffman rejects truncated symbol");
            check(cursor == limit && decoded == 0, "Huffman truncation stops at exact bound");
        }
    }
}

void huffmanTests()
{
    huffman_t huffman{};
    Huff_Init(&huffman);
    Huff_BuildFromData(&huffman.compressDecompress, kisak_huffman_data);
    auto &huff = huffman.compressDecompress;
    check(huff.blocNode == 513, "Huffman complete 257-leaf tree");
    check(huff.freelist == nullptr, "Huffman internal symbols do not overflow loc[]");
    check(huff.tree->parent == nullptr, "Huffman root parent");
    check(huff.tree->weight == std::accumulate(std::begin(kisak_huffman_data), std::end(kisak_huffman_data), 0), "Huffman root frequency");
    for (int i = 0; i < 257; ++i)
    {
        check(huff.loc[i] && huff.loc[i]->symbol == i, "Huffman symbol lookup");
        check(!huff.loc[i]->left && !huff.loc[i]->right, "Huffman symbol is a leaf");
    }
    for (int i = 0; i < huff.blocNode; ++i)
    {
        const auto &node = huff.nodeList[i];
        if (node.left)
            check(node.left->parent == &node && node.right->parent == &node, "Huffman reciprocal parent links");
    }
    nodetype small{}, large{};
    small.weight = INT_MIN;
    large.weight = INT_MAX;
    nodetype *a = &small, *b = &large;
    check(nodeCmp(&a, &b) < 0 && nodeCmp(&b, &a) > 0 && nodeCmp(&a, &a) == 0, "Huffman pointer-width/overflow-safe comparator");
    std::vector<uint8_t> allBytes(256);
    std::iota(allBytes.begin(), allBytes.end(), uint8_t{0});
    for (const int offset : {0, 1, 7, 9})
        roundTrip(huff, allBytes, offset);
    uint32_t random = 0x12345678;
    std::vector<uint8_t> payload(4096);
    for (auto &byte : payload)
    {
        random = random * 1664525u + 1013904223u;
        byte = static_cast<uint8_t>(random >> 24);
    }
    roundTrip(huff, payload, 0);
    roundTrip(huff, std::vector<uint8_t>(1024, 0), 0);
}

bool near(float a, float b, float tolerance = 1e-4f)
{
    return std::fabs(a - b) <= tolerance;
}

void angleTests()
{
    // COD4 axes: +X forward, +Z up, PITCH positive looking down.
    float forward[3], right[3], up[3];
    const float level[3] = {0, 0, 0};
    AngleVectors(level, forward, right, up);
    check(near(forward[0], 1) && near(forward[1], 0) && near(forward[2], 0), "AngleVectors zero yaw faces +X");
    check(near(right[0], 0) && near(right[1], -1) && near(right[2], 0), "AngleVectors zero yaw right vector");
    check(near(up[0], 0) && near(up[1], 0) && near(up[2], 1), "AngleVectors zero yaw up vector");

    const float quarterTurn[3] = {0, 90, 0};
    AngleVectors(quarterTurn, forward, nullptr, nullptr);
    check(near(forward[0], 0) && near(forward[1], 1), "AngleVectors yaw turns towards +Y");

    const float nose[3] = {90, 0, 0};
    AngleVectors(nose, forward, nullptr, nullptr);
    check(near(forward[2], -1), "positive pitch must look down");

    // Orthonormal for arbitrary angles, including roll.
    for (const float roll : {0.0f, 37.0f, -128.0f})
    {
        const float angles[3] = {-24.5f, 143.0f, roll};
        AngleVectors(angles, forward, right, up);
        check(near(Vec3Dot(forward, forward), 1) && near(Vec3Dot(right, right), 1) &&
              near(Vec3Dot(up, up), 1), "AngleVectors basis is not unit length");
        check(near(Vec3Dot(forward, right), 0) && near(Vec3Dot(forward, up), 0) &&
              near(Vec3Dot(right, up), 0), "AngleVectors basis is not orthogonal");

        // AnglesToAxis is the same basis with right negated (see its comment).
        mat3x3 axis;
        AnglesToAxis(angles, axis);
        check(near(axis[0][0], forward[0]) && near(axis[0][1], forward[1]) && near(axis[0][2], forward[2]),
              "AnglesToAxis row 0 is not the forward vector");
        check(near(axis[1][0], -right[0]) && near(axis[1][1], -right[1]) && near(axis[1][2], -right[2]),
              "AnglesToAxis row 1 is not the negated right vector");
        check(near(axis[2][0], up[0]) && near(axis[2][1], up[1]) && near(axis[2][2], up[2]),
              "AnglesToAxis row 2 is not the up vector");

        // AnglesToQuat runs through AxisToQuat, which writes past the first row
        // of its float[3] parameter in the original decompilation.
        float quat[4];
        AnglesToQuat(angles, quat);
        check(near(quat[0] * quat[0] + quat[1] * quat[1] + quat[2] * quat[2] + quat[3] * quat[3], 1, 1e-3f),
              "AnglesToQuat does not produce a unit quaternion");
        mat3x3 fromQuat;
        QuatToAxis(quat, fromQuat);
        for (int row = 0; row < 3; ++row)
            for (int column = 0; column < 3; ++column)
                check(near(fromQuat[row][column], axis[row][column], 1e-3f),
                      "quaternion round trip does not reproduce the axis");

        float restored[3]; // vec3r is a bare float*, so the storage is ours.
        AxisToAngles(axis, restored);
        mat3x3 reAxis;
        AnglesToAxis(restored, reAxis);
        for (int row = 0; row < 3; ++row)
            for (int column = 0; column < 3; ++column)
                check(near(reAxis[row][column], axis[row][column], 1e-3f),
                      "AxisToAngles does not reproduce the axis");
    }

    float vector[3] = {3, 0, 4};
    check(near(Vec3Normalize(vector), 5) && near(vector[0], 0.6f) && near(vector[2], 0.8f),
          "Vec3Normalize returns the wrong length or direction");
    float normalized[3];
    const float zero[3] = {0, 0, 0};
    check(near(Vec3NormalizeTo(zero, normalized), 0), "Vec3NormalizeTo mishandles the zero vector");
    check(std::isfinite(normalized[0]) && std::isfinite(normalized[1]) && std::isfinite(normalized[2]),
          "Vec3NormalizeTo produced a non-finite vector");
}

void matrixTests()
{
    // Both builders fill five cells of a 4x4 and leave the rest zero. The
    // original code reached them with flat indices on a float[4] row.
    const auto onlyCellsSet = [](const mat4x4 &matrix, const char *label) {
        for (int row = 0; row < 4; ++row)
            for (int column = 0; column < 4; ++column)
            {
                const bool expected = (row == column && row < 3) || (row == 2 && column == 3) ||
                                      (row == 3 && column == 2);
                if (!expected)
                    check(matrix[row][column] == 0.0f, label);
            }
    };

    mat4x4 infinite;
    std::memset(infinite, 0xff, sizeof(infinite));
    InfinitePerspectiveMatrix(infinite, 1.0f, 0.5f, 4.0f);
    onlyCellsSet(infinite, "InfinitePerspectiveMatrix wrote outside its five cells");
    check(near(infinite[0][0], MAX_11BIT_FLT) && near(infinite[1][1], MAX_11BIT_FLT / 0.5f),
          "InfinitePerspectiveMatrix scale");
    check(near(infinite[2][2], MAX_11BIT_FLT) && near(infinite[2][3], 1.0f) &&
          near(infinite[3][2], -4.0f * MAX_11BIT_FLT), "InfinitePerspectiveMatrix depth row");

    mat4x4 finite;
    std::memset(finite, 0xff, sizeof(finite));
    FinitePerspectiveMatrix(finite, 1.0f, 0.5f, 4.0f, 1024.0f);
    onlyCellsSet(finite, "FinitePerspectiveMatrix wrote outside its five cells");
    check(near(finite[0][0], 1.0f) && near(finite[1][1], 2.0f), "FinitePerspectiveMatrix scale");
    check(near(finite[2][2], 1024.0f / 1020.0f, 1e-3f) && near(finite[2][3], 1.0f) &&
          near(finite[3][2], -4.0f * 1024.0f / 1020.0f, 1e-3f), "FinitePerspectiveMatrix depth row");

    // ConvertQuatToInverseMat fills four rows through the same pattern.
    DObjAnimMat animation{};
    animation.quat[3] = 1.0f;
    animation.trans[0] = 7.0f;
    animation.trans[1] = -3.0f;
    animation.trans[2] = 11.0f;
    animation.transWeight = 2.0f;
    mat4x3 inverse;
    std::memset(inverse, 0xff, sizeof(inverse));
    ConvertQuatToInverseMat(&animation, inverse);
    for (int row = 0; row < 3; ++row)
        for (int column = 0; column < 3; ++column)
            check(near(inverse[row][column], row == column ? 1.0f : 0.0f),
                  "identity rotation did not produce an identity basis");
    check(near(inverse[3][0], -7.0f) && near(inverse[3][1], 3.0f) && near(inverse[3][2], -11.0f),
          "inverse translation row is wrong");
}

void roundingTests()
{
    // x86 cvtss2si follows MXCSR, which the engine leaves at round-to-nearest,
    // ties to even. The ARM64 replacement has to agree on every tie.
    const std::pair<float, int> vectors[] = {
        {0.5f, 0}, {1.5f, 2}, {2.5f, 2}, {3.5f, 4}, {-0.5f, 0}, {-1.5f, -2}, {-2.5f, -2},
        {2.4f, 2}, {2.6f, 3}, {-2.4f, -2}, {-2.6f, -3}, {0.0f, 0}, {123456.0f, 123456}
    };
    for (const auto &[input, expected] : vectors)
        check(SnapFloatToInt(input) == expected, "SnapFloatToInt does not round half to even");
    check(SnapFloat(2.5f) == 2.0f && SnapFloat(-1.5f) == -2.0f, "SnapFloat disagrees with SnapFloatToInt");
}

void packedVertexTests()
{
    // Packed unit vectors: the encoder searches for the scale that best
    // reproduces the direction, so a round trip has to stay a unit vector
    // pointing the same way.
    const float directions[][3] = {
        {1, 0, 0}, {0, 1, 0}, {0, 0, 1}, {-1, 0, 0}, {0, 0, -1},
        {0.577f, 0.577f, 0.577f}, {-0.267f, 0.535f, -0.802f}, {3, -4, 12}
    };
    for (const auto &direction : directions)
    {
        float expected[3];
        Vec3NormalizeTo(direction, expected);
        const PackedUnitVec packed = Vec3PackUnitVec(direction);
        float decoded[3];
        Vec3UnpackUnitVec(packed, decoded);
        check(near(Vec3Dot(decoded, decoded), 1, 2e-3f), "packed unit vector is not unit length");
        check(Vec3Dot(decoded, expected) > 0.999f, "packed unit vector changed direction");
    }

    // Texture coordinates use a 16-bit format per axis; the round trip is lossy
    // but must preserve sign, magnitude order and exact zero.
    const float coordinates[][2] = {{0, 0}, {1, 1}, {0.5f, -0.5f}, {-3.25f, 7.5f}, {0.001f, -0.001f}};
    for (const auto &coordinate : coordinates)
    {
        float decoded[2];
        Vec2UnpackTexCoords(Vec2PackTexCoords(coordinate), decoded);
        for (int axis = 0; axis < 2; ++axis)
        {
            check(std::isfinite(decoded[axis]), "packed texture coordinate is not finite");
            check(near(decoded[axis], coordinate[axis], std::fabs(coordinate[axis]) * 0.01f + 1e-3f),
                  "packed texture coordinate round trip lost too much precision");
        }
    }

    // Colour packing writes four bytes through a uint32_t in the original code.
    // Run it at every alignment so a misaligned store is caught here.
    const uint8_t rgba[4] = {0x11, 0x22, 0x33, 0x44};
    for (int offset = 0; offset < 4; ++offset)
    {
        uint8_t storage[8] = {};
        uint8_t *native = storage + offset;
        Byte4CopyRgbaToVertexColor(rgba, native);
        check(native[0] == 0x33 && native[1] == 0x22 && native[2] == 0x11 && native[3] == 0x44,
              "RGBA to vertex colour is not a BGRA permutation");
        Byte4CopyBgraToVertexColor(rgba, native);
        check(native[0] == 0x11 && native[1] == 0x22 && native[2] == 0x33 && native[3] == 0x44,
              "BGRA to vertex colour changed the byte order");
    }

    const float colour[4] = {0.0f, 0.25f, 1.0f, 0.5f};
    uint8_t packedColour[4];
    Byte4PackRgba(colour, packedColour);
    check(packedColour[0] == 0 && packedColour[2] == 255, "Byte4PackRgba endpoints");
    float unpacked[4];
    Byte4UnpackRgba(packedColour, unpacked);
    for (int channel = 0; channel < 4; ++channel)
        check(near(unpacked[channel], colour[channel], 0.01f), "RGBA byte round trip");
    uint8_t vertexColour[4];
    Byte4PackVertexColor(colour, vertexColour);
    check(vertexColour[2] == packedColour[0] && vertexColour[1] == packedColour[1] &&
          vertexColour[0] == packedColour[2] && vertexColour[3] == packedColour[3],
          "Byte4PackVertexColor is not the BGRA order of Byte4PackRgba");

    // Out-of-range input must clamp rather than wrap through the byte.
    const float overshoot[4] = {-2.0f, 2.0f, 0.5f, 17.0f};
    Byte4PackRgba(overshoot, packedColour);
    check(packedColour[0] == 0 && packedColour[1] == 255 && packedColour[3] == 255,
          "Byte4PackRgba does not clamp out-of-range channels");
}

void base64Tests()
{
    // RFC 4648 section 10.
    const std::pair<const char *, const char *> vectors[] = {
        {"", ""}, {"f", "Zg=="}, {"fo", "Zm8="}, {"foo", "Zm9v"},
        {"foob", "Zm9vYg=="}, {"fooba", "Zm9vYmE="}, {"foobar", "Zm9vYmFy"}
    };
    for (const auto &[input, expected] : vectors)
    {
        const auto length = static_cast<uint32_t>(std::strlen(input));
        std::vector<unsigned char> encoded(b64e_size(length) + 1, 0xcd);
        const uint32_t written = b64_encode(reinterpret_cast<const unsigned char *>(input), length,
                                            encoded.data());
        // The implementation returns the character count and terminates after it,
        // one less than the "including null byte" wording in base64.h.
        check(written == std::strlen(expected) &&
              std::string(reinterpret_cast<char *>(encoded.data())) == expected,
              "base64 encoding does not match RFC 4648");
    }

    // Round trip every byte value, at each of the three padding alignments.
    std::vector<unsigned char> payload(256);
    std::iota(payload.begin(), payload.end(), static_cast<unsigned char>(0));
    for (const uint32_t length : {254u, 255u, 256u})
    {
        std::vector<unsigned char> encoded(b64e_size(length) + 1, 0);
        const uint32_t encodedLength = b64_encode(payload.data(), length, encoded.data());
        check(encodedLength == b64e_size(length), "b64e_size does not match the encoder");
        std::vector<unsigned char> decoded(b64d_size(encodedLength) + 1, 0);
        const uint32_t decodedLength = b64_decode(encoded.data(), encodedLength, decoded.data());
        check(decodedLength == length &&
              std::equal(payload.begin(), payload.begin() + length, decoded.begin()),
              "base64 round trip lost data");
    }
}
} // namespace

int main()
{
    check(KisakCore_RunSelfTests() == 0, "app startup self-tests");
    digestTests();
    huffmanTests();
    angleTests();
    matrixTests();
    roundingTests();
    packedVertexTests();
    base64Tests();
    std::printf("KisakCOD original MD4/Huffman/math/base64: %d checks, %d failures (%zu-bit pointers)\n", checks, failures, sizeof(void *) * CHAR_BIT);
    return failures ? 1 : 0;
}
