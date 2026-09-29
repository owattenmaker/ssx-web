#pragma once
#include "terrain_contact_math.hpp"
namespace ssx {
struct OriginalBoostHudInput {
 float meter=0,pendingReward=0,previewValue=0,previewMaximum=-1,storedValue=0,storedMaximum=-1;
 bool previewUninitialized=true,storedUninitialized=true;
 int drainMode=1;
};
struct OriginalBoostHudValues {float preview=0,stored=0;};
//117FE0's1188F8..118A24: parameters of1171A8 widget updates5 and6.
// Widget values are stored negated with maximum=-1 by1171A8.
inline OriginalBoostHudValues originalBoostHudValues(const OriginalBoostHudInput& in){
 terrain_original::Rounding rounding;
 auto approach=[](float current,float target){
  constexpr float delta=std::bit_cast<float>(0x3c888889u);
  if(originalScalarAdd(target,delta)<current)return originalScalarSubtract(current,delta);
  if(current<originalScalarSubtract(target,delta))return originalScalarAdd(current,delta);
  return target;
 };
 OriginalBoostHudValues out{in.meter,in.meter};
 if(!in.previewUninitialized){
  const float target=std::min(in.drainMode==3?in.meter:originalScalarAdd(in.meter,in.pendingReward),1.f);
  out.preview=approach(originalScalarDivide(in.previewValue,in.previewMaximum),target);
 }
 if(!in.storedUninitialized)out.stored=approach(originalScalarDivide(in.storedValue,in.storedMaximum),in.meter);
 return out;
}
}
