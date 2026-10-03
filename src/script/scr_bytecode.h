#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

static_assert(sizeof(void *) != 4 || sizeof(int) == 4 && sizeof(float) == 4 && sizeof(unsigned short) == 2);

// Bytecode is packed after one-byte opcodes. Native pointers and 32-bit scalar
// operands have different widths on ARM64; neither has guaranteed alignment.
template<class T> T Scr_PeekBytecode(const void *bytes)
{
    static_assert(std::is_trivially_copyable_v<T>);
    T value;
    std::memcpy(&value, bytes, sizeof(value));
    return value;
}

template<class T> void Scr_WriteBytecode(void *bytes, T value)
{
    static_assert(std::is_trivially_copyable_v<T>);
    std::memcpy(bytes, &value, sizeof(value));
}

template<class T> T Scr_ReadBytecode(const char **pos)
{
    const auto value = Scr_PeekBytecode<T>(*pos);
    *pos += sizeof(T);
    return value;
}

constexpr std::size_t SCR_BYTECODE_SWITCH_ENTRY_BYTES = 2 * sizeof(std::uintptr_t);
const char *Scr_ReadCodePos(const char **pos);
std::uintptr_t Scr_ReadUnsigned(const char **pos);
int Scr_ReadInt(const char **pos);
unsigned short Scr_ReadUnsignedShort(const char **pos);
float Scr_ReadFloat(const char **pos);
// The caller receives one owned script-vector reference.
const float *Scr_ReadVector(const char **pos);
void Scr_SkipSwitchTable(const char **pos, unsigned count);
int CompareCaseInfo(const void *left, const void *right);
