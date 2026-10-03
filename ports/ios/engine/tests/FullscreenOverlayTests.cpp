#include "../../../../src/client/fullscreen_overlay.h"
#include <cassert>
#include <initializer_list>
int main() {
    for(const auto w:{2048.f,2796.f}) {
        const float h=w==2048?945:1290,canvas=h*4/3;
        float x=(w-canvas)/2,pw=canvas;
        assert(ExpandCenteredScreenOverlay(x,0,pw,h,w,h,canvas)&&x==0&&pw==w);
        pw=h*16/9;x=(w-pw)/2;
        assert(ExpandCenteredScreenOverlay(x,0,pw,h,w,h,canvas)&&x==0&&pw==w);
        x=50;pw=100;assert(!ExpandCenteredScreenOverlay(x,0,pw,h,w,h,canvas));
        x=(w-canvas)/2;pw=canvas;assert(!ExpandCenteredScreenOverlay(x,100,pw,100,w,h,canvas));
        assert(x==(w-canvas)/2&&pw==canvas);
    }
}
