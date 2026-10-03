# Apple engine platform services

This library supplies real implementations of a limited subset of the original
engine interfaces: `Sys_CreateEvent`, `Sys_SetEvent`, `Sys_ResetEvent`, blocking
and timed event waits, `Sys_Milliseconds`, `Sys_MillisecondsRaw`, `Sys_Sleep`,
and `InitTiming`. It builds for macOS host tests and iOS ARM64. It is not a
complete replacement for `src/qcommon/threads.cpp` or a campaign runtime.

Events preserve manual-reset broadcast, auto-reset single-consumer signaling,
pending signal coalescing, zero-time polling and monotonic timeouts. Manual
waiters remember a signal generation so an immediate reset cannot retract a
release already issued. `Sys_DestroyAppleEvent` requires all event users to
have stopped; it clears the handle after deletion.

Clock timestamps use `mach_absolute_time` with the system's timebase ratio.
`Sys_Milliseconds` establishes one process epoch with thread-safe first-use
initialization. Integer conversion preserves the engine's uint32 millisecond
wrap. `Sys_ReadRawTimer` and `msecPerRawTimerTick` use the same tick units. Engine
profiling call sites still using `__rdtsc`/QueryPerformanceCounter must be migrated
before linking those modules on Apple. Initialize timing before starting them.

Thread creation, thread-local engine initialization, critical sections, server
and database scheduling, app suspension, filesystem and networking remain
unimplemented. Undefined services are deliberately left as link errors; they
are not supplied as success-returning stubs.

`kisakcod_platform_tests` exercises concurrent clock initialization, calibration
against monotonic elapsed time, manual broadcast, auto-reset contention with
200 acknowledged signals, timeout behavior, blocking waits and event teardown.
Run it through `ports/ios/scripts/build.sh test` on macOS. These are host tests,
not evidence of a campaign or physical iPhone run.
