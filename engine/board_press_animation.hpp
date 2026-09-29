#pragma once
// Original SSX3 board-press animation drivers (docs/attack-boardpress-recovery.md).
//
//   kind 13  0x1047F0  pivots 29/30 seek |+0x280|, 37/38 seek 1-|+0x280| (clamped 0..1)
//   kind 14  0x104728  presses 24/32 seek +0x268, releases 25/33 seek clamp(1-+0x268)
//   kind 15  0x1046B0  holds 28/36: 0x103CC8 three-way by clamp(2 +0x274 - 1)
//                      leaves (negative, centre, positive) 28: 49/48/50, 36: 60/58/59
//   completion kind 9  0x104BD8 -> 0x312BD0: clear flag 63, play 28 (+0x330 == 1) or 36
//
// Kinds 13/14 set slot 0's clock (0x313CF0: slot time, sequence seek flag) and run
// only the sequence fade (0x313800 / 0x3145F8); they never advance the clock.
#include "animation_sequence.hpp"
#include "terrain_contact_math.hpp"
#include "original_float.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#pragma STDC FENV_ACCESS ON
namespace ssx {
inline bool originalBoardPressAnimationKind(int kind){return kind==13||kind==14||kind==15;}
// 0x1047F0 / 0x104728 seek amount (before the duration multiply).
inline float originalBoardPressSeekAmount(int kind,int semantic,float depth268,float pivot280){
    OriginalRounding rounding;
    if(kind==13){
        const float x=(semantic==37||semantic==38)?originalScalarSubtract(1.f,std::abs(pivot280)):std::abs(pivot280);
        return 0.f<=x?std::min(x,1.f):0.f;
    }
    if(semantic==25||semantic==33){const float x=originalScalarSubtract(1.f,depth268);return 0.f<=x?std::min(x,1.f):0.f;}
    return depth268;
}
// 0x313CF0(seq,0,amount*slot0.duration) then 0x313800(seq,ts/60). Returns the fade result.
inline bool originalBoardPressSeekStep(OriginalAnimationSequence& s,float amount,float timeScale){
    OriginalRounding rounding;
    if(s.slots.empty())throw std::runtime_error("Board-press seek sequence has no slot");
    s.slots[0].time=terrain_original::mul(amount,s.slots[0].duration);s.seekPending=true;
    return originalAnimationFadeStep(s,terrain_original::mul(timeScale,std::bit_cast<float>(0x3c888889u)));
}
// 0x1046B0 three-way amount: clamp((x+x)-1,-1,1) of +0x274.
inline float originalBoardPressHoldAmount(float depth274){
    OriginalRounding rounding;
    const float x=originalScalarSubtract(originalScalarAdd(depth274,depth274),1.f);
    return -1.f<=x?std::min(x,1.f):-1.f;
}
// 0x1046B0 lookup leaves {negative, centre, positive}.
inline std::array<uint32_t,3> originalBoardPressHoldLeaves(int semantic){
    return semantic==28?std::array<uint32_t,3>{49,48,50}:std::array<uint32_t,3>{60,58,59};
}
// 0x104BD8 completion kind 9 replacement semantic.
inline int originalBoardPressCompletionSemantic(int32_t press330){return press330==1?28:36;}
}
