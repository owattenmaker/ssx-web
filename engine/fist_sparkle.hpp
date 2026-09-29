#pragma once
// Original PS2 SSX3 (SLUS_207.72) attack fist sparkle: rider FX component RFX+0xC70
// (reset 2F1148, per-tick update 2F1150 from the rider-manager pass 1290C8 after every
// rider's snow pass 2DF920, draw 2F1510). Header-only.
//
// While the attack charge rider+0x350 is positive and a punch clip is in its hit window,
// four camera-facing `ospk` sprites (texture 24, EFFECTS.SSH) are rebuilt every tick at
// the punching hand: no velocity, gravity or lifetime. Draw 2F1510: template 0x501420,
// ALPHA_1 enum 7 (GS 0x48, additive Cd + Cs*As>>7), depth tested (ZTST index 0),
// priority 7, renderer vfunc +0x270 = 0x377CF0 (screen-aligned sprite, half extent =
// size * projection scale / w, UV 0..1, vertex colour lanes (A,R,G,B) * 128 truncated).
//
// Random draws: 20 per active tick from the visual LCG gp+A0C (0x4A3AFC, the board
// track / snow word, OriginalSnowRandom), in the order x,y,z,size,colour per sprite.
#include "snow_emission.hpp"
#include "terrain_contact_math.hpp"
#include "original_float.hpp"
#include <array>
#include <bit>
namespace ssx {
struct OriginalFistSparkleState {
    bool active=false;                              // +0xA0
    std::array<SnowVector,4> positionCm{};          // +0x10 + 16*i (Z-up source centimetres)
    std::array<std::array<float,4>,4> colour{};     // +0x50 + 16*i, lanes (A,R,G,B)
    std::array<float,4> halfExtentCm{};             // +0x90 + 4*i
};
struct OriginalFistSparkleInput {
    float strength=0;                               // rider+0x350 (attack charge, 0..1)
    int clip0=-1,clip1=-1;                          // 312AA0(anim,0), 312AA0(anim,1): requested semantics
    int class1=-1;                                  // 311AE8(anim,1)
    bool marker0=false,marker1=false;               // 1446A0(311B20(anim,1)+0xB0, 0/1)
    bool switchStance=false;                        // rider+0x320
    // Unit world frames (geometry+0x30, 64-byte matrices) of rider+0x8B0 (handleft)
    // and rider+0x8B8 (handright): row 0 axis and row 3 origin.
    SnowVector hand8B0Axis0{},hand8B0Origin{},hand8B8Axis0{},hand8B8Origin{};
};
// 2F1148
inline void originalFistSparkleReset(OriginalFistSparkleState& s){s.active=false;}
// 2F1150. Returns true when the sprites were rebuilt (and the 20 LCG draws happened).
inline bool originalFistSparkleUpdate(OriginalFistSparkleState& s,const OriginalFistSparkleInput& in,OriginalSnowRandom& random){
    using namespace terrain_original;Rounding rounding;
    s.active=0.f<in.strength;if(!s.active)return false;
    // 2F11D4..2F1220: the channel-1 hit window (class 13, marker 0 set, marker 1 clear).
    const bool window=in.class1==13&&!in.marker1&&in.marker0;
    // 2F1224..2F1280: clip 326 (channel 0) needs no window; 323 (channel 1) does. 327/324 swap hands.
    bool useB8;
    if(in.clip0==326||(in.clip1==323&&window))useB8=!in.switchStance;
    else if(in.clip0==327||(in.clip1==324&&window))useB8=in.switchStance;
    else{s.active=false;return false;}
    const SnowVector& axis=useB8?in.hand8B8Axis0:in.hand8B0Axis0;const SnowVector& origin=useB8?in.hand8B8Origin:in.hand8B0Origin;
    // 2F12DC..2F1384: P = row3 + M*(8,0,0,0) (vmulax/vmadday/vmaddaz/vmaddw with y=z=w=0).
    SnowVector point{};for(unsigned k=0;k<3;k++)point[k]=add(origin[k],mul(axis[k],8.f));
    const float gray=std::bit_cast<float>(0x3f666666u); // gp-0x3930 = 0.9
    for(unsigned i=0;i<4;i++){
        // EE FPU scalar ops (mul.s, neg.s, add.s/sub.s = originalScalar*), then vadd for the position.
        const float h=mul(in.strength,3.f),low=-h,span=originalScalarSubtract(h,low);
        const float sizeSpan=originalScalarSubtract(originalScalarAdd(mul(in.strength,15.f),15.f),15.f);
        SnowVector offset{};for(unsigned k=0;k<3;k++)offset[k]=originalScalarAdd(low,mul(span,random.next()));
        for(unsigned k=0;k<3;k++)s.positionCm[i][k]=add(point[k],offset[k]);
        s.halfExtentCm[i]=originalScalarAdd(mul(sizeSpan,random.next()),15.f);
        const float c=mul(mul(in.strength,gray),random.next());
        s.colour[i]={1.f,c,c,c};
    }
    return true;
}
}
