#include "scr_bytecode.h"
#include "scr_vector.h"

const char *Scr_ReadCodePos(const char **pos) { return Scr_ReadBytecode<const char *>(pos); }
std::uintptr_t Scr_ReadUnsigned(const char **pos) { return Scr_ReadBytecode<std::uintptr_t>(pos); }
int Scr_ReadInt(const char **pos) { return Scr_ReadBytecode<int>(pos); }
unsigned short Scr_ReadUnsignedShort(const char **pos) { return Scr_ReadBytecode<unsigned short>(pos); }
float Scr_ReadFloat(const char **pos) { return Scr_ReadBytecode<float>(pos); }

const float *Scr_ReadVector(const char **pos)
{
    auto *vector = Scr_AllocVector();
    std::memcpy(vector, *pos, 3 * sizeof(float));
    *pos += 3 * sizeof(float);
    return vector;
}

void Scr_SkipSwitchTable(const char **pos, unsigned count)
{
    *pos += SCR_BYTECODE_SWITCH_ENTRY_BYTES * count;
}

int CompareCaseInfo(const void *left, const void *right)
{
    const auto a = Scr_PeekBytecode<std::uintptr_t>(left);
    const auto b = Scr_PeekBytecode<std::uintptr_t>(right);
    return a > b ? -1 : a < b ? 1 : 0;
}
