#pragma once
// FPS Compact layout adapted from MC360-Recomp / XeniOS / ReXGlue.
// Copyright 2026 Ben Vanik and MC360-Recomp contributors. BSD-3-Clause.
// See ../app/TouchControls.LICENSE.
#include "../engine/controller_input.h"
#include <array>
#include <vector>

namespace kisak::touch {
using namespace controller;
struct Point { float x=0,y=0; };
struct Rect { float x,y,w,h; Point Center() const {return {x+w*.5f,y+h*.5f};} };
struct Control { int button; const char *label; Rect normalized; };
inline constexpr Control controls[] = {
    {-1,"",{.055f,.56f,.190f,.315f}},
    {Options,"BACK",{.390f,.045f,.080f,.112f}}, {Menu,"START",{.495f,.045f,.085f,.112f}},
    {L1,"LB",{.660f,.050f,.085f,.112f}}, {R1,"RB",{.765f,.050f,.085f,.112f}},
    {L2,"LT",{.095f,.405f,.120f,.110f}}, {North,"Y",{.760f,.455f,.065f,.115f}},
    {West,"X",{.700f,.585f,.065f,.115f}}, {East,"B",{.820f,.585f,.065f,.115f}},
    {South,"A",{.760f,.715f,.065f,.115f}}, {R2,"RT",{.860f,.405f,.120f,.110f}},
    {L3,"LS",{.250f,.585f,.065f,.115f}}, {R3,"RS",{.895f,.715f,.065f,.115f}}
};
inline constexpr size_t count=std::size(controls);
inline bool MenuControl(size_t i) {
    const int b=controls[i].button;
    return b<0 || b==Menu || b==Options || b==South || b==East || b==L1 || b==R1;
}
inline Rect Resolve(const Control &c, Rect safe) {
    const Rect n=c.normalized;
    const float side=std::min(safe.w*n.w,safe.h*n.h);
    return {safe.x+safe.w*(n.x+n.w*.5f)-side*.5f,
            safe.y+safe.h*(n.y+n.h*.5f)-side*.5f,side,side};
}
inline bool Contains(Rect r, Point p) {
    const Point c=r.Center();const float dx=p.x-c.x,dy=p.y-c.y;
    return dx*dx+dy*dy<=r.w*r.w*.25f;
}
// Engine consumes swipe distances exactly once, including coalesced UI samples.
// At 0.25 degrees per point, distance does not depend on event/render frequency.
inline void SwipeAxes(float dx,float dy,float seconds,float &x,float &y) {
    const float dt=std::max(seconds,.001f);
    x=dx*.25f/(180.0f*dt); y=-dy*.25f/(120.0f*dt);
}
class State {
    struct Capture {uintptr_t id; int control,button; Point anchor,point;};
    std::vector<Capture> captures;
    std::array<double,Count> until{},gap{};
    std::array<unsigned,Count> held{};
    float lookX=0,lookY=0;
    Rect safe{0,0,1,1};
    bool gameplay=false,available=false;
public:
    void SetBounds(Rect bounds) {
        // UIKit may lay out an unchanged overlay while a finger is still down.
        if (safe.x==bounds.x && safe.y==bounds.y && safe.w==bounds.w && safe.h==bounds.h)return;
        Cancel();safe=bounds;
    }
    void SetContext(bool enabled,bool inGame) {
        if (available!=enabled || gameplay!=inGame) Cancel();
        available=enabled;gameplay=inGame;
    }
    bool Gameplay() const {return gameplay;}
    bool Available() const {return available;}
    Rect Frame(size_t i) const {return Resolve(controls[i],safe);}
    int Hit(Point p) const {
        if(!available)return -2;
        // Action buttons have priority over the full-screen look zone.
        for(size_t i=1;i<count;++i)
            if((gameplay || MenuControl(i)) && Contains(Frame(i),p))return int(i);
        if(Contains(Frame(0),p))return 0;
        return gameplay?-1:-2;
    }
    void Down(uintptr_t id,Point p,double now) {
        if(std::any_of(captures.begin(),captures.end(),[id](auto &c){return c.id==id;}))return;
        const int i=Hit(p);if(i==-2)return;
        int b=i>0?controls[i].button:-1;
        if(i==0) {
            if(std::any_of(captures.begin(),captures.end(),[](auto &c){return c.control==0 && c.button<0;}))return;
            const Rect r=Frame(0);const Point c=r.Center();const float dx=p.x-c.x,dy=p.y-c.y;
            if(std::hypot(dx,dy)>r.w*.32f)
                b=std::fabs(dy)>=std::fabs(dx)?(dy<0?controller::Up:controller::Down):(dx<0?controller::Left:controller::Right);
        }
        if(i==-1 && std::any_of(captures.begin(),captures.end(),[](auto &c){return c.control==-1;}))return;
        if(b>=0) {
            if(!held[b] && until[b]>now)gap[b]=now+.02;
            ++held[b];until[b]=std::max(until[b],now+.075);
        }
        captures.push_back({id,i,b,p,p});
    }
    void Move(uintptr_t id,Point p) {
        for(auto &c:captures)if(c.id==id) {
            if(gameplay && (c.control==-1 || c.button==R2)) {
                lookX+=p.x-c.point.x;lookY+=p.y-c.point.y;
            }
            c.point=p;return;
        }
    }
    void Up(uintptr_t id) {
        for(auto it=captures.begin();it!=captures.end();++it)if(it->id==id) {
            if(it->button>=0 && held[it->button])--held[it->button];
            captures.erase(it);return;
        }
    }
    void CancelTouch(uintptr_t id) {
        for(const auto &c:captures)if(c.id==id && c.button>=0) {
            // Preserve other fingers holding this button.
            if(held[c.button]<=1)until[c.button]=gap[c.button]=0;
            break;
        }
        Up(id);
    }
    void Cancel() {captures.clear();held.fill(0);until.fill(0);gap.fill(0);lookX=lookY=0;}
    bool Pressed(int b,double now)const {return (held[b] || until[b]>now) && now>=gap[b];}
    Point MoveKnob()const {
        for(auto &c:captures)if(c.control==0 && c.button<0) {
            const float radius=Frame(0).w*.32f;
            const float x=c.point.x-c.anchor.x,y=c.point.y-c.anchor.y,len=std::hypot(x,y);
            const float scale=len>radius?radius/len:1;
            return {x*scale,y*scale};
        }
        return {};
    }
    Snapshot Sample(double now) {
        Snapshot s;s.connected=available;s.touch=true;
        if(!available)return s;
        for(int b=0;b<Count;++b)if(Pressed(b,now))s.buttons|=1u<<b;
        for(auto &c:captures)if(c.control==0 && c.button<0) {
            const Point d=MoveKnob();const float radius=std::max(Frame(0).w*.32f,1.0f);
            s.leftX=d.x/radius;s.leftY=-d.y/radius;
        }
        s.leftTrigger=(s.buttons&(1u<<L2))?1:0;s.rightTrigger=(s.buttons&(1u<<R2))?1:0;
        s.lookDeltaX=lookX;s.lookDeltaY=lookY;lookX=lookY=0;
        return s;
    }
};
}
