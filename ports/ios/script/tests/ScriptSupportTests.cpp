#include <universal/q_shared.h>
#include <script/scr_stringlist.h>
#include <qcommon/critical_sections.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cstring>
#include <iostream>
#include <random>
#include <string>
#include <thread>
#include <vector>

static void check(bool ok, const char *message)
{
    if (!ok) { std::cerr << message << '\n'; std::abort(); }
}

static void memory()
{
    MT_Init();
    struct Allocation { unsigned short index; int size; unsigned char pattern; };
    std::vector<Allocation> live;
    std::mt19937 rng(421);
    for (unsigned i = 0; i < 12000; ++i) {
        if (!live.empty() && (rng() % 2 || live.size() > 400)) {
            const auto slot = rng() % live.size();
            const auto allocation = live[slot];
            const auto *bytes = reinterpret_cast<const unsigned char *>(&scrMemTreeGlob.nodes[allocation.index]);
            check(std::all_of(bytes, bytes + allocation.size, [&](auto byte) { return byte == allocation.pattern; }),
                  "script allocator overlap or corruption");
            MT_FreeIndex(allocation.index, allocation.size);
            live.erase(live.begin() + slot);
        } else {
            const int size = 1 + rng() % 2500;
            const auto index = MT_AllocIndex(size, MT_TYPE_SCRIPT_PARSE);
            auto *pointer = &scrMemTreeGlob.nodes[index];
            check(reinterpret_cast<std::uintptr_t>(pointer) % alignof(void *) == 0, "script pointer alignment");
            const auto pattern = static_cast<unsigned char>(i);
            std::memset(pointer, pattern, size);
            live.push_back({index, size, pattern});
        }
    }
    for (const auto &a : live) MT_FreeIndex(a.index, a.size);
    check(scrMemTreeGlob.totalAlloc == 0 && scrMemTreeGlob.totalAllocBuckets == 0, "script memory leak");
    std::vector<unsigned short> all;
    all.reserve(MEMORY_NODE_COUNT - 1);
    for (int i = 1; i < MEMORY_NODE_COUNT; ++i)
        all.push_back(MT_AllocIndex(1, MT_TYPE_TEMP));
    check(scrMemTreeGlob.totalAllocBuckets == MEMORY_NODE_COUNT - 1, "arena capacity changed");
    std::shuffle(all.begin(), all.end(), rng);
    for (auto index : all) MT_FreeIndex(index, 1);
    for (int bit = 0; bit < MEMORY_NODE_BITS; ++bit)
        check(MT_GetSubTreeSize(scrMemTreeGlob.head[bit]) == 1, "buddy coalescing failed");
    // A native pointer stored in a minimal allocation must retain all bits.
    void **slot = static_cast<void **>(MT_Alloc(sizeof(void *), MT_TYPE_SCRIPT_PARSE));
    *slot = &scrMemTreeGlob;
    check(*slot == &scrMemTreeGlob, "native script pointer truncated");
    MT_Free(reinterpret_cast<unsigned char *>(slot), sizeof(void *));
}

static void strings()
{
    SL_Init();
    auto id = SL_GetString("campaign/checkpoint", 0);
    check(id == SL_GetString("campaign/checkpoint", 0), "string interning");
    check(SL_ConvertFromString(SL_ConvertToString(id)) == id, "string pointer/handle roundtrip");
    SL_RemoveRefToString(id);
    check(std::string(SL_ConvertToString(id)) == "campaign/checkpoint", "live string released too early");
    SL_RemoveRefToString(id);
    check(!SL_FindString("campaign/checkpoint"), "unreferenced string still interned");
    std::vector<std::uint32_t> handles;
    for (unsigned i = 0; i < 5000; ++i) {
        const auto name = "mission_string_" + std::to_string(i);
        handles.push_back(SL_GetString(name.c_str(), 0));
    }
    for (unsigned i = 0; i < handles.size(); ++i)
        check(std::string(SL_ConvertToString(handles[i])) == "mission_string_" + std::to_string(i), "hash collision corruption");
    std::mt19937 rng(81);
    std::shuffle(handles.begin(), handles.end(), rng);
    for (auto handle : handles) SL_RemoveRefToString(handle);
    for (const int size : {1, 11, 12, 13, 15, 16, 17, 255, 256, 257, 1023, 8191}) {
        const std::string value(size, 'x');
        id = SL_GetString(value.c_str(), 0);
        check(SL_GetStringLen(id) == size, "encoded string length");
        check(value == SL_ConvertToString(id), "string payload");
        SL_RemoveRefToString(id);
    }
    id = SL_GetLowercaseString("MAPS/UTILITY", 0);
    check(std::string(SL_ConvertToString(id)) == "maps/utility", "script case folding");
    SL_RemoveRefToString(id);
    id = Scr_CreateCanonicalFilename("MAPS\\_Utility.gsc");
    check(std::string(SL_ConvertToString(id)) == "maps/_utility.gsc", "script path canonicalization");
    SL_RemoveRefToString(id);
    id = SL_GetString("persist-for-system", 1);
    SL_AddUser(id, 2);
    SL_ShutdownSystem(1);
    check(SL_GetUser(id) == 2, "system ownership transfer");
    SL_ShutdownSystem(2);
    check(!SL_FindString("persist-for-system"), "system string leak");
    SL_Shutdown();
    check(scrMemTreeGlob.totalAlloc == 0, "string arena leak");
}

static void concurrency()
{
    SL_Init();
    // A shared base reference keeps payload storage alive while workers obtain
    // independent references; each worker also creates and removes unique data.
    const auto shared = SL_GetString("shared-campaign-script", 0);
    std::vector<std::thread> workers;
    for (unsigned i = 0; i < 6; ++i)
        workers.emplace_back([&, i] {
            for (unsigned j = 0; j < 500; ++j) {
                SL_AddRefToString(shared);
                const auto name = "worker_" + std::to_string(i) + "_" + std::to_string(j);
                const auto id = SL_GetString(name.c_str(), 0);
                check(name == SL_ConvertToString(id), "concurrent string corruption");
                SL_RemoveRefToString(id);
                SL_RemoveRefToString(shared);
            }
        });
    for (auto &worker : workers) worker.join();
    SL_RemoveRefToString(shared);
    SL_Shutdown();
    check(scrMemTreeGlob.totalAlloc == 0, "concurrent string leak");
}

int main()
{
    Sys_InitializeCriticalSections();
    memory();
    strings();
    concurrency();
    std::cout << "Native script arena, interning, reference ownership and concurrent reuse passed\n";
}
