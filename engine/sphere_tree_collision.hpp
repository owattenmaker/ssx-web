#pragma once
#include "body_collision.hpp"
#include <vector>

namespace ssx {
struct CollisionSphereTreeLevel {float radiusCm=0,childOffsetCm=0;uint32_t stride=0;};
struct CollisionSphereTree {
    terrain_original::Vector centerCm{};float radiusScale=1;
    bool compressed=true;
    std::vector<CollisionSphereTreeLevel> levels;
    std::vector<uint8_t> masks;
};
namespace sphere_tree_detail {
// Original 0x32CDB0 preserves ancestor overlap rejection and visits child bits
// in ascending order. A zero mask reached through an occupied parent is a leaf.
inline BodyTriangleContact query(const CollisionSphereTree& tree,const std::vector<uint8_t>& masks,terrain_original::Vector center,float radius,
        terrain_original::Vector nodeCenter,unsigned depth,size_t index) {
    using namespace terrain_original;
    const auto& level=tree.levels.at(depth);
    auto delta=difference(center,nodeCenter);float squared=dot(delta,delta);
    float combined=originalScalarAdd(radius,mul(level.radiusCm,tree.radiusScale));
    if(!(squared<mul(combined,combined)))return {};
    uint8_t mask=masks.at(index);
    if(depth+1<tree.levels.size()&&mask) {
        for(unsigned child=0;child<8;++child)if(mask&(1u<<child)) {
            Vector next=nodeCenter;
            for(unsigned k=0;k<3;++k)next[k]=add(next[k],mul(child&(1u<<(2-k))?1.f:-1.f,tree.levels[depth+1].childOffsetCm));
            auto result=query(tree,masks,center,radius,next,depth+1,index+(child+1)*size_t(level.stride));
            if(result.hit)return result;
        }
        return {};
    }
    float reciprocal=squared>0?div(1,terrain_original::sqrt(squared)):std::numeric_limits<float>::max();
    Vector normal=delta;for(auto& x:normal)x=mul(x,reciprocal);
    float penetration=originalScalarSubtract(combined,originalScalarSqrt(squared));
    float offset=originalScalarSubtract(radius,penetration);BodyTriangleContact result;result.hit=true;
    for(unsigned k=0;k<3;++k) {
        result.translationCm[k]=mul(normal[k],penetration);
        result.pointCm[k]=sub(center[k],mul(normal[k],offset));
    }
    return result;
}
}
// Original 0x327F18: broad rejection, then first child-sphere hit. Unlike the
// triangle kernel this routine does not inspect the active sphere mask.
inline BodyTriangleContact originalBodySphereTreeContact(const BodyCollisionVolume& volume,const CollisionSphereTree& tree,const std::vector<uint8_t>* cachedMasks=nullptr) {
    terrain_original::Rounding rounding;
    if(tree.levels.empty()||tree.masks.empty())throw std::runtime_error("Empty original sphere tree");
    if(volume.count>volume.spheres.size())throw std::runtime_error("Body sphere count exceeds original storage");
    const auto& masks=cachedMasks?*cachedMasks:tree.masks;
    auto broad=sphere_tree_detail::query(tree,masks,volume.broadCenterCm,volume.broadRadiusCm,tree.centerCm,0,0);
    if(!broad.hit||!volume.count)return broad;
    for(unsigned i=0;i<volume.count;++i) {
        auto result=sphere_tree_detail::query(tree,masks,volume.spheres[i].centerCm,volume.spheres[i].radiusCm,tree.centerCm,0,0);
        if(result.hit){result.sphere=int(i);return result;}
    }
    return {};
}
}
