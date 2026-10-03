#include <universal/q_shared.h>
#include "pool_allocator.h"
#include "assertive.h"
#include <cstdint>
#include <cstring>
#include <limits>

// Slot storage can be packed: preserve native pointer width without requiring
// alignment. The original uint32_t stores truncate ARM64 heap addresses.
static freenode *Pool_Next(const freenode *node)
{
    freenode *next;
    std::memcpy(&next, static_cast<const void *>(node), sizeof(next));
    return next;
}
static void Pool_SetNext(freenode *node, freenode *next)
{
    std::memcpy(static_cast<void *>(node), &next, sizeof(next));
}

void __cdecl Pool_Init(char *pool, pooldata_t *pooldata, uint32_t itemSize, uint32_t itemCount)
{
    uint32_t itemIndex; // [esp+4h] [ebp-4h]

    if (!pool)
        MyAssertHandler(".\\universal\\pool_allocator.cpp", 16, 0, "%s", "pool");
    if (!pooldata)
        MyAssertHandler(".\\universal\\pool_allocator.cpp", 17, 0, "%s", "pooldata");
    if (itemSize < sizeof(freenode *))
        MyAssertHandler(".\\universal\\pool_allocator.cpp", 18, 0, "%s", "itemSize >= sizeof( byte * )");
    if (itemCount < 2)
        MyAssertHandler(".\\universal\\pool_allocator.cpp", 19, 0, "%s", "itemCount >= 2");
    if (!pool || !pooldata || itemSize < sizeof(freenode *) || itemCount < 2)
        return;
    if (itemCount > std::numeric_limits<std::size_t>::max() / itemSize)
    {
        MyAssertHandler(__FILE__, __LINE__, 0, "%s", "pool byte size overflow");
        return;
    }
    pooldata->firstFree = pool;

    for (itemIndex = 0; itemIndex < itemCount - 1; ++itemIndex)
        Pool_SetNext(reinterpret_cast<freenode *>(pool + std::size_t(itemSize) * itemIndex),
                     reinterpret_cast<freenode *>(pool + std::size_t(itemSize) * (itemIndex + 1)));

    Pool_SetNext(reinterpret_cast<freenode *>(pool + std::size_t(itemSize) * itemIndex), nullptr);
    pooldata->activeCount = 0;
}

freenode *__cdecl Pool_Alloc(pooldata_t *pooldata)
{
    freenode *item; // [esp+0h] [ebp-4h]

    if (!pooldata)
        MyAssertHandler(".\\universal\\pool_allocator.cpp", 41, 0, "%s", "pooldata");
    item = (freenode *)pooldata->firstFree;
    if (!pooldata->firstFree)
        return 0;
    pooldata->firstFree = Pool_Next(item);
    ++pooldata->activeCount;
    return item;
}

void __cdecl Pool_Free(freenode *data, pooldata_t *pooldata)
{
    freenode *item; // [esp+0h] [ebp-4h]

    if (!data)
        MyAssertHandler(".\\universal\\pool_allocator.cpp", 59, 0, "%s", "data");
    if (!pooldata)
        MyAssertHandler(".\\universal\\pool_allocator.cpp", 60, 0, "%s", "pooldata");
    for (item = (freenode *)pooldata->firstFree; item; item = Pool_Next(item))
    {
        if (item == data)
            MyAssertHandler(".\\universal\\pool_allocator.cpp", 63, 0, "%s", "item != data");
    }
    Pool_SetNext(data, (freenode *)pooldata->firstFree);
    pooldata->firstFree = data;
    --pooldata->activeCount;
}

uint32_t __cdecl Pool_FreeCount(const pooldata_t *pooldata)
{
    const freenode *item; // [esp+0h] [ebp-8h]
    uint32_t count; // [esp+4h] [ebp-4h]

    if (!pooldata)
        MyAssertHandler(".\\universal\\pool_allocator.cpp", 79, 0, "%s", "pooldata");
    count = 0;
    for (item = (const freenode *)pooldata->firstFree; item; item = Pool_Next(item))
        ++count;
    return count;
}
