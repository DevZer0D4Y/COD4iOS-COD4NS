#include <script/scr_variable.h>
#include <script/scr_variable_state.h>
#include <script/scr_opcode.h>
#include <qcommon/critical_sections.h>
#include <algorithm>
#include <array>
#include <cstdlib>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

static void check(bool ok, const char *message)
{
    if (!ok) { std::cerr << message << '\n'; std::abort(); }
}

static void arrayValues()
{
    const auto array = Scr_AllocArray();
    std::array<char, 64> code{};
    const float coordinates[] = {4.5f, -8.0f, 9.25f};
    std::vector<std::uint32_t> keys;
    for (int i = 0; i < 5000; ++i) {
        const auto name = "checkpoint_field_" + std::to_string(i);
        const auto key = SL_GetString(name.c_str(), 0);
        keys.push_back(key);
        const auto slot = GetVariable(array, key);
        VariableValue value{VariableUnion(i), VAR_INTEGER};
        if (i % 3 == 0) { value.type = VAR_FUNCTION; value.u.codePosValue = code.data() + i % code.size(); }
        if (i % 3 == 1) { value.type = VAR_VECTOR; value.u.vectorValue = Scr_AllocVector(coordinates); }
        SetNewVariableValue(slot, &value);
    }
    check(GetArraySize(array) == keys.size(), "variable hash table lost array members");
    for (std::size_t i = 0; i < keys.size(); ++i) {
        const auto slot = FindVariable(array, keys[i]);
        check(slot != 0, "variable lookup failed after hash collisions");
        const auto &value = GetVariableValueAddress(slot)->u;
        if (i % 3 == 0) check(value.codePosValue == code.data() + i % code.size(), "stored function pointer truncated");
        if (i % 3 == 1) check(value.vectorValue[2] == 9.25f, "stored vector pointer corrupted");
        if (i % 3 == 2) check(value.intValue == i, "stored integer changed");
    }
    std::mt19937 random(182);
    std::shuffle(keys.begin(), keys.end(), random);
    for (const auto key : keys) {
        RemoveVariable(array, key);
        SL_RemoveRefToString(key);
    }
    check(GetArraySize(array) == 0, "removed array members remain");
    RemoveRefToObject(array);
}

static void nestedArrays()
{
    const auto parent = Scr_AllocArray();
    const auto child = Scr_AllocArray();
    auto slot = GetArrayVariable(child, static_cast<unsigned>(-37));
    VariableValue value{VariableUnion(91), VAR_INTEGER};
    SetNewVariableValue(slot, &value);
    slot = GetArrayVariable(parent, 7);
    value.type = VAR_POINTER; value.u.pointerValue = child;
    SetNewVariableValue(slot, &value);
    check(FindArrayVariable(child, -37) != 0, "negative array index lost");
    const auto copied = Scr_AllocArray();
    CopyArray(parent, copied);
    const auto copiedChild = FindObject(FindArrayVariable(copied, 7));
    check(copiedChild != child && GetVariableValueAddress(FindArrayVariable(copiedChild, -37))->u.intValue == 91,
          "nested array copy lost its child");
    value = {VariableUnion(123), VAR_INTEGER};
    SetVariableValue(FindArrayVariable(child, -37), &value);
    check(GetVariableValueAddress(FindArrayVariable(copiedChild, -37))->u.intValue == 91,
          "nested array copy shares mutable child state");
    RemoveRefToObject(copied);
    AddRefToObject(parent);
    RemoveRefToObject(parent);
    check(GetArraySize(parent) == 1, "live parent released too early");
    RemoveRefToObject(parent);
}

static void vectorMath()
{
    const float a[] = {6, 12, 18};
    const float b[] = {2, 3, 6};
    for (const auto op : {OP_plus, OP_minus, OP_multiply, OP_divide}) {
        VariableValue left{VariableUnion(), VAR_VECTOR}, right{VariableUnion(), VAR_VECTOR};
        left.u.vectorValue = Scr_AllocVector(a);
        right.u.vectorValue = Scr_AllocVector(b);
        Scr_EvalBinaryOperator(op, &left, &right);
        check(left.type == VAR_VECTOR, "vector expression changed type");
        for (int i = 0; i < 3; ++i) {
            const float wanted = op == OP_plus ? a[i] + b[i] : op == OP_minus ? a[i] - b[i] : op == OP_multiply ? a[i] * b[i] : a[i] / b[i];
            check(left.u.vectorValue[i] == wanted, "native vector arithmetic failed");
        }
        RemoveRefToVector(left.u.vectorValue);
    }
}

static void scalars()
{
    auto integer = [](Opcode_t op, int first, int second, int wanted) {
        VariableValue a{VariableUnion(first), VAR_INTEGER}, b{VariableUnion(second), VAR_INTEGER};
        Scr_EvalBinaryOperator(op, &a, &b);
        check(a.type == VAR_INTEGER && a.u.intValue == wanted, "32-bit script arithmetic semantics");
    };
    integer(OP_plus, INT32_MAX, 1, INT32_MIN);
    integer(OP_minus, INT32_MIN, 1, INT32_MAX);
    integer(OP_multiply, INT32_MAX, 2, -2);
    integer(OP_mod, INT32_MIN, -1, 0);
    integer(OP_shift_left, 1, 33, 2);
    integer(OP_shift_left, 1, -1, INT32_MIN);
    integer(OP_shift_right, -2, 33, -1);
    VariableValue a{VariableUnion(2), VAR_INTEGER}, b{VariableUnion(3.5f), VAR_FLOAT};
    Scr_EvalPlus(&a, &b);
    check(a.type == VAR_FLOAT && a.u.floatValue == 5.5f, "numeric type promotion");
    // Pointer values are compared, never dereferenced, in this regression case.
    if constexpr (sizeof(void *) == 8) {
        a.type = b.type = VAR_FUNCTION;
        a.u.codePosValue = reinterpret_cast<const char *>(std::uintptr_t(0x100001234));
        b.u.codePosValue = reinterpret_cast<const char *>(std::uintptr_t(0x200001234));
        Scr_EvalEquality(&a, &b);
        check(a.type == VAR_INTEGER && !a.u.intValue, "function equality ignored upper pointer bits");
    }
    a = {VariableUnion(static_cast<int>(SL_GetString("mission", 0))), VAR_STRING};
    b = {VariableUnion(static_cast<int>(SL_GetString(" complete", 0))), VAR_STRING};
    Scr_EvalPlus(&a, &b);
    check(std::string(SL_ConvertToString(a.u.stringValue)) == "mission complete", "script string concatenation");
    SL_RemoveRefToString(a.u.stringValue);
    const std::string longString(4500, 'x');
    a = {VariableUnion(static_cast<int>(SL_GetString(longString.c_str(), 0))), VAR_STRING};
    b = {VariableUnion(static_cast<int>(SL_GetString(longString.c_str(), 0))), VAR_STRING};
    bool rejected = false;
    try { Scr_EvalPlus(&a, &b); }
    catch (const std::runtime_error &) { rejected = true; }
    check(rejected && a.type == VAR_UNDEFINED && b.type == VAR_UNDEFINED, "oversized concatenation must release values and report an error");
}

int main()
{
    Sys_InitializeCriticalSections();
    SL_Init();
    Scr_InitVariables();
    scrVarPub.varUsagePos = "native variable tests";
    arrayValues();
    nestedArrays();
    vectorMath();
    scalars();
    check(scrVarPub.numScriptValues == 0 && scrVarPub.numScriptObjects == 0, "script variable database leaked");
    check(scrVarPub.totalObjectRefCount == 0 && scrVarPub.totalVectorRefCount == 0, "script value references leaked");
    scrVarPub.varUsagePos = nullptr;
    SL_Shutdown();
    check(scrMemTreeGlob.totalAlloc == 0, "variable payload arena leaked");
    std::cout << "Original script variable arrays, collisions, native values, nested ownership and vector arithmetic passed\n";
}
