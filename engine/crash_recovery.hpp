#pragma once
#include "crash_control.hpp"
namespace ssx {
enum class OriginalCrashObserver {StopCrash,GroundStopped,GroundMoving,LeaveGround,ClearAirEffect,SpecialRecovery};
struct OriginalCrashRecoveryInputs {int deviceIndex870=-1;bool deviceEnabled87C=false;float resetPermission470=-1;};
struct OriginalCrashContinuationCallbacks {
    std::function<OriginalCrashClipState()> currentClip;
    std::function<float()> updatePlaybackRate;
    std::function<void()> playGroundContinuation,playAirContinuation,playGroundGetUp,playAirGetUp,playSpecialLanding,rebakeResetClip;
    //12D218 requests camera shake15E360, index4, fade0; this is not controller vibration.
    std::function<void(float)> seekClipSeconds,reportImpact,setRecoveryPresentation,cameraShake;
    std::function<void(OriginalCrashObserver)> notify;
    std::function<void(bool)> refundCrashBoost;
    std::function<void(int)> requestReset;
};
// Original12D4E8 (control phase1). Callbacks expose actual native animation
// operations; state/pose must be live because a clip change can affect later reads.
inline void originalCrashAirRecoveryStep(OriginalCrashControlState& control,OriginalCrashMotionState& motion,
        OriginalCrashActorState& actor,const OriginalCrashRecoveryInputs& input,const OriginalCrashContinuationCallbacks& callbacks) {
    using namespace terrain_original;Rounding rounding;if(control.phase!=1)throw std::runtime_error("Air crash recovery outside phase1");
    callbacks.updatePlaybackRate();
    if(control.impactPending54) {
        if(motion.submode==0) {
            float impactSpeed=terrain_original::sqrt(dot(control.impactVelocity60,control.impactVelocity60));
            if(actor.groundNormal[2]>.8999999761581421f&&dot(control.impactVelocity60,actor.groundNormal)<mul(impactSpeed,-.8999999761581421f)) {
                callbacks.rebakeResetClip();float normalSpeed=dot(actor.groundNormal,actor.velocity);for(unsigned k=0;k<3;++k)actor.velocity[k]=mul(actor.groundNormal[k],normalSpeed);control.phase=4;
            } else {
                auto clip=callbacks.currentClip();
                if(clip.animationClass==29||clip.animationClass==25){callbacks.playSpecialLanding();callbacks.notify(OriginalCrashObserver::GroundStopped);}
                else {float progress=clip.progress;callbacks.playGroundContinuation();float duration=callbacks.currentClip().duration10;callbacks.seekClipSeconds(mul(duration,progress));callbacks.notify(OriginalCrashObserver::GroundMoving);}
                control.phase=2;
            }
            callbacks.notify(OriginalCrashObserver::StopCrash);
        }
        control.impactPending54=0;callbacks.reportImpact(terrain_original::sqrt(dot(control.impactVelocity60,control.impactVelocity60)));return;
    }
    auto clip=callbacks.currentClip();if(!clip.complete&&clip.animationClass!=29&&clip.animationClass!=25)return;
    bool quick=control.recovery70>=1||input.resetPermission470>=0;
    if(quick) {
        if(!actor.detached){callbacks.refundCrashBoost(true);callbacks.playAirGetUp();control.phase=3;}
        else {callbacks.requestReset(3);callbacks.refundCrashBoost(true);}
    } else if(motion.submode==0){callbacks.playGroundContinuation();control.phase=2;}
    else if(callbacks.currentClip().complete)callbacks.playAirContinuation();
}
// Original12D160 (control phase2), including the early return after an impact
// notification and the original separate normal/fast/detached reset paths.
inline void originalCrashGroundRecoveryStep(OriginalCrashControlState& control,OriginalCrashMotionState& motion,
        OriginalCrashActorState& actor,const OriginalCrashRecoveryInputs& input,const OriginalCrashContinuationCallbacks& callbacks) {
    using namespace terrain_original;Rounding rounding;if(control.phase!=2)throw std::runtime_error("Ground crash recovery outside phase2");
    float speed=terrain_original::sqrt(dot(actor.velocity,actor.velocity));
    if(input.deviceIndex870>=0&&input.deviceEnabled87C){float strength=mul(speed,.035999998450279236f);strength=strength<0?0:(strength>100?1:collision_scalar::divide(strength,100));callbacks.cameraShake(strength);}
    float playback=callbacks.updatePlaybackRate();callbacks.setRecoveryPresentation(mul(playback,.5f));
    if(control.impactPending54){control.impactPending54=0;callbacks.reportImpact(terrain_original::sqrt(dot(control.impactVelocity60,control.impactVelocity60)));return;}
    if(actor.detached&&speed<27.77777862548828f){callbacks.rebakeResetClip();control.phase=4;callbacks.notify(OriginalCrashObserver::LeaveGround);callbacks.notify(OriginalCrashObserver::ClearAirEffect);return;}
    auto clip=callbacks.currentClip();if(!clip.complete&&clip.animationClass!=29&&clip.animationClass!=25)return;
    float threshold=clip.animationClass==28?1666.666748046875f:694.4444580078125f;
    bool quick=control.recovery70>=1||input.resetPermission470>=0;
    if((!actor.detached&&speed<threshold)||quick) {
        if(actor.surface==18||actor.detached) {
            if(quick){callbacks.requestReset(3);callbacks.refundCrashBoost(true);}
            else if(speed<277.77777099609375f){callbacks.rebakeResetClip();control.phase=4;}
        } else {
            callbacks.refundCrashBoost(quick);
            if(callbacks.currentClip().animationClass==28)callbacks.notify(OriginalCrashObserver::SpecialRecovery);
            callbacks.playGroundGetUp();control.phase=3;
        }
        callbacks.notify(OriginalCrashObserver::LeaveGround);callbacks.notify(OriginalCrashObserver::ClearAirEffect);
    } else if(motion.submode==1){callbacks.playAirContinuation();control.phase=1;callbacks.notify(OriginalCrashObserver::LeaveGround);callbacks.notify(OriginalCrashObserver::ClearAirEffect);}
    else if(callbacks.currentClip().complete)callbacks.playGroundContinuation();
}
}
