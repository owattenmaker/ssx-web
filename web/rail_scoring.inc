// Additional score fields and source commit boundaries shared by rail callbacks.
#include "../engine/score_object.hpp"
// Raw original score object and HUD bank (web/score_gameplay.inc); the typed globals below mirror it.
RIDER_LOCAL static OriginalScoreObject scoreObject;
static OriginalTrickCommitResult score_commit_raw(OriginalScoreBoundaryState&,int,int,int,int,int);static void score_tick();
static OriginalTrickCommitResult score_landing();static float score_bail(bool);static float score_reset(bool);static void score_reset_stance();static void score_init();static void score_trick_start();static void score_recovery(bool);
RIDER_LOCAL static OriginalScoreBoundaryState scoreExtras;
RIDER_LOCAL static OriginalBoostPendingHudResult boostPendingDisplay{true};
RIDER_LOCAL static OriginalBoostFlashHudResult boostFlashDisplay;
RIDER_LOCAL static float boostPendingPhase=-1,boostFlashPhase=-1;
RIDER_LOCAL static OriginalBoostLetterHudResult boostLetterDisplay{true,0,0};
RIDER_LOCAL static OriginalBoostHudValues boostHudDisplay;RIDER_LOCAL static bool boostHudInitialized=false;
RIDER_LOCAL static OriginalRailScoreProfile railScoreProfile;
RIDER_LOCAL static int32_t deferredScore=0,railBankedTotal=0;
RIDER_LOCAL static unsigned railScoreCommits=0,railScoreBonuses=0;
RIDER_LOCAL static float railAwardedMeter=0;
static OriginalScoreBoundaryState capture_score(){auto s=scoreExtras;s.score=scoring;s.identity=trickIdentity;s.identity.grabs60=scoring.history60;return s;}
static void apply_score(const OriginalScoreBoundaryState&s){scoreExtras=s;scoring=s.score;trickIdentity=s.identity;originalStoreScoreView(scoreObject,s);}
static void clear_trick_scoring(){auto s=capture_score();originalResetScoreBoundary(s);apply_score(s);}
static bool reference_stance(){return physicsState.state320Equals324?physicsState.reverseStance:!physicsState.reverseStance;}
// 11A228 runs on the raw object (engine/score_object.cpp: stats, combo, HUD popups, trick name).
static OriginalTrickCommitResult commit_score_state(OriginalScoreBoundaryState& s,int stance,int field04,int style,int flag,int takeoff){
 return score_commit_raw(s,stance,field04,style,flag,takeoff);
}
static void queue_rail_score(const OriginalTrickCommitResult& result){
 if(result.points>0)committedTrickName=originalTrickName(result.identity,trickNameTables);
 deferredScore=std::bit_cast<int32_t>(uint32_t(deferredScore)+uint32_t(result.points));railBankedTotal=std::bit_cast<int32_t>(uint32_t(railBankedTotal)+uint32_t(result.points));++railScoreCommits;
}
static void award_rail_meter(float value){award_boost(value,1);physicsState.boostTierCounter=boostState.tier;railAwardedMeter=terrain_original::add(railAwardedMeter,value);}
static OriginalTrickCommitResult rail_score_entry(bool stance,int style,int flag){
 auto state=capture_score();auto result=originalScoreBoundary(state,int(stance),0,style,flag,commit_score_state);apply_score(state);queue_rail_score(result);grab={};lastScoreId=-1;return result;
}
static void rail_score_takeoff(int field04){
 auto state=capture_score();auto result=originalScoreTakeoffBoundary(state,int(!physicsState.state320Equals324),field04,commit_score_state);apply_score(state);queue_rail_score(result);award_rail_meter(result.meterDelta);
}
static OriginalRailScoreState rail_score_view(){return {scoring.accumulated14,scoreExtras.inverted1C,trickIdentity.time24,trickIdentity.spin34,scoring.multiplier1C4,trickIdentity.style20,scoring.bonusPoints84,scoreExtras.threshold98};}
static void apply_rail_score(const OriginalRailScoreState&s){scoring.accumulated14=s.accumulated14;scoreExtras.inverted1C=s.inverted1C;trickIdentity.time24=s.distance24;trickIdentity.spin34=s.spin34;trickIdentity.style20=s.style20;scoring.bonusPoints84=s.bonusPoints84;scoreExtras.threshold98=s.threshold98;}
static float score_rail_rotation(int style,float spin){auto state=rail_score_view();float result=originalRailRotationScore(state,railScoreProfile,style,spin);apply_rail_score(state);return result;}
static void tick_rail_score(){
 // 117C28 (distance/air/hold/style/timer accrual, thresholds, combo clock, 117FE0 HUD, slot clocks).
 const int32_t distanceBonus=scoreObject.i(0x98);score_tick();if(scoreObject.i(0x98)!=distanceBonus)++railScoreBonuses;
 info[12]=originalCurrentTrickPoints(scoring);
 boostHudDisplay=originalBoostHudValues({boostState.meter,scoring.accumulated14,-boostHudDisplay.preview,-1,-boostHudDisplay.stored,-1,!boostHudInitialized,!boostHudInitialized,boostState.drainEnabled});boostHudInitialized=true;
 boostLetterDisplay=originalBoostLetterHud({boostState.tier,boostLetterDisplay.removed,-boostLetterDisplay.fraction,-1});
 boostPendingDisplay=originalBoostPendingHud({boostState.tier,scoring.uberCount54,scoring.activeUber5C,boostPendingDisplay.removed,-boostPendingDisplay.fraction,-1});
 boostFlashDisplay=originalBoostFlashHud({boostState.superTime,boostState.tier,!boostFlashDisplay.present,-boostFlashDisplay.fraction,-1});
 boostPendingPhase=boostPendingDisplay.removed?-1:originalBoostPendingLetterClock(boostPendingPhase);
 boostFlashPhase=(!boostFlashDisplay.present||boostFlashDisplay.fraction==1.f)?-1:originalBoostFlashClock(boostFlashPhase,boostFlashDisplay.fraction);
}
static void reset_rail_score_tracking(){boostPendingDisplay={true};boostFlashDisplay={};boostPendingPhase=boostFlashPhase=-1;boostLetterDisplay={true,0,0};boostHudDisplay={};boostHudInitialized=false;scoreExtras={};originalResetScoreBoundary(scoreExtras);deferredScore=railBankedTotal=0;railScoreCommits=railScoreBonuses=0;railAwardedMeter=0;}

static void begin_rail_uber_score(int tier,int style){auto state=capture_score();const float delta=originalRailUberScoreBegin(state,tier,style);apply_score(state);award_rail_meter(delta);}
static void end_rail_uber_score(int style){auto state=capture_score();auto result=originalRailUberScoreEnd(state,style,commit_score_state);apply_score(state);queue_rail_score(result);award_rail_meter(result.meterDelta);}
// Shared original meter-award boundary, also used by integration fixtures.
extern "C" EMSCRIPTEN_KEEPALIVE void award_trick_meter(float amount){if(!std::isfinite(amount))throw std::runtime_error("Invalid meter award");award_rail_meter(amount);}
