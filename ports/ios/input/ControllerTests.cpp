#include "../engine/controller_input.h"
#include <cassert>
#include <cstdio>
#include <limits>
using namespace kisak::controller;
int main() {
    float x=.1f, y=.1f; Deadzone(x,y,.2f,.01f); assert(x==0 && y==0);
    x=.6f; y=0; Deadzone(x,y,.2f,.0f); assert(std::fabs(x-.5f)<.0001f && y==0);
    x=.99f; y=0; Deadzone(x,y,.2f,.01f); assert(std::fabs(x-1)<.0001f);
    x=1; y=1; Deadzone(x,y); assert(std::fabs(std::hypot(x,y)-1)<.0001f);
    x=2; y=0; Deadzone(x,y,1); assert(x==0 && y==0);
    x=std::numeric_limits<float>::infinity(); y=1; Deadzone(x,y); assert(x==0 && y==0);
    x=.8f; y=0; Deadzone(x,y,.7f,.9f); assert(x==1 && y==0);
    assert(!TriggerPressed(.12f,false,.13f));
    assert(TriggerPressed(.14f,false,.13f));
    assert(TriggerPressed(.1f,true,.13f));
    assert(!TriggerPressed(.07f,true,.13f));
    assert(!TriggerPressed(0,true,0));
    assert(!TriggerPressed(std::numeric_limits<float>::quiet_NaN(),true,.13f));
    assert(MenuDirection(.8f,.9f,.4f)==(1u<<Up));
    assert(MenuDirection(-.8f,.2f,.4f)==(1u<<Left));
    assert(MenuDirection(.2f,.2f,.4f)==0);
    ButtonState state;
    const uint32_t fire=1u<<R2, jump=1u<<South;
    auto e=state.Update(fire,true,false); assert(!e.pressed && !e.released);
    e=state.Update(0,true,false); assert(!e.pressed && !e.released);
    e=state.Update(fire,true,false); assert(e.pressed==fire && !e.released);
    e=state.Update(fire|jump,true,false); assert(e.pressed==jump && !e.released);
    e=state.Update(fire|jump,true,true); assert(!e.pressed && e.released==(fire|jump));
    e=state.Update(fire|jump,true,true); assert(!e.pressed && !e.released);
    state.Update(0,true,true);
    e=state.Update(jump,true,true); assert(e.pressed==jump);
    e=state.Update(jump,false,true); assert(e.released==jump && !e.pressed);
    e=state.Update(jump,true,true); assert(!e.pressed);
    state.Update(0,true,true);
    e=state.Update(jump,true,true); assert(e.pressed==jump);
    puts("Controller deadzone, trigger, menu and reconnect tests passed");
}
