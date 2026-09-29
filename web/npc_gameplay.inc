// Computer riders (AI racers). In the original a computer rider is the same actor class as the human
// (vtables 0x458660 vs 0x4583A8 differ only in four no-op feedback hooks and the route score 10D410
// vs 1446E8) whose motion owner takes its command words from provider 0x10A768 instead of the pad
// provider 0x127998. The browser therefore runs each computer rider through a complete copy of the
// rider pipeline in its own core instance (web/ai-racers.js) and replaces only:
//   * the command source: originalNpcControl (engine/npc_input.hpp) -> the same per-controller
//     packed layouts the pad provider writes (apply_command_words, web/input_bridge.inc);
//   * the grid seed (npc-riders.json, tools/export_npc_riders.py) and rider identity: actor +0x868
//     coarse body queries, +0x874 not human (reset allows no path end), device index -1;
//   * route progress 0x1125C0 with the NPC score 10D410 (computer branch of 112A50, shared RNG);
//   * the shared-world inputs of 0x10F560 (pair proximity records, designated peer, pacing bounds)
//     and the peers' tick-start state, written by the orchestrator into npc_world_buffer().
// The shared game RNG (0x4FF030) is this instance's `rng`; the orchestrator copies the one world
// state in and out around every stage so all riders draw from a single sequence.
#include "../engine/npc_air.hpp"
#include "../engine/obstacle_collision.hpp"
#include "generated/npc_seed_reader.hpp"
RIDER_LOCAL extern bool browserHumanRider;
RIDER_LOCAL extern void (*browserEventRiderSeed)();
RIDER_LOCAL extern Vec3 position,velocity,normal;RIDER_LOCAL extern bool chaseReady;RIDER_LOCAL extern uint32_t groundFocusTick;
extern "C" void set_event_participant(const OriginalRaceParticipant&,float); // web/race_bridge.cpp
extern "C" void start_event();

// Motion owner +0xDE0 (motion mode) as the original getter reports it: 0 ground, 1 air, 2 crash
// (ragdoll), 3 held (grid/reset), 4 rail, 5 handplant. Also used by web/score_gameplay.inc.
int npc_motion_mode(){
 if(browserNisHold)return 3; // 123640: 11FE78(3), the NIS hold (web/core.cpp nis_hold)
 if(browserResetActive)return 3; // 0x116120 requestMotion(3) for the reset hold
 if(crash.active||browserCrashActive)return 2;
 if(browserStartFrozen)return 3;
 if(browserRailActive)return 4;
 if(browserHandplantMotion5)return 5;
 return (::grounded||groundDeparturePending)?0:1; // a departure switches to air at 13F2CC (animation_bridge.cpp)
}
namespace {
struct NpcPeerView {std::array<float,3> position{},velocity{};int pathIndex=-1;bool human=false,present=false;int upperClass=0;float remaining=0;};
struct NpcRider {
 bool active=false;unsigned slot=0;
 OriginalGroundProfile groundProfile;OriginalGroundState groundState;OriginalBoostState boost;
 OriginalRaceParticipant participant;float progressOrigin=0;
 OriginalStartControlState start;
 OriginalNpcDrivingState driving;OriginalNpcGrabCatalog grabs;
 int16_t role=0;bool allowFlag=false,randomize=false;
 int pacingMode=0;int8_t eventVariant=0;
 std::array<int,6> relationship{};
 bool uberEnabled=false;float timeScale=1;int32_t anchorTick=0;
 OriginalCommandWords lastWords;int lastControl=-1;unsigned lastDraws=0;
};
RIDER_LOCAL NpcRider npc;
} // namespace (npc_rider_slot has external linkage for web/core.cpp)
unsigned npc_rider_slot(){return npc.slot;} // roster slot (0 = the human): the scope refresh phase (web/core.cpp step_rider)
namespace {
// Shared-world inputs (0x10F560 records, designated peer, pacing bounds, peers at the start of the tick).
RIDER_LOCAL std::array<NpcPeerView,6> npcPeers;RIDER_LOCAL unsigned npcParticipants=1;RIDER_LOCAL int32_t npcWorldTick=0;
RIDER_LOCAL std::array<OriginalPairRecord,6> npcRecords;RIDER_LOCAL std::array<bool,6> npcRecordHuman{};
RIDER_LOCAL std::optional<int> npcDesignated;RIDER_LOCAL float npcPacingNegative=-50000,npcPacingPositive=50000;
RIDER_LOCAL std::vector<int> npcOccupancy;
RIDER_LOCAL float npcWorldBuffer[160];
RIDER_LOCAL float npcTickOut[16];RIDER_LOCAL float npcAirInfo[12];
void npc_read_world(){
 const float* w=npcWorldBuffer;npcWorldTick=int32_t(w[0]);npcParticipants=unsigned(w[1]);
 if(npcParticipants<1||npcParticipants>6)throw std::runtime_error("Invalid computer-rider roster");
 npcDesignated.reset();if(w[2]!=0)npcDesignated=int(w[3]);npcPacingNegative=w[4];npcPacingPositive=w[5];
 for(unsigned s=0;s<6;++s){const float* p=w+8+12*s;auto& v=npcPeers[s];
  v.position={p[0],p[1],p[2]};v.velocity={p[3],p[4],p[5]};v.pathIndex=int(p[6]);v.human=p[7]!=0;v.upperClass=int(p[8]);v.remaining=p[9];v.present=p[10]!=0;
  const float* r=w+80+10*s;auto& rec=npcRecords[s];rec.enabled=r[0]!=0;npcRecordHuman[s]=r[1]!=0;rec.planarDistanceCm=r[2];rec.bearing=r[3];
  rec.lastContactTick=int32_t(r[4]);rec.lastCheckedTick=int32_t(r[5]);rec.lastAttackTick=int32_t(r[6]);
  // 115D48 peers: record +0 enabled, +0x1C rival flag (0x115E94: the peer-reaction loop's eligibility, set by
  // 10F560 from relationship >= 2), +8 distance, +C bearing. (+4 peer-is-human is the pacing reference.)
  peers[s].enabled=rec.enabled;peers[s].attackEligible=r[7]!=0;peers[s].distanceCm=rec.planarDistanceCm;peers[s].bearing=rec.bearing;
 }
 npcOccupancy.clear();for(unsigned s=0;s<npcParticipants;++s)if(npcPeers[s].present&&!npcPeers[s].human)npcOccupancy.push_back(s==npc.slot?resetRoute.pathIndex:npcPeers[s].pathIndex);
}
OriginalNpcPathScoreContext npc_score_context(){
 OriginalNpcPathScoreContext c;c.position=physicsState.position;c.velocity=physicsState.velocity;c.roleE00=npc.role;c.allowFlag0E04=npc.allowFlag;c.randomizeE08=npc.randomize;
 c.computerControlled=true;c.currentPathIndex=resetRoute.pathIndex;c.followedPathIndex=npcDesignated&&*npcDesignated>=0&&*npcDesignated<6?npcPeers[*npcDesignated].pathIndex:-1;c.npcPathIndices=npcOccupancy;return c;
}
// 121818 -> 1125C0 for a computer rider: 112A50's NPC branch scores with 10D410 and the shared RNG.
void npc_follow_route(float bestRemaining,int32_t tick){
 if(!physicsAttached)return;
 if(!resetPaths.empty()){ // no route between a path bank's eviction and the next bank: 117C28 still ticks (as the human's)
 npc_read_world();auto context=npc_score_context();
 originalNpcRouteProgress(resetPaths,resetRoute,context,bestRemaining,tick,[](){return rng.nextMotion();});
 physicsState.headingOffset=resetRoute.heading;++routeProgressUpdates;}
 tick_rail_score();
}
void npc_seed_rider(){
 // begin_event_rider has applied the human seed and the shared resets; replace the rider-specific parts.
 physicsState=npc.groundState;physicsProfile=npc.groundProfile;
 // The stat getters 0x1494C0 / 0x1493D8 / 0x148D80 read the rider's own character stats on every surface: a surface change
 // (physicsProfile = physicsMaterials[surface]) keeps them. Peak 2 computer riders ride at stat 0.364 against the human's 0.091
 // (docs/peak2.md); on Peak 1 both are 0.091, so the human seed's materials hid this.
 for(auto& m:physicsMaterials){m.bodyScale=npc.groundProfile.bodyScale;m.topSpeedStat=npc.groundProfile.topSpeedStat;m.speedStat=npc.groundProfile.speedStat;m.edgeStat=npc.groundProfile.edgeStat;}
 landingProfile.bodyScale=npc.groundProfile.bodyScale;
 boostState=npc.boost;const auto actor=OriginalAirState{physicsState.position,physicsState.velocity};position=actor.nativePosition();velocity={};
 normal={physicsState.normal[0],physicsState.normal[2],-physicsState.normal[1]};if(!browserEventRolling)physicsState.controlState=6;publish_motion();
}
std::array<float,3> jvec(const json& j){return j.get<std::array<float,3>>();}
}
// Online races (web/net/mp-game.js): the human rider starts on grid slot k's original countdown seed
// (npc-riders.json riders[k-1].ground.state, the same seeds the computer riders use) instead of slot 0, keeping
// its own ground profile. Cleared by passing an empty string.
RIDER_LOCAL static std::optional<OriginalGroundState> humanGridSeed;
static void human_event_seed_restore(); // web/animation_bridge.cpp: the rider's own seed callback, or none
static void human_grid_seed_apply(){
 if(!humanGridSeed)return;
 physicsState=*humanGridSeed;const auto actor=OriginalAirState{physicsState.position,physicsState.velocity};position=actor.nativePosition();velocity={};
 normal={physicsState.normal[0],physicsState.normal[2],-physicsState.normal[1]};if(!browserEventRolling)physicsState.controlState=6;publish_motion();
}

extern "C" {
// One computer rider (npc-riders.json riders[k], plus event_variant/relationships of the document).
EMSCRIPTEN_KEEPALIVE void npc_configure(const char* riderText,const char* documentText){
 const auto r=json::parse(riderText),doc=json::parse(documentText);npc={};
 npc.slot=r.at("slot");if(npc.slot<1||npc.slot>5)throw std::runtime_error("Computer rider slot");
 npc.groundProfile=browserJsonGroundProfile(r.at("ground").at("profile"));npc.groundState=browserJsonGroundState(r.at("ground").at("state"));
 const auto& b=r.at("settings").at("original_boost").at("state");
 npc.boost={b.at("meter"),b.at("amount"),b.at("window"),b.at("tier"),b.at("drain_enabled"),uint8_t(b.at("feedback_flags").get<int>()),b.at("modifier"),b.at("super_time")};
 const auto& race=r.at("race");const auto& cache=race.at("path_cache");
 npc.participant.human=true; // the tracked participant of this instance's race session (OriginalRaceSession tracks one rider)
npc.participant.finish={race.at("finish_elapsed"),race.at("penalty_ticks"),race.at("finish_ticks")};
 npc.participant.progress.pathIndex=race.at("path_index");npc.participant.progress.remaining=race.at("remaining");npc.participant.progress.bestRemaining=race.at("best_remaining");
 npc.participant.progress.cache={jvec(cache.at("origin")),cache.at("distance"),cache.at("segment")};npc.progressOrigin=r.at("progress_origin");
 const auto& sc=r.at("start_control");npc.start.phase=sc.at("phase");npc.start.steadyTime=sc.at("steady_time");npc.start.low=sc.at("low");npc.start.high=sc.at("high");npc.start.pose=sc.at("pose");
 const auto& n=r.at("npc");const auto& d=n.at("driving_state");auto& s=npc.driving;
 s.desiredSpeedDF0=d.at("desired_speed_df0");s.parameterDF8=d.at("parameter_df8");s.parameterDFC=d.at("parameter_dfc");s.boardTimerE40=d.at("board_timer_e40");
 s.targetPeerE70=d.at("target_peer_e70");s.offRouteTicksE74=d.at("off_route_ticks_e74");s.oppositeHeadingTicksE78=d.at("opposite_heading_ticks_e78");
 s.longFlightTicks484=d.at("long_flight_ticks484");s.defensiveTimerF30=d.at("defensive_timer_f30");s.defensiveDecisionF34=d.at("defensive_decision_f34");
 s.lastAttackTickF4=d.at("last_attack_tick_f4");s.behaviorCounterF38=int16_t(d.at("behavior_counter_f38").get<int>());s.regionStartE50=jvec(d.at("region_start_e50"));s.regionEndE60=jvec(d.at("region_end_e60"));
 const auto& t=d.at("trick");auto& p=s.trick;p.indexE0C=t.at("index_e0c");p.enabledE10=t.at("enabled_e10");p.decisionE14=t.at("decision_e14");p.phaseE18=t.at("phase_e18");
 p.kindE1C=t.at("kind_e1c");p.fieldE20=t.at("field_e20");p.fieldE24=t.at("field_e24");p.fieldE28=t.at("field_e28");p.releaseE2C=t.at("release_e2c");p.startE30=t.at("start_e30");
 p.remainingE34=t.at("remaining_e34");p.spinE38=t.at("spin_e38");p.flipE3C=t.at("flip_e3c");
 p.normalCounts=t.at("normal_counts").get<std::array<int32_t,15>>();p.uberCounts=t.at("uber_counts").get<std::array<int32_t,15>>();p.tweakCounts=t.at("tweak_counts").get<std::array<int32_t,15>>();
 const std::string behavior=n.at("behavior").at("function");
 s.behavior=behavior=="0x00100680"?OriginalNpcBehavior::Cruise100680:behavior=="0x001009e0"?OriginalNpcBehavior::Jump1009E0:behavior=="0x00100b90"?OriginalNpcBehavior::Designated100B90:behavior=="0x00100f88"?OriginalNpcBehavior::Peer100F88:throw std::runtime_error("Unknown NPC behaviour delegate");
 const auto& g=n.at("grab_catalog");auto timing=[](const json& x){return OriginalNpcGrabTiming{x.at("semantic"),x.at("marker1"),x.at("marker2")};};
 for(unsigned i=0;i<15;++i){npc.grabs.normal[i]=timing(g.at("normal").at(i));npc.grabs.tweak[i]=timing(g.at("tweak").at(i));for(unsigned k=0;k<2;++k)npc.grabs.uber[k][i]=timing(g.at("uber").at(k).at(i));}
 npc.grabs.grabStat=g.at("grab_stat");npc.grabs.eventId=g.at("event_id");
 const auto& score=n.at("score_state");npc.role=int16_t(score.at("role_e00").get<int>());npc.allowFlag=score.at("allow_flag0_e04");npc.randomize=score.at("randomize_e08");
 const auto& pacing=n.at("pacing_state");npc.pacingMode=pacing.at("mode_e4");npc.eventVariant=int8_t(doc.at("event_variant").get<int>());
 for(unsigned k=0;k<6;++k)npc.relationship[k]=doc.at("relationships").at("scores").at(npc.slot).at(k);
 const auto& id=r.at("identity");npc.uberEnabled=id.at("capability_b2c").get<uint32_t>()&1;npc.timeScale=id.at("time_scale300");
 riderMask8C0=std::stoull(id.at("upper_mask8c0").get<std::string>(),nullptr,16);riderMask8C8=std::stoull(id.at("upper_mask8c8").get<std::string>(),nullptr,16);riderMask8D0=std::stoull(id.at("upper_mask8d0").get<std::string>(),nullptr,16); //11C298 per character
 if(id.at("human874").get<int>()!=0||id.at("device870").get<int>()>=0)throw std::runtime_error("Computer rider identity");
 npc.anchorTick=doc.at("anchor_tick");
 npc.active=true;browserHumanRider=false;browserEventRiderSeed=npc_seed_rider;browserRouteProgress=npc_follow_route;
 set_event_participant(npc.participant,npc.progressOrigin);
}
EMSCRIPTEN_KEEPALIVE float* npc_world_buffer(){return npcWorldBuffer;}
EMSCRIPTEN_KEEPALIVE void human_grid_seed(const char* stateText){
 const std::string text=stateText?stateText:"";
 // Cleared: the rider's own Snow Jam seed (settings original_event_start, installed by init_animation) is back.
 if(text.empty()){humanGridSeed.reset();if(browserEventRiderSeed==human_grid_seed_apply)human_event_seed_restore();return;}
 humanGridSeed=browserJsonGroundState(json::parse(text));browserEventRiderSeed=human_grid_seed_apply;
}
// The grid start: start_event with this rider's seed. Like the human's (exact from the same anchor),
// the start controller is entered fresh (12BE20) from the seeded stance and tilt; the captured
// start-controller words (npc.start) are kept for diagnostics only.
EMSCRIPTEN_KEEPALIVE void npc_start_event(){
 if(!npc.active)throw std::runtime_error("Computer rider not configured");
 start_event();physicsState.timeScale=npc.timeScale;
}
// Controls phase: provider 0x10A768 for the controller about to run, then the same per-controller
// decode as the pad path. Returns the pad path's command view (pad_tick layout); npc_tick_info()
// holds [word0 lo, word0 hi, word1 lo, word1 hi, control, draws, timeScale, AI path, behaviour].
EMSCRIPTEN_KEEPALIVE float* npc_tick_info(){return npcTickOut;}
EMSCRIPTEN_KEEPALIVE float* npc_tick(){
 if(!npc.active)throw std::runtime_error("Computer rider not configured");
 npc_read_world();
 const int control=browser_control_state();
 OriginalNpcProviderContext input;auto& d=input.driving;
 d.position=physicsState.position;d.velocity=physicsState.velocity;d.boardUp=physicsState.boardUp;d.physicalForward=physicsState.physicalForward;
 d.route=resetRoute;d.boostMeter=boostState.meter;d.superTime=boostState.superTime;// The grid seed is the anchor tick's state; the event clock reaches that tick with every rider held,
 // during which the original kept replaying the start command from the retained DF0 (10B790 draws
 // only on ticks divisible by 20). Earlier grid ticks therefore evaluate 10B790 at the anchor tick.
 d.tick=control==6&&npcWorldTick<npc.anchorTick?npc.anchorTick:npcWorldTick;d.motionMode=npc_motion_mode();d.controlState=control;
 d.trajectoryElapsed=browserTrajectory.elapsed;d.physicalRightZ=originalOrientationBasis(physicsState.quaternion).right[2];d.prewindStyle=physicsState.prewindStyle;d.mainAnimationClass=graph.currentClass(2);
 d.selfSlot=npc.slot;d.participantCount=npcParticipants;d.designatedPeer=npcDesignated;
 for(unsigned s=0;s<6;++s){d.peers[s]=npcRecords[s];d.peerHuman[s]=npcPeers[s].human;d.peerVelocities[s]=npcPeers[s].velocity;d.peerPositions[s]=npcPeers[s].position;d.peerPathIndices[s]=s==npc.slot?resetRoute.pathIndex:npcPeers[s].pathIndex;d.peerUpperAnimationClass[s]=s==npc.slot?graph.currentClass(1):npcPeers[s].upperClass;}
 // 10DEF0: own +4D0 and the first enabled pair record whose peer is human, in slot order.
 input.pacing.remaining=npcPeers[npc.slot].remaining;for(unsigned s=0;s<6;++s)if(npcRecords[s].enabled&&npcRecordHuman[s]){input.pacing.referenceRemaining=npcPeers[s].remaining;break;}
 input.pacing.negativeThresholdDC=npcPacingNegative;input.pacing.positiveThresholdE0=npcPacingPositive;input.pacing.modeE4=npc.pacingMode;input.pacing.eventVariant=npc.eventVariant;
 input.currentTimeScale=physicsState.timeScale;input.scoring=npc_score_context();
 OriginalNpcAirContext air;air.trajectoryStatus=browserPredictionAvailable?browserTrajectory.status:0;air.predictedTime=browserTrajectory.predictedTime;air.elapsed=browserTrajectory.elapsed;
 air.angular=::air;air.trickStat=airProfile.trickStat;air.boostModifier=boostState.modifier;air.superTime=boostState.superTime;air.mainAnimationClass=graph.currentClass(2);
 air.boostTierCounter=boostState.tier;air.uberEnabled=npc.uberEnabled;air.grabs=&npc.grabs;input.air=&air;
 npcAirInfo[0]=air.trajectoryStatus;npcAirInfo[1]=air.predictedTime;npcAirInfo[2]=air.elapsed;npcAirInfo[3]=air.superTime;npcAirInfo[4]=air.boostModifier;npcAirInfo[5]=air.boostTierCounter;npcAirInfo[6]=air.uberEnabled;npcAirInfo[7]=air.mainAnimationClass;npcAirInfo[8]=air.trickStat;
 unsigned draws=0;OriginalNpcProviderResult result;
 {ControllerDraws controllerDraws;result=originalNpcControl(npc.driving,input,resetPaths,[](unsigned s){return npc.relationship.at(s);},[&](){++draws;return rng.next();});}
 resetRoute=input.driving.route;physicsState.timeScale=result.timeScale;
 npc.lastWords={result.words[0],result.words[1]};npc.lastControl=control;npc.lastDraws=draws;
 float* command=apply_command_words(control,npc.lastWords);
 npcTickOut[0]=float(result.words[0]&0xffff);npcTickOut[1]=float(result.words[0]>>16);npcTickOut[2]=float(result.words[1]&0xffff);npcTickOut[3]=float(result.words[1]>>16);
 npcTickOut[4]=control;npcTickOut[5]=draws;npcTickOut[6]=result.timeScale;npcTickOut[7]=resetRoute.pathIndex;npcTickOut[8]=int(npc.driving.behavior);
 return command;
}
// QA: the NPC extension (owner +0xDF0..+0xF40) as (owner offset, value) pairs, like ground_state_dump.
EMSCRIPTEN_KEEPALIVE float* npc_state_dump(){
 RIDER_LOCAL static std::vector<float> out;out.clear();const auto& s=npc.driving;const auto& t=s.trick;
 auto f1=[&](unsigned off,float x){out.push_back(float(off));out.push_back(x);};
 f1(0xdf0,s.desiredSpeedDF0);f1(0xdf8,s.parameterDF8);f1(0xdfc,s.parameterDFC);
 f1(0xe0c,t.indexE0C);f1(0xe10,t.enabledE10);f1(0xe14,t.decisionE14);f1(0xe18,t.phaseE18);f1(0xe1c,t.kindE1C);f1(0xe20,t.fieldE20);f1(0xe24,t.fieldE24);f1(0xe28,t.fieldE28);
 f1(0xe2c,t.releaseE2C);f1(0xe30,t.startE30);f1(0xe34,t.remainingE34);f1(0xe38,t.spinE38);f1(0xe3c,t.flipE3C);f1(0xe40,s.boardTimerE40);
 for(unsigned k=0;k<3;++k){f1(0xe50+4*k,s.regionStartE50[k]);f1(0xe60+4*k,s.regionEndE60[k]);}
 f1(0xe70,s.targetPeerE70);f1(0xe74,s.offRouteTicksE74);f1(0xe78,s.oppositeHeadingTicksE78);f1(0xf30,s.defensiveTimerF30);f1(0xf34,s.defensiveDecisionF34);
 out.insert(out.begin(),float(out.size()/2));return out.data();
}
// QA: the air context the last provider call used [status, predicted, elapsed, superTime, boostModifier, tier, uber, mainClass, trickStat].
EMSCRIPTEN_KEEPALIVE float* npc_air_info(){return npcAirInfo;}
// Per-rider state the orchestrator shares with the other riders and the standings/HUD:
// [pos3, vel3, route path (AB8), upper class (ch1), control, motion, human, quaternion4, timeScale].
EMSCRIPTEN_KEEPALIVE float* rider_world_state(){
 RIDER_LOCAL static float v[20];for(unsigned k=0;k<3;++k){v[k]=physicsState.position[k];v[3+k]=physicsState.velocity[k];}
 v[6]=resetRoute.pathIndex;v[7]=graph.currentClass(1);v[8]=browser_control_state();v[9]=npc_motion_mode();v[10]=browserHumanRider;
 for(unsigned k=0;k<4;++k)v[11+k]=physicsState.quaternion[k];v[15]=physicsState.timeScale;return v;
}
// Any rider instance (human too): apply the shared 0x10F560 records to its 115D48 peers.
EMSCRIPTEN_KEEPALIVE void rider_world_peers(){npc_read_world();}
}

// Shared course-script objects across the six cores (web/ai-racers.js). A rider trigger (121818 ->
// 30A060 slot 2, builtin52 launch-once guard) creates the object once in the one original world and
// draws the shared RNG in that rider's pass. Each core keeps a world copy: the triggering rider's
// core launches (and draws); the orchestrator then replays the launch in every other core without a
// draw, in the same tick, so every copy follows the same deterministic spline.
extern "C" EMSCRIPTEN_KEEPALIVE uint32_t* world_triggers(){RIDER_LOCAL static std::vector<uint32_t> out;out.assign(1,uint32_t(splineOwnersFired.size()));out.insert(out.end(),splineOwnersFired.begin(),splineOwnersFired.end());return out.data();}
extern "C" EMSCRIPTEN_KEEPALIVE void world_trigger_apply(uint32_t key){
 if(std::find(splineOwnersFired.begin(),splineOwnersFired.end(),key)!=splineOwnersFired.end())return;
 const BrowserRandom saved=rng;launch_spline_pieces(key/8,key%8);rng=saved; // replica: the draw belongs to the triggering rider's core
}

// 0x13F488 at 13F4CC (motion-0 ground obstacle query): a computer rider (+0x874 == 0) within 50 cm of
// its route (+0x4C8 lateral, |+0x498 - +0x118| height) skips the 0x3342D0 query and its response;
// 105398 instance contacts and rider pairs still run. The human always queries (browser unchanged).
static bool npc_obstacle_query_enabled(bool air){
 if(browserHumanRider||air)return true;
 return originalObstacleQueryEnabled(physicsState.riderType,false,resetRoute.lateralDistance,physicsState.position[2],resetRoute.closestPoint[2]);
}

// ---- Rider pairs (0x107888) across cores: this rider's view and its response operations. ----
// The shared dispatcher (web/race_world.cpp, OriginalRiderPairSystem) runs in the human core and calls
// back through web/ai-racers.js into the rider cores. View layout (uint32 words, floats bit-cast):
// [0] body valid, [1] count, [2] active mask, [3..5] broad centre, [6] broad radius, [7..86] 20 x
// (centre xyz, radius), [87..89] velocity +1E0, [90..92] ground normal +370, [93..95] physical up,
// [96] motion mode, [97] control, [98] ragdoll submode, [99..101] position, [102] channel-1 class,
// [103] marker 0, [104] marker 1, [105..107] facing +340, [108] strength +350, [109] boost +2FC,
// [110] presentation frame valid, [111..122] presentation right/forward/up/origin, [123..134] physical
// right/forward/up/position, [135] reverse stance, [136] manual spin.
extern "C" EMSCRIPTEN_KEEPALIVE uint32_t* pair_view(){
 RIDER_LOCAL static uint32_t v[140];std::fill(std::begin(v),std::end(v),0u);
 auto F=[](float x){return std::bit_cast<uint32_t>(x);};auto put3=[&](unsigned at,const std::array<float,3>& a){for(unsigned k=0;k<3;++k)v[at+k]=F(a[k]);};
 const bool crashActor=crash.active;
 // Inside this rider's own 121750 a touchdown's 106538 has already moved the AA0 spheres on the PS2 (139D78 before 107888); the
 // port commits that translation to the volume after the contacts, so the pair view adds it (PS2 CRA3 1694: Allegra lands onto Luther).
 if(browserBodyVolume){const auto& b=*browserBodyVolume;const bool landing=animationPostActive;terrain_original::Rounding rounding;
  auto moved=[&](const std::array<float,3>& c){if(!landing)return c;std::array<float,3> r;for(unsigned k=0;k<3;++k)r[k]=terrain_original::add(c[k],browserLandingTranslation[k]);return r;};
  v[0]=1;v[1]=b.count;v[2]=b.activeMask;put3(3,moved(b.broadCenterCm));v[6]=F(b.broadRadiusCm);
  for(unsigned i=0;i<b.count&&i<20;++i){put3(7+4*i,moved(b.spheres[i].centerCm));v[10+4*i]=F(b.spheres[i].radiusCm);}
  if(b.reactionFrame){v[110]=1;put3(111,b.reactionFrame->right);put3(114,b.reactionFrame->forward);put3(117,b.reactionFrame->up);put3(120,b.reactionFrame->origin);}}
 // A rider whose 121750 has not run yet is still unclamped (13F358 is post-work; core.cpp clamps early).
 const bool preClamp=!crashActor&&animationPost.pending&&groundUnclampedVelocity.has_value();
 put3(87,crashActor?crash.actor.velocity:preClamp?*groundUnclampedVelocity:physicsState.velocity);put3(90,crashActor?crash.actor.groundNormal:physicsState.normal);
 const auto basis=originalOrientationBasis(crashActor?crash.actor.quaternion:physicsState.quaternion);put3(93,basis.up);
 v[96]=uint32_t(npc_motion_mode());v[97]=uint32_t(browser_control_state());v[98]=uint32_t(crashActor?crash.motion.submode:0);put3(99,crashActor?crash.actor.position:physicsState.position);
 v[102]=uint32_t(graph.rig?graph.currentClass(1):0);
 if(auto* s=attack_first_sequence(1)){v[103]=uint32_t(s->flags&1);v[104]=uint32_t((s->flags>>1)&1);}
 put3(105,{attackRider.facing[0],attackRider.facing[1],attackRider.facing[2]});v[108]=F(attackRider.strength);v[109]=F(physicsState.boost);
 put3(123,basis.right);put3(126,basis.forward);put3(129,basis.up);put3(132,crashActor?crash.actor.position:physicsState.position);
 v[135]=physicsState.reverseStance;v[136]=F(physicsState.manualSpin);return v;
}
// 0x106538: move the actor, +9D0 presentation offset and the cached AA0 spheres (not the posed bones).
extern "C" EMSCRIPTEN_KEEPALIVE void pair_translate(float x,float y,float z){
 terrain_original::Rounding rounding;const terrain_original::Vector d{x,y,z};
 for(unsigned k=0;k<3;++k)pairCompanion[k]=terrain_original::add(pairCompanion[k],d[k]); // rider+0x9D0 (committed by this rider's 121750)
 auto& p=crash.active?crash.actor.position:physicsState.position;for(unsigned k=0;k<3;++k)p[k]=terrain_original::add(p[k],d[k]);
 if(crash.active)physicsState.position=crash.actor.position;
 if(browserBodyVolume)translateOriginalPairBody(*browserBodyVolume,d);commit_rider_physics();
}
// 0x107E70 velocity change, with the 0x1135B8 predictor reseed for airborne riders.
extern "C" EMSCRIPTEN_KEEPALIVE void pair_set_velocity(float x,float y,float z,int reseed){
 if(crash.active){crash.actor.velocity={x,y,z};physicsState.velocity=crash.actor.velocity;if(reseed)crash.trajectory.reseed({crash.actor.position,crash.actor.velocity});}
 else{physicsState.velocity={x,y,z};if(animationPost.pending&&groundUnclampedVelocity)groundUnclampedVelocity=physicsState.velocity; /*its own 121750 restores and clamps it*/if(reseed&&!::grounded)begin_prediction({physicsState.position,physicsState.velocity});}
 commit_rider_physics();
}
// The typed reaction request of 0x107E70: 1 soft (0x108388, as 105398's soft path), 2 crash
// (0x10EB30 with the request's semantic). event: point xyz, normal xyz, incoming xyz, closing speed.
extern "C" EMSCRIPTEN_KEEPALIVE void pair_react(int kind,int animation,int attack,const float* e){
 OriginalCollisionEvent event;for(unsigned k=0;k<3;++k){event.pointCm[k]=e[k];event.normal[k]=e[3+k];event.incomingDirection[k]=e[6+k];}event.closingSpeedCmps=e[9];(void)attack;
 if(kind==2){if(!browserHardCrash)throw std::runtime_error("Pair crash lifecycle unavailable");if(!crash.active&&!browserResetActive)browserHardCrash(animation,event);return;}
 if(kind!=1||crash.active||browserResetActive)return;
 terrain_original::Rounding rounding;auto physical=originalOrientationBasis(physicsState.quaternion);OriginalCollisionContext context;
 if(browserBodyVolume&&browserBodyVolume->reactionFrame)context.presentation=*browserBodyVolume->reactionFrame;
 context.physical={physical.right,physical.forward,physical.up,physicsState.position};context.velocityCmps=physicsState.velocity;
 context.motionMode=npc_motion_mode();context.controlState=browser_control_state();context.reverseStance=physicsState.reverseStance;context.manualSpin=physicsState.manualSpin;
 auto reaction=originalSoftCollisionReaction(collisionProfile,context,event,[](){return rng.nextMotion();});
 if(reaction.kind==OriginalCollisionReactionKind::Soft&&::grounded&&!reaction.cancelControlOne&&(physicsState.controlState==0||physicsState.controlState==2||(browserRailActive&&physicsState.controlState==7)))soft_collision_begin(reaction.animation,reaction.manualSpin);
}
// QA: the rider's query caches +0x864 (terrain/landing) and +0x868 (body): [valid, resource, u, v, half] each.
extern "C" EMSCRIPTEN_KEEPALIVE float* rider_query_caches(){RIDER_LOCAL static float v[10];const auto& t=ssx::browser_rider_terrain_cache();const auto& b=ssx::browser_rider_body_cache();
 v[0]=t.valid;v[1]=float(t.resource);v[2]=float(t.cellU);v[3]=float(t.cellV);v[4]=float(t.half);v[5]=b.valid;v[6]=float(b.resource);v[7]=float(b.cellU);v[8]=float(b.cellV);v[9]=float(b.half);return v;}
