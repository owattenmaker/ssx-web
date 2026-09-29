#pragma once
#include "terrain_contact_math.hpp"
namespace ssx {
struct OriginalBoostLetterHudInput {
 int tier=0;
 bool uninitialized=true;
 float value=0,maximum=-1;
};
struct OriginalBoostLetterHudResult {bool removed=false;int count=0;float fraction=0;};
// Original118A28..118AF4, slot type8. Zero tier removes the slot.
inline OriginalBoostLetterHudResult originalBoostLetterHud(const OriginalBoostLetterHudInput& in){
 terrain_original::Rounding rounding;
 if(in.tier==0)return {true,0,0};
 const int count=std::min(in.tier,10)-1;
 const float current=in.uninitialized?0.f:originalScalarDivide(in.value,in.maximum);
 constexpr float ninth=std::bit_cast<float>(0x3de38e39u);
 constexpr float delta=std::bit_cast<float>(0x3bda740fu);
 const float target=std::min(terrain_original::mul(float(count),ninth),1.f);
 float fraction=target;
 if(originalScalarAdd(target,delta)<current)fraction=originalScalarSubtract(current,delta);
 else if(current<originalScalarSubtract(target,delta))fraction=originalScalarAdd(current,delta);
 return {false,count,fraction};
}
}
