#include <universal/q_shared.h>
#include <qcommon/threads.h>
#include "apple_platform.h"
#include <chrono>
#include <condition_variable>
#include <limits>
#include <mutex>

namespace {
struct AppleEvent {
    AppleEvent(bool manual, bool initial) : manualReset(manual), signaled(initial) {}
    std::mutex mutex;
    std::condition_variable changed;
    const bool manualReset;
    bool signaled;
    std::uint64_t generation = 0;
};

AppleEvent &getEvent(void **event)
{
    iassert(event && *event);
    return *static_cast<AppleEvent *>(*event);
}

bool waitEvent(AppleEvent &event, const std::chrono::steady_clock::time_point *deadline)
{
    std::unique_lock<std::mutex> lock(event.mutex);
    const auto generation = event.generation;
    // A manual Set releases everyone already waiting, including when Reset
    // runs before a released thread reacquires the mutex. Future waiters must
    // still observe the reset state.
    const auto ready = [&] {
        return event.signaled || (event.manualReset && event.generation != generation);
    };
    if (deadline) {
        if (!event.changed.wait_until(lock, *deadline, ready))
            return false;
    } else {
        event.changed.wait(lock, ready);
    }
    if (!event.manualReset)
        event.signaled = false;
    return true;
}
}

void Sys_CreateEvent(bool manualReset, bool initialState, void **event)
{
    iassert(event);
    *event = new AppleEvent(manualReset, initialState);
}

void Sys_DestroyAppleEvent(void **event)
{
    iassert(event);
    delete static_cast<AppleEvent *>(*event);
    *event = nullptr;
}

void Sys_SetEvent(void **handle)
{
    // Win32 SetEvent/ResetEvent/WaitForSingleObject fail immediately on a null
    // handle; engine code relies on that before some events are created.
    if (!handle || !*handle)
        return;
    auto &event = getEvent(handle);
    std::lock_guard<std::mutex> lock(event.mutex);
    event.signaled = true;
    if (event.manualReset) {
        ++event.generation;
        event.changed.notify_all();
    } else {
        // Repeated Set calls coalesce while signaled, as with Win32 events.
        event.changed.notify_one();
    }
}

void Sys_ResetEvent(void **handle)
{
    if (!handle || !*handle)
        return;
    auto &event = getEvent(handle);
    std::lock_guard<std::mutex> lock(event.mutex);
    event.signaled = false;
}

void Sys_WaitForSingleObject(void **handle)
{
    if (!handle || !*handle)
        return;
    waitEvent(getEvent(handle), nullptr);
}

bool Sys_WaitForSingleObjectTimeout(void **handle, std::uint32_t milliseconds)
{
    if (!handle || !*handle)
        return false;
    // The engine reserves UINT32_MAX for its non-timed wait API.
    iassert(milliseconds != std::numeric_limits<std::uint32_t>::max());
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(milliseconds);
    return waitEvent(getEvent(handle), &deadline);
}
