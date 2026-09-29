#pragma once
#include "terrain_contact_math.hpp"
#include <array>
#include <optional>
#include <span>
#include <stdexcept>
namespace ssx {
struct OriginalPainterTree {
 float scale=1,originX=0,originY=0;uint16_t root=0;
 std::span<const std::array<uint16_t,4>> nodes;
};
//2C1CD8: nullopt represents the tree's explicit outside leaf at header+18.
inline std::optional<uint16_t> originalPainterPoint(const OriginalPainterTree& tree,float x,float y){
 terrain_original::Rounding rounding;
 x=terrain_original::mul(originalScalarSubtract(x,tree.originX),tree.scale);
 y=terrain_original::mul(originalScalarSubtract(y,tree.originY),tree.scale);
 //EE cvt.w.s truncates in the game's rounding mode. Small negative fractions
 //therefore select cell0; floor/clamping would change the source boundary.
 if(!(x>-1.f&&x<32768.f&&y>-1.f&&y<32768.f))return {};
 uint32_t a=(uint32_t(int32_t(x))<<1)&0xffff,b=(uint32_t(int32_t(y))<<1)&0xffff;
 uint16_t index=tree.root;
 for(size_t steps=0;steps<=tree.nodes.size();++steps){
  if(index>=tree.nodes.size())throw std::runtime_error("Painter node index outside package");
  const auto& node=tree.nodes[index];if(!(node[0]&1))return index;
  unsigned quadrant=((a>>15)<<1)|(b>>15);
  a=(a<<1)&0xfffc;b=(b<<1)&0xfffc;index=node[quadrant]>>1;
 }
 throw std::runtime_error("Cyclic painter tree");
}
}
namespace ssx {
//2BAF90 returns leaf+4.2C0A10 interpretsFFFFFFFF as no payload and otherwise
//selects the corresponding8-byte section table entry. The host validates bounds.
inline std::optional<uint32_t> originalPainterPayload(const OriginalPainterTree& tree,float x,float y,
                                                     uint32_t outsideValue,size_t payloadCount){
 auto leaf=originalPainterPoint(tree,x,y);
 uint32_t index=leaf?uint32_t(tree.nodes[*leaf][2])|(uint32_t(tree.nodes[*leaf][3])<<16):outsideValue;
 if(index==0xffffffffu)return {};
 if(index>=payloadCount)throw std::runtime_error("Painter payload index outside package");
 return index;
}
}
