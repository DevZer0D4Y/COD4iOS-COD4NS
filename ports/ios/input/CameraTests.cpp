#include "Camera.hpp"
#include <cstdlib>
#include <iostream>
using namespace kisakcod::ios;
static void check(bool ok,const char* reason) { if(!ok) {std::cerr<<reason<<'\n';std::exit(1);} }
static bool close(float a,float b) {return std::abs(a-b)<0.002f;}
int main() {
    Camera a,b; CameraInput i; i.forward=1;
    for(int n=0;n<60;++n) a.update(i,1.0/60);
    for(int n=0;n<120;++n) b.update(i,1.0/120);
    check(close(a.position[0],320)&&close(a.position[0],b.position[0]),"movement depends on frame rate");
    Camera diagonal; i.strafe=1; diagonal.update(i,0.05);
    check(close(std::hypot(diagonal.position[0],diagonal.position[1]),16),"diagonal movement faster than axial");
    Camera paused; paused.update(i,60); check(close(std::hypot(paused.position[0],paused.position[1]),16),"resume delta is unbounded");
    Camera lookA,lookB; CameraInput look; look.lookPointsX=100; look.lookPointsY=10000;
    lookA.update(look,1.0/30); lookB.update(look,1.0/120);
    check(close(lookA.angles[YAW],lookB.angles[YAW])&&close(lookA.angles[PITCH],Camera::kPitchLimit),
          "touch sensitivity or pitch clamp incorrect");
    // COD4 stores a positive pitch as looking down, which is what a downward drag
    // must produce for a map's own spawn angles to aim the view correctly.
    Camera down; CameraInput drag; drag.lookPointsY=200; down.update(drag,1.0/60);
    check(down.angles[PITCH]>0,"downward drag does not raise COD4 pitch");
    Camera aimed; aimed.position={0,0,0}; aimed.angles={45,0,0};
    auto forwardOf=[](const Camera& camera) {
        float forward[3];
        AngleVectors(camera.angles.data(),forward,nullptr,nullptr);
        return Vec3{forward[0],forward[1],forward[2]};
    };
    check(forwardOf(aimed)[2]<0,"positive pitch does not point below the horizon");
    Camera camera; camera.position={0,0,0}; auto m=camera.viewProjection(2);
    // Looking down +X: points on the view axis stay centered; Metal depth is [0,1].
    auto depth=[&](float x) {return (m[2]*x+m[14])/(m[3]*x+m[15]);};
    check(close(depth(1),0)&&close(depth(camera.farPlane),1),"incorrect Metal depth convention");
    check(close(m[0],0)&&close(m[1],0),"view axis not centered");
    camera.frameBounds({-10,-20,-30},{10,20,30});
    check(camera.angles[PITCH]>0,"framing view does not look down at the map");
    for(float value:camera.viewProjection(0)) check(std::isfinite(value),"nonfinite camera matrix");
    std::cout<<"Camera movement, frame timing, touch, COD4 angles and Metal projection checks passed\n";
}
