#pragma once
#include "terrain_contact_math.hpp"
#include <bit>
#include <cstdint>
namespace ssx {
struct OriginalSpatialCell {int level=11;std::array<int32_t,3> coordinate{};};
// Original328F28 assigns each authored box to a loose octree cell. Bounds are
// widened by0.2 cells, and subdivision stops at level11 (2048 source cm).
inline OriginalSpatialCell originalSpatialCell(terrain_original::Vector low,terrain_original::Vector high) {
    using namespace terrain_original;Rounding rounding;Vector extent=difference(high,low),center;
    for(unsigned k=0;k<3;++k)center[k]=add(low[k],mul(extent[k],.5f));
    float maximum=std::max({extent[0],extent[1],extent[2]});
    float estimate=mul(maximum,.7142857313156128f);OriginalSpatialCell cell;
    cell.level=std::max(11,int((std::bit_cast<uint32_t>(estimate)>>23)&255)-127);
    float size=std::bit_cast<float>(uint32_t(cell.level+127)<<23);
    for(unsigned k=0;k<3;++k){float value=originalScalarDivide(center[k],size);int32_t at=int32_t(value);if(value<float(at))--at;cell.coordinate[k]=at;}
    for(;;) {
        size=std::bit_cast<float>(uint32_t(cell.level+127)<<23);bool contains=true;
        for(unsigned k=0;k<3;++k) {
            float minimum=mul(originalScalarSubtract(float(cell.coordinate[k]),.20000000298023224f),size);
            float maximum=mul(originalScalarAdd(float(cell.coordinate[k]+1),.20000000298023224f),size);
            contains&=minimum<=low[k]&&high[k]<=maximum;
        }
        if(contains)return cell;
        if(cell.level>=30)throw std::runtime_error("Authored collision box exceeds original spatial range");
        ++cell.level;for(auto& x:cell.coordinate)x>>=1;
    }
}
inline unsigned originalSpatialRoot(const OriginalSpatialCell& cell) {
    return unsigned(cell.coordinate[0]<0)*4+unsigned(cell.coordinate[1]<0)*2+unsigned(cell.coordinate[2]<0);
}
// The position of child index (x << 2 | y << 1 | z) in 33B748's visit: its eight unrolled blocks load the children
// +0x0, +0x4, +0xC, +0x8, +0x18, +0x1C, +0x14, +0x10 (0x33BC10 .. 0x33CAB0), the reflected Gray order 0 1 3 2 6 7 5 4.
inline unsigned originalSpatialChildPosition(unsigned child) {
    return child^(child>>1)^(child>>2);
}
// 332DB8 visits the roots 0..7 and a node's lists before its children; 33B748 (a node the query box overlaps) visits
// the children in the Gray order above. A node the query contains goes to 340DC0, which visits children 0..7 instead,
// but the rider scope box (rider+0x400/+0x410, a few metres) never contains a loose cell with children (level 12 and
// up: 1.4 x 4096 cm on every axis), so for the rider's scope lists the order is Gray at every level (PS2 hl/hl-aimetro-17
// 2493: patches 171792, 245776, 64528, 293648; ascending children gave 64528 first). A common virtual root at level 31
// adds only shared prefixes, so actual streamed root growth does not change the relative order of these cells.
inline bool originalSpatialBefore(const OriginalSpatialCell& a,const OriginalSpatialCell& b) {
    unsigned ar=originalSpatialRoot(a),br=originalSpatialRoot(b);if(ar!=br)return ar<br;
    for(int level=30;level>=std::max(a.level,b.level);--level) {
        auto child=[&](const OriginalSpatialCell& c){unsigned result=0;for(unsigned k=0;k<3;++k)result=(result<<1)|(uint32_t(c.coordinate[k])>>unsigned(level-c.level)&1);return result;};
        unsigned ac=child(a),bc=child(b);
        if(ac!=bc)return originalSpatialChildPosition(ac)<originalSpatialChildPosition(bc);
    }
    return a.level>b.level;
}
}
