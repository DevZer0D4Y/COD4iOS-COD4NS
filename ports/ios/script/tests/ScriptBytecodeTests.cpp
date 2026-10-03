#include <script/scr_bytecode.h>
#include <script/scr_opcode.h>
#include <script/scr_vector.h>
#include <script/scr_variable_state.h>
#include <script/scr_stringlist.h>
#include <qcommon/critical_sections.h>
#include <array>
#include <cstdlib>
#include <iostream>

static void check(bool ok, const char *message)
{
    if (!ok) { std::cerr << message << '\n'; std::abort(); }
}

int main()
{
    Sys_InitializeCriticalSections();
    SL_Init();
    std::array<char, 128> targets{};
    for (std::size_t offset = 0; offset < 16; ++offset) {
        std::array<char, 512> code{};
        auto *write = code.data() + offset;
        auto emit = [&](auto value) {
            Scr_WriteBytecode(write, value);
            write += sizeof(value);
        };
        *write++ = OP_GetInteger; emit(std::int32_t(-2000000001));
        *write++ = OP_GetFunction; emit(static_cast<const char *>(targets.data() + 31));
        *write++ = OP_GetFloat; emit(1.375f);
        *write++ = OP_GetVector; emit(1.0f); emit(-2.0f); emit(3.25f);
        *write++ = OP_endswitch; emit(std::uint16_t(4));
        auto *table = write;
        for (const auto key : {7u, 0u, 21u, 3u}) {
            emit(std::uintptr_t(key));
            emit(static_cast<const char *>(targets.data() + key));
        }
        std::qsort(table, 4, SCR_BYTECODE_SWITCH_ENTRY_BYTES, CompareCaseInfo);
        *write++ = OP_End;

        const char *read = code.data() + offset;
        check(*read++ == OP_GetInteger && Scr_ReadInt(&read) == -2000000001, "32-bit integer bytecode width");
        check(*read++ == OP_GetFunction && Scr_ReadCodePos(&read) == targets.data() + 31, "native function operand truncated");
        check(*read++ == OP_GetFloat && Scr_ReadFloat(&read) == 1.375f, "unaligned float operand");
        check(*read++ == OP_GetVector, "bytecode cursor drift before vector");
        const auto *vector = Scr_ReadVector(&read);
        check(vector[0] == 1 && vector[1] == -2 && vector[2] == 3.25f, "packed vector decoding");
        check(reinterpret_cast<std::uintptr_t>(vector) % alignof(float) == 0, "decoded vector alignment");
        RemoveRefToVector(vector);
        check(*read++ == OP_endswitch && Scr_ReadUnsignedShort(&read) == 4, "switch count width");
        const char *skip = read;
        Scr_SkipSwitchTable(&skip, 4);
        check(*skip == OP_End, "switch skip used a 32-bit pointer stride");
        for (const auto key : {21u, 7u, 3u, 0u}) {
            check(Scr_ReadUnsigned(&read) == key, "switch sorting split a native entry");
            check(Scr_ReadCodePos(&read) == targets.data() + key, "switch target mismatched to key");
        }
        check(*read++ == OP_End && read == write, "bytecode instruction boundaries changed");
    }
    check(scrVarPub.totalVectorRefCount == 0, "bytecode constants leaked vector references");
    SL_Shutdown();
    check(scrMemTreeGlob.totalAlloc == 0, "bytecode vector arena leak");
    std::cout << "Packed bytecode scalars, native targets, owned vectors and switch tables passed\n";
}
