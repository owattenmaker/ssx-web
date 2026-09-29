#pragma once
#include "terrain_contact_math.hpp"
#include "obstacle_collision.hpp"
#include "collision_frame.hpp"
#include <array>
#include <optional>
#include <cstdint>

namespace ssx {
struct BodyCollisionSphere {terrain_original::Vector centerCm{};float radiusCm=0;unsigned bone=0;};
struct BodyAnimationState {int semantic=0,animationClass=0;uint64_t flags=0;bool completed=false;};
struct BodyCollisionVolume {
    terrain_original::Vector broadCenterCm{};float broadRadiusCm=0;
    uint32_t activeMask=0;unsigned count=0;
    std::array<BodyCollisionSphere,20> spheres;
    std::optional<OriginalCollisionFrame> reactionFrame;
    std::optional<terrain_original::Vector> landingCenterCm;
    std::optional<terrain_original::Vector> airPivotCm;
    std::optional<BodyAnimationState> mainAnimation;
};
inline BodyCollisionVolume bodyVolumeFromBones(const std::array<terrain_original::Vector,22>& worldBoneCentersCm,float bodyScale) {
    terrain_original::Rounding rounding;BodyCollisionVolume result;
    result.broadCenterCm=worldBoneCentersCm[1];result.broadRadiusCm=terrain_original::mul(92,bodyScale);
    result.count=10;result.activeMask=0xffffffff;
    for(unsigned i=0;i<10;++i)result.spheres[i]={worldBoneCentersCm[originalBodySpheres[i].bone],originalBodySpheres[i].radiusCm,originalBodySpheres[i].bone};
    return result;
}
struct BodyTriangleContact {bool hit=false;terrain_original::Vector translationCm{},pointCm{};int sphere=-1;};
namespace body_collision_detail {
using namespace terrain_original;
inline bool edgeOverlap(Vector center,float radiusSquared,Vector start,Vector end) {
    auto offset=difference(center,start),edge=difference(end,start);
    float length=terrain_original::sqrt(dot(edge,edge));
    if(length==0)return dot(offset,offset)<radiusSquared;
    float along=collision_scalar::divide(dot(offset,edge),length);
    if(along<0)return false;
    float perpendicular=originalScalarSubtract(dot(offset,offset),mul(along,along));
    if(radiusSquared<perpendicular)return false;
    float near=originalScalarSubtract(along,collision_scalar::squareRoot(originalScalarSubtract(radiusSquared,perpendicular)));
    return !(length<near);
}
inline bool overlap(Vector center,float radius,Vector projected,Vector a,Vector b,Vector c) {
    if(pointInTriangle(a,b,c,projected))return true;
    float squared=mul(radius,radius);auto delta=difference(center,a);
    return dot(delta,delta)<squared||edgeOverlap(center,squared,a,b)||edgeOverlap(center,squared,a,c)||edgeOverlap(center,squared,b,c);
}
}
// Original 0x32A1C0: broad sphere rejection, then enabled bone spheres.
// Selection uses the smallest SIGNED plane distance, not maximum penetration.
// The returned point is the center's plane projection even for edge overlap;
// this intentionally preserves the original algorithm's response convention.
inline BodyTriangleContact originalBodyTriangleContact(const BodyCollisionVolume& volume,
        terrain_original::Vector a,terrain_original::Vector b,terrain_original::Vector c,terrain_original::Vector normal,bool doubleSided=false) {
    using namespace terrain_original;Rounding rounding;
    if(volume.count>20)throw std::runtime_error("Body sphere count exceeds original storage");
    float broadDistance=dot(normal,difference(volume.broadCenterCm,a));
    if(volume.broadRadiusCm<std::abs(broadDistance))return {};
    if(doubleSided&&broadDistance<0){broadDistance=-broadDistance;for(auto& x:normal)x=-x;}
    Vector projected;for(unsigned k=0;k<3;++k)projected[k]=sub(volume.broadCenterCm[k],mul(normal[k],broadDistance));
    if(!body_collision_detail::overlap(volume.broadCenterCm,volume.broadRadiusCm,projected,a,b,c))return {};
    BodyTriangleContact result;
    if(!volume.activeMask) {
        float penetration=originalScalarSubtract(volume.broadRadiusCm,broadDistance);result.hit=true;result.pointCm=projected;
        for(unsigned k=0;k<3;++k)result.translationCm[k]=mul(normal[k],penetration);
        return result;
    }
    float smallest=10000000000.f;
    for(unsigned i=0;i<std::min<unsigned>(volume.count,20);++i) {
        if(!(volume.activeMask&(1u<<i)))continue;
        const auto& sphere=volume.spheres[i];float distance=dot(normal,difference(sphere.centerCm,a));
        if(!((doubleSided?distance:std::abs(distance))<sphere.radiusCm)||!(distance<smallest))continue;
        for(unsigned k=0;k<3;++k)projected[k]=sub(sphere.centerCm[k],mul(normal[k],distance));
        if(!body_collision_detail::overlap(sphere.centerCm,sphere.radiusCm,projected,a,b,c))continue;
        smallest=distance;result.hit=true;result.sphere=int(i);result.pointCm=projected;
        float penetration=originalScalarSubtract(sphere.radiusCm,distance);
        for(unsigned k=0;k<3;++k)result.translationCm[k]=mul(normal[k],penetration);
    }
    return result;
}
// Original 0x329590 / 0x32B2B8 box path tests face projections, not a generic
// sphere-vs-box closest-point distance. Edge/corner-only overlaps are rejected.
// 0x32B2B8 repeatedly queries the broad sphere for enabled mask bits; it never
// advances the sphere pointer, so preserving that behavior is intentional.
inline BodyTriangleContact originalBodyBoxContact(const BodyCollisionVolume& volume,
        terrain_original::Vector low,terrain_original::Vector high) {
    using namespace terrain_original;Rounding rounding;
    if(volume.count>20)throw std::runtime_error("Body sphere count exceeds original storage");
    if(volume.activeMask) {
        bool enabled=false;for(unsigned i=0;i<volume.count;++i)enabled|=bool(volume.activeMask&(1u<<i));
        if(!enabled)return {};
    }
    float best=1.0000000150474662e30f;BodyTriangleContact result;
    for(unsigned axis=0;axis<3;++axis) {
        bool inside=true;
        for(unsigned k=0;k<3;++k)if(k!=axis)inside&=low[k]<=volume.broadCenterCm[k]&&volume.broadCenterCm[k]<=high[k];
        if(!inside)continue;
        float lower=originalScalarAdd(originalScalarSubtract(volume.broadCenterCm[axis],low[axis]),volume.broadRadiusCm);
        float upper=originalScalarAdd(originalScalarSubtract(high[axis],volume.broadCenterCm[axis]),volume.broadRadiusCm);
        if(lower<0||upper<0)continue;
        if(lower<best){best=lower;result.hit=true;result.pointCm=volume.broadCenterCm;result.pointCm[axis]=low[axis];result.translationCm={};result.translationCm[axis]=-lower;}
        if(upper<best){best=upper;result.hit=true;result.pointCm=volume.broadCenterCm;result.pointCm[axis]=high[axis];result.translationCm={};result.translationCm[axis]=upper;}
    }
    return result;
}

}
