#pragma once
#include "terrain_contact_math.hpp"
#include <bit>
namespace ssx {
struct OriginalSpatialRegion {int32_t exponent=0;std::array<int32_t,3> cell{};};
//328360: source classification codes, used by332DB8's region traversal.
// Preserve the final asymmetric comparisons; do not replace with generic
// containment. In particular, the original repeats its Y condition and has
// no corresponding final minimum-Z condition.
inline int originalSpatialRegionClassify(const OriginalSpatialRegion& region,
    const std::array<float,3>& minimum,const std::array<float,3>& maximum){
    terrain_original::Rounding rounding;
    const float scale=std::bit_cast<float>((uint32_t(region.exponent)+127u)<<23);
    constexpr float padding=0.20000000298023224f;
    std::array<float,3> low{},high{};
    for(unsigned i=0;i<3;++i){
        low[i]=terrain_original::mul(originalScalarSubtract(float(region.cell[i]),padding),scale);
        if(maximum[i]<low[i])return 1;
    }
    for(unsigned i=0;i<3;++i){
        const int32_t next=std::bit_cast<int32_t>(uint32_t(region.cell[i])+1u);
        high[i]=terrain_original::mul(originalScalarAdd(float(next),padding),scale);
        if(high[i]<minimum[i])return 1;
    }
    if(high[0]<maximum[0]||high[1]<maximum[1]||high[2]<maximum[2])return 2;
    if(minimum[0]<high[0]||minimum[1]<high[1])return 2;
    return 0;
}
}
