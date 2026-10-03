#include "scr_variable_state.h"
#include "scr_compile_state.h"
#include <cstdint>

scrVarPub_t scrVarPub;

bool Scr_IsInOpcodeMemory(const char *pos)
{
    if (!scrVarPub.programBuffer || !pos || scrCompilePub.programLen <= 0)
        return false;
    const auto address = reinterpret_cast<std::uintptr_t>(pos);
    const auto base = reinterpret_cast<std::uintptr_t>(scrVarPub.programBuffer);
    return address >= base && address - base < static_cast<std::uintptr_t>(scrCompilePub.programLen);
}
