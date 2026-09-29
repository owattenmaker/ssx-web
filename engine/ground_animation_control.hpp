#pragma once
#include "ground_motion.hpp"
#include "input.hpp"
#include <functional>
namespace ssx {
// Original12EE30 directional prewind selector and43D788 five-style table.
int originalPrewindAnimation(float spin,float flip,bool reverseStance,int style);
// Same12EE30 routine with release table43D840, before12EACC clears style.
int originalAirReleaseAnimation(float spin,float flip,bool reverseStance,int style);
float originalAirReleaseAnimationRate(float trickStat,float spinRate,float flipRate);
// Complete131878..131C04 selector and12EE30 prewind table. The caller owns
//114CC0 physical reversal; pass its result or its already-selected semantic21.
// nextRandom is consumed only by the original steep-bob branch.
bool originalSelectGroundAnimation(const OriginalGroundProfile&,OriginalGroundState&,const RiderInput&,float prewindSpin=0,float prewindFlip=0,
    const std::function<uint32_t()>& nextRandom={},bool reverseTurnTriggered=false);
}
