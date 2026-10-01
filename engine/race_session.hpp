#pragma once
#include "race_event.hpp"
#include "terrain_contact_math.hpp"
#include <bit>
#include <stdexcept>
#include <string>
namespace ssx {
// Original 125228 time-limit test: (float)(u32)limit * 1/60 (0x3C888889, EE chop mul), truncated by __fixsfsi 4139F8,
// times 60, strictly below the race ticks (modeobj+0xC). The caller applies the gates (unfinished +0x470 < 0,
// mode byte 0x535C10 != 4, timed GMM+0x88, bit 9 of *0x5308D0 clear).
inline bool originalRaceTimeLimitExpired(int32_t limitTicks,int32_t raceTicks){
    terrain_original::Rounding rounding;const uint32_t u=uint32_t(limitTicks);
    const float value=int32_t(u)<0?terrain_original::add(float(int32_t((u>>1)|(u&1))),float(int32_t((u>>1)|(u&1)))):float(int32_t(u));
    const float seconds=terrain_original::mul(value,std::bit_cast<float>(0x3c888889u));
    return int32_t(seconds)*60<raceTicks;
}
struct OriginalRaceParticipant {
    bool human=false;int motionMode=0,controlState=0,dnf=0;
    std::array<float,3> position{},velocity{};std::array<float,4> quaternion{};
    OriginalRiderRaceFinish finish;OriginalRaceProgress progress;
};
struct OriginalRaceEventAsset {
    OriginalRaceClock clock;std::vector<OriginalRacePath> paths;
    std::vector<OriginalRaceParticipant> participants;OriginalRaceCheckpoints checkpoints;
    std::array<int,4> configuration{};std::string sourceEEHash,courseHash;
};

struct OriginalRaceSessionSnapshot {
    OriginalRaceClock clock;
    OriginalRaceProgress progress;
    OriginalRiderRaceFinish finish;
    OriginalRaceCheckpoints checkpoints;
    RaceClockEffects clockEffects;
    RaceCourseEffects courseEffects;
    std::vector<OriginalRacePathEvent> unhandledEvents;
    // Freestyle time limit (GameModeMan+0x78 = 0x1454F8(course,round) x 60 when GMM+0x88 is set; 0 = untimed) and
    // rider+0x480 (DNF: set by the 125228 timeout or by the pause menu Give Up 1253D0; the HUD banner 1EB9E8 reads it:
    // state+0x88 = +0x480 ? 2 (TIME'S UP) : 1 (FINISH!)).
    int32_t timeLimitTicks=0;bool timedOut=false;
    // Checkpoint bonus (112FB0 -> 10E558 -> 1194C0), human only. gameMode = byte 0x535C12 (1 = slope style), handler =
    // the event handler index *(G+0xC0)+4 (table 0x536668), globalFlags = *0x5308D0 (bit 9 disables it). bonusAwards
    // lists this tick's accepted values in order (the caller posts 117B88(score, 0x29, value, 0, 2.5) and, for a
    // rider with +0x874 && +0x87C, the Arcade_Bonus speech 2A3CE8(audio, rider, 1)).
    OriginalRaceBonusTable bonus;int gameMode=0,bonusHandler=1,freestyleKind=0;uint32_t globalFlags=0; //freestyleKind = GMM+8 (the event object G+0xC0: 2 on the BHP1 pipe)
    std::vector<int32_t> bonusAwards;
    // 10E5D8 finish eligibility (a type-1 course event finishes the rider only when true): event kind 0x535C10 0..3; kinds
    // 5/6 (peak runs) with GMM+0x98; or the current course index 1 or 5..13; or a single event (0x535C11). The host sets it
    // for the streamed world (web/peak_world.inc); every race event keeps the default.
    bool finishEligible=true;
};
// One human's recovered event lifecycle; opponent movement and event-side
// audio/results UI remain external systems. No recorded positions are replayed.
class OriginalRaceSession {
    std::vector<OriginalRacePath> paths;
    bool tickOpen=false;
public:
    OriginalRaceSessionSnapshot state;
    explicit OriginalRaceSession(const OriginalRaceEventAsset& asset):paths(asset.paths) {
        if(asset.configuration[0]!=0)throw std::runtime_error("Race session game type not recovered");
        const OriginalRaceParticipant* human=nullptr;
        for(const auto& participant:asset.participants)if(participant.human){
            if(human)throw std::runtime_error("Multiple-human race scheduling not implemented");human=&participant;
        }
        if(!human)throw std::runtime_error("Race session has no human");
        state.clock=asset.clock;state.progress=human->progress;state.finish=human->finish;state.checkpoints=asset.checkpoints;
    }
    // Pause menu "Give Up" (0x20DA58 -> 1253D0(human)): an unfinished rider (+0x470 < 0) gets +0x480 = 1 and the give-up
    // flag 0x5366D0[slot] = 1; the menu closes (gp-0x9F4 = 3) and the next game tick's 125228 finishes the run (below).
    void giveUp(){if(state.finish.elapsed<0)state.timedOut=true;}
    // 12A340 (a streamed location with id < 22 delivers its AIP: 26AF00 clears the bank 0x4D33A0, 26AD70 loads it) then
    // 112180(rider, 0): the human is re-attached to the race path of its region row (26B5E0 kind 1): fresh path cache
    // (+0xAC0 +0x14 = -1), 26A638 distance d, +0x4D0 = +0x4D4 = remainingAtOrigin - d, or remainingAtOrigin itself when
    // it is below d (c.lt.s / bc1fl at 0x1122B0).
    void replacePaths(std::vector<OriginalRacePath> next,int pathIndex,std::array<float,3> position){
        if(next.empty()){paths.clear();state.progress.pathIndex=-1;return;} // 12A490 -> 26ADA0 at the location's eviction: no bank until the next one
        if(pathIndex<0||size_t(pathIndex)>=next.size())throw std::runtime_error("Region row race path outside the bank");
        paths=std::move(next);auto& p=state.progress;p.pathIndex=pathIndex;p.cache.segment=-1;
        const auto projection=originalRacePathProject(paths[size_t(pathIndex)],position,p.cache,true);
        const float r=paths[size_t(pathIndex)].remainingAtOrigin;terrain_original::Rounding rounding;
        p.remaining=p.bestRemaining=r<projection.distance?r:terrain_original::sub(r,projection.distance);
    }
    const std::vector<OriginalRacePath>& pathBank() const {return paths;}
    void beginTick() {
        if(tickOpen)throw std::runtime_error("Race tick already open");
        state.clockEffects=originalRaceClockBeginTick(state.clock,{&state.finish.elapsed,1});
        originalRaceFinishElapsedStep(state.finish);state.courseEffects={};state.unhandledEvents.clear();tickOpen=true;
    }
    // held: rider+0xAC4 (an NIS hold, 123640): 121818 skips 112338, so the course progress (+0x4D0 / +0x4D4, the path cache) and its
    // events stay as they are (PS2 c0a-ws13: the human's +0x4D0 = 353496 from WS13's start-row attach through the gondola, records
    // 14017..14317, though the NIS carries it far off the course).
    void endTick(std::array<float,3> position,std::array<float,3> velocity,bool held=false) {
        if(!tickOpen)throw std::runtime_error("Race tick was not begun");
        std::vector<int32_t> crossings;state.bonusAwards.clear();
        // A streamed world between a bank's eviction and the next location's read has no bank (112338 finds no path).
        auto events=paths.empty()||held?std::vector<OriginalRacePathEvent>{}:originalRaceProgressStep(paths,state.progress,position,velocity,state.clock.totalTicks,&state.bonus,&crossings);
        for(int32_t value:crossings){
            if(state.globalFlags&0x200u)continue;if(state.gameMode!=1)continue; //10E558: bit 9 of 0x5308D0, then 0x535C12 == 1
            bool accept=false; //1194C0 -> 238510 -> handler vfunc +0x30
            switch(state.bonusHandler){
            case 1:case 2:case 5:case 6:accept=true;break; //0x244530
            case 0: //0x2398E8 (freestyle handler): GMM+8 == 1 adds value x 60 ticks to the limit GMM+0x78 and accepts unless the new limit is below
                    //*(*(G+0x84)+0xC)+8 (sltu at 0x239920; +8 is the TOTAL tick count, +0xC the race ticks that 125228 reads: savestates
                    //kick-doubt-full.tick418 +8 = 418, +0xC = 238); any other kind accepts
                if(state.freestyleKind==1){state.timeLimitTicks=int32_t(uint32_t(state.timeLimitTicks)+uint32_t(value)*60u);accept=!(uint32_t(state.timeLimitTicks)<uint32_t(state.clock.totalTicks));}else accept=true;break;
            default:throw std::runtime_error("Checkpoint bonus handler "+std::to_string(state.bonusHandler)+" (rival modes 0x23B5F8/0x23C560) is not ported");
            }
            if(accept)state.bonusAwards.push_back(value);
        }
        state.courseEffects=originalRaceApplyCourseEvents(events,state.finish,state.checkpoints,0,state.clock.raceTicks,state.finishEligible);
        // 125228 (per rider from 121818, after the course events 112338/1125C0): an unfinished rider of a timed event whose
        // race ticks pass int(limit/60)*60 (strict, so tick 7201 for 2:00) gets +0x480 = 1 (DNF) and the finish
        // routine 125108 (1193E0 payout by the caller, +0x478 = ticks + penalty, +0x470 = 0).
        if(state.timeLimitTicks>0&&state.finish.elapsed<0&&state.clock.phase==RacePhase::Race&&originalRaceTimeLimitExpired(state.timeLimitTicks,state.clock.raceTicks)){
            originalRaceMarkFinished(state.finish,state.clock.raceTicks);state.courseEffects.finished=true;state.timedOut=true;}
        // 125368 (timed or not, when the limit has not expired): +0x480 == 1 already (Give Up) and the game screen is back
        // (*(gp-0x848)+0x84 +0x214 == 4, i.e. unpaused) -> the same finish routine 125108, without the time-up sound 2A3C00.
        else if(state.timedOut&&state.finish.elapsed<0&&state.clock.phase==RacePhase::Race){
            originalRaceMarkFinished(state.finish,state.clock.raceTicks);state.courseEffects.finished=true;}
        for(const auto& event:events)if(event.type!=1&&event.type!=11)state.unhandledEvents.push_back(event);
        originalRaceClockEndTick(state.clock);tickOpen=false;
    }
};
}
