#pragma once
#include "air_trajectory_world.hpp"
namespace ssx {
struct OriginalLandingProbe {terrain_original::Vector centerCm{},presentationUp{0,0,1};};
// Original13A7B0 uses posed board-root (rider+8A0, Zoe22) and the final
// presentation-up, not actor position or physical/terrain normal.
inline std::array<terrain_original::Vector,2> originalLandingEndpoints(const OriginalLandingProbe& probe) {
    using namespace terrain_original;Rounding rounding;std::array<Vector,2> endpoints;
    for(unsigned k=0;k<3;++k){float offset=mul(probe.presentationUp[k],200);endpoints[0][k]=sub(probe.centerCm[k],offset);endpoints[1][k]=add(probe.centerCm[k],offset);}
    return endpoints;
}
inline OriginalWorldSegmentHit originalLandingContact(const CollisionWorld& terrain,const WorldBodyCollision* world,
        const OriginalLandingProbe& probe,terrain_original::ContactCache* cache=nullptr,const terrain_original::RiderScope* scope=nullptr) {
    // 3342D0 walks the rider's query scope (rider+0x860), not the whole world: an inverted
    // rider's probe reaches 200cm past the board root, outside the query bounds the scope was
    // built from (pipe-tricks 1361). Callers without a scope query every patch.
    auto endpoints=originalLandingEndpoints(probe);
    return queryOriginalWorldSegment(terrain,world,endpoints[0],endpoints[1],0,.574999988079071f,true,cache,scope);
}
}
