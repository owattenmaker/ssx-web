#pragma once
// UVScroll modifier (builtin 21, 0x2FE2C0; "UVScroll" 0x48E940): per-tick texture
// coordinate offset of an entity's model. Development recovery.
//  * 0x2FE2C0 fills a 0x34-byte argument block over its defaults (key k at +4k:
//    k1 mode 5, k2/k3 start u/v 0, k4/k5 u/v step 1/60 (0x3C88889A), k6 on-time 1,
//    k7 off-time 0, k8 spin (per second), k9..k11 spin axis 0, key 12 byte) and
//    attaches a 0x50-byte object (ctor 0x35F6E8) to the entity's modifier
//    container +4 (0x355B90 -> 0x355550).
//  * Entity tick 0x355714 -> container tick 0x352C70 -> 0x35F7D0 once per game
//    tick while the entity exists (Object entities built by the same stage
//    slot-1 program when the instance's section activates).
//  * Draw: 0x35FC20 builds the texture matrix: identity (or a rotation by the
//    accumulated spin about the axis) with translation row (u, v): uv' = uv + (u, v).
// Arithmetic: EE scalar FPU (MUL chop, ADD/SUB guard bit, DIV nearest) under OriginalRounding.
#include <array>
#include <bit>
#include <cstdint>
#include "original_float.hpp"
#include "terrain_contact_math.hpp"

namespace ssx {

// Argument block of builtin 21 (0x34 bytes, key k at +4k). Types (0x446238):
// keys 0,1 int, 2..11 float, 12 int.
struct OriginalUvScrollArguments {
    std::array<uint32_t,13> words{};
    static OriginalUvScrollArguments defaults(){
        OriginalUvScrollArguments a;auto f=[&](unsigned k,float v){a.words[k]=std::bit_cast<uint32_t>(v);};
        const float step=std::bit_cast<float>(0x3C88889Au);
        a.words[0]=0xFFFFFFFFu;a.words[1]=5;f(2,0);f(3,0);f(4,step);f(5,step);f(6,1);f(7,0);f(8,0);f(9,0);f(10,0);f(11,1);a.words[12]=0;
        return a;
    }
    float real(unsigned k)const{return std::bit_cast<float>(words.at(k));}
};

struct OriginalUvScroll {                 // 0x50-byte object
    int32_t mode=5;                       // +0x00 (2/6 reverse the step at each on-period end; 6 eases)
    float timer=0;                        // +0x04 seconds in the current period
    float onTime=1,offTime=0;             // +0x08/+0x0C
    float angle=0,spin=0;                 // +0x10 accumulated / +0x14 per tick
    std::array<float,4> axis{};           // +0x20
    float u=0,v=0;                        // +0x30/+0x34 texture translation
    float stepU=0,stepV=0;                // +0x38/+0x3C per tick
    int32_t active=1;                     // +0x40
};

// 0x35F6E8. fps = [[gp+0x2A74]+0x10].
inline OriginalUvScroll originalUvScroll(const OriginalUvScrollArguments& a,int32_t fps){
    OriginalRounding rounding;OriginalUvScroll s;
    s.active=1;s.mode=int32_t(a.words[1]);s.u=a.real(2);s.v=a.real(3);s.onTime=a.real(6);s.offTime=a.real(7);s.timer=0;s.angle=0;
    s.spin=terrain_original::mul(a.real(8),originalScalarDivide(1.f,float(fps)));
    s.axis={a.real(9),a.real(10),a.real(11),0.f};
    s.stepU=a.real(4);s.stepV=a.real(5);
    return s;
}

// 0x35F7D0, one game tick.
inline void originalUvScrollTick(OriginalUvScroll& s,int32_t fps){
    OriginalRounding rounding;using terrain_original::mul;auto add=originalScalarAdd;auto sub=originalScalarSubtract;auto div=originalScalarDivide;
    if(!(0.f<s.onTime)&&!(0.f<s.offTime))return;
    const float timer=add(s.timer,div(1.f,float(fps)));s.timer=timer;
    if(!s.active){
        if(!(s.offTime<=timer))return;
        s.timer=0;
        if(0.f<s.onTime)s.active=1;
        return;
    }
    if(!(s.spin==0.f)){
        const float twoPi=std::bit_cast<float>(0x40C90FDBu),rate=s.spin;
        s.angle=add(s.angle,rate);
        // The source compares/wraps the step (not the accumulated angle).
        if(twoPi<=rate)s.spin=sub(rate,twoPi);
        else if(rate<=twoPi)s.spin=add(rate,twoPi);
    }
    if(s.onTime<=s.timer){
        s.timer=0;
        if(0.f<s.offTime)s.active=0;
        if(s.mode==2||s.mode==6){s.stepU=-s.stepU;s.stepV=-s.stepV;}
    }
    if(!s.active)return;
    if(s.mode!=6){s.u=add(s.u,s.stepU);s.v=add(s.v,s.stepV);}
    else{
        const float su=s.stepU,on=s.onTime,t=s.timer;
        float du,dv;
        if(su<0.f){
            if(t<mul(on,.5f)){const float inv=div(1.f,on);dv=mul(mul(s.stepV,t),inv);du=mul(mul(su,t),inv);}
            else{const float rest=sub(on,t),inv=div(1.f,on);dv=mul(mul(s.stepV,rest),inv);du=mul(mul(su,rest),inv);}
        }else{
            const float left=sub(on,t);
            if(left<mul(on,.5f)){const float inv=div(1.f,on);dv=mul(mul(s.stepV,left),inv);du=mul(mul(su,left),inv);}
            else{const float back=sub(on,left),inv=div(1.f,on);dv=mul(mul(s.stepV,back),inv);du=mul(mul(su,back),inv);}
        }
        s.v=add(s.v,dv);s.u=add(s.u,du);
    }
    if(1.f<s.u)s.u=sub(s.u,1.f);else if(s.u<-1.f)s.u=add(s.u,1.f);
    if(1.f<s.v)s.v=sub(s.v,1.f);else if(s.v<-1.f)s.v=add(s.v,1.f);
}

// 0x35FC20 translation part (the spin rotation is only built when angle != 0;
// no Snow Jam UVScroll sets key 8). Row-vector texture matrix: uv' = uv + (u, v).
inline std::array<float,2> originalUvScrollOffset(const OriginalUvScroll& s){return {s.u,s.v};}
}
