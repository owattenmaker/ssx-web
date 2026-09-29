#pragma once
#include "irradiance_rim_pipeline.hpp"
#include "spotlight_irradiance.hpp"
#include "point_light_irradiance.hpp"
#include <span>
namespace ssx {
struct OriginalRiderLocalLight {int kind=0;OriginalSpotlight light;};
struct OriginalRiderDirectionalLight {std::array<float,3> direction{},color{};};
struct OriginalRiderExtraLighting {
    std::array<float,3> ambient{};
    std::span<const OriginalRiderDirectionalLight> directional;
};
//1220D8 assembly order. Environment/selection/view providers remain caller-owned.
//Kinds other than1/2 are ignored by the original38A6A8 contribution routine.
inline OriginalIrradianceCoefficients originalRiderIrradiance(
    const OriginalIrradianceCoefficients& environment,const std::array<float,16>& view,
    const std::array<float,4>& point,float rimScale,const std::array<float,5>& rimConstants,
    const std::array<const OriginalRiderLocalLight*,8>& lights,const OriginalRiderExtraLighting* extra=nullptr){
    terrain_original::Rounding rounding;
    auto out=environment;
    originalIrradianceViewRim(out,view,point,rimScale,rimConstants);
    const std::array<float,3> position{point[0],point[1],point[2]};
    for(const auto* entry:lights){
        if(!entry)continue;
        const auto& light=entry->light;
        if(entry->kind==1)originalSpotlightIrradiance(out,position,light);
        else if(entry->kind==2)originalPointLightIrradiance(out,position,
            {light.geometry.radius,light.intensity,light.geometry.position,light.color,light.distanceMode});
    }
    if(extra){
        for(unsigned channel=0;channel<3;++channel)out[0][channel]=originalScalarAdd(out[0][channel],extra->ambient[channel]);
        for(const auto& light:extra->directional)originalIrradianceDirectional(out,light.direction,light.color,1.f);
    }
    return out;
}
}
