#pragma once
#include "animation_motion.hpp"
#include "orientation_motion.hpp"
#include "terrain_contact_math.hpp"
namespace ssx {
struct OriginalCrashRootBake {AnimationTransform physical,animationRootOffset;};
// Original10EDD4..10F164. currentLocal has already been multiplied by the
// geometry's authored scale; sampledRoot is the separate30ECD8 crash preview.
// The inverse delta is sent to311BF0 before3128E8 plays the selected semantic.
inline OriginalCrashRootBake originalCrashRootBake(AnimationTransform physical,
        const AnimationTransform& currentLocal,const AnimationTransform& sampledRoot) {
    using namespace terrain_original;Rounding rounding;
    auto inverse=[](const AnimationTransform& transform){AnimationTransform q;q.rotation={-transform.rotation[0],-transform.rotation[1],-transform.rotation[2],transform.rotation[3]};
        auto rotated=originalAnimationCompose(q,{transform.position,{0,0,0,1}});for(unsigned k=0;k<3;++k)q.position[k]=add(mul(rotated.position[k],-1),0);return q;};
    auto delta=originalAnimationCompose(currentLocal,inverse(sampledRoot));OriginalCrashRootBake result;
    result.physical=originalAnimationCompose(physical,delta);result.physical.rotation=originalRebuildOrientation(result.physical.rotation).quaternion;
    result.animationRootOffset=inverse(delta);return result;
}
}
