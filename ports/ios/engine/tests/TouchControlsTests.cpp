#include "../../input/TouchControls.h"
#include <cassert>
#include <cmath>
using namespace kisak;
int main() {
    touch::State s;s.SetBounds({40,0,850,400});s.SetContext(true,true);
    for(size_t i=1;i<touch::count;++i) {
        s.Cancel();auto p=s.Frame(i).Center();s.Down(1,p,1);auto a=s.Sample(1);
        assert(a.connected && a.touch && (a.buttons&(1u<<touch::controls[i].button)));
        s.Up(1);assert(s.Sample(1.03).buttons);assert(s.Sample(1.2).buttons==0);
    }
    s.Cancel();auto stick=s.Frame(0);auto c=stick.Center();s.Down(1,c,2);
    s.Move(1,{c.x+100,c.y-100});auto a=s.Sample(2);assert(a.leftX>0 && a.leftY>0 && std::hypot(a.leftX,a.leftY)<=1.001f);
    auto fire=s.Frame(10).Center();s.Down(2,fire,2);s.Move(2,{fire.x+8,fire.y+4});
    a=s.Sample(2);assert(a.rightTrigger==1 && a.leftY>0 && a.lookDeltaX==8 && a.lookDeltaY==4);
    assert(s.Sample(2).lookDeltaX==0);
    s.Up(1);assert(s.Sample(2.1).leftY==0);s.Up(2);
    s.Cancel();s.Down(1,{c.x,c.y-stick.w*.41f},3);assert(s.Sample(3).buttons&(1u<<controller::Up));
    s.Cancel();s.Down(1,{c.x+stick.w*.41f,c.y},3);assert(s.Sample(3).buttons&(1u<<controller::Right));
    s.Cancel();s.Down(1,{c.x,c.y+stick.w*.41f},3);assert(s.Sample(3).buttons&(1u<<controller::Down));
    s.Cancel();s.Down(1,{c.x-stick.w*.41f,c.y},3);assert(s.Sample(3).buttons&(1u<<controller::Left));
    s.Cancel();auto jump=s.Frame(9).Center();s.Down(1,jump,4);s.Up(1);s.Down(2,jump,4.01);
    assert(!(s.Sample(4.01).buttons&(1u<<controller::South)));assert(s.Sample(4.04).buttons&(1u<<controller::South));
    s.SetContext(false,true);assert(!s.Sample(4.04).connected && s.Sample(4.04).buttons==0);
    s.SetContext(true,true);assert(s.Sample(4.05).connected && s.Sample(4.05).buttons==0);
    s.SetContext(true,false);assert(s.Hit({450,250})==-2);s.Down(3,s.Frame(10).Center(),5);assert(s.Sample(5).rightTrigger==0);
    s.SetContext(true,true);s.Down(1,{450,250},6);s.Move(1,{470,260});a=s.Sample(6);
    assert(a.lookDeltaX==20 && a.lookDeltaY==10);s.SetContext(true,false);assert(s.Sample(6.1).lookDeltaX==0);
    // Repeated UIKit layout must retain a live swipe and movement capture.
    s.Cancel();s.SetContext(true,true);s.Down(1,{450,250},7);s.Down(2,c,7);
    s.SetBounds({40,0,850,400});s.Move(1,{460,255});s.Move(2,{c.x,c.y-40});
    a=s.Sample(7);assert(a.lookDeltaX==10 && a.leftY>0);
    // Cancelling one touch must not kill the other finger's aim.
    s.CancelTouch(2);s.Move(1,{475,255});a=s.Sample(7.1);
    assert(a.lookDeltaX==15 && a.leftY==0);
    s.CancelTouch(1);s.Move(1,{490,255});assert(s.Sample(7.2).lookDeltaX==0);
    s.Down(1,fire,8);s.CancelTouch(1);assert(s.Sample(8).rightTrigger==0);
    // A real bounds change still releases all held inputs.
    s.Down(1,c,9);s.SetBounds({0,0,850,400});assert(s.Sample(9).leftY==0);
    float x,y;touch::SwipeAxes(20,10,1.f/60,x,y);const float yaw60=x*180/60,pitch60=y*120/60;
    touch::SwipeAxes(20,10,1.f/30,x,y);assert(std::fabs(yaw60-x*180/30)<.001 && std::fabs(pitch60-y*120/30)<.001);
}
