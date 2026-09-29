#pragma once
#include "ground_motion.hpp"
#include "air_trajectory_world.hpp"
#include <cstdint>
namespace ssx {
struct OriginalLandingMaterial {float depth1=0,depth3=0,normalImpulseFactor=0,maximumNormalSpeed=0;uint32_t recovery=0;};
struct OriginalLandingProfile {std::array<OriginalLandingMaterial,19> materials{};float landingStat=0,bodyScale=0;};
struct OriginalAnimationEvent {uint64_t serial=0;int semantic=-1;bool airExit=false,reverseTurn=false;std::array<float,4> reverseRootQuaternion{0,0,0,1};float rate=1;};
struct OriginalLandingRuntime {uint32_t tick=0,lastGroundLeaveTick=0,groundFocusTick=0;int manualState330=0,animationClass=0;uint64_t animationFlags=0;};
struct OriginalLandingState {
    OriginalGroundState rider;
    std::array<float,3> physicalRight{1,0,0},groundPoint{},translationCm{};
    float impactNormalSpeed=0,trajectoryPredictionTime=0;int surface=0;
    uint32_t patchId=0xffffffffu;float patchU=0,patchV=0;
};
struct OriginalLandingImpact {bool contact=false;float relativeNormalSpeed=0,speedBefore=0;};
// Original139CB4..139D54. The query fraction is coarse even when its point and
// normal were refined. Do not substitute time-of-impact from a position segment.
OriginalLandingImpact originalLandingImpact(const OriginalLandingState&,const OriginalWorldSegmentHit&);
// Original contact velocity impulse/clamp, shared with the complete resolver.
void originalLandingVelocityResponse(OriginalGroundState&,const OriginalLandingMaterial&,float relativeNormalSpeed);
// Original139D78..13A148, after any control5 exit has baked presentation.
// Returns false for original recovery surfaces/flags; caller must route recovery.
bool originalLandingResolveContact(OriginalLandingState&,const OriginalWorldSegmentHit&,
    const OriginalLandingMaterial&,float relativeNormalSpeed);
struct OriginalLandingClassification {
    int animationClass=0;uint64_t animationFlags=0;int manualState330=0;
    float landingStat=0;uint32_t randomWord=0;
};
struct OriginalLandingChoice {int crashAnimation=0x1b6;bool consumedRandom=false;};
// Original13A14C..13A4C8 crash/clean classification, with the external RNG word
// supplied only for the original branch that consumes317810.
OriginalLandingChoice originalLandingClassify(const OriginalLandingState&,const OriginalLandingClassification&);
// Original13F410 mode0 exit and13C7A8 mode0 entry. Ticks are original1298C8.
void originalLandingGroundLeave(OriginalGroundState&,uint32_t currentTick,uint32_t& lastLeaveTick);
void originalLandingGroundEnter(OriginalGroundState&,const OriginalLandingMaterial&,float bodyScale,
    uint32_t currentTick,uint32_t lastLeaveTick);
// Original13A968 ordinary landing clip selection. Changes2DC on severe impacts.
int originalLandingAnimation(float relativeNormalSpeed,float& manualSpin);
int originalReverseLandingAnimation(float relativeNormalSpeed,float& manualSpin);
}
