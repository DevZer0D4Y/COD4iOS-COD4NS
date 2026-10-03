#include <universal/memfile.h>
#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <random>
#include <stdexcept>
#include <vector>

static void check(bool ok, const char *message)
{
    if (!ok) { std::cerr << message << '\n'; std::abort(); }
}

static void roundTrip(bool compressed)
{
    std::mt19937 random(2741);
    std::array<std::vector<byte>, 8> payloads;
    for (std::size_t segment = 0; segment < payloads.size(); ++segment) {
        auto &data = payloads[segment];
        data.resize(segment ? 12000 + segment * 71 : 0);
        for (std::size_t i = 0; i < data.size(); ++i)
            data[i] = (i / 67) % 3 ? static_cast<byte>(random()) : 0;
    }
    // Offsetting the backing store also verifies unaligned segment headers.
    std::vector<byte> storage(512 * 1024 + 1);
    auto *bytes = storage.data() + 1;
    MemoryFile file{};
    MemFile_InitForWriting(&file, storage.size() - 1, bytes, true, compressed);
    for (std::size_t segment = 0; segment < payloads.size(); ++segment) {
        if (segment) MemFile_StartSegment(&file, segment);
        const auto &data = payloads[segment];
        for (std::size_t i = 0; i < data.size();) {
            const auto count = std::min<std::size_t>(1 + random() % 211, data.size() - i);
            MemFile_WriteData(&file, count, data.data() + i);
            i += count;
        }
        MemFile_WriteCString(&file, "checkpoint after waittill");
    }
    MemFile_StartSegment(&file, -1);
    check(!file.memoryOverflow, "save unexpectedly overflowed");
    const auto length = MemFile_GetUsedSize(&file);
    check(length == MemFile_CopySegments(&file, 0, nullptr), "copy reports wrong byte count");
    std::vector<byte> copy(length);
    check(MemFile_CopySegments(&file, 0, copy.data()) == length, "copy length changed");
    check(std::memcmp(bytes, copy.data(), length) == 0, "save copy lost bytes");
    const auto *third = MemFile_GetSegmentAddess(&file, 3);
    check(MemFile_CopySegments(&file, 3, nullptr) == length - (third - bytes), "segment suffix length");
    MemFile_InitForReading(&file, length, bytes, compressed);
    for (const auto segment : {7, 0, 3, 1, 6, 2, 5, 4}) {
        MemFile_MoveToSegment(&file, segment);
        std::vector<byte> actual(payloads[segment].size());
        for (std::size_t i = 0; i < actual.size();) {
            const auto count = std::min<std::size_t>(1 + random() % 193, actual.size() - i);
            MemFile_ReadData(&file, count, actual.data() + i);
            i += count;
        }
        check(actual == payloads[segment], "checkpoint data changed across serialization");
        check(std::strcmp(MemFile_ReadCString(&file), "checkpoint after waittill") == 0, "saved string changed");
    }
    MemFile_MoveToSegment(&file, -1);
    MemFile_Shutdown(&file);
}

static void wireAndBounds()
{
    // Known legacy RLE bytes: one raw byte, then three nonzero bytes and one zero.
    std::vector<byte> fixture{10, 0, 0, 0, 0, 0x23, 0xc0, 1, 2, 3};
    MemoryFile file{};
    MemFile_InitForReading(&file, fixture.size(), fixture.data(), false);
    std::array<byte, 5> actual{};
    MemFile_ReadData(&file, actual.size(), actual.data());
    check(actual == std::array<byte, 5>{0x23, 1, 2, 3, 0}, "legacy RLE compatibility");
    bool rejected = false;
    try { byte extra; MemFile_ReadData(&file, 1, &extra); }
    catch (const std::runtime_error &) { rejected = true; }
    check(rejected && file.memoryOverflow, "reading beyond a segment must fail");
    for (const std::uint32_t invalid : {0u, 3u, 100u, UINT32_MAX}) {
        std::array<byte, 8> bytes{};
        std::memcpy(bytes.data(), &invalid, sizeof(invalid));
        rejected = false;
        try { MemFile_InitForReading(&file, bytes.size(), bytes.data(), false); }
        catch (const std::runtime_error &) { rejected = true; }
        check(rejected, "invalid save segment length accepted");
    }
    // A valid outer length must not make an invalid zlib stream acceptable.
    std::array<byte, 8> badCompressed{8, 0, 0, 0, 0xff, 0xff, 0xff, 0xff};
    rejected = false;
    try {
        MemFile_InitForReading(&file, badCompressed.size(), badCompressed.data(), true);
        byte value;
        MemFile_ReadData(&file, 1, &value);
    } catch (const std::runtime_error &) { rejected = true; }
    check(rejected && file.memoryOverflow, "invalid compressed stream accepted");
    std::array<byte, 80> tiny{};
    std::array<byte, 500> data{};
    for (std::size_t i = 0; i < data.size(); ++i) data[i] = static_cast<byte>(i + 1);
    MemFile_InitForWriting(&file, tiny.size(), tiny.data(), false, false);
    MemFile_WriteData(&file, data.size(), data.data());
    check(file.memoryOverflow, "bounded save writer failed to report overflow");
}

int main()
{
    roundTrip(false);
    roundTrip(true);
    wireAndBounds();
    roundTrip(true); // Error recovery must leave the compressor usable.
    std::cout << "Native save compression, eight segments, legacy RLE, bounds and recovery passed\n";
}
