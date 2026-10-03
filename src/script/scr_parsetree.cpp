#include <universal/q_shared.h>
#include "scr_parsetree.h"
#include <universal/assertive.h>
#include <universal/com_memory.h>


HunkUser *g_allocNodeUser;

void __cdecl Scr_InitAllocNode()
{
    iassert(!g_allocNodeUser);
    g_allocNodeUser = Hunk_UserCreate(0x10000, "Scr_InitAllocNode", 0, 1, 7);
}

void __cdecl Scr_ShutdownAllocNode()
{
    if (g_allocNodeUser)
    {
        Hunk_UserDestroy(g_allocNodeUser);
        g_allocNodeUser = 0;
    }
}

sval_u *__cdecl Scr_AllocNode(int size)
{
    iassert(g_allocNodeUser);
    iassert(size > 0 && size <= 0x10000 / sizeof(sval_u));
    return (sval_u *)Hunk_UserAlloc(g_allocNodeUser, sizeof(sval_u) * size, alignof(sval_u));
}

sval_u __cdecl node0(Enum_t type)
{
    sval_u result; // eax

    result.node = Scr_AllocNode(1);
    result.node[0] = sval_u(static_cast<int>(type));
    return result;
}

// Untagged grammar pairs can contain a native pointer or an integer ID in
// their first slot. Passing that slot through Enum_t loses its upper bits.
sval_u node_pair(sval_u first, sval_u second)
{
    sval_u result;
    result.node = Scr_AllocNode(2);
    result.node[0] = first;
    result.node[1] = second;
    return result;
}

sval_u __cdecl node1(Enum_t type, sval_u val1)
{
    sval_u result; // eax

    result.node = Scr_AllocNode(2);
    result.node[0] = sval_u(static_cast<int>(type));
    result.node[1] = val1;
    return result;
}

sval_u __cdecl node2(Enum_t type, sval_u val1, sval_u val2)
{
    sval_u result; // eax

    result.node = Scr_AllocNode(3);
    result.node[0] = sval_u(static_cast<int>(type));
    result.node[1] = val1;
    result.node[2] = val2;
    return result;
}

sval_u __cdecl node3(Enum_t type, sval_u val1, sval_u val2, sval_u val3)
{
    sval_u result; // eax

    result.node = Scr_AllocNode(4);
    result.node[0] = sval_u(static_cast<int>(type));
    result.node[1] = val1;
    result.node[2] = val2;
    result.node[3] = val3;
    return result;
}

sval_u __cdecl node4(Enum_t type, sval_u val1, sval_u val2, sval_u val3, sval_u val4)
{
    sval_u result; // eax

    result.node = Scr_AllocNode(5);
    result.node[0] = sval_u(static_cast<int>(type));
    result.node[1] = val1;
    result.node[2] = val2;
    result.node[3] = val3;
    result.node[4] = val4;
    return result;
}

sval_u __cdecl node5(Enum_t type, sval_u val1, sval_u val2, sval_u val3, sval_u val4, sval_u val5)
{
    sval_u result; // eax

    result.node = Scr_AllocNode(6);
    result.node[0] = sval_u(static_cast<int>(type));
    result.node[1] = val1;
    result.node[2] = val2;
    result.node[3] = val3;
    result.node[4] = val4;
    result.node[5] = val5;
    return result;
}

sval_u __cdecl node6(Enum_t type, sval_u val1, sval_u val2, sval_u val3, sval_u val4, sval_u val5, sval_u val6)
{
    sval_u result; // eax

    result.node = Scr_AllocNode(7);
    result.node[0] = sval_u(static_cast<int>(type));
    result.node[1] = val1;
    result.node[2] = val2;
    result.node[3] = val3;
    result.node[4] = val4;
    result.node[5] = val5;
    result.node[6] = val6;
    return result;
}

sval_u __cdecl node7(
    Enum_t type,
    sval_u val1,
    sval_u val2,
    sval_u val3,
    sval_u val4,
    sval_u val5,
    sval_u val6,
    sval_u val7)
{
    sval_u result; // eax

    result.node = Scr_AllocNode(8);
    result.node[0] = sval_u(static_cast<int>(type));
    result.node[1] = val1;
    result.node[2] = val2;
    result.node[3] = val3;
    result.node[4] = val4;
    result.node[5] = val5;
    result.node[6] = val6;
    result.node[7] = val7;
    return result;
}

sval_u __cdecl node8(
    Enum_t type,
    sval_u val1,
    sval_u val2,
    sval_u val3,
    sval_u val4,
    sval_u val5,
    sval_u val6,
    sval_u val7,
    sval_u val8)
{
    sval_u result; // eax

    result.node = Scr_AllocNode(9);
    result.node[0] = sval_u(static_cast<int>(type));
    result.node[1] = val1;
    result.node[2] = val2;
    result.node[3] = val3;
    result.node[4] = val4;
    result.node[5] = val5;
    result.node[6] = val6;
    result.node[7] = val7;
    result.node[8] = val8;
    return result;
}

sval_u linked_list_end(sval_u val)
{
    sval_u *node;
    sval_u result;

    node = Scr_AllocNode(2);
    node[0] = val;
    node[1].node = 0;
    result.node = Scr_AllocNode(2);
    result.node[0].node = node;
    result.node[1].node = node;
    return result;
}

sval_u prepend_node(sval_u val1, sval_u val2)
{
    sval_u *node;

    node = Scr_AllocNode(2);
    node[0] = val1;
    node[1].node = val2.node[0].node;
    val2.node[0].node = node;
    return val2;
}

sval_u append_node(sval_u val1, sval_u val2)
{
    sval_u *node;

    node = Scr_AllocNode(2);
    node[0] = val2;
    node[1].node = 0;
    val1.node[1].node[1].node = node;
    val1.node[1].node = node;
    return val1;
}
