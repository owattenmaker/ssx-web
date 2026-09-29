#pragma once
#include "animation_motion.hpp"
#include "collision_frame.hpp"
namespace ssx {
struct RiderLegBinding {int thigh=0,shin=0,foot=0;AnimationTransform boardLocalFoot;};
struct RiderPoseContact {
    AnimationVector boardDirection{},normal{};
    float boardAlignment=0,boardLiftCm=0,legWeight=0;
    int boardRoot=22,boardChild=23;
    std::array<RiderLegBinding,2> legs;
};
// Original11EB98 post-FK board alignment and11F3D8 two-link leg solve.
// Intermediate318 weights blend the animated foot target before the full solve.
// Original11F3D8 one-leg solve. The caller11EB98 skips it when weight==0.
void originalRiderLegContact(std::vector<AnimationTransform>& world,const std::vector<AnimationTransform>& local,AnimationVector scale,const RiderLegBinding&,AnimationTransform target,float weight);
void originalRiderPoseContact(std::vector<AnimationTransform>& world,const std::vector<AnimationTransform>& local,AnimationVector scale,const RiderPoseContact&);
}
namespace ssx {
struct RiderRootPresentation {
    float turn=0,extraLean=0,brake=0,roll=0,liftCm=0;
    AnimationVector lateral{};
    int controlState=0;
};
// Original11FA10 followed by the conditional2C8 lift in11EB98.
AnimationTransform originalRiderRootPresentation(AnimationTransform physical,AnimationVector boardLocalPosition,AnimationVector scale,const RiderRootPresentation&,AnimationTransform* secondaryRoot=nullptr);
}

namespace ssx {OriginalCollisionFrame originalRiderCollisionFrame(const AnimationTransform&);}
