#pragma once
#include "spotlight_distance.hpp"
#include "spotlight_angular.hpp"
#include "irradiance.hpp"
namespace ssx {
struct OriginalSpotlight {
 OriginalLocalLight geometry;float intensity=0,outerCosine=0,innerCosine=0;
 int8_t distanceMode=0,angularMode=0;std::array<float,3> color{};
};
//Complete kind1 path of38A6A8; rejected lights leave the coefficient bank alone.
inline void originalSpotlightIrradiance(OriginalIrradianceCoefficients& bank,const std::array<float,3>& point,const OriginalSpotlight& light){
 terrain_original::Rounding rounding;OriginalLocalLightQuery query;
 if(!originalLocalLightQuery(point,light.geometry,query))return;
 auto radial=originalSpotlightDistance({light.geometry.radius,light.intensity,light.outerCosine,light.distanceMode},query);if(!radial)return;
 float weight=originalSpotlightAngular(terrain_original::mul(radial->intensity,radial->fade),query.axisCosine,light.outerCosine,light.innerCosine,light.angularMode);
 std::array<float,3> color;for(unsigned i=0;i<3;++i)color[i]=terrain_original::mul(light.color[i],weight);
 originalIrradianceDirectional(bank,{query.direction[0],query.direction[1],query.direction[2]},color,1.f);
}
}
