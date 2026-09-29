#pragma once
#include "generated/irradiance_transform.hpp" // tools/generate_irradiance_transform.py (from the ELF, git-ignored)
namespace ssx {
//389620: only lane3 participates; RGB remains untouched.
inline void originalIrradianceAddFourth(OriginalIrradianceCoefficients& destination,const OriginalIrradianceCoefficients& source,float scale){
 terrain_original::Rounding rounding;
 for(unsigned i=0;i<10;++i)destination[i][3]=originalScalarAdd(destination[i][3],terrain_original::mul(source[i][3],scale));
}
//389CB8 final38A4D4..38A4EC. View-dependent shape/matrix preparation is separate.
inline void originalIrradianceRimCompose(OriginalIrradianceCoefficients& destination,
 OriginalIrradianceCoefficients shape,const std::array<float,9>& rotation,float scale,float globalRimScale){
 terrain_original::Rounding rounding;originalIrradianceTransform(shape,rotation,3,3);
 originalIrradianceAddFourth(destination,shape,terrain_original::mul(scale,globalRimScale));
}
}
namespace ssx {
//389CB8 entry: only these ten fourth-lane scratch values are initialized.
//Unused RGB scratch is zeroed here; the original tail consumes lane3 only.
inline OriginalIrradianceCoefficients originalIrradianceRimShape(float constant,float ySquared,float zSquared,float xLinear){
 OriginalIrradianceCoefficients shape{};shape[0][3]=constant;shape[2][3]=ySquared;shape[3][3]=zSquared;shape[7][3]=xLinear;return shape;
}
}
