#include "jump_motion.hpp"
#include "original_float.hpp"
#include "terrain_contact_math.hpp"
#include <algorithm>
#include <bit>
#include <cfenv>
#include <cmath>
#include <stdexcept>
#pragma STDC FENV_ACCESS ON
namespace ssx {
namespace {
using V=std::array<float,3>;
float A(float x,float y){return originalScalarAdd(x,y);}
float S(float x,float y){return originalScalarSubtract(x,y);}
constexpr float f(uint32_t b){return std::bit_cast<float>(b);}
using Round=OriginalRounding;
V add(V a,V b){for(int i=0;i<3;i++)a[i]=terrain_original::add(a[i],b[i]);return a;}
V sub(V a,V b){for(int i=0;i<3;i++)a[i]=terrain_original::sub(a[i],b[i]);return a;}
V mul(V a,float b){for(float& x:a)x=terrain_original::mul(x,b);return a;}
// VU0 forms (docs/ps2-float.md "The VU0 forms"): the horizontal dot's z product goes through 1.0 x (vmaddaz.x with the 1.0 vector,
// 0x1142EC); the crosses' subtracted products have the second vector as vopmsub's fs (0x114C08).
float dot(V a,V b){float x=terrain_original::mul(a[0],b[0]),y=terrain_original::mul(a[1],b[1]),z=terrain_original::mul(a[2],b[2]);return terrain_original::add((terrain_original::add(x,y)),terrain_original::mul(1.f,z));}
float len(V a){return terrain_original::sqrt(dot(a,a));}
V unit(V a){return mul(a,terrain_original::div(1.f,len(a)));}
}
float originalJumpChargeStep(float current,bool held){
    Round round;
    float target=held?1.f:0.f;
    float difference=std::abs(S(target,current));
    float rate=difference>=f(0x3dcccccdu)?terrain_original::mul(difference,f(0x3dcccdc2u)):f(0x3c23d7cfu);
    if(A(target,rate)<current)return S(current,rate);
    if(current<S(target,rate))return A(current,rate);
    return target;
}
float originalJumpCameraLaunch(V velocity,V normal){
    Round round;
    //114B78..114C6C: normalized (world up x velocity), then cross with contact normal.
    // up x velocity on VU0 (0x114B8C / 0x114B90, up = (0,0,1)): the y lane is vopmula's 1.0 (fs) x velocity.x, one ULP low on the console.
    V side={-velocity[1],terrain_original::mul(1.f,velocity[0]),0};const float magnitude=len(side);
    if(!(0.f<magnitude))return 0;
    side=mul(side,originalScalarDivide(1.f,magnitude));
    V direction={terrain_original::sub(terrain_original::mul(side[1],normal[2]),terrain_original::mul(normal[1],side[2])),terrain_original::sub(terrain_original::mul(side[2],normal[0]),terrain_original::mul(normal[2],side[0])),terrain_original::sub(terrain_original::mul(side[0],normal[1]),terrain_original::mul(normal[0],side[1]))};
    return terrain_original::mul(dot(velocity,normal),len(direction));
}
void originalJumpTakeoff(OriginalJumpState& s){
    Round round;
    V old=s.velocity;
    float speed=len(old);
    float impulse;
    s.groundLeaveSentinel=s.charge<0;s.rampTakeoff=false;
    if(s.charge<0)impulse=f(0x3e2751e5u);
    else {
        float base=speed<f(0x44730b93u)?A(terrain_original::mul(speed,f(0x3eb6dbeeu)),f(0x43b49a00u)):f(0x44311a83u);
        impulse=terrain_original::mul((terrain_original::mul(s.charge,s.charge)),base);
        impulse=std::clamp(impulse,f(0x43b49a00u),f(0x44311a83u));
    }
    // Fixed original cosine values at 50/70 degrees. The source polynomial
    // results are supplied as constants to avoid a host libm dependency.
#if SSX_PS2_EXACT_FPU
    // The PS2 evaluates them each time (the cosine polynomial at 0x31C0A0..0x31C0CC); on the console model it gives
    // 0x3F248DB9 / 0x3EAF1D3E (exact-base air-tricks, PS2 pass 383: 0x1143DC / 0x114404).
    const bool exact=software_float::exactArithmetic;
    const float c50=f(exact?0x3f248db9u:0x3f248dbau),c70=f(exact?0x3eaf1d3eu:0x3eaf1d41u);
#else
    const float c50=f(0x3f248dbau),c70=f(0x3eaf1d41u);
#endif
    if(s.motionMode==4)s.velocity=add(s.velocity,mul(s.boardUp,impulse));
    else {
        V direction=unit(add(s.normal,mul(s.forward,f(0x3e4ccccdu))));
        float slope=terrain_original::mul(S(s.normal[2],c50),originalScalarDivide(1.f,S(c70,c50)));
        if(s.forward[2]<0 || slope<0 || s.velocity[2]<0)s.velocity=add(s.velocity,mul(direction,impulse));
        else {
            V horizontalNormal=unit(V{s.normal[0],s.normal[1],0});
            V projectedForward=unit(sub(s.forward,mul(horizontalNormal,dot(s.forward,horizontalNormal))));
            V projectedVelocity=unit(sub(s.velocity,mul(horizontalNormal,dot(s.velocity,horizontalNormal))));
            // Original sin(0), sin(20 degrees), positive ramp branch.
            s.rampTakeoff=true;
            float blend=std::clamp(terrain_original::mul((terrain_original::mul(slope,projectedForward[2])),originalScalarDivide(1.f,f(0x3eaf1d45u))),0.f,f(0x3f733333u));
            float remain=S(1.f,blend);
            direction=unit(add(mul(direction,remain),mul(projectedForward,blend)));
            s.velocity=add(add(mul(s.velocity,remain),mul(projectedVelocity,terrain_original::mul(blend,speed))),mul(mul(direction,impulse),.5f));
        }
    }
    if(s.normal[2]>A(c70,f(0x3ca3d70au))){
        V delta=sub(s.velocity,old);delta[2]=0;V horizontal=old;horizontal[2]=0;
        if(dot(horizontal,delta)<0){
            float lower=terrain_original::mul(s.speedLimit,.5f),upper=terrain_original::mul(s.speedLimit,f(0x3f333333u));
            float scale=speed<lower?0.f:(speed<=upper?originalScalarDivide(S(speed,lower),S(upper,lower)):1.f);
            s.velocity=sub(s.velocity,mul(delta,scale));
        }
    }
    V horizontal={s.takeoffNormal[0],s.takeoffNormal[1],0};
    if(std::abs(horizontal[0])>0 || std::abs(horizontal[1])>0)horizontal=mul(horizontal,terrain_original::div(1.f,len(horizontal)));
    else horizontal={1,0,0}; // Source 0x4FF140, verified by conformance fixture.
    s.cameraWallNormal=horizontal; //114998 is an unconditional branch delay-slot store.
    if(s.takeoffNormal[2]<f(0x3d4ccccdu)&&s.takeoffNormal[2]>f(0xbd4ccccdu)){
        float projection=originalScalarDivide(dot(s.velocity,horizontal),dot(horizontal,horizontal));
        s.velocity=sub(s.velocity,mul(horizontal,projection));
        s.position=add(s.position,mul(horizontal,4.f));
        if(!(s.flags&0x20))s.velocity=mul(s.velocity,f(0x3f4ccb33u));
    }
    float currentSpeed=len(s.velocity);
    if(currentSpeed>s.speedLimit)s.velocity=mul(s.velocity,originalScalarDivide(s.speedLimit,currentSpeed));
    if(s.charge>=0 && !(s.riderState>=11&&s.riderState<=13)){
        float time=terrain_original::mul(float(s.ticksSinceGroundFocus),f(0x3c888889u)),scale;
        if(time<=f(0x3f09f484u))scale=f(0x3f31a937u);
        else {float mix=std::min(terrain_original::mul((S(time,f(0x3f09f484u))),f(0x3f9dc837u)),1.f);scale=A(mix,terrain_original::mul(S(1.f,mix),f(0x3f31a937u)));}
        s.velocity=mul(s.velocity,scale);
    }
    s.cameraLaunchValue=originalJumpCameraLaunch(s.velocity,s.normal);
}
}
