#pragma once
#include "terrain_contact_math.hpp"
namespace ssx {
struct OriginalBoostFlashHudInput {float timer=0;int tier=0;bool uninitialized=true;float value=0,maximum=-1;};
struct OriginalBoostFlashHudResult {bool present=false,shown=false,hidden=false,updated=false;int paletteTier=0;float fraction=0;};
//118C2C..118D4C: timer-driven slot9. Initialization does not advance it again.
inline OriginalBoostFlashHudResult originalBoostFlashHud(const OriginalBoostFlashHudInput& in){
 terrain_original::Rounding rounding;
 if(in.uninitialized)return in.timer>0?OriginalBoostFlashHudResult{true,true,false,true,0,0}:OriginalBoostFlashHudResult{};
 const float current=originalScalarDivide(in.value,in.maximum);
 if(current==1.f&&in.timer==0)return {false,false,true,false,0,0};
 const float target=originalScalarSubtract(1.f,terrain_original::mul(in.timer,std::bit_cast<float>(0x3d4ccccdu)));
 constexpr float delta=std::bit_cast<float>(0x3c888889u);
 float fraction=target;
 if(originalScalarAdd(target,delta)<current)fraction=originalScalarSubtract(current,delta);
 else if(current<originalScalarSubtract(target,delta))fraction=originalScalarAdd(current,delta);
 return {true,false,false,true,std::min(in.tier,11),fraction};
}
}
