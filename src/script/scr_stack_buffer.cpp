#include "scr_stack_buffer.h"
#include "scr_memorytree.h"
#include <qcommon/qcommon.h>

static std::size_t CheckedStackBytes(std::size_t count)
{
    const auto bytes = Scr_StackBufferBytes(count);
    if (!bytes)
        Com_Error(ERR_DROP, "script stack exceeds archive capacity (%zu entries)", count);
    return bytes;
}

VariableStackBuffer *Scr_AllocStackBuffer(std::size_t count)
{
    const auto bytes = CheckedStackBytes(count);
    auto *stack = reinterpret_cast<VariableStackBuffer *>(MT_Alloc(bytes, MT_TYPE_THREAD));
    stack->pos = nullptr;
    stack->size = static_cast<std::uint16_t>(count);
    stack->bufLen = static_cast<std::uint16_t>(bytes);
    stack->localId = 0;
    stack->time = 0;
    return stack;
}

VariableStackBuffer *Scr_GrowStackBuffer(VariableStackBuffer *stack, std::size_t count)
{
    iassert(stack);
    iassert(count >= stack->size);
    const auto bytes = CheckedStackBytes(count);
    if (!MT_Realloc(stack->bufLen, bytes))
    {
        auto *replacement = Scr_AllocStackBuffer(count);
        replacement->pos = stack->pos;
        replacement->localId = stack->localId;
        replacement->time = stack->time;
        std::memcpy(replacement->buf, stack->buf, stack->size * SCR_STACK_ENTRY_BYTES);
        MT_Free(reinterpret_cast<unsigned char *>(stack), stack->bufLen);
        stack = replacement;
    }
    stack->size = static_cast<std::uint16_t>(count);
    stack->bufLen = static_cast<std::uint16_t>(bytes);
    return stack;
}
