#pragma once
#include "terrain_contact_math.hpp"
#include <span>
#include <vector>
namespace ssx {
using OriginalSkinMatrix=std::array<float,16>;
struct OriginalSkinInfluence {int16_t weight=0;uint8_t bone=0;};
using OriginalSkinWeights=std::vector<OriginalSkinInfluence>;
//386BD0: integer percentage weights, source0.01 constant and VU accumulation.
//No division by sum, matrix normalization or inverse-transpose is performed.
inline std::vector<OriginalSkinMatrix> originalSkinPalette(std::span<const OriginalSkinMatrix> bones,std::span<const OriginalSkinWeights> groups){
 terrain_original::Rounding rounding;using namespace terrain_original;
 std::vector<OriginalSkinMatrix> result;result.reserve(groups.size());
 constexpr float percent=std::bit_cast<float>(0x3c23d70au);
 for(const auto& group:groups){
  if(group.empty())throw std::runtime_error("Empty original skin weight group");
  OriginalSkinMatrix matrix{};
  for(unsigned i=0;i<group.size();++i){const auto& influence=group[i];if(influence.bone>=bones.size())throw std::runtime_error("Skin bone outside palette");
   const float weight=mul(float(influence.weight),percent);
   // VU accumulation acc*1 + value: acc is a toward-zero product or sum (already flushed), so acc*1 == acc exactly.
   for(unsigned lane=0;lane<16;++lane){const float value=mul(bones[influence.bone][lane],weight);matrix[lane]=i?add(matrix[lane],value):value;}
  }
  result.push_back(matrix);
 }
 return result;
}
}
