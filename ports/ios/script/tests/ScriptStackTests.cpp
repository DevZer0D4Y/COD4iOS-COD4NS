#include <script/scr_stack_buffer.h>
#include <script/scr_memorytree.h>
#include <qcommon/critical_sections.h>
#include <array>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <vector>

static void check(bool ok, const char *message)
{
    if (!ok) { std::cerr << message << '\n'; std::abort(); }
}

static void checkValue(const VariableValue &actual, const VariableValue &expected)
{
    check(actual.type == expected.type, "archive changed value type");
    switch (expected.type) {
    case VAR_VECTOR: check(actual.u.vectorValue == expected.u.vectorValue, "vector pointer truncated"); break;
    case VAR_STACK: check(actual.u.stackValue == expected.u.stackValue, "stack pointer truncated"); break;
    case VAR_CODEPOS:
    case VAR_FUNCTION: check(actual.u.codePosValue == expected.u.codePosValue, "code pointer truncated"); break;
    case VAR_FLOAT: check(actual.u.floatValue == expected.u.floatValue, "float changed"); break;
    default: check(actual.u.intValue == expected.u.intValue, "scalar changed"); break;
    }
}

int main()
{
    Sys_InitializeCriticalSections();
    MT_Init();
    const std::array<float, 3> vector{1.5f, -20.0f, 4096.0f};
    const char code[] = "campaign continuation";
    auto *nested = Scr_AllocStackBuffer(0);
    std::array<VariableValue, 10> values{{
        {VariableUnion(), VAR_UNDEFINED}, {VariableUnion(-2147483647), VAR_INTEGER},
        {VariableUnion(1.125f), VAR_FLOAT}, {VariableUnion(65535), VAR_STRING},
        {VariableUnion(32767), VAR_POINTER}, {VariableUnion(code + 3), VAR_CODEPOS},
        {VariableUnion(code + 8), VAR_FUNCTION}, {VariableUnion(), VAR_VECTOR},
        {VariableUnion(), VAR_STACK}, {VariableUnion(0x71345268), VAR_ANIMATION}
    }};
    values[7].u.vectorValue = vector.data();
    values[8].u.stackValue = nested;
    if constexpr (sizeof(void *) == 8)
        check(reinterpret_cast<std::uintptr_t>(nested) > UINT32_MAX, "test needs a pointer above 4 GiB");

    // Exercise every payload alignment; ARM64 packed archives have a nine-byte stride.
    for (std::size_t offset = 0; offset < 16; ++offset) {
        std::vector<unsigned char> bytes(offset + values.size() * SCR_STACK_ENTRY_BYTES + 16, 0xa5);
        for (std::size_t i = 0; i < values.size(); ++i)
            Scr_WriteStackEntry(bytes.data() + offset + i * SCR_STACK_ENTRY_BYTES, values[i]);
        for (std::size_t i = values.size(); i-- > 0;)
            checkValue(Scr_ReadStackEntry(bytes.data() + offset + i * SCR_STACK_ENTRY_BYTES), values[i]);
        for (std::size_t i = 0; i < offset; ++i) check(bytes[i] == 0xa5, "archive prefix overwritten");
        for (std::size_t i = bytes.size() - 16; i < bytes.size(); ++i) check(bytes[i] == 0xa5, "archive suffix overwritten");
    }

    auto *stack = Scr_AllocStackBuffer(values.size());
    stack->pos = code + 2;
    stack->localId = 0x7234;
    stack->time = 0xf3;
    for (std::size_t i = 0; i < values.size(); ++i)
        Scr_WriteStackEntry(stack->buf + i * SCR_STACK_ENTRY_BYTES, values[i]);
    auto count = values.size();
    for (const std::size_t next : {11u, 12u, 32u, 100u, 2048u}) {
        stack = Scr_GrowStackBuffer(stack, next);
        check(stack->pos == code + 2 && stack->localId == 0x7234 && stack->time == 0xf3,
              "growing a suspended thread lost its continuation, owner or time");
        check(stack->size == next && stack->bufLen == Scr_StackBufferBytes(next), "archive size inconsistent");
        for (std::size_t i = 0; i < count; ++i)
            checkValue(Scr_ReadStackEntry(stack->buf + i * SCR_STACK_ENTRY_BYTES), values[i % values.size()]);
        for (std::size_t i = count; i < next; ++i)
            Scr_WriteStackEntry(stack->buf + i * SCR_STACK_ENTRY_BYTES, values[i % values.size()]);
        count = next;
    }
    const auto before = scrMemTreeGlob.totalAlloc;
    bool rejected = false;
    try { Scr_GrowStackBuffer(stack, SCR_STACK_MAX_ENTRIES + 1); }
    catch (const std::runtime_error &) { rejected = true; }
    check(rejected && scrMemTreeGlob.totalAlloc == before && stack->size == count,
          "overflow must leave the suspended thread intact");
    check(!Scr_StackBufferBytes(SIZE_MAX), "size arithmetic overflow");
    auto *maximum = Scr_AllocStackBuffer(SCR_STACK_MAX_ENTRIES);
    Scr_WriteStackEntry(maximum->buf + (maximum->size - 1) * SCR_STACK_ENTRY_BYTES, values[8]);
    checkValue(Scr_ReadStackEntry(maximum->buf + (maximum->size - 1) * SCR_STACK_ENTRY_BYTES), values[8]);
    for (auto *allocation : {maximum, stack, nested})
        MT_Free(reinterpret_cast<unsigned char *>(allocation), allocation->bufLen);
    check(scrMemTreeGlob.totalAlloc == 0 && scrMemTreeGlob.totalAllocBuckets == 0, "archive arena leak");
    std::cout << "Native script archive values, growth, overflow and cleanup passed\n";
}
