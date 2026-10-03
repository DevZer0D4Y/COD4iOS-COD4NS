#pragma once

// View angles follow COD4: degrees, ordered PITCH/YAW/ROLL, with PITCH positive
// looking down. The basis comes from the engine's own AngleVectors(), so a map's
// spawn angles are used exactly as the game stores them.
#include <universal/q_shared.h> // PITCH, YAW, ROLL
#include <universal/com_math.h> // AngleVectors

#include <algorithm>
#include <array>
#include <cmath>

namespace kisakcod::ios {
using Vec3 = std::array<float, 3>;
using Mat4 = std::array<float, 16>;
inline Vec3 add(Vec3 a, Vec3 b) { return {a[0]+b[0],a[1]+b[1],a[2]+b[2]}; }
inline Vec3 scale(Vec3 a, float s) { return {a[0]*s,a[1]*s,a[2]*s}; }
inline float dot(Vec3 a, Vec3 b) { return a[0]*b[0]+a[1]*b[1]+a[2]*b[2]; }
inline Mat4 multiply(const Mat4& a, const Mat4& b) {
    Mat4 out{};
    for (int col=0; col<4; ++col)
        for (int row=0; row<4; ++row)
            for (int k=0; k<4; ++k) out[col*4+row] += a[k*4+row]*b[col*4+k];
    return out;
}

struct CameraInput {
    float strafe=0, forward=0, vertical=0;
    float lookPointsX=0, lookPointsY=0; // Touch displacement, independent of frame rate.
    float turnRateX=0, turnRateY=0;     // Controller axes, integrated over frame time.
};

class Camera {
public:
    // Degrees per touch point and per second of full controller deflection.
    static constexpr float kTouchDegreesPerPoint = 0.2292f;
    static constexpr float kStickYawDegreesPerSecond = 120.3f;
    static constexpr float kStickPitchDegreesPerSecond = 97.4f;
    static constexpr float kPitchLimit = 85.0f; // Keeps the up vector away from +/-Z.

    Vec3 position{0,0,64};
    Vec3 angles{0,0,0}; // PITCH, YAW, ROLL in degrees.
    float speed=320, farPlane=100000;
    void frameBounds(Vec3 low, Vec3 high) {
        Vec3 center=scale(add(low,high),0.5f);
        float span=std::max({high[0]-low[0],high[1]-low[1],high[2]-low[2],128.0f});
        position=add(center,{-span,0,span*0.6f});
        // Look along +X and down onto the map from the offset above.
        angles={std::atan2(0.6f,1.0f)*(180.0f/3.14159265358979323846f),0,0};
        speed=std::clamp(span*0.2f,80.0f,2000.0f);
        farPlane=std::clamp(span*8.0f,10000.0f,1000000.0f);
    }
    void update(const CameraInput& input, double elapsed) {
        float dt=static_cast<float>(std::clamp(elapsed,0.0,0.05));
        angles[YAW] -= input.lookPointsX*kTouchDegreesPerPoint
            + input.turnRateX*kStickYawDegreesPerSecond*dt;
        angles[YAW]=std::remainder(angles[YAW],360.0f);
        angles[PITCH]=std::clamp(angles[PITCH]+input.lookPointsY*kTouchDegreesPerPoint
            - input.turnRateY*kStickPitchDegreesPerSecond*dt,-kPitchLimit,kPitchLimit);
        // Movement stays on the map's ground plane; vertical is the flight control.
        const Vec3 heading{0,angles[YAW],0};
        float forward[3], right[3];
        AngleVectors(heading.data(),forward,right,nullptr);
        Vec3 step=add(scale({forward[0],forward[1],forward[2]},input.forward),
                      scale({right[0],right[1],right[2]},input.strafe));
        step[2]=input.vertical;
        float length=std::sqrt(dot(step,step));
        if(length>1) step=scale(step,1/length);
        position=add(position,scale(step,speed*dt));
    }
    Mat4 viewProjection(float aspect) const {
        float forward[3], right[3], up[3];
        AngleVectors(angles.data(),forward,right,up);
        Vec3 f{forward[0],forward[1],forward[2]};
        Vec3 r{right[0],right[1],right[2]};
        Vec3 u{up[0],up[1],up[2]};
        Mat4 view{r[0],u[0],-f[0],0,
                  r[1],u[1],-f[1],0,
                  r[2],u[2],-f[2],0,
                  -dot(r,position),-dot(u,position),dot(f,position),1};
        constexpr float nearPlane=1.0f;
        constexpr float cotHalfFov=1.42814800674f; // Vertical FOV 70 degrees.
        float z=farPlane/(nearPlane-farPlane);
        Mat4 projection{cotHalfFov/std::max(aspect,0.01f),0,0,0,
                        0,cotHalfFov,0,0, 0,0,z,-1,
                        0,0,nearPlane*z,0};
        return multiply(projection,view);
    }
};
}
