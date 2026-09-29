#pragma once
#include "grab_score.hpp"
#include "trick_identity.hpp"
#include "trick_bonus.hpp"
namespace ssx {
struct OriginalTrickCommitProfile {
 OriginalTrickIdentityProfile identity;
 OriginalNamedTrickTable named;
 float namedPointScale=0,scoreScale=0,spinScale=0,flipScale=0,invertedPointScale=10000.f;
};
struct OriginalTrickCommitResult {
 OriginalTrickIdentity identity{};
 int32_t namedBonus=0,repeats=0,points=0,committedUbers=0,invertedPoints=0;
 float meterDelta=0;
};
void originalTrickRotationScore(OriginalGrabScoreState&,OriginalTrickIdentityState&,const OriginalTrickCommitProfile&,float spin,float flip);
// Ordinary single-event award arithmetic in11A228. Persistent race statistics
// and UI/event dispatch are caller-owned; this does not reset the scoring state.
// Optional inverted reward uses117908: separately rounded points, outside
// trick/repeat multipliers, without increasing the returned boost delta.
OriginalTrickCommitResult originalOrdinaryTrickCommit(OriginalGrabScoreState&,
 OriginalTrickIdentityState&,OriginalTrickHistory&,const OriginalTrickCommitProfile&,
 const OriginalTrickIdentityInput&,float trickMultiplier,float invertedReward=0);
}
