#include <universal/q_shared.h>
#include <universal/timing.h>
#include <qcommon/threads.h>
#include "apple_platform.h"
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <thread>
#include <vector>

static void check(bool passed, const char *message)
{
    if (!passed) { std::cerr << message << '\n'; std::abort(); }
}

template<class Predicate> static void eventually(Predicate predicate)
{
    const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (!predicate()) {
        check(std::chrono::steady_clock::now() < end, "worker synchronization timed out");
        std::this_thread::yield();
    }
}

static void events()
{
    void *manual = nullptr;
    Sys_CreateEvent(true, false, &manual);
    check(!Sys_WaitForSingleObjectTimeout(&manual, 0), "unset manual event");
    std::atomic<unsigned> ready{0}, done{0};
    std::vector<std::thread> workers;
    for (unsigned i = 0; i < 8; ++i)
        workers.emplace_back([&] {
            ++ready;
            check(Sys_WaitForSingleObjectTimeout(&manual, 2000), "manual broadcast lost");
            ++done;
        });
    eventually([&] { return ready == 8; });
    Sys_SetEvent(&manual);
    for (auto &worker : workers) worker.join();
    check(done == 8, "manual Set must release every waiter");
    for (int i = 0; i < 10; ++i)
        check(Sys_WaitForSingleObjectTimeout(&manual, 0), "manual signal must persist");
    Sys_ResetEvent(&manual);
    const auto start = std::chrono::steady_clock::now();
    check(!Sys_WaitForSingleObjectTimeout(&manual, 15), "reset must clear event");
    check(std::chrono::steady_clock::now() - start >= std::chrono::milliseconds(15), "early timeout");
    Sys_DestroyAppleEvent(&manual);
    check(!manual, "event teardown must clear handle");

    void *automatic = nullptr;
    Sys_CreateEvent(false, true, &automatic);
    check(Sys_WaitForSingleObjectTimeout(&automatic, 0), "initial auto signal");
    check(!Sys_WaitForSingleObjectTimeout(&automatic, 0), "auto signal must be consumed");
    Sys_SetEvent(&automatic);
    Sys_SetEvent(&automatic);
    check(Sys_WaitForSingleObjectTimeout(&automatic, 0), "pending auto signal");
    check(!Sys_WaitForSingleObjectTimeout(&automatic, 0), "auto Set must coalesce");

    // Contended producer/consumer handoff must neither lose a wake nor release
    // a second waiter for a single signal. Acknowledgments prevent coalescing.
    void *ack = nullptr;
    Sys_CreateEvent(false, false, &ack);
    workers.clear();
    done = 0;
    for (int i = 0; i < 4; ++i)
        workers.emplace_back([&] {
            for (int j = 0; j < 50; ++j) {
                check(Sys_WaitForSingleObjectTimeout(&automatic, 2000), "lost auto wake");
                ++done;
                Sys_SetEvent(&ack);
            }
        });
    for (unsigned i = 1; i <= 200; ++i) {
        Sys_SetEvent(&automatic);
        check(Sys_WaitForSingleObjectTimeout(&ack, 2000), "lost acknowledgment");
        check(done == i, "one auto Set released multiple waiters");
    }
    for (auto &worker : workers) worker.join();
    check(!Sys_WaitForSingleObjectTimeout(&automatic, 0), "unexpected residual signal");
    std::thread nonTimed([&] { Sys_WaitForSingleObject(&automatic); });
    Sys_SetEvent(&automatic);
    nonTimed.join();
    Sys_DestroyAppleEvent(&automatic);
    Sys_DestroyAppleEvent(&ack);
}

static void timing()
{
    // Exercise concurrent first-use initialization of the process clock.
    std::array<std::uint32_t, 8> readings{};
    std::vector<std::thread> workers;
    for (unsigned i = 0; i < readings.size(); ++i)
        workers.emplace_back([&, i] { readings[i] = Sys_Milliseconds(); });
    for (auto &worker : workers) worker.join();
    const auto now = Sys_Milliseconds();
    for (auto reading : readings)
        check(reading <= now, "threads disagree on clock epoch");
    InitTiming();
    check(msecPerRawTimerTick > 0 && qpc2msec > 0, "invalid Apple clock calibration");
    const auto ticks = Sys_ReadRawTimer();
    const auto raw = Sys_MillisecondsRaw();
    const auto relative = Sys_Milliseconds();
    const auto wall = std::chrono::steady_clock::now();
    Sys_Sleep(25);
    const auto elapsed = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - wall).count();
    const auto calibrated = (Sys_ReadRawTimer() - ticks) * msecPerRawTimerTick;
    check(calibrated >= 25 && std::abs(calibrated - elapsed) < 10, "raw clock calibration differs from monotonic time");
    check(std::abs(double(Sys_Milliseconds() - relative) - elapsed) < 10, "relative milliseconds differ from monotonic time");
    check(std::abs(double(Sys_MillisecondsRaw() - raw) - elapsed) < 10, "raw milliseconds differ from monotonic time");
    Sys_Sleep(0);
}

int main()
{
    timing();
    events();
    std::cout << "Apple engine events, contention, teardown and monotonic clocks passed\n";
}
