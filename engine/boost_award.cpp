#include "boost_award.hpp"
#include "original_float.hpp"
#include "terrain_contact_math.hpp"
#include <algorithm>
#include <bit>
#include <cfenv>
#include <stdexcept>
#pragma STDC FENV_ACCESS ON
namespace ssx {
void originalLandingUberProgression(OriginalBoostState& s,int32_t before,int32_t after){
 if(before>=after||s.tier>=10)return;
 const int32_t delta=std::bit_cast<int32_t>(uint32_t(after)-uint32_t(before));
 const int32_t remaining=std::bit_cast<int32_t>(10u-uint32_t(s.tier));
 s.tier=std::bit_cast<int32_t>(uint32_t(s.tier)+uint32_t(std::min(delta,remaining)));
 if(s.tier==10)s.superTime=60.f;
}
OriginalBoostAwardEffects originalBoostAward(OriginalBoostState& s,
    const OriginalBoostAwardContext& context,float delta,uint32_t category){
 OriginalRounding round;
 OriginalBoostAwardEffects e;
 if(s.drainEnabled==3||delta==0)return e;
 if(delta>0){e.positiveRewardEvent=true;if((context.rewardMask&category)==0)return e;}
 e.meterChangeEvent=true;e.previousMeter=s.meter;e.delta=delta;
 const float sum=originalScalarAdd(s.meter,delta);
 s.meter=sum>=0?std::min(sum,1.f):0.f;
 if(s.meter==1&&delta>0&&context.enableTricky){
  e.fullMeterNotification=s.superTime==0;
  // A positive existing tier is NOT decremented: the source decrements only
  // a register used by the subsequent unsigned range test.
  if(s.tier<=0)s.tier=std::bit_cast<int32_t>(uint32_t(s.tier)+1u);
  if(uint32_t(s.tier)-1u<9u)s.superTime=std::max(s.superTime,20.f);
 }else if(delta<0&&s.tier<10)s.superTime=0;
 return e;
}
}
