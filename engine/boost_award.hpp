#pragma once
#include "boost_control.hpp"
namespace ssx {
struct OriginalBoostAwardContext {
    uint32_t rewardMask=0; // rider+B28
    bool enableTricky=false; // rider+B2C
};
struct OriginalBoostAwardEffects {
    bool positiveRewardEvent=false; //149690, before eligibility mask
    bool meterChangeEvent=false; //29AB08, before mutation
    bool fullMeterNotification=false; //10E028(type5)
    float previousMeter=0,delta=0;
};
//10E9B4..10E9F4: committed Uber-count growth advances the tier, capped at10.
void originalLandingUberProgression(OriginalBoostState&,int32_t previousCount,int32_t committedCount);
// Complete10E098 arithmetic. Caller supplies the original award delta and
// category mask; this does not substitute a points-to-meter conversion.
OriginalBoostAwardEffects originalBoostAward(OriginalBoostState&,
    const OriginalBoostAwardContext&,float delta,uint32_t category);
}
