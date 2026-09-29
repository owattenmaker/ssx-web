#include "rider_local.hpp"
#include <bit>
extern void browser_reset_pickups();
extern void advance_world_entities(); // web/roller_gameplay.inc: entity updates precede the rider
extern void browser_stage_triggers(); // web/stage_script_gameplay.inc: 121818 for the rider's selected contact
#include "../engine/race_session.hpp"
#include "../engine/collision.hpp"
#include "json.hpp"
#include "generated/event_start_seed.hpp"
#include <memory>
#include <optional>
#include <emscripten/emscripten.h>
using namespace ssx;using nlohmann::json;
RIDER_LOCAL extern Vec3 position,velocity;extern std::unique_ptr<CollisionWorld> cameraTerrain;
RIDER_LOCAL void (*browserRouteProgress)(float,int32_t)=nullptr;
RIDER_LOCAL void (*browserRaceAfterReset)()=nullptr; // streamed world (web/peak_world.inc): the location's path bank and finish rule survive a new session
extern void browser_camera_finish(int32_t raceRiders);extern void browser_score_finish();extern void browser_race_bonus(int32_t value);extern void browser_camera_race_reset();
RIDER_LOCAL static int32_t eventRiders=1; // authored roster (game-info +0x78), read by POST_RACE_1
RIDER_LOCAL static OriginalRaceEventAsset asset;RIDER_LOCAL static std::unique_ptr<OriginalRaceSession> race;RIDER_LOCAL static int32_t eventTimeLimit=0;RIDER_LOCAL static float info[8];RIDER_LOCAL static float progressOrigin=0;
void browser_camera_terrain_json(nlohmann::json& data);
nlohmann::json animation_document(const char* text); // web/animation_bridge.cpp
extern "C" {
EMSCRIPTEN_KEEPALIVE void init_terrain(const char* text){auto data=json::parse(text);browser_camera_terrain_json(data);}
}
// init_terrain's work on a parsed document (web/world_bridge.cpp terrain_part: one parse for both terrain systems, pv eventSlices).
void browser_camera_terrain_json(json& data){std::vector<TerrainPatch> patches;for(auto&p:data["patches"]){TerrainPatch t;t.resource=p["resource_id"];t.authoredFlags=p["authored_flags"];t.authoredSurface=p["authored_surface_id"];t.hasAuthoredBounds=true;auto lo=p["authored_bounds_min"],hi=p["authored_bounds_max"];t.authoredMinimum={lo[0],lo[1],lo[2]};t.authoredMaximum={hi[0],hi[1],hi[2]};for(int i=0;i<16;i++){auto c=p["coefficients"][i];t.coefficients[i]={c[0],c[1],c[2]};}patches.push_back(t);}RIDER_LOCAL extern bool browserWorldAppend;/* streamed locations (web/peak_world.inc) */if(browserWorldAppend&&cameraTerrain){cameraTerrain->setTerrainPatches(std::move(patches),true);return;}cameraTerrain=std::make_unique<CollisionWorld>(std::vector<Triangle>{});cameraTerrain->setTerrainPatches(std::move(patches));}
extern "C" {
EMSCRIPTEN_KEEPALIVE void init_race(const char* text){auto cfg=animation_document(text)["original_race_event"]["original_race_event"];/* a prepared parse (web/animation_bridge.cpp animation_prepare) or the text parsed here */asset={};auto c=cfg["clock"];asset.clock={RacePhase(int(c["phase"])),RacePhase(int(c["previous"])),RacePhase(int(c["previous_handler"])),c["total_ticks"],c["race_ticks"],c["countdown_ticks"],c["race_enabled"],c["pre_race_local"]};
 for(auto&p:cfg["paths"]){OriginalRacePath path;path.origin=p["origin"].get<std::array<float,3>>();path.low=p["low"].get<std::array<float,3>>();path.high=p["high"].get<std::array<float,3>>();path.remainingAtOrigin=p["remaining_at_origin"];path.segments=p["segments"].get<std::vector<std::array<float,4>>>();for(auto&e:p["events"])path.events.push_back({e["type"],e["value"],e["start"],e["end"]});asset.paths.push_back(std::move(path));}
 for(auto&p:cfg["participants"]){if(!p["human"].get<bool>())continue;OriginalRaceParticipant human;human.human=true;human.finish={p["finish_elapsed"],p["penalty_ticks"],p["finish_ticks"]};human.progress.pathIndex=p["path_index"];human.progress.remaining=p["remaining"];human.progress.bestRemaining=p["best_remaining"];auto cache=p["path_cache"];human.progress.cache={cache["origin"].get<std::array<float,3>>(),cache["distance"],cache["segment"]};asset.participants.push_back(human);}
 eventRiders=int32_t(cfg["participants"].size());
 auto ch=cfg["checkpoints"];asset.checkpoints.count=ch["count"];for(unsigned i=0;i<ch["human_masks"].size();i++)asset.checkpoints.humanMasks[i]=ch["human_masks"][i];progressOrigin=asset.paths.front().remainingAtOrigin;race=std::make_unique<OriginalRaceSession>(asset);
}
// Checkpoint bonus configuration (engine/race_session.hpp): the list 0x4D33B8 (6 x {int32 value, float distance}), the game
// mode byte 0x535C12, the event handler index and GMM+8, and *0x5308D0. Zero on every browser course; the host (or a
// capture comparison with poked values) sets it. Survives race resets.
RIDER_LOCAL static OriginalRaceBonusTable bonusTable;RIDER_LOCAL static int bonusMode=0,bonusHandlerIndex=1,bonusFreestyleKind=0;RIDER_LOCAL static uint32_t bonusFlags=0;
static void apply_race_bonus(){if(!race)return;race->state.bonus=bonusTable;race->state.gameMode=bonusMode;race->state.bonusHandler=bonusHandlerIndex;race->state.freestyleKind=bonusFreestyleKind;race->state.globalFlags=bonusFlags;}
EMSCRIPTEN_KEEPALIVE void set_race_bonus(const int32_t* words,int mode,int handler,int freestyleKind,uint32_t flags){
 for(unsigned k=0;k<6;k++){bonusTable.value[k]=words[2*k];bonusTable.distance[k]=std::bit_cast<float>(words[2*k+1]);}
 bonusMode=mode;bonusHandlerIndex=handler;bonusFreestyleKind=freestyleKind;bonusFlags=flags;apply_race_bonus();}
RIDER_LOCAL static bool rollingRacePending=false; // backcountry rolling start (docs/backcountry.md)
EMSCRIPTEN_KEEPALIVE void reset_race(){rollingRacePending=false;browser_camera_race_reset();browser_reset_pickups();progressOrigin=asset.paths.front().remainingAtOrigin;race=std::make_unique<OriginalRaceSession>(asset);race->state.timeLimitTicks=eventTimeLimit;apply_race_bonus();if(browserRaceAfterReset)browserRaceAfterReset();}
// Freestyle events (web/career-ui.js): the run time limit in ticks (0x440B38 seconds x 60; 0 = untimed race). Kept across
// race resets; 125228 in OriginalRaceSession::endTick ends the run with a DNF once it passes.
EMSCRIPTEN_KEEPALIVE void race_time_limit(int32_t ticks){eventTimeLimit=ticks>0?ticks:0;if(race)race->state.timeLimitTicks=eventTimeLimit;}
EMSCRIPTEN_KEEPALIVE int race_timed_out(){return race&&race->state.timedOut?1:0;}
// Live limit GMM+0x78: slope style checkpoints (0x2398E8, freestyle kind 1) add value x 60 ticks during the run; the HUD clock
// 1EC3F8 counts down from it (web/freestyle-event.js, docs/slopestyle-bigair.md).
EMSCRIPTEN_KEEPALIVE int race_time_limit_now(){return race?race->state.timeLimitTicks:eventTimeLimit;}
// Pause menu Give Up (career-ui.js; original 0x20DA58 -> 1253D0): DNF the unfinished human; the next race tick finishes the run
// (125228/125368), which shows the TIME'S UP banner and then the results like a timeout.
EMSCRIPTEN_KEEPALIVE void race_give_up(){if(race)race->giveUp();}
// Streamed world runs (web/peak_world.inc): the race clock runs from 0 without a countdown (free ride: each location entry;
// the peak runs: the objectives card's Continue, peakrun notes). The finish marker is cleared.
EMSCRIPTEN_KEEPALIVE void race_clock_restart(){if(!race)return;auto& c=race->state.clock;c.phase=c.previous=c.previousHandler=RacePhase::Race;c.raceTicks=0;c.countdownTicks=0;c.raceEnabled=1;race->state.finish={};race->state.timedOut=false;}
// 10E5D8 finish eligibility for the streamed world (web/peak_world.inc sets it from the event kind and the current course).
EMSCRIPTEN_KEEPALIVE void race_finish_eligible(int eligible){if(race)race->state.finishEligible=eligible!=0;}
// A computer rider's core instance (web/npc_gameplay.inc) tracks its own course progress/finish.
RIDER_LOCAL static std::optional<OriginalRaceParticipant> eventParticipantOverride;RIDER_LOCAL static float eventProgressOriginOverride=0;
void set_event_participant(const OriginalRaceParticipant& participant,float origin){eventParticipantOverride=participant;eventProgressOriginOverride=origin;}
void begin_event_clock(){browser_camera_race_reset();auto event=asset;event.participants={eventParticipantOverride?*eventParticipantOverride:browserEventParticipant()};event.clock={RacePhase::PreRace,RacePhase::None,RacePhase::None,0,0,0,1,0};race=std::make_unique<OriginalRaceSession>(event);race->state.timeLimitTicks=eventTimeLimit;apply_race_bonus();progressOrigin=eventParticipantOverride?eventProgressOriginOverride:browserEventProgressOrigin;const float unfinished=-1;originalRaceClockBeginTick(race->state.clock,{&unfinished,1});/*rolling start: the ready state is PreRace; the overlay's Continue selects Race (233AA0) before game tick 0, whose update enters it and counts race tick 1 (PS2 bc-race-idle: phase 3 at record 0, phase 5 / race tick 1 at record 1)*/rollingRacePending=browserEventRolling;if(!browserEventRolling)originalRaceSelect(race->state.clock,RacePhase::Countdown);std::fill(std::begin(info),std::end(info),0.f);}
int event_phase(){return race?int(race->state.clock.phase):0;}int event_ticks(){return race?race->state.clock.raceTicks:0;}
EMSCRIPTEN_KEEPALIVE float* race_result_info(){RIDER_LOCAL static float result[6];std::fill(std::begin(result),std::end(result),0.f);if(!race)return result;const auto&s=race->state;result[0]=s.finish.elapsed>=0;result[1]=s.finish.finishTicks;result[2]=s.finish.penaltyTicks;result[3]=s.finish.elapsed;result[4]=s.clock.raceTicks;result[5]=int(s.clock.phase);return result;}
// Standings inputs (rider +0x4D0 current / +0x4D4 best remaining, +0x470 finish marker, +0x478 finish ticks).
float browser_finish_elapsed(){return race?race->state.finish.elapsed:-1.f;}
void browser_finish_elapsed_min(float v){if(race&&race->state.finish.elapsed<v)race->state.finish.elapsed=v;} // 12C964 max.s
EMSCRIPTEN_KEEPALIVE float* race_progress_info(){RIDER_LOCAL static float v[8];std::fill(std::begin(v),std::end(v),0.f);if(!race)return v;const auto&s=race->state;v[0]=s.progress.remaining;v[1]=s.progress.bestRemaining;v[2]=s.progress.pathIndex;v[3]=s.finish.elapsed;v[4]=s.finish.finishTicks;v[5]=s.finish.penaltyTicks;v[6]=s.clock.raceTicks;v[7]=int(s.clock.phase);return v;}
EMSCRIPTEN_KEEPALIVE void race_begin(){advance_world_entities();if(race){if(rollingRacePending){originalRaceSelect(race->state.clock,RacePhase::Race);rollingRacePending=false;}race->beginTick();}}
// Six-rider races (web/ai-racers.js): the other riders' 121750 passes precede this rider's 121818.
EM_JS(void,js_rider_before_progress,(),{if(Module.riderHost&&Module.riderHost.beforeProgress)Module.riderHost.beforeProgress();});
// 0x239230 / 0x23A05C at a freestyle finish: the host ranks the run with the round's posted scores (Module.finishHost.place, main.js ->
// career.js finishPlace; compare-ps2-capture.mjs --finish-place); -1: no freestyle standing (races keep their own rule).
EM_JS(int,js_finish_place,(int score),{return Module.finishHost&&Module.finishHost.place?Module.finishHost.place(score):-1;});
extern "C" void finish_standing(int place);extern "C++" int32_t browser_run_score(); // (this part of the file is inside extern "C")
EMSCRIPTEN_KEEPALIVE float* race_end(){js_rider_before_progress();browser_stage_triggers(); /*121818: stage trigger programs (web/stage_script_gameplay.inc)*/if(!race)return info;race->endTick({float(position.x*100),float(-position.z*100),float(position.y*100)},{float(velocity.x*100),float(-velocity.z*100),float(velocity.y*100)});auto&s=race->state;for(int32_t value:s.bonusAwards)browser_race_bonus(value); /*112FB0 -> 10E558 -> 1194C0*/if(s.courseEffects.finished){browser_score_finish();{const int place=js_finish_place(browser_run_score());if(place>=0)finish_standing(place);} /*before 121818's route passes and 117C28: the HUD meter slots follow at once (crows-invert 2079)*/browser_camera_finish(eventRiders);}if(browserRouteProgress)browserRouteProgress(s.progress.bestRemaining,s.clock.totalTicks-1);info[0]=s.clock.raceTicks/60.f;info[1]=100*(1-s.progress.remaining/progressOrigin);info[2]=s.courseEffects.finished;info[3]=s.clockEffects.requestResults;info[4]=s.finish.elapsed;info[5]=int(s.clock.phase);info[6]=s.clock.countdownTicks;info[7]=s.clockEffects.raceStartNotification;return info;}
}
// A streamed location's race path bank (web/peak_world.inc, 12A340 -> 112180). Returns false without a race session.
bool browser_race_replace_paths(std::vector<ssx::OriginalRacePath> paths,int pathIndex){
 if(!race)return false;race->replacePaths(std::move(paths),pathIndex,{float(position.x*100),float(-position.z*100),float(position.y*100)});return true;
}
