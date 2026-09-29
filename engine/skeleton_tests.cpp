#include "skeleton.hpp"
#include <cassert>
#include <cmath>
#include <cstdio>
int main() {
    ssx::Skeleton skeleton({{-1,{1,0,0},{0,0,0,1}},{0,{0,2,0},{0,0,0,1}}});
    for (const auto& m:skeleton.palette())
        for (int c=0;c<4;++c) for (int r=0;r<4;++r) assert(std::abs(m.columns[c][r]-float(c==r))<1e-5);
    float q=std::sqrt(.5f);
    auto posed=skeleton.palette({simd::float4{0,0,q,q},simd::float4{0,0,0,1}});
    simd::float4 p=posed[1]*simd::float4{1,2,0,1};
    assert(std::abs(p.x+1)<1e-5 && std::abs(p.y)<1e-5);
    // Per-bone translation channels include non-root board_childg (MNF flags3).
    auto local=skeleton.palette({}, {}, false, {}, {simd::float3{4,0,0},simd::float3{0,5,0}});
    auto child=local[1]*simd::float4{1,2,0,1};
    assert(simd_length(child-simd::float4{4,5,0,1})<1e-5);
    bool cycle=false;
    try {ssx::Skeleton invalid({{1,{},{0,0,0,1}},{0,{},{0,0,0,1}}});} catch (...) {cycle=true;}
    assert(cycle);
    auto identity=ssx::afbQuaternion({1,1,1},{0,1,2,3});
    assert(simd_length(identity-simd::float4{0,0,0,1})<1e-6);
    for (int x=-8;x<9;++x) for (int y=-8;y<9;++y) for (int z=-8;z<9;++z) {
        auto q=ssx::afbQuaternion({x*.41f,y*.31f,z*.27f},{5,4,7,6});
        assert(std::abs(simd_length_squared(q)-1)<1e-5);
    }
    std::puts("Native skeleton: bind pose identity, parent rotation propagation, hierarchy validation passed");
}
