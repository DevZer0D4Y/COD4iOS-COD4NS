#include <universal/q_shared.h>
#include <qcommon/critical_sections.h>
#include <array>
#include <mutex>

namespace {
auto &sections()
{
    static std::array<std::recursive_mutex, CRITSECT_COUNT> locks;
    return locks;
}
}

void Sys_InitializeCriticalSections()
{
    (void)sections();
}

void Sys_EnterCriticalSection(int section)
{
    iassert(section >= 0 && section < CRITSECT_COUNT);
    sections()[section].lock();
}

void Sys_LeaveCriticalSection(int section)
{
    iassert(section >= 0 && section < CRITSECT_COUNT);
    sections()[section].unlock();
}
