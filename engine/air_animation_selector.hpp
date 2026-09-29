#pragma once
#include "air_control.hpp"
#include "ground_motion.hpp"
#include <functional>
namespace ssx {
struct OriginalAirAnimationState {
    GroundControlValue adjustment28C,adjustment298;
    float nextRate=1; //animator+1C; reset to1 only after a new play request.
};
struct OriginalAirAnimationContext {
    int mainSemantic=287,mainClass=2;
    bool mainCompleted=false,reverseStance=false,grabActive=false;
    float trickStat=0,boostModifier=0;
    int trajectoryStatus=0;float predictedTime=0,elapsed=0;
};
struct OriginalAirAnimationAccess {
    //312790 must resolve the original semantic variant first. It may consume
    // shared RNG through104CF8/311710; return0 for invalid basic519.
    std::function<float(int semantic)> duration;
    std::function<void(float)> setNextRate;
    std::function<void(int semantic,float rate)> play;
};
struct OriginalAirAnimationResult {int semantic=287;bool played=false;float rate=1;};
//Complete13437C..134C6C, after angular/scoring updates and before1211F8.
// No direct RNG draws; duration/animation variant callbacks preserve their own
// original shared RNG behavior and are invoked only on original branches.
OriginalAirAnimationResult originalSelectAirAnimation(const OriginalAirControlState&,
    const OriginalAirControlFrame&,OriginalAirAnimationState&,const OriginalAirAnimationContext&,
    const OriginalAirAnimationAccess&);
}
