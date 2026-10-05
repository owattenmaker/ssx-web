// Original trick-score object *(rider+0x790) and its HUD message bank (engine/score_object.hpp,
// docs/tricks-scoring.md). The typed views (scoring / trickIdentity / scoreExtras / trickHistory)
// used by the older verified helpers stay authoritative between calls; push_score() mirrors them
// into the raw object before an original routine runs on it and pull_score() reloads them after.
static const OriginalScoreTables scoreTables=originalScoreTables();
RIDER_LOCAL static uint32_t scoreCommits=0,scoreComboPayouts=0,scoreGlobalTick=0;RIDER_LOCAL static int32_t scoreLastPoints=0;
static void push_score(){originalStoreScoreView(scoreObject,capture_score());originalStoreScoreHistory(scoreObject,trickHistory);}
static void pull_score(){auto v=originalScoreView(scoreObject);scoreExtras=v;scoring=v.score;trickIdentity=v.identity;trickHistory=originalScoreHistory(scoreObject);}
// 12A250(race clock): every human rider has finished (+0x470 >= 0) -> 11A228 returns at once (no commit, no combo clock).
// A computer-rider core learns the humans' state from web/ai-racers.js (set_humans_finished); -1 = this core's own rider.
extern "C" float browser_finish_elapsed();
RIDER_LOCAL static int browserHumansFinished=-1;
extern "C" EMSCRIPTEN_KEEPALIVE void set_humans_finished(int finished){browserHumansFinished=finished;}
static bool humans_finished(){return browserHumansFinished>=0?browserHumansFinished!=0:browser_finish_elapsed()>=0.f;}
// 119EF8(score, kind, amount) -> 1597B0 -> 159818 -> 150A90: every career award (trick 0 / pickup points 1 / combo 2 /
// collectible 3 / Big Challenge 4) adds to the character block's cash C+0xAC4, which 117FE0 shows in HUD slot 0x19 (150960)
// on the same tick. The core keeps the human's cash (the host sets it before every run, set_score_career_cash) and queues
// the score kinds 0..2 for the career save (score_career_events; kinds 3 / 4 reach it through the collect and mission
// queues). 0x535C10 == 4 && 0x535C11 == 0 (Conquer the Mountain free ride, Big Challenges too): 11A228 / 119608 / 117718
// pay min(points / 500, 20) through 119EF8 instead of the points popups (PS2 runs/peak2/fr-d-glide 2371: 690 -> $1).
static bool score_points_to_career(); // web/stage_script_gameplay.inc (0x535C10 / 0x535C11)
RIDER_LOCAL static int32_t scoreCareerCash=0;
RIDER_LOCAL static std::vector<int32_t> scoreCareerEvents;
static void score_career_award(int kind,int32_t amount){
 scoreCareerCash=int32_t(uint32_t(scoreCareerCash)+uint32_t(amount));
 if(kind>=0&&kind<=2&&scoreCareerEvents.size()<2*256)scoreCareerEvents.insert(scoreCareerEvents.end(),{kind,amount});
}
static OriginalScoreEnvironment score_environment(){
 OriginalScoreEnvironment e;
 e.pointsToCareer=score_points_to_career();e.slot19Points=scoreCareerCash;
 e.commitBlocked=humans_finished();
 e.velocity={physicsState.velocity[0],physicsState.velocity[1],physicsState.velocity[2],0.f};
 e.timeScale=physicsState.timeScale;e.up2=physicsState.boardUp[2];
 e.motionMode=npc_motion_mode();e.control=browser_control_state();
 e.owner2C0=crash.control.phase;e.owner330=crash.control.recovery70; // crash control object owner+0x2C0 (+0x70 = +0x330)
 e.air18=air.progressFlip;e.air1C=air.progressSpin;e.air50=air.inputAngle; // air control object owner+0x230
 e.stick2A4=prewind.spin.current;e.stick2B0=prewind.flip.current;       // rider+0x2A4/+0x2B0 filtered right stick
 e.globalTick=scoreGlobalTick; // 1298C8 frame counter (the capture tick); only phases the spin dial wobble
 e.superTime2F0=boostState.superTime;e.meter2F8=boostState.meter;e.tier2F4=boostState.tier;e.drain304=boostState.drainEnabled;
 return e;
}
static std::string score_trick_name(const OriginalTrickIdentity& id){return originalTrickName(id,trickNameTables);}
void browser_mission_trick(const std::array<uint32_t,2>& record); // web/mission_gameplay.inc: 309918 (WScript event kind 3)
// 118FF8: the name (116950) for the HUD text, then 309918 posts the WScript trick event with the same record. The score object
// calls its name builder only there (engine/score_object.cpp, 11A228 / 11A168 with the HUD present).
static std::string score_trick_post(const OriginalTrickIdentity& id){std::string name=score_trick_name(id);browser_mission_trick(id);return name;}
// 11A168 / 11A228 -> 29B7E0: named-trick arcade commentary (audio observer only).
// 119EF8's career side (score_career_award above) on every score routine that can pay one.
static const OriginalScoreHooks audioScoreHooks{score_career_award,[](const OriginalTrickIdentity&){audio_event(AE_NAMED_TRICK);},[]{return scoreCareerCash;}};
// 11A228 on the raw object for a boundary in flight: the typed state `s` is the caller's copy.
static OriginalTrickCommitResult score_commit_raw(OriginalScoreBoundaryState& s,int stance,int field04,int style,int flag,int takeoff){
 s.identity.grabs60=s.score.history60;originalStoreScoreView(scoreObject,s);originalStoreScoreHistory(scoreObject,trickHistory);
 const int32_t ubers=scoreObject.i(0x54),runUbers=scoreObject.i(0x114);
 auto r=originalScoreCommit(scoreObject,commitProfile,stance,field04,style,flag,takeoff,reference_stance(),score_environment(),score_trick_post,audioScoreHooks);
 if(ubers>0)audio_event(AE_COMBO,float(ubers),float(runUbers)); /*11A598..11A5B0: 29B430(rider, +0x54, +0x114) with the run count read before 11A5CC adds this commit's Ubers*/
 s=originalScoreView(scoreObject);trickHistory=originalScoreHistory(scoreObject);
 ++scoreCommits;if(r.points>0)scoreLastPoints=r.points;
 OriginalTrickCommitResult out;out.identity=r.identity;out.repeats=r.repeats;out.points=r.points;out.committedUbers=ubers;out.meterDelta=r.meterDelta;
 return out;
}
// 117C28 once per tick from 121818 (race_end route progress), after the rider update.
static void score_tick(){
 push_score();const int32_t combos=scoreObject.i(0x134);
 originalScoreTick(scoreObject,scoreTables,score_environment(),audioScoreHooks);++scoreGlobalTick;
 if(scoreObject.i(0x134)!=combos)++scoreComboPayouts;
 pull_score();
}
// 10E910 (from landing 139C88): 119D40(score,(320^324)!=0, normal.z<0.7, 0, +0x330), Uber tier
// progression from the committed Uber count (+0x114), 10E098 category 1 and +0x2EC.
// 149778 (the "great trick" test of 10E910 for reaction kind 1): false for a rider whose 0x534FE0 record has bit 2 set (1477E8,
// not modelled: no such rider in the browser); else the landing's total gain >= table 0x45A2B0[clamp(level - 1, 0, 10)], level =
// 148950: max(1, sum of the seven attribute bytes / 5 (each truncated) / 7). The +0xB34 level override is not read there.
static bool score_great_trick(int32_t gain){
 static constexpr std::array<int32_t,11> thresholds{3000,5000,7000,9000,12000,15000,20000,25000,30000,40000,50000};
 int32_t sum=0;for(int32_t raw:browserAttributeRaw)sum+=raw/5;const int32_t level=std::max(1,sum/7);
 return gain>=thresholds[std::clamp(level-1,0,10)];
}
// 10E910(rider, a1, a2, a3): 119D40(score, stanceDiffers, a1, a2, a3) then the rest; the landing 139C88 passes (normal.z < 0.7,
// 0, +0x330), 1057B8's landing on a scenery top (0, 0, 0).
static OriginalTrickCommitResult score_landing_args(int a1,int a2,int a3){
 const int32_t before=scoreObject.i(0x114),combosBefore=scoreObject.i(0x134),totalBefore=scoreObject.i(0x198);
 auto state=capture_score();
 auto result=originalScoreBoundary(state,int(!physicsState.state320Equals324),a1,a2,a3,score_commit_raw);
 apply_score(state);
 const int32_t after=scoreObject.i(0x114);
 if(physicsAttached){
  originalLandingUberProgression(boostState,before,after);award_boost(result.meterDelta,1);
  if(0<result.meterDelta)boostState.modifier=0;physicsState.boostTierCounter=boostState.tier;
  // 10EA28..10EAA4 (human riders): a combo paid out at this landing -> reaction 3, else committed Ubers -> 2 (Uber landing),
  // else a "great trick" (149778) -> 1 (315 + 29FF80 easy speech in 115B58; PS2 trick-stall 724).
  if(browserHumanRider){const int kind=combosBefore<scoreObject.i(0x134)?3:before<after?2:score_great_trick(scoreObject.i(0x198)-totalBefore)?1:0;
   // +0x354 = the logic tick 1298C8 of this landing's 10E910, as the other 10E028 requests: 115B58 expires the request at age 180
   // (runs/riders/nate-uber-b: requested at 1081, expired at 1261 unplayed; motionTick, one ahead, played 315).
   if(kind){upperRequest358=kind;upperRequestTick354=int32_t(controllerGround.logicTick);}}
 }
 return result;
}
static OriginalTrickCommitResult score_landing(){return score_landing_args(int(physicsState.normal[2]<std::bit_cast<float>(0x3f333333u)),0,board_press_style());}
// 119E38 from 114298 (charged, passive and controller takeoffs): commit with t1=1, then 10E098.
void browser_score_takeoff(bool ramp){
 auto state=capture_score();
 auto result=originalScoreTakeoffBoundary(state,int(!physicsState.state320Equals324),int(ramp),score_commit_raw);
 apply_score(state);queue_rail_score(result);award_rail_meter(result.meterDelta);
}
// 1194C0 (checkpoint bonus from 112FB0 -> 10E558, accepted by the event handler; web/race_bridge.cpp): the HUD popup
// 117B88(score, 0x29, value, 0, 2.5) and, for a human with +0x874 && +0x87C (always set for the player), the
// Arcade_Bonus speech 2A3CE8(audio, rider, 1).
void browser_race_bonus(int32_t value){push_score();originalHudPostPoints(scoreObject,0x29,value,0,2.5f);pull_score();audio_event(AE_ARCADE_BONUS,float(value));}
// 1193E0 from the rider finish 125108: pays out the open combo (117718) so it reaches the total (+0x198).
void browser_score_finish(){push_score();originalScoreComboExpire(scoreObject,score_environment(),audioScoreHooks);pull_score();}
int32_t browser_run_score(){return scoreObject.i(0x198);} // the run's score +0x198 (web/race_bridge.cpp freestyle finish)
// 119B08 (hard crash 10EB30): lost-point popups, crash counters, reset and the -0.25 bail penalty.
static float score_bail(bool attacked){push_score();const float penalty=originalScoreBail(scoreObject,attacked);pull_score();return penalty;}
// 119C98 + 10E098 (air control 133734): a new D-pad trick records the finished rotation's name.
static void score_trick_start(){
 push_score();const float delta=originalScoreTrickStart(scoreObject,commitProfile,int(!physicsState.state320Equals324),reference_stance(),score_trick_post,audioScoreHooks);pull_score();
 if(delta!=0)award_rail_meter(delta);
}
// 10F280 (crash recovery 12D4E8): 119BB0 then 10E098 category 1.
static void score_recovery(bool quick){push_score();const float delta=originalScoreRecovery(scoreObject,quick);pull_score();
 if(delta!=0){award_boost(delta,1);physicsState.boost=boostState.amount;}}
// 119368 (reset placement 12F398): returns the boost change the caller awards.
static float score_reset(bool penalize){push_score();const float delta=originalScoreCrash(scoreObject,penalize,boostState.meter);pull_score();return delta;}
// 119E38(score,0,0) at the end of the reset fade.
// 11A088 (116120 for reset reasons 1 and 4): HUD text message 0x33 "Wrong Way!" (0x457888) for 1.5 s through 117AE8.
static void score_wrong_way(){push_score();if(scoreObject.hudPresent)originalHudPostText(scoreObject,0x33,"Wrong Way!",0,1.5f);pull_score();}
static void score_reset_stance(){auto state=capture_score();originalScoreTakeoffBoundary(state,0,0,score_commit_raw);apply_score(state);}
// 119E38(score, +0x320 != +0x324, 0) of a crash get-up that ends in the air (12D848, 0x12D984); its meter result is unused.
static void score_crash_air_exit(bool stanceDiffers){auto state=capture_score();originalScoreTakeoffBoundary(state,int(stanceDiffers),0,score_commit_raw);apply_score(state);}
// 117248 construction (+1C4=1, bank cleared) then 117540 run reset (11D390 at event start):
// statistics, history, combo and HUD bank. The typed views are reloaded from the result.
static void score_init(){scoreObject={};scoreObject.f(0x1C4)=1;originalScoreRunReset(scoreObject);scoreCommits=scoreComboPayouts=0;scoreLastPoints=0;pull_score();}

extern "C" {
// 10E468 (crash attack on another rider, from 107E70) on the ATTACKER: 119400 counts the knockout (+0x128, the career
// KOPeopleRace stat), posts HUD popup 0x2C for 1.5 s and returns 1.0, which 10E098 awards in category 2.
EMSCRIPTEN_KEEPALIVE void pair_knockout(){push_score();scoreObject.u(0x128)+=1u;originalHudPostPoints(scoreObject,0x2C,0,0,1.5f);pull_score();
 award_boost(1.f,2);physicsState.boost=boostState.amount;}
// Peak runs (docs/peak3.md "Splits"): the station split 23B5F8 / 23C560 posts 1195A8 / 1195D8 = 117B88(score, 0x2A time /
// 0x2B points, value, 0, 5.0); web/free-ride.js posts it at the 22D088 into a station, web/trick-hud.js draws it.
EMSCRIPTEN_KEEPALIVE int score_hud_post(int type,int value,float duration){push_score();const int k=originalHudPostPoints(scoreObject,type,value,0,duration);pull_score();return k;}
// The career cash of the human's character block (C+0xAC4, 150960) before a run (web/stage-collect.js collectStart; the
// capture comparer seeds it from HUD slot 0x19). 119EF8 awards add to it in the core.
EMSCRIPTEN_KEEPALIVE void set_score_career_cash(int32_t cash){scoreCareerCash=cash;}
EMSCRIPTEN_KEEPALIVE int32_t score_career_cash(){return scoreCareerCash;}
// Score cash since the last call (119EF8 kinds 0 trick / 1 point pickup / 2 combo): [count, (kind, amount)...] for the
// career save (web/stage-collect.js collectPoll -> web/career.js earnCash).
EMSCRIPTEN_KEEPALIVE int32_t* score_career_events(){
 RIDER_LOCAL static std::vector<int32_t> out;out.assign(1,int32_t(scoreCareerEvents.size()/2));
 out.insert(out.end(),scoreCareerEvents.begin(),scoreCareerEvents.end());scoreCareerEvents.clear();return out.data();
}
// QA: raw score object (0x1D0 bytes) followed by the first six words of each of the 44 HUD slots,
// in the PS2 capture layout (tools/ps2_capture.py SCORE_RECORD / HUD_SLOTS_RECORD).
EMSCRIPTEN_KEEPALIVE uint32_t* score_object_dump(){
 RIDER_LOCAL static std::array<uint32_t,OriginalScoreObject::kBytes/4+kOriginalHudSlotCount*6> out{};push_score();
 std::memcpy(out.data(),scoreObject.w.data(),OriginalScoreObject::kBytes);
 for(int k=0;k<kOriginalHudSlotCount;k++)std::memcpy(out.data()+OriginalScoreObject::kBytes/4+6*k,&scoreObject.hud[k],24);
 return out.data();
}
// QA: seed the raw object and slot headers from a capture record (same layout as the dump).
EMSCRIPTEN_KEEPALIVE void score_object_seed(const uint32_t* words,uint32_t globalTick){
 scoreGlobalTick=globalTick;
 std::memcpy(scoreObject.w.data(),words,OriginalScoreObject::kBytes);
 for(int k=0;k<kOriginalHudSlotCount;k++){std::memcpy(&scoreObject.hud[k],words+OriginalScoreObject::kBytes/4+6*k,24);
  auto& s=scoreObject.hud[k];if(s.type!=kOriginalHudFree&&s.type!=0&&s.type!=0xE)std::snprintf(s.text.data(),16,"%d",s.points);}
 pull_score();
}
// HUD: 44 slots x {type, maximum, value, arg, field10, points} as floats, then the slot texts
// (0x80 bytes each) for types 0 (trick name) and 0xE (landing message).
EMSCRIPTEN_KEEPALIVE float* score_hud_slots(){
 RIDER_LOCAL static std::array<float,kOriginalHudSlotCount*6+4> out{};
 for(int k=0;k<kOriginalHudSlotCount;k++){const auto& s=scoreObject.hud[k];float* o=out.data()+6*k;
  o[0]=float(s.type);o[1]=s.maximum;o[2]=s.value;o[3]=float(s.arg);o[4]=float(s.field10);o[5]=float(s.points);}
 float* tail=out.data()+kOriginalHudSlotCount*6;tail[0]=float(scoreObject.i(0x198));tail[1]=float(scoreCommits);tail[2]=float(scoreComboPayouts);tail[3]=float(scoreLastPoints);
 return out.data();
}
// QA: rider boost state words (+0x2E8 window, +0x2EC modifier, +0x2F0 superTime, +0x2F4 tier, +0x2F8 meter,
// +0x2FC amount, +0x304 drain) for captures whose savestate was poked (tools/ps2_capture.py --poke).
EMSCRIPTEN_KEEPALIVE void boost_state_seed(float window,float modifier,float superTime,int tier,float meter,float amount,int drain){
 boostState.window=window;boostState.modifier=modifier;boostState.superTime=superTime;boostState.tier=tier;boostState.meter=meter;boostState.amount=amount;boostState.drainEnabled=drain;
 physicsState.boost=amount;physicsState.boostWindow=window;physicsState.boostTierCounter=tier;
}
EMSCRIPTEN_KEEPALIVE const char* score_hud_text(int slot){if(slot<0||slot>=kOriginalHudSlotCount)return "";return scoreObject.hud[slot].text.data();}
}
