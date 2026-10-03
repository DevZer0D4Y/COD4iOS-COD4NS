#include <script/scr_value_io.h>
#include <script/scr_variable_state.h>
#include <script/scr_compile_state.h>
#include <script/scr_stack_buffer.h>
#include <script/scr_stringlist.h>
#include <script/scr_vector.h>
#include <universal/memfile.h>
#include <qcommon/critical_sections.h>
#include <array>
#include <cstring>
#include <iostream>
#include <random>
#include <thread>
#include <vector>

static std::array<unsigned, 32768> objectReferences{};
static void check(bool ok, const char *message)
{
    if (!ok) { std::cerr << message << '\n'; std::abort(); }
}

// Test-only boundary: the full object database is not part of this unit test.
// Record the exact IDs for which the original save reader requests references.
void AddRefToObject(uint32_t id)
{
    check(id < objectReferences.size(), "save reader requested invalid object");
    ++objectReferences[id];
}

static void freeLoadedStack(VariableStackBuffer *stack)
{
    for (std::size_t i = 0; i < stack->size; ++i) {
        const auto value = Scr_ReadStackEntry(stack->buf + i * SCR_STACK_ENTRY_BYTES);
        if (value.type == VAR_STACK) freeLoadedStack(value.u.stackValue);
        if (value.type == VAR_STRING || value.type == VAR_ISTRING) SL_RemoveRefToString(value.u.stringValue);
        if (value.type == VAR_VECTOR) RemoveRefToVector(value.u.vectorValue);
    }
    MT_Free(reinterpret_cast<byte *>(stack), stack->bufLen);
    --scrVarPub.numScriptThreads;
}

static void saveAndLoad(bool compressed)
{
    objectReferences.fill(0);
    SL_Init();
    std::array<char, 128> program{};
    std::array<char, 128> relocatedProgram{};
    scrVarPub.programBuffer = program.data();
    scrCompilePub.programLen = program.size();
    scrVarPub.saveIdMap[101] = 9;
    scrVarPub.saveIdMap[102] = 10;
    scrVarPub.saveIdMapRev[9] = 211;
    scrVarPub.saveIdMapRev[10] = 212;
    check(Scr_IsInOpcodeMemory(program.data()) && Scr_IsInOpcodeMemory(program.data() + 127), "program range rejected valid offset");
    check(!Scr_IsInOpcodeMemory(program.data() + 128) && !Scr_IsInOpcodeMemory(nullptr), "program range accepted invalid offset");
    const auto name = SL_GetString("checkpoint notification", 0);
    const float position[] = {5.5f, -20.25f, 65536.0f};
    const auto *vector = Scr_AllocVector(position);
    auto *nested = Scr_AllocStackBuffer(2);
    nested->pos = nullptr;
    nested->localId = 102;
    nested->time = 0x17;
    Scr_WriteStackEntry(nested->buf, {VariableUnion(program.data() + 37), VAR_FUNCTION});
    Scr_WriteStackEntry(nested->buf + SCR_STACK_ENTRY_BYTES, {VariableUnion(static_cast<int>(name)), VAR_STRING});
    auto *original = Scr_AllocStackBuffer(9);
    original->pos = program.data() + 91;
    original->localId = 101;
    original->time = 0xfa;
    std::array<VariableValue, 9> values{{
        {VariableUnion(-123456789), VAR_INTEGER}, {VariableUnion(8.125f), VAR_FLOAT},
        {VariableUnion(), VAR_VECTOR}, {VariableUnion(), VAR_STACK},
        {VariableUnion(101), VAR_POINTER}, {VariableUnion(102), VAR_POINTER},
        {VariableUnion(101), VAR_POINTER}, {VariableUnion(0x71725364), VAR_ANIMATION},
        {VariableUnion(), VAR_CODEPOS}
    }};
    values[2].u.vectorValue = vector;
    values[3].u.stackValue = nested;
    for (std::size_t i = 0; i < values.size(); ++i)
        Scr_WriteStackEntry(original->buf + i * SCR_STACK_ENTRY_BYTES, values[i]);
    std::vector<byte> bytes(65536);
    MemoryFile file{};
    Scr_ResetSaveIdHistory();
    MemFile_InitForWriting(&file, bytes.size(), bytes.data(), true, compressed);
    WriteStack(original, &file);
    MemFile_StartSegment(&file, -1);
    const auto length = file.bufferSize;
    // Dispose of the source objects before decoding, so loading cannot reuse pointers.
    RemoveRefToVector(vector);
    SL_RemoveRefToString(name);
    MT_Free(reinterpret_cast<byte *>(nested), nested->bufLen);
    MT_Free(reinterpret_cast<byte *>(original), original->bufLen);
    scrVarPub.programBuffer = relocatedProgram.data();
    Scr_ResetSaveIdHistory();
    MemFile_InitForReading(&file, length, bytes.data(), compressed);
    auto *loaded = Scr_ReadStack(&file);
    MemFile_MoveToSegment(&file, -1);
    check(loaded->localId == 211 && loaded->time == 0xfa && loaded->pos == relocatedProgram.data() + 91, "saved continuation header changed");
    check(loaded->size == values.size() && scrVarPub.numScriptThreads == 2, "nested suspended thread count");
    auto at = [&](std::size_t i) { return Scr_ReadStackEntry(loaded->buf + i * SCR_STACK_ENTRY_BYTES); };
    check(at(0).u.intValue == -123456789 && at(1).u.floatValue == 8.125f, "saved scalar values changed");
    check(at(2).type == VAR_VECTOR && std::memcmp(at(2).u.vectorValue, position, sizeof(position)) == 0, "saved vector changed");
    const auto *child = at(3).u.stackValue;
    check(child->localId == 212 && child->time == 0x17 && !child->pos, "nested stack changed");
    check(Scr_ReadStackEntry(child->buf).u.codePosValue == relocatedProgram.data() + 37, "function offset not relocated");
    check(std::strcmp(SL_ConvertToString(Scr_ReadStackEntry(child->buf + SCR_STACK_ENTRY_BYTES).u.stringValue), "checkpoint notification") == 0, "saved string changed");
    check(at(4).u.pointerValue == 211 && at(5).u.pointerValue == 212 && at(6).u.pointerValue == 211, "saved object history not remapped");
    check(at(7).u.intValue == 0x71725364 && !at(8).u.codePosValue, "animation or null code position changed");
    check(objectReferences[211] == 3 && objectReferences[212] == 2, "loaded object references missing");
    freeLoadedStack(loaded);
    check(scrVarPub.numScriptThreads == 0 && scrVarPub.totalVectorRefCount == 0, "loaded values leaked");
    SL_Shutdown();
    check(scrMemTreeGlob.totalAlloc == 0, "saved script arena leak");
    scrVarPub.programBuffer = nullptr;
    scrCompilePub.programLen = 0;
}

static void vectors()
{
    SL_Init();
    const float position[] = {1, 2, 3};
    const auto *vector = Scr_AllocVector(position);
    std::vector<std::thread> workers;
    for (unsigned i = 0; i < 6; ++i)
        workers.emplace_back([&] {
            for (unsigned j = 0; j < 1000; ++j) {
                AddRefToVector(vector);
                check(vector[2] == 3, "live vector corrupted");
                RemoveRefToVector(vector);
            }
        });
    for (auto &worker : workers) worker.join();
    RemoveRefToVector(vector);
    check(scrVarPub.totalVectorRefCount == 0, "vector references leaked");
    SL_Shutdown();
    check(scrMemTreeGlob.totalAlloc == 0, "vector arena leak");
}

static void idHistory()
{
    objectReferences.fill(0);
    std::array<unsigned, 32768> expected{};
    for (unsigned i = 1; i <= 100; ++i) {
        scrVarPub.saveIdMap[400 + i] = i;
        scrVarPub.saveIdMapRev[i] = 1000 + i;
    }
    std::mt19937 random(419);
    std::array<unsigned, 1000> ids{};
    for (auto &id : ids) id = random() % 101;
    std::array<byte, 8192> bytes{};
    MemoryFile file{};
    Scr_ResetSaveIdHistory();
    MemFile_InitForWriting(&file, bytes.size(), bytes.data(), true, false);
    for (unsigned i = 0; i < ids.size(); ++i)
        WriteId(ids[i] ? 400 + ids[i] : 0, i & 1, &file);
    MemFile_StartSegment(&file, -1);
    Scr_ResetSaveIdHistory();
    MemFile_InitForReading(&file, file.bufferSize, bytes.data(), false);
    for (unsigned i = 0; i < ids.size(); ++i) {
        byte header;
        MemFile_ReadData(&file, 1, &header);
        check((header & 7) == (i & 1), "object history changed opcode bits");
        const auto wanted = ids[i] ? 1000 + ids[i] : 0;
        check(Scr_ReadId(&file, header) == wanted, "object history wraparound or relocation failed");
        if (wanted) ++expected[wanted];
    }
    MemFile_MoveToSegment(&file, -1);
    check(objectReferences == expected, "object history reference accounting");
}

int main()
{
    Sys_InitializeCriticalSections();
    saveAndLoad(false);
    saveAndLoad(true);
    idHistory();
    vectors();
    std::cout << "Original script value save/load, nested stacks, ID relocation and vector ownership passed\n";
}
