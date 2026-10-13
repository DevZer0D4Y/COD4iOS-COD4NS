#include "../../input/TouchControls.h"
#include <cassert>
int main(){
    using namespace kisak::touch;State state;state.SetBounds({10,10,1000,600});state.SetContext(true,true);
    state.SetPlacement(9,.5,.5,1.5);auto frame=state.Frame(9);auto center=frame.Center();
    assert(state.Hit(center)==9);state.Down(1,center,0);assert(state.Sample(0).buttons & (1u<<kisak::controller::South));
    state.SetPlacement(9,.6,.6,1);assert(!(state.Sample(1).buttons & (1u<<kisak::controller::South)));
    state.SetPlacement(9,-2,3,10);frame=state.Frame(9);assert(frame.x>=10 && frame.y>=10 && frame.x+frame.w<=1010 && frame.y+frame.h<=610);
    auto before=state.GetPlacement(9);state.SetPlacement(9,NAN,1,1);assert(state.GetPlacement(9).x==before.x);
    state.ResetLayout();assert(!state.GetPlacement(9).custom);
}
