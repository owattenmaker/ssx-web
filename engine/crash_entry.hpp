#pragma once
#include "crash_bake.hpp"
#include "collision_event.hpp"
#include <functional>
namespace ssx {
struct OriginalHardCrashEntryState {
    AnimationTransform physical;
    terrain_original::Vector physicalUp{0,0,1};
    int prewindStyle328=0;
    std::array<float,3> steering1F0{},control208{},control250{};
    float roll330=0;
};
enum class OriginalHardCrashObserver {StopTrickEffect,ClearTrickEffect,BoostPenaltyFeedback};
struct OriginalHardCrashEntryCallbacks {
    std::function<void(float)> reportPeakImpact,changeBoostMeter,rotateAnimationRoot;
    //119B08 returns the actual manager penalty (normally0 or−.25).
    std::function<float(bool)> recordCrash;
    std::function<void(OriginalHardCrashObserver,float)> notify;
    std::function<void(const OriginalCollisionEvent&,int)> reportImpact;
    std::function<void(int,OriginalHardCrashEntryState&)> enterControl,enterMotion;
    std::function<void(const AnimationTransform&)> resetAnimationRootBasis;
    std::function<AnimationTransform(const OriginalHardCrashEntryState&)> presentedRoot;
    std::function<AnimationTransform(int)> previewCrashRoot;
    std::function<AnimationTransform()> currentScaledLocalRoot;
    std::function<void(const AnimationTransform&)> offsetAnimationRoots;
    std::function<void(int)> playAnimation;
};
// Complete10EB30 ordering with native engine callbacks at the original manager,
// animation, and control/motion boundaries. Those callbacks must execute real
// native state transitions or explicitly report their unsupported lifecycle.
inline void originalHardCrashEnter(OriginalHardCrashEntryState& state,int semantic,bool attacked,int impactType,
        const OriginalCollisionEvent& event,const OriginalHardCrashEntryCallbacks& callbacks) {
    using namespace terrain_original;Rounding rounding;
    callbacks.reportPeakImpact(mul(event.closingSpeedCmps,5));float penalty=callbacks.recordCrash(attacked);
    callbacks.changeBoostMeter(penalty);callbacks.notify(OriginalHardCrashObserver::StopTrickEffect,0);
    callbacks.notify(OriginalHardCrashObserver::ClearTrickEffect,0);callbacks.notify(OriginalHardCrashObserver::BoostPenaltyFeedback,penalty);
    callbacks.reportImpact(event,impactType);callbacks.enterControl(13,state);
    if(state.prewindStyle328==3||state.prewindStyle328==4){float angle=state.prewindStyle328==3?1.5707963705062866f:-1.5707963705062866f;
        auto rotated=originalRebuildOrientation(originalRotateOrientation(state.physical.rotation,state.physicalUp,angle));
        state.physical.rotation=rotated.quaternion;state.physicalUp=rotated.up;
        callbacks.rotateAnimationRoot(angle);callbacks.resetAnimationRootBasis({{}, {-0.f,-0.f,-0.f,1.f}});}
    state.prewindStyle328=0;
    state.physical=callbacks.presentedRoot(state);auto basis=originalRebuildOrientation(state.physical.rotation);state.physical.rotation=basis.quaternion;state.physicalUp=basis.up;
    state.steering1F0={};state.control208={};state.control250={};state.roll330=0;
    auto preview=callbacks.previewCrashRoot(semantic);auto current=callbacks.currentScaledLocalRoot();auto bake=originalCrashRootBake(state.physical,current,preview);
    state.physical=bake.physical;state.physicalUp=originalOrientationBasis(state.physical.rotation).up;
    callbacks.offsetAnimationRoots(bake.animationRootOffset);callbacks.playAnimation(semantic);
    callbacks.enterControl(8,state);callbacks.enterMotion(2,state);
}
}
