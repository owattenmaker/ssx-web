#pragma once
#include "crash_motion.hpp"
#include "animation_motion.hpp"
#include <functional>
namespace ssx {
struct OriginalCrashControlState {
    int phase=0; // control owner+2C0: source five-state recovery machine
    AnimationTransform previousPrimary,previousSecondary; // +10/+30
    float previousProgress=0,recovery70=0;
    int impactPending54=0;terrain_original::Vector impactVelocity60{};
};
struct OriginalCrashControlEntryEffects {bool clearBoostWindows=true,clearLegWeight=false;};
// Control8 enters BEFORE motion2, so detach uses the existing crash submode.
inline OriginalCrashControlEntryEffects originalCrashControlBegin(OriginalCrashControlState& control,
        OriginalCrashMotionState& motion,OriginalCrashActorState& actor,
        AnimationTransform posedPrimary,AnimationTransform posedSecondary,terrain_original::Vector offset9D0,int animationClass2) {
    using namespace terrain_original;Rounding rounding;control.phase=0;control.impactPending54=0;control.recovery70=0;
    for(unsigned k=0;k<3;++k){posedPrimary.position[k]=add(posedPrimary.position[k],offset9D0[k]);posedSecondary.position[k]=add(posedSecondary.position[k],offset9D0[k]);}
    control.previousPrimary=posedPrimary;control.previousSecondary=posedSecondary;OriginalCrashControlEntryEffects effects;
    if(animationClass2==22){originalCrashDetach(motion,actor,posedSecondary.position,posedSecondary.rotation,actor.velocity,{});effects.clearLegWeight=true;}
    return effects;
}
struct OriginalCrashClipState {
    float progress=0,duration10=1,speed90=1,controllerScale1C=1;
    bool complete=false;
    int semantic=328,animationClass=27;
    AnimationTransform primary,secondary;
};
struct OriginalCrashControlStepEffects {
    bool beginPredictor=false,playContinuation=false;
    int continuationSemantic=-1;
    bool updatePlaybackRate=false;
    bool groundContinuationObserverRequired=false;
};
// Original12DA88 table at457FA0. Left values are sliding, right are airborne.
inline int originalCrashContinuationSemantic(int semantic,bool airborne) {
    static constexpr int sliding[34]={362,363,364,368,366,367,370,371,365,369,362,363,364,368,366,371,369,365,367,370,368,369,364,366,371,368,368,370,363,362,368,370,372,372};
    if(semantic<328||semantic>361)return semantic;return sliding[semantic-328]+(airborne?11:0);
}
// Original136DE0; control's clip-derived angular velocity is distinct from
// physical translational velocity, which is retained/projected here.
inline bool originalCrashSetAngularVelocity(OriginalCrashMotionState& motion,OriginalCrashActorState& actor,
        terrain_original::Vector angularVelocity) {
    using namespace terrain_original;Rounding rounding;motion.angularVelocity=angularVelocity;motion.flag4=0;
    if(motion.submode==0){float amount=dot(actor.groundNormal,actor.velocity);for(unsigned k=0;k<3;++k)actor.velocity[k]=sub(actor.velocity[k],mul(actor.groundNormal[k],amount));return false;}
    return true;
}
inline terrain_original::Vector originalCrashPoseAngularVelocity(const AnimationTransform& previous,const AnimationTransform& current,float factor) {
    using namespace terrain_original;AnimationQuaternion delta,conjugate{-current.rotation[0],-current.rotation[1],-current.rotation[2],current.rotation[3]};
    for(unsigned k=0;k<4;++k)delta[k]=sub(current.rotation[k],previous.rotation[k]);auto result=originalAnimationCompose({{},delta},{{},conjugate}).rotation;
    Vector angular;for(unsigned k=0;k<3;++k)angular[k]=mul(result[k],factor);return angular;
}
// Original12CD20 initial crash-clip phase, including clip-derived angular
// release and optional class23 board detachment. Playback change is explicit.
inline OriginalCrashControlStepEffects originalCrashInitialControlStep(OriginalCrashControlState& control,
        OriginalCrashMotionState& motion,OriginalCrashActorState& actor,const OriginalCrashClipState& clip) {
    using namespace terrain_original;Rounding rounding;OriginalCrashControlStepEffects effects;
    if(control.phase!=0)throw std::runtime_error("Initial crash clip phase requested in another recovery state");
    if(!clip.complete){if(clip.progress<.949999988079071f){control.previousProgress=clip.progress;control.previousPrimary=clip.primary;control.previousSecondary=clip.secondary;}return effects;}
    float denominator=mul(originalScalarSubtract(clip.progress,control.previousProgress),clip.duration10);
    float factor=collision_scalar::divide(clip.speed90,denominator),twice=originalScalarAdd(factor,factor);
    Vector linear;for(unsigned k=0;k<3;++k)linear[k]=mul(mul(sub(clip.primary.position[k],control.previousPrimary.position[k]),clip.controllerScale1C),factor);
    auto angular=originalCrashPoseAngularVelocity(control.previousPrimary,clip.primary,twice);
    effects.beginPredictor=originalCrashSetAngularVelocity(motion,actor,angular);
    if(clip.animationClass==23){for(unsigned k=0;k<3;++k)linear[k]=mul(mul(sub(clip.secondary.position[k],control.previousSecondary.position[k]),clip.controllerScale1C),factor);auto secondaryAngular=originalCrashPoseAngularVelocity(control.previousSecondary,clip.secondary,twice);originalCrashDetach(motion,actor,clip.secondary.position,clip.secondary.rotation,linear,secondaryAngular);}
    control.phase=motion.submode==1?1:2;
    effects.continuationSemantic=originalCrashContinuationSemantic(clip.semantic,control.phase==1);
    effects.playContinuation=effects.continuationSemantic!=clip.semantic;effects.updatePlaybackRate=effects.playContinuation;
    effects.groundContinuationObserverRequired=control.phase==2;
    return effects;
}
struct OriginalCrashMeterEffects {bool changedByInput=false,reachedRecovery=false;float value=0;};
// Original12CB68 after114130 and the116120 reset request have completed.
// Commands are original control8 words; the caller owns their native decoding.
inline OriginalCrashMeterEffects originalCrashRecoveryMeter(OriginalCrashControlState& control,uint32_t command,int riderCategoryB20) {
    using namespace terrain_original;Rounding rounding;OriginalCrashMeterEffects result;
    if(control.phase==3){result.value=control.recovery70;return result;}
    float value=control.recovery70;
    if(value>.00416986271739006f)value=originalScalarSubtract(value,.00416986271739006f);
    else if(value<-.00416986271739006f)value=originalScalarAdd(value,.00416986271739006f);
    else value=0;
    if(command&0x2000){float speed=riderCategoryB20==0?8.001495361328125f:(riderCategoryB20==2?34.00276565551758f:17.00261688232422f);value=originalScalarAdd(value,mul(speed,.01666666753590107f));result.changedByInput=true;result.reachedRecovery=value>=1;}
    control.recovery70=result.value=value;return result;
}
struct OriginalCrashPlaybackRate {float base=0,applied=0;};
// Original12E528 returns its clamped base even when the applied rate differs.
inline OriginalCrashPlaybackRate originalCrashPlaybackRate(terrain_original::Vector angularVelocity,
        float duration10,int animationClass,float recovery70,float resetPermission470) {
    using namespace terrain_original;Rounding rounding;OriginalCrashPlaybackRate result;
    result.base=std::clamp(mul(mul(terrain_original::sqrt(dot(angularVelocity,angularVelocity)),duration10),.15915493667125702f),.5f,2.f);
    result.applied=(animationClass==29||animationClass==25)?1.f:((recovery70>=1||resetPermission470>=0)?std::max(result.base,2.f):result.base);return result;
}

struct OriginalCrashRecoveryCallbacks {
    std::function<void(float)> reportImpact,setRecoveryPresentation;
    std::function<void()> stopCrashEffect;
    std::function<void(int)> playAnimation,enterControl,enterMotion;
    std::function<void(bool)> setAirScoringStance,refundCrashBoost;
    std::function<void(int)> requestReset;
};
// Control phase3 /12D848: the caller must implement real control/motion entry,
// not change IDs while retaining crash or ordinary physics from another mode.
inline void originalCrashGetUpStep(OriginalCrashControlState& control,int motionSubmode,
        float clipProgress,bool clipComplete,bool stanceDiffers,const OriginalCrashRecoveryCallbacks& callbacks) {
    using namespace terrain_original;Rounding rounding;
    if(control.phase!=3)throw std::runtime_error("Crash get-up step requested outside phase3");
    if(control.impactPending54){control.impactPending54=0;callbacks.reportImpact(terrain_original::sqrt(dot(control.impactVelocity60,control.impactVelocity60)));callbacks.stopCrashEffect();}
    if(motionSubmode==0){float value=originalScalarSubtract(1,clipProgress);callbacks.setRecoveryPresentation(originalScalarAdd(value,value));}
    if(!clipComplete)return;
    if(motionSubmode==0){callbacks.playAnimation(5);callbacks.enterControl(0);callbacks.enterMotion(0);}
    else {callbacks.setAirScoringStance(stanceDiffers);callbacks.playAnimation(287);callbacks.enterControl(5);callbacks.enterMotion(1);}
}
// Control phase4 /12D9D8: detached-board reset after its recovery clip, or the
// original fast-recovery permission. Actual116120/control9/motion3 is external.
inline void originalCrashResetClipStep(const OriginalCrashControlState& control,
        float resetPermission470,bool clipComplete,const OriginalCrashRecoveryCallbacks& callbacks) {
    if(control.phase!=4)throw std::runtime_error("Crash reset clip step requested outside phase4");
    bool quick=control.recovery70>=1||resetPermission470>=0;
    if(quick||clipComplete){callbacks.refundCrashBoost(quick);callbacks.requestReset(2);}
}

}
