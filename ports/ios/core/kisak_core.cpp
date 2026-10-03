#include "kisak_core.h"
#include "kisak_huffman_data.h"
#include <qcommon/huffman.h>
#include <qcommon/md4.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <mutex>

bool KisakCore_MD4(const uint8_t *input, size_t length, uint8_t output[16])
{
    if (!output || (!input && length))
        return false;
    MD4_CTX context;
    MD4Init(&context);
    // The original interface uses 32-bit lengths; bounded chunks also handle
    // size_t inputs larger than 4 GiB without narrowing the total length.
    while (length)
    {
        const auto chunk = static_cast<uint32_t>(std::min<size_t>(length, 1024 * 1024));
        MD4Update(&context, const_cast<uint8_t *>(input), chunk);
        input += chunk;
        length -= chunk;
    }
    MD4Final(output, &context);
    return true;
}

int KisakCore_RunSelfTests(void)
{
    // The upstream Huffman bit cursor is global. Serialize this diagnostic.
    static std::mutex mutex;
    const std::lock_guard<std::mutex> lock(mutex);
    constexpr uint8_t expected[16] = {
        0xa4, 0x48, 0x01, 0x7a, 0xaf, 0x21, 0xd8, 0x52,
        0x5f, 0xc1, 0x0a, 0xe8, 0x7a, 0xa6, 0x72, 0x9d
    };
    uint8_t digest[16];
    int failures = 0;
    if (!KisakCore_MD4(reinterpret_cast<const uint8_t *>("abc"), 3, digest)
        || std::memcmp(digest, expected, sizeof(expected)))
        failures |= 1;

    huffman_t huffman{};
    Huff_Init(&huffman);
    Huff_BuildFromData(&huffman.compressDecompress, kisak_huffman_data);
    std::array<uint8_t, 8192> encoded{}; // More than the worst case 256 * 256 bits.
    int encodedBits = 0;
    for (int symbol = 0; symbol < 256; ++symbol)
        Huff_offsetTransmit(&huffman.compressDecompress, symbol, encoded.data(), &encodedBits);
    int cursor = 0;
    for (int symbol = 0; symbol < 256; ++symbol)
    {
        int decoded = -1;
        if (!Huff_offsetReceive(huffman.compressDecompress.tree, &decoded,
                encoded.data(), &cursor, encodedBits) || decoded != symbol)
        {
            failures |= 2;
            break;
        }
    }
    if (cursor != encodedBits)
        failures |= 2;
    return failures;
}
