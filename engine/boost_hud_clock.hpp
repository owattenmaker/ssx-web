#pragma once
#include "terrain_contact_math.hpp"
namespace ssx {
// Original1EBDF4..1EBE60. Caller handles absent/full widget and colour selection.
inline float originalBoostFlashClock(float phase,float widgetFraction){
 terrain_original::Rounding rounding;
 if(phase<0)return 1.f;
 const float denominator=originalScalarAdd(terrain_original::mul(originalScalarSubtract(1.f,widgetFraction),std::bit_cast<float>(0x3f6910ebu)),std::bit_cast<float>(0x3cf58904u));
 phase=originalScalarAdd(phase,originalScalarDivide(std::bit_cast<float>(0x3d088889u),denominator));
 while(phase>2.f)phase=originalScalarSubtract(phase,2.f);
 return phase;
}
// Original1EBED4..1EBF2C. Caller resets phase to-1 when slot10 is absent.
inline float originalBoostPendingLetterClock(float phase){
 terrain_original::Rounding rounding;
 if(phase<0)return .5f;
 phase=originalScalarAdd(phase,std::bit_cast<float>(0x3db40d7cu));
 while(phase>1.f)phase=originalScalarSubtract(phase,1.f);
 return phase;
}
}
