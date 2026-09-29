#pragma once
#include "original_float.hpp"
#include "terrain_contact_math.hpp"
#include <array>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>
namespace ssx {
//2C1CD8's 8-byte spatial-tree records: a leaf stores its payload index in
//the final word; an internal record stores four (childIndex<<1)|flags links.
struct OriginalEnvironmentRegionNode {std::array<uint16_t,4> words{};};
struct OriginalEnvironmentRegions {
    float scale=0,originX=0,originY=0;
    uint16_t root=0;
    int32_t outside=-1;
    std::vector<OriginalEnvironmentRegionNode> nodes;
};
inline int32_t originalEnvironmentRegionAt(const OriginalEnvironmentRegions&r,float x,float y){
    OriginalRounding rounding;
    auto coordinate=[&](float value,float origin)->uint32_t{
        float n=terrain_original::mul(originalScalarSubtract(value,origin),r.scale);
        //EE conversion overflow/NaN produces80000000, which fails the bounds test.
        if(!(n>=-2147483648.f&&n<2147483648.f))return 0x80000000u;
        return uint32_t(int32_t(n));
    };
    uint32_t ix=coordinate(x,r.originX),iy=coordinate(y,r.originY);
    if(ix>0x7fff||iy>0x7fff)return r.outside;
    uint32_t bx=(ix<<1)&0xffff,by=(iy<<1)&0xffff,index=r.root;
    for(size_t depth=0;depth<=r.nodes.size();++depth){
        if(index>=r.nodes.size())throw std::runtime_error("Original environment region link out of range");
        const auto&node=r.nodes[index];
        if(!(node.words[0]&1))return std::bit_cast<int32_t>(uint32_t(node.words[2])|(uint32_t(node.words[3])<<16));
        unsigned quadrant=((bx>>15)<<1)|(by>>15);
        index=node.words[quadrant]>>1;bx=(bx<<1)&0xfffc;by=(by<<1)&0xfffc;
    }
    throw std::runtime_error("Cyclic original environment region tree");
}
}
