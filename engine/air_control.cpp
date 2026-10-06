#include "air_control.hpp"
#include "ground_motion.hpp"
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
float A(float x,float y){return originalScalarAdd(x,y);}
float S(float x,float y){return originalScalarSubtract(x,y);}
constexpr float f(unsigned b){return std::bit_cast<float>(b);}
using Round=OriginalRounding;
float presentationBlend(float spinRate,float flipRate);
float approach(float current,float target,float rate){if(A(target,rate)<current)return S(current,rate);if(current<S(target,rate))return A(current,rate);return target;}
float atan2Original(float y,float x){
    if(x==0){if(y==0)return y;return y>=0?f(0x3fc90fdbu):f(0xbfc90fdbu);}
    float a=originalAtan(originalScalarDivide(y,x));if(x<0)a=y>0?A(a,f(0x40490fdbu)):S(a,f(0x40490fdbu));return a;
}
constexpr float pi=f(0x40490fdcu),tau=f(0x40c90fdcu),dt=f(0x3c888889u),snap=f(0x3f490fdcu);
}
OriginalAirControlState originalAirControlBegin(float prewindSpin,float prewindFlip){
    Round round;OriginalAirControlState s;
    s.maxSpin=s.maxFlip=f(0x408ffd3au);s.inputAngle=f(0x7149f2cau);
    if(prewindSpin!=0||prewindFlip!=0){
        s.phase=0;s.mode=1;
        float amount=std::max(std::abs(prewindSpin),std::abs(prewindFlip));
        if(prewindSpin!=0)s.maxSpin=s.spinRate=std::max(terrain_original::mul(amount,f(0x40e00327u)),f(0x408ffd3au));
        if(prewindFlip!=0){float speed=std::max(terrain_original::mul(amount,f(0x40e00327u)),f(0x408ffd3au));s.maxFlip=s.flipRate=terrain_original::mul(speed,f(0x3f2aaaabu));}
        s.axisBlend=presentationBlend(s.spinRate,s.flipRate);
        s.targetSpin=prewindSpin>0?pi:prewindSpin<0?-pi:0;s.targetFlip=prewindFlip>0?tau:prewindFlip<0?-tau:0;
    }
    return s;
}
std::array<float,2> originalAirDirection(float flip,float spin,float step,float deadzone){
    Round round;if(std::abs(spin)<=deadzone)spin=0;if(std::abs(flip)<=deadzone)flip=0;
    float norm=originalScalarSqrt(A(terrain_original::mul(spin,spin),terrain_original::mul(flip,flip)));if(norm==0)return {flip,spin};
    float angle=atan2Original(flip,spin),scaled=originalScalarDivide(angle,step);
    scaled=A(scaled,(angle<0?-.5f:.5f));
    auto sc=originalSinCos(terrain_original::mul(float(int(scaled)),step));
    spin=std::clamp(terrain_original::mul(sc[1],norm),-1.f,1.f);flip=std::clamp(terrain_original::mul(sc[0],norm),-1.f,1.f);
    if(std::abs(spin)<f(0x3a83126fu))spin=0;if(std::abs(flip)<f(0x3a83126fu))flip=0;
    return {flip,spin};
}
void originalAirControlStep(OriginalAirControlState& s,const RiderInput& in,const OriginalAirControlProfile& p,OriginalAirControlFrame* output){
    const int phaseBefore=s.phase;bool trickStarted=false;
    Round round;
    if((in.grabMask&&!p.grabLifecycleResolved)||in.handplant)throw std::runtime_error("Air-control grab/handplant interruption not recovered");
    float flip=in.flip,spin=in.spin,adjFlip=in.airAdjustFB,adjSpin=in.airAdjustLR;
    // 0x133448..0x1334DC: command-prefix modes. Mode 0 + LateSpin (word0 bit15, INPUT2.MAP "Pro": Cross held) with an
    // air-adjust input enters late-spin mode 2; a spin/flip input clears any mode; releasing LateSpin turns 2 into 1.
    // Modes 1 and 2 (0x1334E4: mode-1 < 2) then read the air-adjust axes as spin/flip.
    if(s.mode==0){if(in.lateSpin&&(adjSpin!=0||adjFlip!=0))s.mode=2;}
    else if(spin!=0||flip!=0)s.mode=0;
    if(s.mode==2&&!in.lateSpin)s.mode=1;
    if(s.mode==1||s.mode==2){
        if(adjSpin==0&&adjFlip==0&&s.idleTime>f(0x3e99999au))s.mode=0;
        if(spin==0)spin=adjSpin;if(flip==0)flip=adjFlip;
        adjFlip=adjSpin=0;
    }
    if(!p.grabActive&&s.mode==3)s.mode=0; // Completed grab release in the explicit no-grab context.
    float flipAdjustLimit=f(0x3fdf66f4u);
    if(in.boardPress!=0){adjFlip=in.boardPress;flipAdjustLimit=f(0x3eb2b8c4u);}
    if(p.grabActive){adjFlip=adjSpin=0;if(s.mode==0)s.mode=3;}
    const bool switched=!p.grabActive&&s.phase==3&&!p.landingAnimation&&p.airSwitch&&p.airSwitch(s); //0x133694 -> 0x133750
    if(!switched&&!p.grabActive&&s.phase==3&&!p.landingAnimation&&(flip!=0||spin!=0)){
        auto direction=originalAirDirection(flip,spin,snap,0);flip=direction[0];spin=direction[1];
        s.targetSpin=spin>0?pi:spin<0?-pi:0;s.targetFlip=flip>0?tau:flip<0?-tau:0;
        s.progressSpin=A(s.progressSpin,s.adjustSpin);s.progressFlip=A(s.progressFlip,s.adjustFlip);
        s.totalSpin=A(s.totalSpin,s.adjustSpin);s.totalFlip=A(s.totalFlip,s.adjustFlip);
        s.adjustSpin=s.adjustFlip=0;s.scoredSpin=s.scoredFlip=0;
        s.maxSpin=s.spinRate=f(0x408ffd3au);s.maxFlip=s.flipRate=f(0x403ffc4eu);s.phase=0;trickStarted=true;
    }
    if(s.phase==0){
        if(s.targetSpin==0&&s.targetFlip==0){flip=spin=0;if(s.progressSpin==0&&s.progressFlip==0)s.phase=3;}
        else if(std::abs(S(s.progressSpin,s.targetSpin))<S(pi,f(0x3db2b8c4u))&&std::abs(S(s.progressFlip,s.targetFlip))<S(tau,f(0x3db2b8c4u)))s.phase=1;
        else if(flip==0&&spin==0)s.targetFlip=s.targetSpin=0;
    }else if(s.phase==1){
        if(flip==0&&spin==0){
            float targetSpin=s.targetSpin,targetFlip=s.targetFlip;
            s.idleTime=A(s.idleTime,dt);
            if(std::abs(S(targetSpin,s.progressSpin))>pi)targetSpin=A(targetSpin,(targetSpin<0?pi:-pi));
            if(std::abs(S(targetFlip,s.progressFlip))>tau)targetFlip=A(targetFlip,(targetFlip<0?tau:-tau));
            float tolerance=0;
            if(s.idleTime>f(0x3e99999au)){s.targetSpin=targetSpin;s.targetFlip=targetFlip;tolerance=snap;}
            if(std::abs(S(targetSpin,s.progressSpin))<=tolerance&&std::abs(S(targetFlip,s.progressFlip))<=tolerance)s.phase=2;
        }else {
            s.idleTime=0;s.holdSpin=spin==0?0:A(s.holdSpin,dt);s.holdFlip=flip==0?0:A(s.holdFlip,dt);
            if(std::abs(S(s.progressSpin,s.targetSpin))<=snap&&s.holdSpin>(s.targetSpin==0?f(0x3e4ccccdu):0)){
                if(s.targetSpin==0){s.targetSpin=spin<0?-pi:pi;s.extended=1;}
                else s.targetSpin=A(s.targetSpin,(s.targetSpin<0?-pi:pi));
                if(std::abs(s.targetSpin)>tau)s.maxSpin=std::max(terrain_original::mul(s.maxSpin,f(0x3f4cc9a3u)),f(0x403ff8c8u));
            }
            if(std::abs(S(s.progressFlip,s.targetFlip))<=snap&&s.holdFlip>(s.targetFlip==0?f(0x3e4ccccdu):0)){
                if(s.targetFlip==0){s.targetFlip=flip<0?-tau:tau;s.extended=1;}
                else s.targetFlip=A(s.targetFlip,(s.targetFlip<0?-tau:tau));
                if(std::abs(s.targetFlip)>tau)s.maxFlip=std::max(terrain_original::mul(s.maxFlip,f(0x3f23d1fbu)),f(0x403ff8c8u));
            }
            if(p.grabActive)s.extended=1;
        }
    }else if(s.phase==2){
        s.mode=0;spin=flip=0;
        if(s.progressSpin==s.targetSpin&&s.progressFlip==s.targetFlip){
            s.phase=3;s.progressSpin=s.targetSpin=s.progressFlip=s.targetFlip=0;s.extended=0;
        }else if(!p.grabActive&&!p.landingAnimation){if(p.airSwitch)p.airSwitch(s);} //0x133BE4, result ignored
        else if(s.progressFlip!=s.targetFlip&&
            ((s.progressFlip<0&&adjFlip>0)||(s.progressFlip>0&&adjFlip<0)))adjFlip=-adjFlip;
    }
    float reduction=originalScalarDivide(1.f,A(terrain_original::mul(p.trickStat,f(0x3eb2f114u)),1.f)),increase=A(terrain_original::mul(p.trickStat,f(0x3f0010fcu)),1.f);
    s.inputAngle=flip==0&&spin==0?f(0x7149f2cau):atan2Original(flip,spin);
    auto rate=[&](float current,float target,float input,float other,float maximum){
        if(current==target)return 0.f;
        float result;
        if(terrain_original::mul(input,target)<0)return std::min(maximum,terrain_original::mul(reduction,f(0x40000229u)));
        else if(input!=0)result=maximum;
        else {
            float difference=S(current,target);
            if(other==0){
                float blend=terrain_original::mul(std::abs(difference),f(0x3e22f982u));
                float complement=S(1.f,blend);
                float value=A(terrain_original::mul(blend,f(0x409ffff0u)),terrain_original::mul(complement,f(0x40f00234u)));
                maximum=std::max(value,maximum);
            }
            float blend=originalScalarDivide(std::abs(difference),snap);
            result=blend<=1.f?terrain_original::mul(maximum,A(terrain_original::mul(blend,f(0x3f4ccccdu)),f(0x3e4cccccu))):maximum;
        }
        result=terrain_original::mul(result,increase);if(p.boostModifier)result=terrain_original::mul(result,f(0x3ffff7d1u));return result;
    };
    s.spinRate=rate(s.progressSpin,s.targetSpin,spin,flip,s.maxSpin);
    s.flipRate=rate(s.progressFlip,s.targetFlip,flip,spin,s.maxFlip);
    if(std::abs(adjSpin)<f(0x3e4ccccdu))adjSpin=0;if(std::abs(adjFlip)<f(0x3e4ccccdu))adjFlip=0;
    float beforeSpin=s.progressSpin,beforeFlip=s.progressFlip;
    auto adjust=[&](float current,float input,float limit,float denominator,float coefficient){
        float target=input<0?-limit:input>0?limit:current;
        if(coefficient==f(0x3d4cd331u)&&target>0)target=std::min(snap,target);
        float speed=terrain_original::mul(std::abs(S(target,current)),10.f);speed=originalScalarDivide(speed,denominator);speed=std::clamp(speed,f(0x3dcccccdu),1.f);
        float amount=A(terrain_original::mul(std::abs(input),.5f),.5f);float step=terrain_original::mul(speed,coefficient);step=terrain_original::mul(amount,step);
        return approach(current,target,step);
    };
    float previous=s.adjustSpin;
    s.adjustSpin=adjust(previous,adjSpin,f(0x3fdf66f4u),f(0x3fdf66f4u),f(0x3d4cc9c7u));
    if(s.progressSpin<s.targetSpin)s.progressSpin=std::min(s.targetSpin,S(A(s.progressSpin,s.adjustSpin),previous));
    else if(s.targetSpin<s.progressSpin)s.progressSpin=std::max(s.targetSpin,S(A(s.progressSpin,s.adjustSpin),previous));
    float progress=approach(s.progressSpin,s.targetSpin,terrain_original::mul(s.spinRate,dt));
    s.totalSpin=A(s.totalSpin,S(progress,s.progressSpin));s.progressSpin=progress;
    previous=s.adjustFlip;
    s.adjustFlip=adjust(previous,adjFlip,flipAdjustLimit,f(0x3fdf66f4u),f(0x3d4cd331u));
    if(s.progressFlip<s.targetFlip)s.progressFlip=std::min(s.targetFlip,S(A(s.progressFlip,s.adjustFlip),previous));
    else if(s.targetFlip<s.progressFlip)s.progressFlip=std::max(s.targetFlip,S(A(s.progressFlip,s.adjustFlip),previous));
    progress=approach(s.progressFlip,s.targetFlip,terrain_original::mul(s.flipRate,dt));
    s.totalFlip=A(s.totalFlip,S(progress,s.progressFlip));s.progressFlip=progress;
    s.scoredSpin=A(s.scoredSpin,S(s.progressSpin,beforeSpin));s.scoredFlip=A(s.scoredFlip,S(s.progressFlip,beforeFlip));
    if(output)*output={phaseBefore,spin,flip,trickStarted};
}
}
namespace ssx {
namespace {
using V3=std::array<float,3>;using Q4=std::array<float,4>;
// VU0 forms of 134DD0 (docs/ps2-float.md "The VU0 forms"): vopmsub's fs is b (0x134EB4, 0x134F44, 0x1350D0); the w lane's y / z
// terms go through vmsubay.w / vmsubz.w with vf0 (1.0) as fs (0x134F54 / 0x134F58); the flip axis length is the horizontal dot
// (vmaddaz.x with the 1.0 vector, 0x135054) and its scale by the half-angle sine has the sine as mul.s's fs (0x135088).
V3 cross(V3 a,V3 b){return {terrain_original::sub(terrain_original::mul(a[1],b[2]),terrain_original::mul(b[1],a[2])),terrain_original::sub(terrain_original::mul(a[2],b[0]),terrain_original::mul(b[2],a[0])),terrain_original::sub(terrain_original::mul(a[0],b[1]),terrain_original::mul(b[0],a[1]))};}
Q4 multiply(Q4 a,Q4 b){
    V3 c=cross({a[0],a[1],a[2]},{b[0],b[1],b[2]});Q4 out;
    for(int i=0;i<3;i++){float x=terrain_original::add(terrain_original::mul(a[i],b[3]),terrain_original::mul(b[i],a[3]));out[i]=terrain_original::add(x,c[i]);}
    float w=terrain_original::sub(terrain_original::mul(a[3],b[3]),terrain_original::mul(a[0],b[0]));w=terrain_original::sub(w,terrain_original::mul(1.f,terrain_original::mul(a[1],b[1])));out[3]=terrain_original::sub(w,terrain_original::mul(1.f,terrain_original::mul(a[2],b[2])));return out;
}
V3 rotate(Q4 q,V3 v){
    V3 u={q[0],q[1],q[2]},a=cross(u,v),b=cross(u,a),out;
    for(int i=0;i<3;i++){float x=terrain_original::add(v[i],terrain_original::mul(a[i],q[3]));x=terrain_original::add(x,terrain_original::mul(a[i],q[3]));x=terrain_original::add(x,b[i]);out[i]=terrain_original::add(x,b[i]);}return out;
}
float presentationBlend(float spinRate,float flipRate){
    float x=std::abs(spinRate),y=std::abs(flipRate);
    float speed=originalScalarSqrt(A(terrain_original::mul(spinRate,spinRate),terrain_original::mul(flipRate,flipRate)));speed=terrain_original::mul(speed,f(0x3e124716u));
    float angle=atan2Original(y,x);
    float blend=std::clamp(S(speed,f(0x3e99999au)),0.f,f(0x3f333333u));
    float weight=originalScalarDivide(std::abs(S(angle,f(0x3f490fdcu))),f(0x3f490fdcu));
    blend=terrain_original::mul(blend,f(0x3fb6db6eu));return terrain_original::mul(blend,S(1.f,weight));
}
}
static OriginalAirPresentation presentationImpl(OriginalAirControlState& s,OriginalAirPresentation p,V3 pivot,bool advanceBlend){
    Round round;
    auto offset=rotate(p.quaternion,pivot);for(int i=0;i<3;i++)p.position[i]=terrain_original::add(p.position[i],offset[i]);
    float spin=-(A(s.totalSpin,s.adjustSpin)),flip=A(s.totalFlip,s.adjustFlip);
    auto spinSC=originalSinCos(terrain_original::mul(spin,.5f));
    p.quaternion=multiply(p.quaternion,{0,0,spinSC[0],spinSC[1]});
    float target=presentationBlend(s.spinRate,s.flipRate);
    float rate=terrain_original::mul(std::abs(S(s.axisBlend,target)),f(0x3d088889u));if(advanceBlend)s.axisBlend=approach(s.axisBlend,target,rate);
    // 0x134FDC / 0x13501C: the axis is built from (0,1,0) x blend and (1,0,0) x (1 - blend) on VU0, 1.0 as fs (one ULP low on the
    // console); the PS2's x lane is 0 - that (+0 for blend 1, where this negation gives -0: kept for mode-1 bits).
    V3 axis={-terrain_original::mul(1.f,S(1.f,s.axisBlend)),terrain_original::mul(1.f,s.axisBlend),0};
    float length=terrain_original::sqrt(terrain_original::add((terrain_original::add(terrain_original::mul(axis[0],axis[0]),terrain_original::mul(axis[1],axis[1]))),terrain_original::mul(1.f,terrain_original::mul(axis[2],axis[2]))));
    float inverse=terrain_original::div(1.f,length);for(float& v:axis)v=terrain_original::mul(v,inverse);
    auto flipSC=originalSinCos(terrain_original::mul(flip,.5f));
    p.quaternion=multiply(p.quaternion,{terrain_original::mul(flipSC[0],axis[0]),terrain_original::mul(flipSC[0],axis[1]),terrain_original::mul(flipSC[0],axis[2]),flipSC[1]});
    for(float& v:pivot)v=terrain_original::mul(v,-1.f);
    offset=rotate(p.quaternion,pivot);for(int i=0;i<3;i++)p.position[i]=terrain_original::add(p.position[i],offset[i]);
    return p;
}
OriginalAirPresentation originalAirPresentation(OriginalAirControlState& s,OriginalAirPresentation p,V3 pivot){return presentationImpl(s,p,pivot,true);}
OriginalAirPresentation originalAirPresentationCurrent(const OriginalAirControlState& s,OriginalAirPresentation p,V3 pivot){auto copy=s;return presentationImpl(copy,p,pivot,false);}

}
