#pragma once
// Original SSX3 (PS2 USA, SLUS_207.72) in-flight stance switch, control 5 (0x133308):
//
//   0x135BE0  called from the phase-3 branch (0x133694, before the D-pad spin/flip input) and the
//             phase-2 branch (0x133BE4) when no grab is active and the channel-2 request (0x312AA0)
//             is not 288. It returns 0x114DB8's result; on a switch it wraps +0x20 total spin by pi
//             into [-pi,pi), negates the flip fields +0x10/+0x18/+0x24/+0x2C and plays
//             0x3128E8(animator,288,0,-1.0) (clip 0x7500, 0.3 s, completion kind 1 -> 287).
//   0x114DB8  predicted-landing facing test. It needs predictor status 1 or 3, then compares the
//             last pose's board-root frame (+0x170 forward, +0x180 up) with the landing normal and
//             heading (predictor +0x20/+0x10, or rider +0x180/+0x1E0 on surface 18 / a surface
//             with table+0x44 set). When the projected forward faces away from the projected
//             heading (dot <= *(0x4A0EA8) = -0.0) it moves +0x110 by the 180-degree turn about the
//             animated pivot, then runs 0x11E098, 0x115168 and 0x1135B8(pred,+0x110,+0x1E0,3333.33).
//   0x115168  stance flip: toggles +0x320, 0x311B48(animator,pi), animator root (+0x30 = 0,0,0,1,
//             +0x40 = sincos(-pi/2 | -0) about Z), animator+0x18 = +0x320, negates +0x3A0/+0x3B0 and
//             the current/target of +0x1F0, +0x1FC, +0x208, +0x214 and +0x280.
//
// The air-adjust stick (AirAdjRotLR) turns the presented rider through +0x28 (up to 100 degrees),
// so holding it past ~90 degrees from the landing heading performs this switch mid-air.
// VU vector arithmetic uses terrain_original under OriginalRounding (chop); EE scalar ADD/SUB keep
// the guard bit. Oracle: tests/air_switch_reference.cpp, tools/test_air_switch_native.py.
#include "air_control.hpp"
#include "ground_motion.hpp"
#include "orientation_motion.hpp"
#include "original_float.hpp"
#include "terrain_contact_math.hpp"
#include <bit>
#include <functional>
#include <stdexcept>

namespace ssx {
using AirSwitchQuad=std::array<float,4>;
struct OriginalAirSwitchRider {
    AirSwitchQuad position{0,0,0,1},quaternion{0,0,0,1};   // +0x110, +0x120
    AirSwitchQuad right{},forward{},up{},rebuiltPosition{}; // +0x1A0/+0x1B0/+0x1C0/+0x1D0 written by 0x11E098
    AirSwitchQuad velocity{};                               // +0x1E0
    AirSwitchQuad frameForward{},frameUp{0,0,1,0};          // +0x170/+0x180: last pose's board-root frame
    AirSwitchQuad surfaceForward{},lateral{};               // +0x3A0/+0x3B0
    GroundControlValue turn,animationTurn,extraLean,brake,balance; // +0x1F0/+0x1FC/+0x208/+0x214/+0x280
    int reverseStance=0;                                    // +0x320
    AirSwitchQuad pivot{},scale{1,1,1,1};                   // geometry+0x24[rider+0x89C] local position, geometry+0x140
};
struct OriginalAirSwitchPrediction {
    int status=0,surface=-1,surfaceProperty44=0;            // predictor +0xAC, +0x90, surface table[+0x90]+0x44
    AirSwitchQuad heading{},normal{0,0,1,0};                // predictor +0x10, +0x20
};
struct OriginalAirSwitchAccess {
    std::function<void(float)> rotateAnimation;                                    // 0x311B48(animator,angle)
    std::function<void(AirSwitchQuad translation,AirSwitchQuad rotation)> setAnimationRoot; // animator+0x30/+0x40
    std::function<void(int)> setAnimationMirror;                                  // animator+0x18 = +0x320
    std::function<void(AirSwitchQuad position,AirSwitchQuad velocity,float speed)> beginFlight; // 0x1135B8
    std::function<void(int semantic)> play;                                        // 0x3128E8(animator,288,0,-1.0)
};

namespace air_switch_original {
using namespace terrain_original;
inline float bits(uint32_t b){return std::bit_cast<float>(b);}
// vmul.xyzw; vadday.x; vmaddaz.x (1*z); vmaddw.x (1*w)
inline float dot4(const AirSwitchQuad& a,const AirSwitchQuad& b){
    float x=mul(a[0],b[0]),y=mul(a[1],b[1]),z=mul(a[2],b[2]),w=mul(a[3],b[3]);
    return add(add(add(x,y),z),w);
}
// vmulx.xyzw by the dot, vsub.xyzw, then vrsqrt/vmulq normalisation of all four lanes.
inline AirSwitchQuad projectNormalized(const AirSwitchQuad& v,const AirSwitchQuad& n,float d){
    AirSwitchQuad r;for(unsigned k=0;k<4;++k)r[k]=sub(v[k],mul(n[k],d));
    float length=dot4(r,r),q=div(1.f,terrain_original::sqrt(length));
    for(float& x:r)x=mul(x,q);return r;
}
// 0x1150D4..0x115100: v + 2w(q x v) + 2 q x (q x v), accumulated as ((v + a*w) + a*w + b) + b.
inline AirSwitchQuad rotate(const AirSwitchQuad& q,const AirSwitchQuad& v){
    auto cross=[](const AirSwitchQuad& a,const AirSwitchQuad& b){return AirSwitchQuad{sub(mul(a[1],b[2]),mul(b[1],a[2])),sub(mul(a[2],b[0]),mul(b[2],a[0])),sub(mul(a[0],b[1]),mul(b[0],a[1])),0.f};};
    auto a=cross(q,v),b=cross(q,a);AirSwitchQuad out{0,0,0,0};
    for(unsigned k=0;k<3;++k){float x=add(v[k],mul(a[k],q[3]));x=add(x,mul(a[k],q[3]));x=add(x,b[k]);out[k]=add(x,b[k]);}
    return out;
}
template<class F> const F& need(const F& f,const char* what){if(!f)throw std::runtime_error(std::string("Air switch callback missing: ")+what);return f;}
}

// 0x114DB8 facing test only: true when the switch will happen. Pure; no state is written.
inline bool originalAirSwitchRequired(const OriginalAirSwitchRider& r,const OriginalAirSwitchPrediction& p){
    using namespace air_switch_original;OriginalRounding rounding;
    if(p.status!=1&&p.status!=3)return false;
    const bool riderFrame=p.surface==18||p.surfaceProperty44!=0;
    const AirSwitchQuad normal=riderFrame?r.frameUp:p.normal,heading=riderFrame?r.velocity:p.heading;
    if(dot4(r.frameUp,normal)<0)return false;
    if(bits(0x438ae38e)<dot4(r.frameForward,r.velocity))return false;           // 277.78 cm/s
    const float facing=dot4(r.frameForward,normal);
    if(bits(0x3f666666)<facing)return false;                                     // 0.9
    const auto forward=projectNormalized(r.frameForward,normal,facing);
    const auto along=projectNormalized(heading,normal,dot4(heading,normal));
    return !(bits(0x80000000)<dot4(forward,along));                              // *(0x4A0EA8) = -0.0
}

// 0x115168 on the rider fields; animation effects go through access.
inline void originalAirStanceFlip(OriginalAirSwitchRider& r,const OriginalAirSwitchAccess& a){
    using namespace air_switch_original;OriginalRounding rounding;
    r.reverseStance^=1;need(a.rotateAnimation,"rotateAnimation")(bits(0x40490fdb));
    auto sc=originalSinCos(r.reverseStance?bits(0xbfc90fdb):-0.f);
    need(a.setAnimationRoot,"setAnimationRoot")({0,0,0,1},{mul(sc[0],0.f),mul(sc[0],0.f),mul(sc[0],1.f),sc[1]});
    need(a.setAnimationMirror,"setAnimationMirror")(r.reverseStance);
    for(auto* v:{&r.surfaceForward,&r.lateral})for(float& x:*v)x=mul(x,-1.f);
    for(auto* v:{&r.turn,&r.animationTurn,&r.extraLean,&r.brake,&r.balance}){v->current=-v->current;v->target=-v->target;}
}

// Complete 0x114DB8: returns false without side effects, or performs the switch.
inline bool originalAirSwitch(OriginalAirSwitchRider& r,const OriginalAirSwitchPrediction& p,const OriginalAirSwitchAccess& a){
    using namespace air_switch_original;OriginalRounding rounding;
    if(!originalAirSwitchRequired(r,p))return false;
    AirSwitchQuad pivot{mul(r.pivot[0],r.scale[0]),mul(r.pivot[1],r.scale[1]),mul(r.pivot[2],r.scale[2]),mul(r.pivot[3],r.scale[3])};
    AirSwitchQuad offset{sub(pivot[0],0.f),sub(pivot[1],0.f),0.f,sub(pivot[3],1.f)};      // - 0x4FF130, then sp+0x58 = 0
    for(float& x:offset)x=mul(x,2.f);
    const auto moved=rotate(r.quaternion,offset);
    for(unsigned k=0;k<4;++k)r.position[k]=add(r.position[k],moved[k]);
    const auto basis=originalRebuildOrientation(r.quaternion);                      // 0x11E098
    r.quaternion=basis.quaternion;
    r.right={basis.right[0],basis.right[1],basis.right[2],0};r.forward={basis.forward[0],basis.forward[1],basis.forward[2],0};
    r.up={basis.up[0],basis.up[1],basis.up[2],0};r.rebuiltPosition=r.position;
    originalAirStanceFlip(r,a);
    need(a.beginFlight,"beginFlight")(r.position,r.velocity,bits(0x45505556));     // 3333.3335
    return true;
}

// 0x135BE0 after 0x114DB8 switched: control-state wrap/negations and the 288 request.
inline void originalAirSwitchControl(OriginalAirControlState& s,const OriginalAirSwitchAccess& a){
    using namespace air_switch_original;OriginalRounding rounding;
    const float turned=originalScalarAdd(s.totalSpin,bits(0x40490fdb));
    float turns=originalScalarAdd(mul(turned,bits(0x3e22f983)),.5f);
    float whole=float(int(turns));if(turns<whole)whole=originalScalarSubtract(whole,1.f); // cvt.w.s truncates
    const float span=mul(whole,bits(0x40c90fdb));
    s.totalFlip=-s.totalFlip;s.adjustFlip=-s.adjustFlip;s.totalSpin=originalScalarSubtract(turned,span);
    s.progressFlip=-s.progressFlip;s.targetFlip=-s.targetFlip;
    need(a.play,"play")(288);
}
}
