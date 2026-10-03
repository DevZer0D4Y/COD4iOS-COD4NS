#include <universal/q_shared.h>
#include <universal/timing.h>
#include <qcommon/threads.h>
#include <mach/mach_time.h>
#include <chrono>
#include <thread>

namespace {
const mach_timebase_info_data_t &timebase()
{
    static const auto value = [] {
        mach_timebase_info_data_t info{};
        const auto result = mach_timebase_info(&info);
        iassert(result == KERN_SUCCESS && info.numer && info.denom);
        return info;
    }();
    return value;
}

std::uint32_t milliseconds(std::uint64_t ticks)
{
    const auto &scale = timebase();
    // Use integer conversion and a wide intermediate to preserve the engine's
    // intentional uint32 millisecond wrap without an earlier tick overflow.
    const auto nanos = static_cast<__uint128_t>(ticks) * scale.numer / scale.denom;
    return static_cast<std::uint32_t>(nanos / 1000000);
}
}

long double msecPerRawTimerTick;
double qpc2msec;

double SecondsPerTick()
{
    const auto &scale = timebase();
    return static_cast<double>(scale.numer) / scale.denom / 1000000000.0;
}

void InitTiming()
{
    const auto &scale = timebase();
    msecPerRawTimerTick = static_cast<long double>(scale.numer) / scale.denom / 1000000.0L;
    qpc2msec = static_cast<double>(msecPerRawTimerTick);
}

std::uint64_t Sys_ReadRawTimer()
{
    return mach_absolute_time();
}

std::uint32_t Sys_Milliseconds()
{
    // C++ static initialization establishes one shared epoch, even if worker
    // threads make the first calls simultaneously.
    static const auto start = Sys_ReadRawTimer();
    return milliseconds(Sys_ReadRawTimer() - start);
}

std::uint32_t Sys_MillisecondsRaw()
{
    return milliseconds(Sys_ReadRawTimer());
}

void Sys_Sleep(std::uint32_t milliseconds)
{
    if (milliseconds == 0)
        std::this_thread::yield();
    else
        std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
}
