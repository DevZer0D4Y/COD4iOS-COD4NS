// Regression checks against the actual 64-bit engine, without retail assets.
#include <script/scr_vm.h>
#include <script/scr_variable_state.h>
#include <script/scr_readwrite.h>
#include <game/actor_state.h>
#include <qcommon/qcommon.h>
#include <qcommon/critical_sections.h>
#include <universal/timing.h>
#include <cstdio>
#include <cstdlib>
#include <vector>
void Sys_InitMainThread();
void Com_InitParse();
static void check(bool ok, const char *message)
{
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", message); std::abort(); }
}
int main()
{
    Sys_InitializeCriticalSections();
    Sys_InitMainThread();
    Com_InitParse();
    Dvar_Init();
    InitTiming();
    SL_Init();
    Scr_InitVariables();
    Scr_Init();
    Scr_InitSystem(1);
    // Unsigned negative indexing previously read 64 GiB beyond the VM stack.
    for (unsigned count : {0u, 1u, 3u, 16u}) {
        std::vector<char> code(count, OP_DecTop);
        code.insert(code.end(), {OP_checkclearparams, OP_GetByte, 42, OP_Return});
        scrVarPub.varUsagePos = code.data();
        for (unsigned i = 0; i < count; ++i) Scr_AddInt(100 + i);
        AddRefToObject(scrVarPub.levelId);
        const auto result = VM_Execute(AllocThread(scrVarPub.levelId), code.data(), count);
        check(scrVmPub.top == scrVmPub.stack + 1, "VM stack not restored");
        check(scrVmPub.top->type == VAR_INTEGER && scrVmPub.top->u.intValue == 42, "VM return value");
        check(scrVmPub.inparamcount == 1 && scrVmPub.function_count == 0, "VM parameter/frame counts");
        RemoveRefToObject(result);
        RemoveRefToValue(scrVmPub.top);
        --scrVmPub.top;
        scrVmPub.inparamcount = 0;
    }
    Scr_SavePre(1);
    actor_s actor{};
    actor.Physics.bIsAlive = true;
    Actor_SetDefaultState(&actor);
    check(Actor_PushState(&actor, AIS_SCRIPTEDANIM), "actor scripted animation push");
    check(actor.simulatedStateLevel == 1 && actor.eSimulatedState[1] == AIS_SCRIPTEDANIM, "actor simulated stack");
    check(actor.transitionCount == 1 && actor.StateTransitions[0].eTransition == AIS_TRANSITION_PUSH, "actor transition overwritten");
    check(actor.StateTransitions[0].eState == AIS_SCRIPTEDANIM, "actor queued state overwritten");
    Actor_PopState(&actor);
    check(actor.simulatedStateLevel == 0 && actor.transitionCount == 0, "actor push/pop simplification");
    actor.stateLevel = 1;
    actor.eState[0] = AIS_EXPOSED;
    actor.eState[1] = AIS_SCRIPTEDANIM;
    check(Actor_GetNextPopedState(&actor) == AIS_EXPOSED, "actor popped state native layout");
    std::puts("Native VM arguments/returns and actor state queue regressions passed");
}
