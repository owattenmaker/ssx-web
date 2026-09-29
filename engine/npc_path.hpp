#pragma once
#include "race_event.hpp"
#include <span>
namespace ssx {
struct OriginalNpcPath {OriginalRacePath geometry;uint32_t flags38=0,field3C=0;};
// Complete26AFB8/26A9B0 ordering. This is the candidate stage, before112A50
// projects paths and invokes the NPC-specific10D410 score. Original112A50
// requests at most6 candidates with requireField3C=false.
std::vector<int> originalNpcPathCandidates(std::span<const OriginalNpcPath>,std::array<float,3> position,
    size_t maximum=6,bool requireField3C=false);
}
#include <functional>
namespace ssx {
struct OriginalNpcPathScoreContext {
    std::array<float,3> position{},velocity{};
    int16_t roleE00=0;
    bool allowFlag0E04=false,randomizeE08=false;
    bool computerControlled=true; //112A50: human uses geometric minimum, never NPC score/RNG.
    int currentPathIndex=-1,followedPathIndex=-1;
    std::span<const int> npcPathIndices; // all computer riders, including self
};
float originalNpcRouteAffinity(int16_t role,bool flag3,bool flag2,bool flag1); //10D870
// Original10D410. Reads the current shared-world path occupancy; RNG is called
// only on the source branch that consumes317810, not for every candidate.
float originalNpcPathScore(const OriginalNpcPath&,int pathIndex,const OriginalNpcPathScoreContext&,
    const std::function<uint32_t()>& nextRandom);
}
namespace ssx {
struct OriginalNpcRouteState {
    int pathIndex=-1;OriginalRacePathCache cache;
    std::array<float,3> closestPoint{},lookaheadPoint{},previousLookaheadPoint{};
    float previousDistance=0,currentDistance=0,lateralDistance=0,heading=0;
};
// Original112A50 human and computer branches, including26AFB8 candidate order,
// path-end rejection,10D410 scores, switch-only cache invalidation/projection.
// Position/velocity come from context; its currentPathIndex is taken from state.
bool originalNpcSelectPath(std::span<const OriginalNpcPath>,OriginalNpcRouteState&,
    OriginalNpcPathScoreContext,bool excludeCurrent,const std::function<uint32_t()>& nextRandom);
}

namespace ssx {
// Full1125C0 postmotion AI-route progress. The original1125B8 returns0, so
// this uses the retained horizontal cache and796cm lookahead. courseRemaining
// is actor4D4 after course-progress update; tick is the shared game tick.
bool originalNpcRouteProgress(std::span<const OriginalNpcPath>,OriginalNpcRouteState&,
    OriginalNpcPathScoreContext,float courseRemaining,int32_t tick,const std::function<uint32_t()>& nextRandom);
}
namespace ssx {
// Direct26AA80 AI-path interval query (unlike course remaining-distance wrapper).
std::vector<OriginalRacePathEvent> originalNpcPathEvents(const OriginalNpcPath&,float previousDistance,
    float currentDistance,size_t maximum=12);
}
#include <optional>
namespace ssx {
struct OriginalNpcSpeedZone {float speedCmps=0;bool started=false;};
struct OriginalNpcJumpZone {
    float speedCmps=0;bool started=false;
    std::array<int32_t,4> flags{}; // Original value bits3,2,1,0.
    std::array<float,3> startPoint{},endPoint{};
};
std::optional<OriginalNpcSpeedZone> originalNpcSpeedZone(const OriginalNpcPath&,float previous,float current); //10BB18.
std::optional<OriginalNpcJumpZone> originalNpcJumpZone(const OriginalNpcPath&,float previous,float current); //10B980.
std::optional<bool> originalNpcToggleZone(const OriginalNpcPath&,float previous,float current); //10BBF8,type19.
}
