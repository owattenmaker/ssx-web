#pragma once
#include "../engine/animation_motion.hpp"
namespace ssx {
// Browser skin poses are relative to the presented physical root. Original
// 11FA10 lateral shift and 11EB98 contact directions are world-space vectors;
// transform them into that frame before applying the unchanged pose routines.
inline AnimationVector browserPoseDirection(AnimationQuaternion root,AnimationVector world) {
    AnimationQuaternion inverse{-root[0],-root[1],-root[2],root[3]};
    return originalAnimationCompose({{},inverse},{world,{0,0,0,1}}).position;
}
}
