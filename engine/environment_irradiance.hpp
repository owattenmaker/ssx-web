#pragma once
#include "irradiance.hpp"
namespace ssx {
//2ED92C..2EDB18 after painter/terrain updates. Bank resolution and2EDF00
//brightness calculation are caller-owned. Neither weight is clamped here.
inline OriginalIrradianceCoefficients originalEnvironmentIrradiance(
 const OriginalIrradianceCoefficients& previous,const OriginalIrradianceCoefficients& bright,
 const OriginalIrradianceCoefficients& dark,const OriginalIrradianceCoefficients& alternate,
 float selector,float brightness,float gain,float incoming){
 terrain_original::Rounding rounding;
 OriginalIrradianceCoefficients next;
 if(selector>std::bit_cast<float>(0x3dcccccdu))next=alternate;
 else {const float weight=terrain_original::mul(brightness,gain);next=originalIrradianceSum(originalIrradianceScale(bright,weight),originalIrradianceScale(dark,originalScalarSubtract(1.f,weight)));}
 return originalIrradianceSum(originalIrradianceScale(next,incoming),originalIrradianceScale(previous,originalScalarSubtract(1.f,incoming)));
}
}
