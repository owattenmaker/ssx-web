#pragma once
#include "irradiance.hpp"
#include "collision_scalar.hpp"
namespace ssx {
struct OriginalPointLight {float radius=0,intensity=0;std::array<float,3> position{},color{};int8_t distanceMode=0;};
//38A618 and38AAA0..38ABE0, including EE division saturation at coincidence.
inline void originalPointLightIrradiance(OriginalIrradianceCoefficients& bank,const std::array<float,3>& point,const OriginalPointLight& light){
 terrain_original::Rounding rounding;using terrain_original::mul;std::array<float,3> direction;
 for(unsigned i=0;i<3;++i)direction[i]=originalScalarSubtract(light.position[i],point[i]);
 float squared=originalScalarAdd(originalScalarAdd(mul(direction[0],direction[0]),mul(direction[1],direction[1])),mul(direction[2],direction[2]));
 if(mul(light.radius,light.radius)<squared)return;
 float distance=originalScalarSqrt(squared),inverse=collision_scalar::divide(1.f,distance);
 for(auto&v:direction)v=mul(v,inverse);
 float fade=1;if(!(light.radius<5000.f)&&!(distance<=3750.f))fade=originalScalarSubtract(1.f,mul(originalScalarSubtract(distance,3750.f),.0007999999797903001f));
 float radial=mul(inverse,100.f);
 if(radial<1){if(light.distanceMode==2)radial=mul(radial,radial);else if(light.distanceMode==3)radial=mul(mul(radial,radial),radial);else if(light.distanceMode!=1)radial=1;}else radial=1;
 float weight=mul(mul(radial,light.intensity),fade);weight=0.f<=weight?std::min(weight,5.f):0.f;
 std::array<float,3> color;for(unsigned i=0;i<3;++i)color[i]=mul(light.color[i],weight);
 originalIrradianceDirectional(bank,direction,color,1.f);
}
}
