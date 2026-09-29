#pragma once
#include <array>
#include <cstdint>
#include <functional>
namespace ssx {
struct OriginalUpperPeer {
    bool enabled=false,attackEligible=false;
    float distanceCm=0,bearing=0;
    uint32_t lastReactionTick=0;
};
struct OriginalUpperReactionContext {
    int upperClass=0;
    std::array<float,3> physicalForward{};
    bool reverseStance=false;
    uint32_t clockTick=0;
    uint64_t reactionMask=~uint64_t(0),lookbackMask=~uint64_t(0);
};
struct OriginalUpperReaction {
    int semantic=-1,peer=-1;
    uint64_t mask=~uint64_t(0);
    unsigned randomDraws=0;
};
// Original115D48, called at131870 after turn/crouch targets and115B58,
// before normal main-animation selection and the common1211F8 pass.
// The RNG callback must consume the one shared game generator, in actor order.
OriginalUpperReaction originalUpperReaction(float& idleSeconds,
    std::array<OriginalUpperPeer,6>&,const OriginalUpperReactionContext&,
    const std::function<uint32_t()>& random);
}
