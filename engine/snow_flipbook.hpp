#pragma once
#include "terrain_contact_math.hpp"
namespace ssx {
//Shared recurrence:370838..370860 and live rider birth371220..371248.
//Caller owns update timing and alive/allocation gating; inactive births still advance.
inline float originalSnowFlipbookStep(float phase,int frames,float rate,float elapsed,bool emitterAlive){
 if(!emitterAlive)return phase;terrain_original::Rounding rounding;
 const float next=originalScalarAdd(phase,terrain_original::mul(rate,elapsed));
 return int32_t(next)<frames?next:0.f;
}
}
