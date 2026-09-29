#pragma once
#include "spotlight_irradiance.hpp"
#include <span>
namespace ssx {
struct OriginalLightRankingInput {int kind=0;OriginalSpotlight light;float brightness=0;};
//2F6168 differs from38A530: dot the unnormalized displacement with the axis
// before multiplying by inverse distance. Coincidence uses distance/inverse1.
inline float originalLocalLightRank(const std::array<float,3>& point,const OriginalLightRankingInput& in){
    terrain_original::Rounding rounding;using terrain_original::mul;
    if(in.kind==0)return in.brightness;
    if(in.kind!=1&&in.kind!=2)return 0;
    const auto& light=in.light;std::array<float,3> delta;
    for(unsigned i=0;i<3;++i)delta[i]=originalScalarSubtract(light.geometry.position[i],point[i]);
    const float squared=originalScalarAdd(originalScalarAdd(mul(delta[0],delta[0]),mul(delta[1],delta[1])),mul(delta[2],delta[2]));
    if(mul(light.geometry.radius,light.geometry.radius)<squared)return 0;
    float distance=1,inverse=1;
    if(squared!=0){distance=originalScalarSqrt(squared);inverse=originalScalarDivide(1,distance);}
    float cosine=0;
    if(in.kind==1){
        cosine=originalScalarAdd(originalScalarAdd(mul(delta[0],light.geometry.axis[0]),mul(delta[1],light.geometry.axis[1])),mul(delta[2],light.geometry.axis[2]));
        cosine=mul(-cosine,inverse);if(!(light.outerCosine<=cosine))return 0;
    }
    float fade=1;
    if(!(light.geometry.radius<5000.f)&&!(distance<=3750.f))fade=originalScalarSubtract(1.f,mul(originalScalarSubtract(distance,3750.f),.0007999999797903001f));
    float radial=mul(inverse,100.f);
    if(radial<1){if(light.distanceMode==2)radial=mul(radial,radial);else if(light.distanceMode==3)radial=mul(mul(radial,radial),radial);else if(light.distanceMode!=1)radial=1;}else radial=1;
    float weight=mul(mul(radial,light.intensity),fade);
    if(in.kind==1){
        if(light.innerCosine<=cosine)weight=mul(weight,originalSpotlightPower(cosine,light.angularMode));
        else {weight=mul(weight,originalSpotlightPower(light.innerCosine,light.angularMode));weight=mul(weight,originalScalarDivide(originalScalarSubtract(cosine,light.outerCosine),originalScalarSubtract(light.innerCosine,light.outerCosine)));}
    }
    // Ranking is brightness-weighted and deliberately not clamped to shading's0..5.
    return mul(in.brightness,weight);
}
struct OriginalLocalLightCandidate {uint32_t id=0;int nodeKind=6;float rank=0;};
//2F5B68 with the rider's capacity8. Candidate order matters: new equal scores
// precede existing equals, but ties at the full-list cutoff are rejected.
inline std::array<uint32_t,8> originalSelectLocalLights(std::span<const OriginalLocalLightCandidate> candidates){
    std::array<uint32_t,8> ids{};std::array<float,8> ranks{};unsigned count=0;
    for(const auto& c:candidates){
        if(c.nodeKind!=6||!(ranks[7]<c.rank))continue;
        unsigned at=0;while(at<count&&c.rank<ranks[at])++at;
        if(at>=8)continue;
        count=std::min(count+1,8u);
        for(unsigned i=count-1;i>at;--i){ids[i]=ids[i-1];ranks[i]=ranks[i-1];}
        ids[at]=c.id;ranks[at]=c.rank;
    }
    return ids;
}
}
