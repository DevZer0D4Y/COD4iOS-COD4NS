#pragma once
#include <cmath>
#include <cstddef>
#include <cstdint>
// Compare register numbers, not bytes reached by arithmetic from the count field.
// The decompilation read pairs of 16-bit registers and pointer storage,
// making material ordering depend on uninitialized stack data on 64-bit builds.
template <class Block>
bool R_InsertShaderConstant(Block &block, uint32_t dest, const float *value) {
    const size_t capacity = sizeof(block.dest) / sizeof(block.dest[0]);
    if (block.count >= capacity || dest > UINT16_MAX || !value) return false;
    size_t index = block.count;
    while (index && block.dest[index - 1] > dest) {
        block.dest[index] = block.dest[index - 1];
        block.value[index] = block.value[index - 1];
        --index;
    }
    block.dest[index] = static_cast<uint16_t>(dest);
    block.value[index] = value;
    ++block.count;
    return true;
}
inline int R_CompareShaderFloat(float a, float b) {
    // NaNs form one equivalence class after numeric values for strict ordering.
    if (std::isnan(a)) return std::isnan(b) ? 0 : 1;
    if (std::isnan(b)) return -1;
    return a < b ? -1 : a > b ? 1 : 0;
}
