#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>

// Native runtime values. Savegames encode fields explicitly; they never dump these layouts.
// LWSS: Custom enum typename
enum Vartype_t : std::int32_t
{
    VAR_UNDEFINED = 0x0,
    VAR_BEGIN_REF = 0x1,
    VAR_POINTER = 0x1,
    VAR_STRING = 0x2,
    VAR_ISTRING = 0x3,
    VAR_VECTOR = 0x4,
    VAR_END_REF = 0x5,
    VAR_FLOAT = 0x5,
    VAR_INTEGER = 0x6,
    VAR_CODEPOS = 0x7,
    VAR_PRECODEPOS = 0x8,
    VAR_FUNCTION = 0x9,
    VAR_STACK = 0xA,
    VAR_ANIMATION = 0xB,
    VAR_DEVELOPER_CODEPOS = 0xC,
    VAR_INCLUDE_CODEPOS = 0xD,
    VAR_THREAD = 0xE,
    VAR_NOTIFY_THREAD = 0xF,
    VAR_TIME_THREAD = 0x10,
    VAR_CHILD_THREAD = 0x11,
    VAR_OBJECT = 0x12,
    VAR_DEAD_ENTITY = 0x13,
    VAR_ENTITY = 0x14,
    VAR_ARRAY = 0x15,
    VAR_DEAD_THREAD = 0x16,
    VAR_COUNT = 0x17,
    VAR_THREAD_LIST = 0x18,
    VAR_ENDON_LIST = 0x19,
};

struct VariableStackBuffer // sizeof=0xC
{
    const char *pos;
    uint16_t size;
    uint16_t bufLen;
    uint16_t localId;
    uint8_t time;
    char buf[1];
};
static_assert(offsetof(VariableStackBuffer, buf) == sizeof(void *) + 7);
static_assert(sizeof(VariableStackBuffer) == (sizeof(void *) == 8 ? 16 : 12));

union VariableUnion // sizeof=0x4
{                                       // ...
    VariableUnion(float f) : codePosValue(nullptr)
    {
        floatValue = f;
    }
    VariableUnion(int i) : codePosValue(nullptr)
    {
        intValue = i;
    }
    VariableUnion(char *str)
    {
        codePosValue = str;
    }
    VariableUnion(const char *str)
    {
        codePosValue = str;
    }
    VariableUnion() : codePosValue(nullptr) {}

    int intValue;
    float floatValue;
    uint32_t stringValue;
    const float *vectorValue;
    const char *codePosValue;
    uint32_t pointerValue;
    VariableStackBuffer *stackValue;
    uint32_t entityOffset;
};
static_assert(sizeof(VariableUnion) == sizeof(void *));
static_assert(std::is_trivially_copyable_v<VariableUnion>);

struct VariableValue // sizeof=0x8
{   
    // ...
    VariableUnion u;                    // ...
    Vartype_t type;                           // ...
};
static_assert(sizeof(VariableValue) == 2 * sizeof(void *));

