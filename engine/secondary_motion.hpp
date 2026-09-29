#pragma once
#include "animation_motion.hpp"
#include "terrain_contact_math.hpp"
#include "original_float.hpp"
#include <array>
#include <bit>
#include <cmath>
namespace ssx {
// Original120378: secondary-motion (hair "SH_*") channels 3..5.
// Every tick the first playback sequence of each channel receives rate
// clamp(|velocity-surfaceVelocity|/1666.67, 0.5 below, 2 above). When the
// race logic tick (1298C8) modulo 6 equals rider+86C, each enabled channel
// (+940/+970/+9A0) requests one of seven semantics from the relative wind in
// the frame of cached world bone (+8A8/+8AC/+89C) times a per-stance local
// rotation (+950/+980/+9B0 forward, +960/+990/+9C0 when rider+320 is set).
struct OriginalSecondaryMotionProfile {
    int slot=0;                                   // rider+86C
    std::array<bool,3> enabled{};                 // rider+940/+970/+9A0
    std::array<int,3> bone{};                     // rider+8A8/+8AC/+89C (compiled world slot)
    std::array<AnimationQuaternion,3> forward{},reverse{};
};
struct OriginalSecondaryMotionInput {
    std::array<float,4> velocity{},surfaceVelocity{}; // rider+1E0/+3D0 before motion
    bool reverse=false;                               // rider+320
    int controlState=0;                               // owner+DE4 (11FEE8)
    uint32_t logicTick=0;                             // 1298C8
    std::array<AnimationQuaternion,3> boneRotation{};  // cached world quaternion of each bone
    std::array<int,3> requested{438,438,438};          // 312AA0 for channels 3..5
};
struct OriginalSecondaryMotionResult {float rate=.5f;bool selected=false;std::array<int,3> request{-1,-1,-1};};

inline OriginalSecondaryMotionResult originalSecondaryMotion(const OriginalSecondaryMotionProfile& p,const OriginalSecondaryMotionInput& in){
    using namespace terrain_original;Rounding rounding;
    auto f=[](uint32_t bits){return std::bit_cast<float>(bits);};
    OriginalSecondaryMotionResult out;
    // 1203A0..1203E8: VU relative wind and |wind| (vsqrt).
    std::array<float,4> wind;for(unsigned k=0;k<4;++k)wind[k]=sub(in.velocity[k],in.surfaceVelocity[k]);
    auto dot4=[&](const std::array<float,4>& a,const std::array<float,4>& b){return add(add(add(mul(a[0],b[0]),mul(a[1],b[1])),mul(1.f,mul(a[2],b[2]))),mul(1.f,mul(a[3],b[3])));};
    const float speed=sqrt(dot4(wind,wind));
    const float ratio=originalScalarDivide(speed,f(0x44d05556u));
    out.rate=.5f;if(.5f<=ratio)out.rate=std::min(ratio,2.f);
    if(std::bit_cast<int32_t>(in.logicTick)%6!=p.slot)return out;
    out.selected=true;
    bool still=speed<=f(0x430ae38eu);
    if(in.controlState==11){still=false;wind=std::array<float,4>{add(wind[0],0.f),add(wind[1],0.f),add(wind[2],1000.f),add(wind[3],0.f)};}
    for(unsigned c=0;c<3;++c){
        if(!p.enabled[c])continue;
        const int base=0x19B+7*int(c);int semantic;
        if(still)semantic=base;
        else{
            // VU quaternion product bone*local (vopmula/vopmsub cross, mulaw/maddaw, w chain).
            const auto& a=in.boneRotation[c];const auto& b=in.reverse?p.reverse[c]:p.forward[c];
            std::array<float,3> cross{sub(mul(a[1],b[2]),mul(b[1],a[2])),sub(mul(a[2],b[0]),mul(b[2],a[0])),sub(mul(a[0],b[1]),mul(b[0],a[1]))};
            AnimationQuaternion q;
            for(unsigned k=0;k<3;++k){float acc=mul(a[k],b[3]);acc=add(acc,mul(b[k],a[3]));q[k]=add(acc,mul(cross[k],1.f));}
            float w=sub(mul(a[3],b[3]),mul(a[0],b[0]));w=sub(w,mul(1.f,mul(a[1],b[1])));q[3]=sub(w,mul(1.f,mul(a[2],b[2])));
            const float x=q[0],y=q[1],z=q[2],qw=q[3];
            // 1205BC..12063C: EE scalar right (x) and up (z) axes.
            float yy=mul(y,y),zz=mul(z,z),xx=mul(x,x),xz=mul(x,z),wy=mul(qw,y),wz=mul(qw,z),xy=mul(x,y);
            float r0x=originalScalarAdd(yy,zz);float zy=mul(z,y);float xxyy=originalScalarAdd(xx,yy);float wx=mul(qw,x);
            float r0z=originalScalarSubtract(xz,wy);float r0y=originalScalarAdd(xy,wz);r0x=originalScalarAdd(r0x,r0x);
            float r2x=originalScalarAdd(xz,wy);float r2y=originalScalarSubtract(zy,wx);xxyy=originalScalarAdd(xxyy,xxyy);
            r0x=originalScalarSubtract(1.f,r0x);r0z=originalScalarAdd(r0z,r0z);r0y=originalScalarAdd(r0y,r0y);r2x=originalScalarAdd(r2x,r2x);
            float r2z=originalScalarSubtract(1.f,xxyy);r2y=originalScalarAdd(r2y,r2y);
            const float side=dot4(wind,{r0x,r0y,r0z,0.f}),up=dot4(wind,{r2x,r2y,r2z,0.f});
            if(f(0x445e38e4u)<std::abs(up)&&std::abs(side)<std::abs(up))semantic=0.f<up?base+1:base+2;
            else if(f(0x43d05556u)<std::abs(side))semantic=side<0.f?base+4:base+3;
            else{
                // 120704..120790: forward (y) axis.
                float xx2=mul(x,x),zz2=mul(z,z),yz=mul(y,z),wx2=mul(qw,x);zz2=originalScalarAdd(zz2,xx2);float yx=mul(y,x);float wz2=mul(qw,z);
                zz2=originalScalarAdd(zz2,zz2);yz=originalScalarAdd(yz,wx2);yx=originalScalarSubtract(yx,wz2);float r1y=originalScalarSubtract(1.f,zz2);
                yz=originalScalarAdd(yz,yz);yx=originalScalarAdd(yx,yx);
                semantic=dot4(wind,{yx,r1y,yz,0.f})<0.f?base+6:base+5;
            }
        }
        if(semantic!=in.requested[c])out.request[c]=semantic;
    }
    return out;
}
}
