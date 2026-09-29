#include "trick_commit.hpp"
#include "original_float.hpp"
#include "terrain_contact_math.hpp"
#include <cfenv>
#include <algorithm>
#include <cmath>
#include <stdexcept>
#pragma STDC FENV_ACCESS ON
namespace ssx {
void originalTrickRotationScore(OriginalGrabScoreState& s,OriginalTrickIdentityState& id,const OriginalTrickCommitProfile& p,float spin,float flip){
 OriginalRounding round;
 auto update=[&](float& old,float value,float scale){float removed=terrain_original::mul(std::abs(old),scale),added=terrain_original::mul(std::abs(value),scale);old=value;s.accumulated14=std::max(0.f,originalScalarAdd(originalScalarSubtract(s.accumulated14,removed),added));};
 update(id.spin34,spin,p.spinScale);update(id.flip38,flip,p.flipScale);
}
OriginalTrickCommitResult originalOrdinaryTrickCommit(OriginalGrabScoreState& s,
 OriginalTrickIdentityState& state,OriginalTrickHistory& history,const OriginalTrickCommitProfile& p,
 const OriginalTrickIdentityInput& input,float trickMultiplier,float invertedReward){
 OriginalRounding round;
 OriginalTrickCommitResult result;
 auto identity=originalTrickIdentity(state,p.identity,input);result.identity=identity.identity;
 if(originalCurrentTrickPoints(s)<=0)return result;
 if(identity.valid){
  result.namedBonus=originalNamedTrickBonus(result.identity,p.named);
  if(result.namedBonus>0)s.accumulated14=originalScalarAdd(s.accumulated14,terrain_original::mul(float(result.namedBonus),p.namedPointScale));
  result.repeats=originalTrickRepeatCount(history,result.identity,{state.field08,state.style20,state.active70,state.field7C,state.flag28});
 }
 const int divisor=result.repeats+1;
 //117990 rounds before the integer repeat division;11A454 rounds again after.
 float points=terrain_original::mul(s.multiplier1C4,trickMultiplier);points=terrain_original::mul(points,s.accumulated14);points=terrain_original::mul(points,p.scoreScale);
 int32_t rounded=int32_t(originalScalarAdd(points,5));rounded-=rounded%10;
 rounded/=divisor;rounded-=rounded%10;
 //117908/11A458: inverted points have their own rounding and bypass repeats.
 result.invertedPoints=int32_t(originalScalarAdd(terrain_original::mul(invertedReward,p.invertedPointScale),5));result.invertedPoints-=result.invertedPoints%10;
 result.points=std::bit_cast<int32_t>(uint32_t(rounded)+uint32_t(s.bonusPoints84)+uint32_t(result.invertedPoints));
 result.committedUbers=s.uberCount54;
 result.meterDelta=originalScalarDivide(s.accumulated14,float(divisor));
 return result;
}
}
