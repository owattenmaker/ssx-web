#pragma once
#include "terrain_contact_math.hpp"
#include "collision_scalar.hpp"
#include <array>
#include <limits>

namespace ssx {
struct OriginalBodySphere {unsigned bone;float radiusCm;};
inline constexpr std::array<OriginalBodySphere,10> originalBodySpheres{{
    {1,25},{5,20},{7,15},{9,15},{12,15},{14,15},{16,25},{17,20},{19,25},{20,20}
}};
struct ObstacleResponse {
    bool accepted=false,moved=false,bounced=false;
    terrain_original::Vector normal{},translationCm{},velocityCmps{},relativeDirection{};
    float closingSpeedCmps=0;
};
inline bool originalObstacleQueryEnabled(int riderState,bool field874,float field4c8,float positionZ,float field498) {
    terrain_original::Rounding rounding;
    if(riderState==9)return false;
    return field874||!(field4c8<50&&std::abs(terrain_original::sub(field498,positionZ))<50);
}
// Original linear contact response at 0x13F5C4..0x13F7C4. Source Z-up
// centimeters and cm/s. The caller owns contact generation, volume translation,
// 0x1065B0 orientation response, and 0x105D98 collision/event notification.
inline ObstacleResponse originalObstacleResponse(float penetrationCm,terrain_original::Vector groundNormal,
        terrain_original::Vector hitNormal,terrain_original::Vector velocity,terrain_original::Vector surfaceVelocity) {
    using namespace terrain_original;Rounding rounding;
    ObstacleResponse result;result.velocityCmps=velocity;
    if(penetrationCm<0)return result;
    float alignment=dot(groundNormal,hitNormal);
    if(alignment<-.9998999834060669f)return result;
    if(alignment<0) {
        for(unsigned k=0;k<3;++k)hitNormal[k]=sub(hitNormal[k],mul(groundNormal[k],alignment));
        float length=terrain_original::sqrt(dot(hitNormal,hitNormal));float inverse=length>0?div(1,length):std::numeric_limits<float>::max();
        for(auto& x:hitNormal)x=mul(x,inverse);
    }
    result.accepted=result.moved=true;result.normal=hitNormal;
    for(unsigned k=0;k<3;++k)result.translationCm[k]=mul(mul(hitNormal[k],1.100000023841858f),penetrationCm);
    Vector relative=difference(velocity,surfaceVelocity),negative;
    for(unsigned k=0;k<3;++k)negative[k]=-hitNormal[k];
    float speed=dot(negative,relative);result.closingSpeedCmps=speed;
    if(speed<0)return result;
    float length=terrain_original::sqrt(dot(relative,relative));float inverse=length>0?div(1,length):std::numeric_limits<float>::max();
    for(unsigned k=0;k<3;++k)result.relativeDirection[k]=mul(relative[k],inverse);
    float bounce=std::max(mul(speed,.05000000074505806f),27.77777862548828f);bounce=originalScalarAdd(speed,bounce);
    for(unsigned k=0;k<3;++k)result.velocityCmps[k]=add(velocity[k],mul(hitNormal[k],bounce));
    result.bounced=true;return result;
}
// Original13AA48 airborne second-phase response. It uses raw actor velocity,
// does not project the normal onto the ground tangent, and does not steer the
// quaternion through1065B0. The caller only rebuilds orientation after a bounce.
inline ObstacleResponse originalAirObstacleResponse(float penetrationCm,
        terrain_original::Vector hitNormal,terrain_original::Vector velocity) {
    using namespace terrain_original;Rounding rounding;ObstacleResponse result;result.velocityCmps=velocity;
    if(penetrationCm<0)return result;
    result.accepted=result.moved=true;result.normal=hitNormal;
    Vector negative;for(unsigned k=0;k<3;++k){result.translationCm[k]=mul(mul(hitNormal[k],1.100000023841858f),penetrationCm);negative[k]=-hitNormal[k];}
    float closing=dot(negative,velocity);result.closingSpeedCmps=closing;
    if(closing<0)return result;
    float length=terrain_original::sqrt(dot(velocity,velocity));float inverse=length>0?div(1,length):std::numeric_limits<float>::max();
    for(unsigned k=0;k<3;++k)result.relativeDirection[k]=mul(velocity[k],inverse);
    float bounce=std::max(mul(closing,.05000000074505806f),27.77777862548828f);
    float total=originalScalarAdd(closing,bounce);
    for(unsigned k=0;k<3;++k)result.velocityCmps[k]=add(velocity[k],mul(hitNormal[k],total));
    result.bounced=true;return result;
}
// Original 0x1065B0: steer board heading away from an impact, using its own
// post-multiplied quaternion convention. The caller performs11E098 afterward.
inline std::array<float,4> originalObstacleOrientation(std::array<float,4> q,
        terrain_original::Vector boardUp,terrain_original::Vector bodyForward,
        terrain_original::Vector hitNormal,bool* rotated=nullptr) {
    using namespace terrain_original;Rounding rounding;if(rotated)*rotated=false;
    auto right=cross(boardUp,hitNormal);float length=terrain_original::sqrt(dot(right,right));
    float reciprocal=length>0?div(1,length):std::numeric_limits<float>::max();for(auto& x:right)x=mul(x,reciprocal);
    auto forward=cross(boardUp,right);float cosine=dot(bodyForward,forward),sine=dot(bodyForward,right),angle;
    if(cosine==0)angle=sine==0?0:(sine>=0?1.5707963705062866f:-1.5707963705062866f);
    else {angle=collision_scalar::atan(collision_scalar::divide(sine,cosine));if(cosine<0)angle=sine>0?originalScalarAdd(angle,3.1415927410125732f):originalScalarSubtract(angle,3.1415927410125732f);}
    float change;
    if(angle>0)change=std::clamp(originalScalarSubtract(1.9198623895645142f,angle),0.f,.349065899848938f);
    else change=std::clamp(originalScalarSubtract(-1.9198623895645142f,angle),-.349065899848938f,0.f);
    if(change==0)return q;
    auto sc=collision_scalar::sincos(mul(-change,.5f));std::array<float,4> delta{mul(sc[0],boardUp[0]),mul(sc[0],boardUp[1]),mul(sc[0],boardUp[2]),sc[1]},out;
    // VU0 product (0x1067D8..0x1067F4): vopmsub's fs is delta, and the w lane's y / z terms go through vf0 (1.0) as fs.
    for(unsigned i=0;i<3;++i){unsigned j=(i+1)%3,k=(i+2)%3;float crossed=sub(mul(q[j],delta[k]),mul(delta[j],q[k]));out[i]=add(add(mul(q[i],delta[3]),mul(delta[i],q[3])),crossed);}
    out[3]=sub(sub(sub(mul(q[3],delta[3]),mul(q[0],delta[0])),mul(1.f,mul(q[1],delta[1]))),mul(1.f,mul(q[2],delta[2])));
    if(rotated)*rotated=true;return out;
}

}
