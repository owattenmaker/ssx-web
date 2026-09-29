#pragma once
#include "spatial_region.hpp"
#include <span>
#include <vector>
namespace ssx {
struct OriginalSpatialLightNode {
    std::array<int32_t,8> children{-1,-1,-1,-1,-1,-1,-1,-1};
    std::vector<uint32_t> lights;
};
struct OriginalSpatialLightRoot {OriginalSpatialRegion region;int32_t node=-1;};
// Light-only projection of332DB8/33B748/340DC0. Other output lists do not affect
// the extra-node list consumed by2F5B68. Preserve parent/list/child visitation order.
inline std::vector<uint32_t> originalSpatialLightQuery(std::span<const OriginalSpatialLightNode> nodes,
    const std::array<OriginalSpatialLightRoot,8>& roots,const std::array<float,3>& minimum,const std::array<float,3>& maximum){
    terrain_original::Rounding rounding;std::vector<uint32_t> out;
    auto nodeAt=[&](int32_t index)->const OriginalSpatialLightNode&{
        if(index<0||size_t(index)>=nodes.size())throw std::runtime_error("Invalid spatial light node");return nodes[index];
    };
    auto all=[&](auto&& self,int32_t index,unsigned depth)->void{
        if(depth>64)throw std::runtime_error("Cyclic spatial light tree");
        const auto& node=nodeAt(index);out.insert(out.end(),node.lights.begin(),node.lights.end());
        for(auto child:node.children)if(child>=0)self(self,child,depth+1);
    };
    auto partial=[&](auto&& self,int32_t index,const OriginalSpatialRegion& region)->void{
        if(region.exponent<11||region.exponent>30)throw std::runtime_error("Invalid partial spatial depth");
        const auto& node=nodeAt(index);out.insert(out.end(),node.lights.begin(),node.lights.end());
        if(region.exponent==11)return;
        const float scale=std::bit_cast<float>((uint32_t(region.exponent)+126u)<<23);
        //33B748 visits intersecting children in Gray-code order;340DC0
        //whole-subtree traversal above uses ordinary0..7 order.
        for(unsigned slot:std::array<unsigned,8>{0,1,3,2,6,7,5,4}){
            if(node.children[slot]<0)continue;
            OriginalSpatialRegion child;child.exponent=region.exponent-1;bool intersects=true,contained=true;
            for(unsigned axis=0;axis<3;++axis){
                child.cell[axis]=std::bit_cast<int32_t>((uint32_t(region.cell[axis])<<1)+((slot>>(2-axis))&1));
                const auto next=std::bit_cast<int32_t>(uint32_t(child.cell[axis])+1u);
                const float low=terrain_original::mul(originalScalarSubtract(float(child.cell[axis]),.20000000298023224f),scale);
                const float high=terrain_original::mul(originalScalarAdd(float(next),.20000000298023224f),scale);
                intersects&=minimum[axis]<high&&low<maximum[axis];
                contained&=minimum[axis]<low&&high<maximum[axis];
            }
            if(!intersects)continue;
            if(contained)all(all,node.children[slot],0);else self(self,node.children[slot],child);
        }
    };
    for(const auto& root:roots){
        if(root.node<0)continue;
        const int classification=originalSpatialRegionClassify(root.region,minimum,maximum);
        if(classification==0)all(all,root.node,0);else if(classification==2)partial(partial,root.node,root.region);
    }
    return out;
}
}
