#pragma once
#include <cstdint>
#include <span>
namespace ssx {
// Original game-info states named by 0x113B48, distinct from rider motion modes.
enum class RacePhase:int32_t {None=0,GameInit=1,Freeride=2,PreRace=3,Countdown=4,Race=5,EndRace=6,Shutdown=7};
struct OriginalRaceClock {
    RacePhase phase=RacePhase::None,previous=RacePhase::None,previousHandler=RacePhase::None;
    int32_t totalTicks=0,raceTicks=0,countdownTicks=0;
    int32_t raceEnabled=0,preRaceLocal=0;
};
struct RaceClockEffects {
    RacePhase exited=RacePhase::None,entered=RacePhase::None;
    bool raceStartNotification=false,raceFinishNotification=false,requestResults=false,shutdown=false;
};
// Original113B10 selection: enter/exit work is deferred to the following tick.
void originalRaceSelect(OriginalRaceClock&,RacePhase);
// Original113C20 state dispatch plus129134 final tick increment. External
// loading/UI/network, opponent scheduling and course events remain callers.
// Runtime ordering: begin before riders, end after motion and course events.
RaceClockEffects originalRaceClockBeginTick(OriginalRaceClock&,std::span<const float> humanFinishElapsed);
void originalRaceClockEndTick(OriginalRaceClock&);
RaceClockEffects originalRaceClockStep(OriginalRaceClock&,std::span<const float> humanFinishElapsed);
// Original12A250: all configured human riders have nonnegative +470 timers.
bool originalRaceHumansFinished(std::span<const float> humanFinishElapsed);
struct OriginalRiderRaceFinish {float elapsed=-1;int32_t penaltyTicks=0,finishTicks=0;};
// Arithmetic portion12515C..125188, separately from results/audio notification.
void originalRaceMarkFinished(OriginalRiderRaceFinish&,int32_t raceTicks);
// Original12102C..12104C, once per rider tick after a finish marker is set.
void originalRaceFinishElapsedStep(OriginalRiderRaceFinish&);
}
#include <array>
#include <vector>
namespace ssx {
struct OriginalRacePathEvent {uint32_t type=0,value=0;float start=0,end=0;};
struct OriginalRacePath {
    std::array<float,3> origin{},low{},high{};
    std::vector<std::array<float,4>> segments; // authored directionXYZ,lengthW
    float remainingAtOrigin=0;
    std::vector<OriginalRacePathEvent> events; // runtime event IDs, not raw disk IDs
};
struct OriginalRacePathCache {std::array<float,3> origin{};float distance=0;int32_t segment=-1;};
struct OriginalRacePathProjection {std::array<float,3> point{};float distance=0,lateralDistance=0;};
// Complete original26A638, including horizontal metric and3000cm cached window.
OriginalRacePathProjection originalRacePathProject(const OriginalRacePath&,
    std::array<float,3> position,OriginalRacePathCache&,bool horizontal=true);
// Original26A090 wrapper and26AA80 inclusive interval filter. Caller supplies
// old/new remaining-distance progress; maximum12 is the rider's event capacity.
std::vector<OriginalRacePathEvent> originalRacePathEvents(const OriginalRacePath&,
    float previousRemaining,float currentRemaining,size_t maximum=12);
}
namespace ssx {
struct OriginalRaceCheckpoints {
    int32_t count=0;
    std::array<uint32_t,4> humanMasks{};
    uint8_t pendingHumanMask=0;
    uint32_t mode=0,field60C=0,field610=0,field61C=0,field620=0;
};
// Original270AB0 after270730 resolves the human roster index. Retains the
// source duplicate suppression and MIPS low-five-bit shift semantics.
bool originalRaceRecordCheckpoint(OriginalRaceCheckpoints&,int humanIndex,int checkpoint);
}

namespace ssx {
struct OriginalRaceProgress {
    int pathIndex=-1;float remaining=0,bestRemaining=0;
    OriginalRacePathCache cache;
};
std::array<float,3> originalRacePathSample(const OriginalRacePath&,float distance);
// Full1127F0 selection, including26B178 three-candidate bounds ordering and
// the original796cm velocity lookahead/current-path bias.
void originalRaceSelectPath(std::span<const OriginalRacePath>,OriginalRaceProgress&,
    std::array<float,3> position,std::array<float,3> velocity,bool excludeCurrent=false);
}
namespace ssx {
// Global checkpoint-bonus list at 0x4D33B8 (path manager 0x4D33A0 +0x18): six (int32 value, float remaining distance)
// entries in decreasing distance, a zero distance ends the list. Every event on ARA1/BRA2/BHP1 leaves it zero.
struct OriginalRaceBonusTable {std::array<int32_t,6> value{};std::array<float,6> distance{};};
// 112FB0 (0x113014..0x1130A8): when the new remaining distance +0x4D0 is below the best one +0x4D4, every entry with
// new <= distance < best is crossed, in list order; each crossing calls 10E558(rider, entry).
std::vector<int32_t> originalRaceBonusCrossings(const OriginalRaceBonusTable&,float bestRemaining,float remaining);
// Recovered112FB0/112338 path-progress stages for authored course events.
// Returned intervals use the original12-record capacity. Finish/checkpoint
// effect handling remains the caller, after this path state has been updated.
// With a bonus table the crossed entries' values are appended to *bonusCrossings.
std::vector<OriginalRacePathEvent> originalRaceProgressStep(std::span<const OriginalRacePath>,
    OriginalRaceProgress&,std::array<float,3> position,std::array<float,3> velocity,int32_t totalTicks,
    const OriginalRaceBonusTable* bonus=nullptr,std::vector<int32_t>* bonusCrossings=nullptr);
}
namespace ssx {
struct RaceCourseEffects {bool finished=false;uint32_t checkpointMask=0;};
// Arithmetic/event subset10E5D8. Caller supplies the original mode eligibility;
// Snow Jam's configuration game type0 takes its unconditional eligible branch.
// Other course event types must be handled by their respective game systems.
RaceCourseEffects originalRaceApplyCourseEvents(std::span<const OriginalRacePathEvent>,
    OriginalRiderRaceFinish&,OriginalRaceCheckpoints&,int humanIndex,int32_t raceTicks,bool finishEligible);
}
