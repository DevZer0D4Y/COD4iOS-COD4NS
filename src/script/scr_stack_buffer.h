#pragma once

#include "scr_value_types.h"
#include <cstddef>
#include <cstring>
#include <limits>

// This is an in-memory archive, not the savegame's serialized representation.
// Entries are packed and therefore must never be accessed through typed pointers.
constexpr std::size_t SCR_STACK_VALUE_BYTES = sizeof(VariableUnion);
constexpr std::size_t SCR_STACK_ENTRY_BYTES = 1 + SCR_STACK_VALUE_BYTES;
constexpr std::size_t SCR_STACK_HEADER_BYTES = offsetof(VariableStackBuffer, buf);
constexpr std::size_t SCR_STACK_MAX_ENTRIES =
    (std::numeric_limits<std::uint16_t>::max() - SCR_STACK_HEADER_BYTES) / SCR_STACK_ENTRY_BYTES;

inline std::size_t Scr_StackBufferBytes(std::size_t count)
{
    if (count > SCR_STACK_MAX_ENTRIES)
        return 0;
    return SCR_STACK_HEADER_BYTES + count * SCR_STACK_ENTRY_BYTES;
}

inline VariableUnion Scr_ReadStackValue(const void *payload)
{
    VariableUnion value;
    std::memcpy(&value, payload, sizeof(value));
    return value;
}

inline void Scr_WriteStackValue(void *payload, const VariableUnion &value)
{
    std::memcpy(payload, &value, sizeof(value));
}

inline VariableValue Scr_ReadStackEntry(const void *entry)
{
    const auto *bytes = static_cast<const unsigned char *>(entry);
    return {Scr_ReadStackValue(bytes + 1), static_cast<Vartype_t>(*bytes)};
}

inline void Scr_WriteStackEntry(void *entry, const VariableValue &value)
{
    auto *bytes = static_cast<unsigned char *>(entry);
    *bytes = static_cast<unsigned char>(value.type);
    Scr_WriteStackValue(bytes + 1, value.u);
}

// Allocates through the script memory tree and initializes every header field.
// The VM remains responsible for reference counts and numScriptThreads.
VariableStackBuffer *Scr_AllocStackBuffer(std::size_t count);
VariableStackBuffer *Scr_GrowStackBuffer(VariableStackBuffer *stack, std::size_t count);
