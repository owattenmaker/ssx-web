#pragma once
#include "npc_path.hpp"
namespace ssx {
// Original269F18: event-aware sample, including its by-reference distance update.
std::array<float,3> originalResetPathSample(const OriginalRacePath&,float& distance);
// Original26A8B8 segment-direction lookup, including the inclusive boundary.
std::array<float,3> originalResetPathDirection(const OriginalRacePath&,float distance);
struct OriginalResetRouteEffects {bool refreshed=false,changed=false;};
// Original112D58, distinct from ordinary NPC112A50 selection: six candidates
// require field3C, no velocity lookahead, optional path-end rejection, and
// the retained route is refreshed even when the candidate winner is unchanged.
OriginalResetRouteEffects originalResetRoute(std::span<const OriginalNpcPath>,OriginalNpcRouteState&,
    std::array<float,3> position,bool allowPathEnd);
}
