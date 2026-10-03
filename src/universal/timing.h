#pragma once
#include <cstdint>


void InitTiming();

extern long double msecPerRawTimerTick;
extern double qpc2msec;

#ifdef __APPLE__
// Apple profiling timestamps use mach_absolute_time ticks, calibrated by
// InitTiming. Callers migrating from x86 __rdtsc must use this same timebase.
std::uint64_t Sys_ReadRawTimer();
#endif
