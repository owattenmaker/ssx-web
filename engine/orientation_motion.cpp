#include "orientation_motion.hpp"
#include "original_float.hpp"
#include "terrain_contact_math.hpp"
#include <bit>
#include <algorithm>
#include <cfenv>
#include <cmath>
#include <stdexcept>
#pragma STDC FENV_ACCESS ON
namespace ssx {
namespace {
constexpr float f(unsigned bits){return std::bit_cast<float>(bits);}
using Round=OriginalRounding;
}
std::array<float,4> originalRotateOrientation(std::array<float,4> q,std::array<float,3> axis,float angle){
    Round round;
    auto sc=originalSinCos(terrain_original::mul(angle,.5f));
    // 0x11E024..0x11E06C: delta = sin x axis (mul.s f3,f0,f3); the cross is vopmsub vf6, vf5 (q), vf4; the w products go through vf0 (1.0)
    std::array<float,4> delta={terrain_original::mul(sc[0],axis[0]),terrain_original::mul(sc[0],axis[1]),terrain_original::mul(sc[0],axis[2]),sc[1]},out;
    for(int i=0;i<3;i++){
        int j=(i+1)%3,k=(i+2)%3;
        float cross=terrain_original::sub(terrain_original::mul(delta[j],q[k]),terrain_original::mul(q[j],delta[k]));
        float weighted=terrain_original::add(terrain_original::mul(delta[i],q[3]),terrain_original::mul(q[i],delta[3]));
        out[i]=terrain_original::add(weighted,cross);
    }
    float product=terrain_original::sub(terrain_original::mul(delta[3],q[3]),terrain_original::mul(delta[0],q[0]));
    product=terrain_original::sub(product,terrain_original::mul(1.f,terrain_original::mul(delta[1],q[1])));
    out[3]=terrain_original::sub(product,terrain_original::mul(1.f,terrain_original::mul(delta[2],q[2])));
    return out;
}
}
namespace ssx {
float originalAsin(float value){
    Round round;
    bool lower=value<f(0xbf3504f3u),upper=value>f(0x3f3504f3u);
    float square=terrain_original::mul(value,value);
    if(lower||upper){square=originalScalarSubtract(1.f,square);value=originalScalarSqrt(square);}
    float polynomial=terrain_original::mul(square,f(0x3d6137abu));polynomial=originalScalarAdd(f(0x3d55033au),polynomial);
    polynomial=terrain_original::mul(square,polynomial);polynomial=originalScalarAdd(f(0x3d8a908bu),polynomial);
    polynomial=terrain_original::mul(square,polynomial);polynomial=originalScalarAdd(f(0x3e2bba25u),polynomial);
    polynomial=terrain_original::mul(square,polynomial);
    if(lower||upper){polynomial=originalScalarAdd(polynomial,1.f);polynomial=terrain_original::mul(value,polynomial);return lower?originalScalarAdd(polynomial,f(0xbfc90fdbu)):originalScalarSubtract(f(0x3fc90fdbu),polynomial);}
    polynomial=terrain_original::mul(polynomial,value);return originalScalarAdd(polynomial,value);
}
std::array<float,4> originalGroundAlignment(std::array<float,4> q,std::array<float,3> n,
    std::array<float,3> up,float clearance,float rate,bool* rotated){
    Round round;
    if(rotated)*rotated=false;
    if(!(clearance<5.f))return q;
    n[2]=terrain_original::add(n[2],terrain_original::mul(n[2],.5f));
    auto length=[](auto a){float x=terrain_original::mul(a[0],a[0]),y=terrain_original::mul(a[1],a[1]),z=terrain_original::mul(a[2],a[2]);return terrain_original::sqrt(terrain_original::add((terrain_original::add(x,y)),terrain_original::mul(1.f,z)));}; // 0x13EDCC / 0x13EE28
    float scale=terrain_original::div(1.f,length(n));for(float& x:n)x=terrain_original::mul(x,scale);
    std::array<float,3> axis;
    for(int i=0;i<3;i++){int j=(i+1)%3,k=(i+2)%3;axis[i]=terrain_original::sub(terrain_original::mul(n[j],up[k]),terrain_original::mul(up[j],n[k]));} // 0x13EE04 vopmsub: b x a
    float sine=length(axis);
    if(!(f(0x3a83126fu)<sine))return q;
    scale=terrain_original::div(1.f,sine);for(float& x:axis)x=terrain_original::mul(x,scale);
    float angle=-originalAsin(std::min(sine,1.f));angle=terrain_original::mul(angle,f(0x3c888889u));angle=terrain_original::mul(angle,rate);
    if(rotated)*rotated=true;
    return originalRotateOrientation(q,axis,angle);
}
}
namespace ssx {
namespace {
using V=std::array<float,3>;
// 0x13E320: vadda x + y, then vmadda 1.0 x z. 0x13E53C crossV: vopmsub's fs is the second vector, so the subtracted product is b x a.
float dotV(V a,V b){float x=terrain_original::mul(a[0],b[0]),y=terrain_original::mul(a[1],b[1]),z=terrain_original::mul(a[2],b[2]);return terrain_original::add((terrain_original::add(x,y)),terrain_original::mul(1.f,z));}
V crossV(V a,V b){return {terrain_original::sub(terrain_original::mul(a[1],b[2]),terrain_original::mul(b[1],a[2])),terrain_original::sub(terrain_original::mul(a[2],b[0]),terrain_original::mul(b[2],a[0])),terrain_original::sub(terrain_original::mul(a[0],b[1]),terrain_original::mul(b[0],a[1]))};}
V scaleV(V a,float x){for(float& v:a)v=terrain_original::mul(v,x);return a;}
float A(float x,float y){return originalScalarAdd(x,y);}
float S(float x,float y){return originalScalarSubtract(x,y);}
float curveV(float x,const GroundCurve& c){
    unsigned i=0;
    if(c[1].x<x){i=1;if(c[2].x<x){i=2;if(c[3].x<x)return c[3].y;}}
    else if(x<c[0].x)return c[0].y;
    float distance=S(x,c[i].x),span=S(c[i+1].x,c[i].x);
    float rise=S(c[i+1].y,c[i].y);rise=terrain_original::mul(rise,distance);rise=originalScalarDivide(rise,span);
    return A(c[i].y,rise);
}
float atan2V(float y,float x){
    if(x==0){if(y==0)return y;return y>=0?f(0x3fc90fdbu):f(0xbfc90fdbu);}
    float result=originalAtan(originalScalarDivide(y,x));
    if(x<0)result=y>0?A(result,f(0x40490fdbu)):S(result,f(0x40490fdbu));
    return result;
}
}
float originalGroundHeading(const OriginalHeadingProfile& p,OriginalHeadingState& s){
    Round round;
    auto relative=s.relativeVelocity;
    float speed=terrain_original::sqrt(dotV(relative,relative)),angle=0;
    float turn=-s.turn;
    if((s.controlState!=2||s.normal[2]>f(0x3f266666u))&&speed>f(0x3727c5acu)){
        V cross=scaleV(crossV(relative,s.forward),terrain_original::div(1.f,speed));
        float desired=-originalAsin(std::clamp(dotV(cross,s.normal),-1.f,1.f));
        float kmh=terrain_original::mul(speed,f(0x3d1374bcu));
        float gainCurve=curveV(kmh,p.crouchTurnCurve);
        float bias=S(1.f,terrain_original::mul(turn,turn));bias=terrain_original::mul(bias,f(0x3ecccccdu));bias=terrain_original::mul(p.surface30,bias);bias=A(bias,f(0x3f8ccccdu));
        float gain=terrain_original::mul(gainCurve,f(0x3c8efa36u));bias=terrain_original::mul(bias,turn);bias=terrain_original::mul(bias,gain);
        float denominator=terrain_original::mul(s.charge,f(0x3f7edd8du));denominator=A(denominator,1.f);bias=originalScalarDivide(bias,denominator);
        bias=terrain_original::mul(bias,curveV(kmh,p.crouchSpeedCurve));
        desired=A(desired,bias);if(dotV(relative,s.forward)<0)desired=-desired;
        float response=terrain_original::mul(p.surface34,(terrain_original::mul(speed,f(0x358637bdu))));response=terrain_original::mul(response,speed);response=terrain_original::mul(response,s.dt);response=std::min(response,1.f);
        V across=crossV(s.normal,{0,0,1});float acrossLength=terrain_original::sqrt(dotV(across,across));
        V downhill=s.bodyForward;
        if(acrossLength>f(0x3a83126fu))downhill=crossV(s.normal,scaleV(across,terrain_original::div(1.f,acrossLength)));
        float ratio=originalScalarDivide(std::abs(dotV(relative,downhill)),speed);
        float coefficient=std::min(originalScalarDivide(speed,f(0x440ae38eu)),1.f);coefficient=terrain_original::mul(coefficient,S(.5f,ratio));
        coefficient=terrain_original::mul(coefficient,A(p.surface28,p.surface28));coefficient=std::max(coefficient,0.f);
        coefficient=A(p.surface2C,coefficient);coefficient=std::min(coefficient,1.f);
        if(desired<=0){
            float limit=terrain_original::mul((-turn),f(0x3f666666u));
            response=limit>=coefficient?terrain_original::mul((terrain_original::mul(response,f(0x3f666666u))),(-turn)):terrain_original::mul(response,coefficient);
        }else {
            float limit=terrain_original::mul(turn,f(0x3f666666u));
            response=limit>=coefficient?terrain_original::mul((terrain_original::mul(response,f(0x3f666666u))),turn):terrain_original::mul(response,coefficient);
        }
        if(s.manualSpin!=0)response=0;
        float limit=terrain_original::mul(s.dt,f(0x40c90fdcu));
        angle=std::clamp(terrain_original::mul(desired,response),-limit,limit);
    }
    if(s.manualSpin!=0){
        V projected=relative;V normalPart=scaleV(s.normal,dotV(relative,s.normal));
        for(int i=0;i<3;i++)projected[i]=terrain_original::sub(projected[i],normalPart[i]);
        float x=dotV(projected,s.forward),y=dotV(projected,s.lateral);
        if(s.reverseStance){x=-x;y=-y;}
        float theta=atan2V(y,x);
        if(terrain_original::mul(theta,s.manualSpin)>0){
            theta=S(f(0x40490fdbu),theta);
            float scaled=terrain_original::mul(theta,f(0x3e22f983u));scaled=A(scaled,.5f);
            float whole=float(int(scaled));if(scaled<whole)whole=S(whole,1.f);
            theta=S(theta,terrain_original::mul(whole,f(0x40c90fdbu)));
        }else theta=-theta;
        constexpr float rate=f(0x41033fb0u);
        if(std::abs(s.manualSpin)>f(0x40490fdbu)){
            theta=A(theta,(theta<0?f(0xc0490fdbu):f(0x40490fdbu)));
            float step=terrain_original::mul(rate,s.dt);
            if(step<s.manualSpin)s.manualSpin=S(s.manualSpin,step);
            else if(s.manualSpin<-step)s.manualSpin=A(s.manualSpin,step);
            else s.manualSpin=0;
        }
        if(std::abs(theta)<f(0x3e860a93u))s.manualSpin=0;
        else {
            float delta=terrain_original::mul(theta,originalScalarDivide(rate,f(0x3f490fdcu)));delta=std::clamp(delta,-rate,rate);delta=terrain_original::mul((-s.dt),delta);
            if(delta<0&&angle<0)angle=std::min(angle,delta);
            else if(delta>0&&angle>0)angle=std::max(angle,delta);
            else angle=A(angle,delta);
        }
    }
    float facing=0;if(speed>0)facing=originalScalarDivide(dotV(relative,s.bodyForward),speed);
    facing=std::max(S(facing,f(0x3dcccccdu)),0.f);
    float steering=terrain_original::mul(facing,s.turn);steering=terrain_original::mul(steering,curveV(terrain_original::mul(speed,f(0x3d1374bcu)),p.directSteerCurve));
    return S(angle,steering);
}
}
namespace ssx {
OriginalPhysicalOrientation originalOrientationBasis(std::array<float,4> q){
    Round round;
    V doubled={terrain_original::add(q[0],q[0]),terrain_original::add(q[1],q[1]),terrain_original::add(q[2],q[2])};
    V diagonal={terrain_original::mul(doubled[0],q[0]),terrain_original::mul(doubled[1],q[1]),terrain_original::mul(doubled[2],q[2])};
    V weighted={terrain_original::mul(doubled[0],q[3]),terrain_original::mul(doubled[1],q[3]),terrain_original::mul(doubled[2],q[3])};
    V mixed={terrain_original::mul(doubled[1],q[2]),terrain_original::mul(doubled[2],q[0]),terrain_original::mul(doubled[0],q[1])};
    // 0x11E0CC..0x11E134: every row element is an ACC stage then an FMAC whose fs is one = vf0 + vf0.w (1.0), so the second term goes
    // through mul(1, x); the cross terms start from 0 + M (vadda with vf0 = +0, turning -0 into +0). Identical in mode-1 arithmetic
    // except for that -0, which the PS2 also makes +0 (docs/ps2-float.md).
    auto one=[](float x){return terrain_original::mul(1.f,x);};
    auto zero=[](float x){return terrain_original::add(0.f,x);};
    return {q,
        {terrain_original::sub(terrain_original::sub(1.f,diagonal[1]),one(diagonal[2])),terrain_original::add(zero(mixed[2]),one(weighted[2])),terrain_original::sub(zero(mixed[1]),one(weighted[1]))},
        {terrain_original::sub(zero(mixed[2]),one(weighted[2])),terrain_original::sub(terrain_original::sub(1.f,diagonal[2]),one(diagonal[0])),terrain_original::add(zero(mixed[0]),one(weighted[0]))},
        {terrain_original::add(zero(mixed[1]),one(weighted[1])),terrain_original::sub(zero(mixed[0]),one(weighted[0])),terrain_original::sub(terrain_original::sub(1.f,diagonal[0]),one(diagonal[1]))}};
}
OriginalPhysicalOrientation originalRebuildOrientation(std::array<float,4> q){
    Round round;
    std::array<float,4> squared;for(int i=0;i<4;i++)squared[i]=terrain_original::mul(q[i],q[i]);
    // 11E098: ACC.x = S.x + S.y, then ACC += one.x * S.z, R = ACC + one.x * S.w (one.x = vf0.x + vf0.w as the fs operand of the
    // products: identity in mode-1 arithmetic, one ULP low on the console for some S; docs/ps2-float.md)
    float sum=terrain_original::add(squared[0],squared[1]);sum=terrain_original::add(sum,terrain_original::mul(1.f,squared[2]));sum=terrain_original::add(sum,terrain_original::mul(1.f,squared[3]));
    float inverse=terrain_original::div(1.f,terrain_original::sqrt(sum));for(float& x:q)x=terrain_original::mul(x,inverse);
    return originalOrientationBasis(q);
}
}
