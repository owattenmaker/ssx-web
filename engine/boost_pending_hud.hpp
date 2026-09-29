#pragma once
#include "terrain_contact_math.hpp"
namespace ssx {
struct OriginalBoostPendingHudInput {int tier=0,uberCount=0,activeUber=0;bool uninitialized=true;float value=0,maximum=-1;};
struct OriginalBoostPendingHudResult {bool removed=false,shown=false,hidden=false;int count=0;float fraction=0;};
//118AF8..118C28: pending Uber letter slot10, including show/hide requests.
inline OriginalBoostPendingHudResult originalBoostPendingHud(const OriginalBoostPendingHudInput& in){
 terrain_original::Rounding rounding;
 const int remaining=10-std::min(in.tier,10);
 if((!in.activeUber&&!in.uberCount)||remaining<=0)return {true,false,!in.uninitialized,0,0};
 const int count=std::min(in.uberCount+(in.activeUber?1:0),remaining);
 const float target=std::min(terrain_original::mul(float(count),std::bit_cast<float>(0x3de38e39u)),1.f);
 const float current=in.uninitialized?0:originalScalarDivide(in.value,in.maximum);
 constexpr float delta=std::bit_cast<float>(0x3c888889u);
 float fraction=target;
 if(originalScalarAdd(target,delta)<current)fraction=originalScalarSubtract(current,delta);
 else if(current<originalScalarSubtract(target,delta))fraction=originalScalarAdd(current,delta);
 return {false,in.uninitialized,false,count,fraction};
}
}
