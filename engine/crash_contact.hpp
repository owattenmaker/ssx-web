#pragma once
#include "air_trajectory_world.hpp"
namespace ssx {
struct OriginalCrashContactProbe {terrain_original::Vector positionCm{},normal{0,0,1};};
// Original138960 probes physical actor position along the caller-supplied
// normal, unlike13A7B0's animated board center/presentation up. Both use the
// original kind2 refined segment and the shared rider+864 contact cache.
inline OriginalWorldSegmentHit originalCrashContact(const CollisionWorld& terrain,const WorldBodyCollision* world,
        const OriginalCrashContactProbe& probe,terrain_original::ContactCache* cache=nullptr) {
    using namespace terrain_original;Rounding rounding;Vector start,end;
    for(unsigned k=0;k<3;++k){float delta=mul(probe.normal[k],200);start[k]=sub(probe.positionCm[k],delta);end[k]=add(probe.positionCm[k],delta);}
    return queryOriginalWorldSegment(terrain,world,start,end,0,.574999988079071f,true,cache);
}
}
