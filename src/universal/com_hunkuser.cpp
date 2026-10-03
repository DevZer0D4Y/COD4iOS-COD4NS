// The original expandable hunk allocator, separated from Windows services and
// updated to native address width and the platform's actual VM page size.
#include <universal/q_shared.h>
#include <universal/com_memory.h>
#include <qcommon/qcommon.h>
#include <algorithm>
#include <cstring>
#include <limits>

static std::uintptr_t Hunk_AlignUp(std::uintptr_t value, std::size_t alignment)
{
    return (value + alignment - 1) & ~(std::uintptr_t(alignment) - 1);
}

HunkUser *Hunk_UserCreate(int maxSize, const char *name, bool fixed, bool tempMem, int type)
{
    iassert(maxSize > 0 && maxSize % 4096 == 0);
    static_assert(sizeof(void *) != 4 || offsetof(HunkUser, buf) % 32 == 0);
    auto *user = static_cast<HunkUser *>(Z_VirtualReserve(maxSize));
    Z_VirtualCommit(user, offsetof(HunkUser, buf));
    user->current = user;
    user->next = nullptr;
    user->maxSize = maxSize;
    user->end = reinterpret_cast<std::uintptr_t>(user) + maxSize;
    user->pos = reinterpret_cast<std::uintptr_t>(user->buf);
    user->name = name;
    user->fixed = fixed;
    user->tempMem = tempMem;
    user->type = type;
    return user;
}

void *Hunk_UserAlloc(HunkUser *user, std::uint32_t size, int alignment)
{
    iassert(user);
    iassert(alignment > 0 && !(alignment & (alignment - 1)) && alignment <= HUNK_MAX_ALIGNEMT);
    // Reject requests that cannot fit even an empty chunk. Otherwise growing
    // the chain repeatedly would never make progress.
    const auto startOffset = Hunk_AlignUp(offsetof(HunkUser, buf), alignment);
    if (startOffset > std::size_t(user->maxSize) || size > std::size_t(user->maxSize) - startOffset) {
        Com_Error(ERR_FATAL, "Hunk_UserAlloc: request exceeds chunk capacity");
        return nullptr;
    }
    // A HunkUser always starts on a page boundary (Z_VirtualReserve). Multiplayer has been seen
    // reaching here with user->current holding a non-pointer value - something writes over the
    // header - and following it faults. Recover to the head chunk instead of crashing.
    if ((reinterpret_cast<std::uintptr_t>(user->current) & (Z_VirtualPageSize() - 1)) != 0) {
        Com_PrintWarning(CON_CHANNEL_SYSTEM, "Hunk_UserAlloc: '%s' chunk pointer was corrupt (%p); recovering\n",
                         user->name ? user->name : "?", static_cast<void *>(user->current));
        user->current = user;
    }
    auto *current = user->current;
    auto result = Hunk_AlignUp(current->pos, alignment);
    if (result > current->end || size > current->end - result) {
        if (user->fixed) {
            Com_Error(ERR_FATAL, "Hunk_UserAlloc: out of memory");
            return nullptr;
        }
        auto *next = Hunk_UserCreate(user->maxSize, user->name, false, user->tempMem, user->type);
        current->next = next;
        current = user->current = next;
        result = Hunk_AlignUp(current->pos, alignment);
    }
    const auto end = result + size;
    const auto firstUncommittedPage = Hunk_AlignUp(current->pos, Z_VirtualPageSize());
    if (end > firstUncommittedPage)
        Z_VirtualCommit(reinterpret_cast<void *>(firstUncommittedPage), int(end - firstUncommittedPage));
    current->pos = end;
    return reinterpret_cast<void *>(result);
}

void *Hunk_UserAllocAlignStrict(HunkUser *user, std::uint32_t size)
{
    return Hunk_UserAlloc(user, size, 1);
}

void Hunk_UserSetPos(HunkUser *user, std::uint8_t *pos)
{
    iassert(user && user->fixed);
    const auto address = reinterpret_cast<std::uintptr_t>(pos);
    iassert(address >= reinterpret_cast<std::uintptr_t>(user->buf) && address <= user->pos);
    user->pos = address;
}

void Hunk_UserReset(HunkUser *user)
{
    iassert(user);
    if (user->next) Hunk_UserDestroy(user->next);
    user->next = nullptr;
    user->current = user;
    const auto firstPageEnd = Hunk_AlignUp(reinterpret_cast<std::uintptr_t>(user->buf), Z_VirtualPageSize());
    const auto committedEnd = Hunk_AlignUp(user->pos, Z_VirtualPageSize());
    if (committedEnd > firstPageEnd)
        Z_VirtualDecommit(reinterpret_cast<void *>(firstPageEnd), int(committedEnd - firstPageEnd));
    user->pos = reinterpret_cast<std::uintptr_t>(user->buf);
    // Preserve the header and clear live bytes sharing its first physical page.
    std::memset(user->buf, 0, std::min(firstPageEnd, user->end) - user->pos);
}

void Hunk_UserDestroy(HunkUser *user)
{
    while (user) {
        auto *next = user->next;
        Z_VirtualFree(user);
        user = next;
    }
}

char *Hunk_CopyString(HunkUser *user, const char *input)
{
    const auto size = std::strlen(input) + 1;
    iassert(size <= std::numeric_limits<std::uint32_t>::max());
    auto *result = static_cast<char *>(Hunk_UserAlloc(user, std::uint32_t(size), 1));
    std::memcpy(result, input, size);
    return result;
}
