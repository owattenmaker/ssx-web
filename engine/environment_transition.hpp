#pragma once
#include "environment_properties.hpp"
#include "environment_regions.hpp"
namespace ssx {
struct OriginalEnvironmentPayload {float transition=0;std::array<float,19> values{};};
struct OriginalEnvironmentTransition {
    OriginalEnvironmentProperties properties;
    float distance=-99999.f,lastX=0,lastY=0;
};
//2C0778 for the verified class12 property map. Stage map selection is external.
inline void originalEnvironmentTransitionStep(OriginalEnvironmentTransition&s,
        const OriginalEnvironmentRegions&regions,const std::vector<OriginalEnvironmentPayload>&payloads,
        const std::array<float,19>&defaults,float x,float y,float overrideWeight=-99999.f){
    OriginalRounding rounding;
    bool initial=s.distance==-99999.f;
    if(!initial){
        float dx=originalScalarSubtract(s.lastX,x),dy=originalScalarSubtract(s.lastY,y);
        float squared=originalScalarAdd(terrain_original::mul(dx,dx),terrain_original::mul(dy,dy));
        s.distance=originalScalarAdd(s.distance,originalScalarSqrt(squared));
    }
    s.lastX=x;s.lastY=y;
    int index=originalEnvironmentRegionAt(regions,x,y);
    if(index<0){s.properties.current=defaults;s.distance=0;return;}
    if(size_t(index)>=payloads.size())throw std::runtime_error("Environment payload index out of range");
    const auto&p=payloads[index];
    if(s.properties.current==p.values)s.distance=0;
    if(initial){originalEnvironmentPropertiesBlend(s.properties,p.values,-1);s.distance=0;return;}
    if(overrideWeight!=-99999.f){originalEnvironmentPropertiesBlend(s.properties,p.values,overrideWeight);return;}
    if(p.transition>=0&&s.distance>=p.transition){originalEnvironmentPropertiesBlend(s.properties,p.values,1);s.distance=0;return;}
    originalEnvironmentPropertiesBlend(s.properties,p.values,-p.transition);
}
}
