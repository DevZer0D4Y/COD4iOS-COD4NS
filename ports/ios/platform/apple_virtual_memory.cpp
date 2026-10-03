#include <universal/q_shared.h>
#include <universal/com_memory.h>
#include <qcommon/qcommon.h>
#include <sys/mman.h>
#include <unistd.h>
#include <cerrno>
#include <map>
#include <mutex>

namespace {
struct Reservation { std::size_t bytes; };
std::mutex reservationMutex;
std::map<std::uintptr_t, Reservation> reservations;

std::uintptr_t floorPage(std::uintptr_t address) { return address & ~(Z_VirtualPageSize() - 1); }
std::uintptr_t ceilPage(std::uintptr_t address) { return floorPage(address + Z_VirtualPageSize() - 1); }

void verifyRange(std::uintptr_t begin, std::size_t size)
{
    auto next = reservations.upper_bound(begin);
    if (next != reservations.begin()) {
        const auto &entry = *std::prev(next);
        const auto offset = begin - entry.first;
        if (offset <= entry.second.bytes && size <= entry.second.bytes - offset) return;
    }
    Com_Error(ERR_FATAL, "Virtual memory range lies outside its reservation");
}
}

std::size_t Z_VirtualPageSize()
{
    static const auto pageSize = [] {
        const auto value = sysconf(_SC_PAGESIZE);
        iassert(value > 0 && !(value & (value - 1)));
        return std::size_t(value);
    }();
    return pageSize;
}

void *Z_VirtualReserve(int size)
{
    iassert(size > 0);
    const auto bytes = ceilPage(std::size_t(size));
    void *result = mmap(nullptr, bytes, PROT_NONE, MAP_PRIVATE | MAP_ANON, -1, 0);
    if (result == MAP_FAILED) {
        Com_Error(ERR_FATAL, "Virtual memory reservation failed: %d", errno);
        return nullptr;
    }
    std::lock_guard<std::mutex> lock(reservationMutex);
    try { reservations.emplace(reinterpret_cast<std::uintptr_t>(result), Reservation{bytes}); }
    catch (...) { munmap(result, bytes); throw; }
    return result;
}

void Z_VirtualCommit(void *ptr, int size)
{
    iassert(size >= 0);
    if (!size) return;
    const auto address = reinterpret_cast<std::uintptr_t>(ptr);
    const auto begin = floorPage(address);
    const auto bytes = ceilPage(address + std::size_t(size)) - begin;
    std::lock_guard<std::mutex> lock(reservationMutex);
    verifyRange(begin, bytes);
    if (mprotect(reinterpret_cast<void *>(begin), bytes, PROT_READ | PROT_WRITE) != 0)
        Com_Error(ERR_FATAL, "Virtual memory commit failed: %d", errno);
}

void Z_VirtualDecommit(void *ptr, int size)
{
    iassert(size >= 0);
    if (!size) return;
    // Callers work in 4 KB pages; Apple arm64 pages are 16 KB. Release only the
    // whole system pages inside the range, so neighbouring live data stays committed.
    const auto address = reinterpret_cast<std::uintptr_t>(ptr);
    const auto begin = ceilPage(address);
    const auto end = floorPage(address + std::size_t(size));
    if (end <= begin) return;
    const auto bytes = end - begin;
    std::lock_guard<std::mutex> lock(reservationMutex);
    verifyRange(begin, bytes);
    // Replace only an already owned page range. Fresh anonymous pages ensure
    // zero-filled data after recommit, unlike advisory memory-discard calls.
    if (mmap(reinterpret_cast<void *>(begin), bytes, PROT_NONE, MAP_PRIVATE | MAP_ANON | MAP_FIXED, -1, 0) == MAP_FAILED)
        Com_Error(ERR_FATAL, "Virtual memory decommit failed: %d", errno);
}

void Z_VirtualFree(void *ptr)
{
    if (!ptr) return;
    std::lock_guard<std::mutex> lock(reservationMutex);
    const auto entry = reservations.find(reinterpret_cast<std::uintptr_t>(ptr));
    if (entry == reservations.end()) {
        Com_Error(ERR_FATAL, "Virtual memory free requires a reservation base");
        return;
    }
    if (munmap(ptr, entry->second.bytes) != 0) {
        Com_Error(ERR_FATAL, "Virtual memory release failed: %d", errno);
        return;
    }
    reservations.erase(entry);
}

// com_memory.cpp builds its allocation helpers on these primitives.
bool Z_TryVirtualCommitInternal(void *ptr, int size)
{
    iassert(size >= 0);
    if (!size) return true;
    const auto address = reinterpret_cast<std::uintptr_t>(ptr);
    const auto begin = floorPage(address);
    const auto bytes = ceilPage(address + std::size_t(size)) - begin;
    std::lock_guard<std::mutex> lock(reservationMutex);
    verifyRange(begin, bytes);
    return mprotect(reinterpret_cast<void *>(begin), bytes, PROT_READ | PROT_WRITE) == 0;
}

void Z_VirtualCommitInternal(void *ptr, int size) { Z_VirtualCommit(ptr, size); }
void Z_VirtualDecommitInternal(void *ptr, int size) { Z_VirtualDecommit(ptr, size); }
void Z_VirtualFreeInternal(void *ptr) { Z_VirtualFree(ptr); }
