#pragma once
#include "generated/lighting_math.hpp" // tools/generate_lighting_math.py (from the ELF, git-ignored)
namespace ssx {
//2EDF00: source luminance and nonlinear brightness used for irradiance mixing.
inline float originalLightingBrightness(const std::array<float,4>& argb){
 terrain_original::Rounding rounding;auto f=[](uint32_t bits){return std::bit_cast<float>(bits);};
 float r=terrain_original::mul(argb[1],f(0x3e991687u)),g=terrain_original::mul(argb[2],f(0x3f1645a2u)),b=terrain_original::mul(argb[3],f(0x3de978d5u));
 float value=originalScalarAdd(originalScalarAdd(r,g),b),low=f(0x3dcccccdu),high=f(0x3ee66666u);
 if(value<=low)return 0;if(high<=value)return 1;
 value=originalScalarDivide(originalScalarSubtract(value,low),f(0x3eb33333u));
 value=originalLightingExp(terrain_original::mul(originalLightingLog(value),f(0x3f19a14eu)));
 return 0.f<=value?std::min(value,1.f):0.f;
}
}
