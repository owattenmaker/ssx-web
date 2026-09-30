#include "rider_local.hpp"
#include "../engine/trick_name.hpp"
#include "rider_attributes.hpp"
#include "../engine/snow_flipbook.hpp"
#include "../engine/pose_matrix.hpp"
#include "../engine/skin_palette.hpp"
#include "../engine/environment_transition.hpp"
#include "animation_graph.hpp"
#include "rail_bridge.hpp"
#include "../engine/rail_score.hpp"
#include "../engine/boost_hud.hpp"
#include "../engine/boost_letter_hud.hpp"
#include "../engine/boost_pending_hud.hpp"
#include "../engine/boost_flash_hud.hpp"
#include "../engine/boost_hud_clock.hpp"
#include "../engine/score_boundary.hpp"
#include "../engine/jump_motion.hpp"
#include "json.hpp"
#include "../engine/air_entry.hpp"
#include "../engine/air_animation_selector.hpp"
#include "../engine/air_switch.hpp"
#include "../engine/landing_motion.hpp"
#include "../engine/upper_reaction.hpp"
#include "../engine/grab_score.hpp"
#include "../engine/trick_commit.hpp"
#include "../engine/boost_award.hpp"
#include "../engine/collision_event.hpp"
#include "../engine/stance_restore.hpp"
#include "../engine/passive_air_control.hpp"
#include "../engine/rider_pose_motion.hpp"
#include "../engine/secondary_motion.hpp"
#include "pose_space.hpp"
#include "crash_runtime.hpp"
#include "../engine/reset_route.hpp"
#include "../engine/reset_placement.hpp"
#include "../engine/reset_control.hpp"
#include "../engine/reset_fade.hpp"
#include "../engine/board_trail.hpp"
#include "../engine/snow_particles.hpp"
#include "presentation_fast.hpp"
#include "../engine/snow_context.hpp"
#include "../engine/snow_crash.hpp"
#include "../engine/wake_physics.hpp"
#include <emscripten/emscripten.h>
using nlohmann::json;using namespace ssx;
#include "audio_events.hpp" // audio observers (web/audio_events.inc, docs/audio-logic.md 5)
RIDER_LOCAL extern bool (*browserLandingAirExit)();RIDER_LOCAL extern bool browserPosedLandingEnabled;
bool resolve_posed_landing(const BodyCollisionVolume&);RIDER_LOCAL extern float output[16];
RIDER_LOCAL extern float rideBoard,rideSpin,rideFlip,rideAxes[12];
RIDER_LOCAL extern bool rideLatched;
RIDER_LOCAL extern void (*browserReverseAnimation)(std::array<float,4>);RIDER_LOCAL extern uint32_t reverseTurnSerial;
RIDER_LOCAL extern void (*browserRestoreStance)(int);RIDER_LOCAL extern bool grounded;RIDER_LOCAL extern bool browserRailActive;RIDER_LOCAL extern bool browserStarting,browserStartFrame,browserStartFrozen;static void clear_start();void publish_motion();
bool begin_soft_control(int,float);void soft_animation_complete(bool);int take_soft_transition();RIDER_LOCAL extern bool browserSoftActive,browserSoftFrame;
void align_air_orientation(int,float);
RIDER_LOCAL extern std::array<float,3> browserBoardNormalForPose,browserTrailContact;RIDER_LOCAL extern float browserLandingSpeed;
RIDER_LOCAL extern OriginalGroundState physicsState;RIDER_LOCAL extern OriginalGroundProfile physicsProfile;RIDER_LOCAL static bool physicsAttached=false;
RIDER_LOCAL_LAZY static BrowserAnimationGraph graph;RIDER_LOCAL static AnimationTransform initialDefaultRoot;RIDER_LOCAL static bool initialDefaultMirror=false;struct BrowserRandom:OriginalRandomState{
 // QA (compare-ps2-capture --sync-rng): computer-rider draws fall between the human's controller-phase
 // draws and its motion-phase draws (event-race 899 landing crash vs 1472 reaction variant). Draws
 // outside a ControllerDraws scope first apply the deferred opponent draws.
 int deferred=0,controllerDepth=0;
 // Six-rider scheduling (web/ai-racers.js, split): controller-phase draws (pass 121068) continue on
 // `words`; the first motion-phase draw (121750/121818) starts a separate cursor `deferred` draws
 // further on, so every rider's controller draws precede every motion draw as in 0x128AF0.
 bool split=false,motionStarted=false;OriginalRandomState motion;uint32_t controllerCount=0,motionCount=0;
 uint32_t next(){
  if(split){
   if(controllerDepth){++controllerCount;return OriginalRandomState::next();}
   if(!motionStarted){motion.words=words;for(;deferred>0;--deferred)motion.next();motionStarted=true;}
   ++motionCount;return motion.next();
  }
  if(!controllerDepth)while(deferred>0){--deferred;OriginalRandomState::next();}return OriginalRandomState::next();}
 uint32_t nextMotion(){return next();}
};RIDER_LOCAL static BrowserRandom rng;
struct ControllerDraws{ControllerDraws(){++rng.controllerDepth;}~ControllerDraws(){--rng.controllerDepth;}ControllerDraws(const ControllerDraws&)=delete;};
uint32_t browser_shared_random_next(){return rng.next();} /* stage-program draws of the shared RNG 0x4FF030 (web/set_piece_gameplay.inc) */
RIDER_LOCAL static OriginalGroundState gs;RIDER_LOCAL static OriginalGroundProfile gp;
RIDER_LOCAL static OriginalGrabState grab;RIDER_LOCAL static OriginalGrabProfile grabProfile;
RIDER_LOCAL static bool passiveMode=false,heldAirMode=false;RIDER_LOCAL static OriginalPassiveAirState passive;RIDER_LOCAL static OriginalAirPrewindState prewind;RIDER_LOCAL static OriginalAirControlState air;RIDER_LOCAL static bool airAdjustLive=false; //air control owned rider+0x28C/+0x298 last tick
RIDER_LOCAL static OriginalAirAnimationState airAnimation;RIDER_LOCAL_LAZY static OriginalAirControlProfile airProfile;
RIDER_LOCAL static bool previousGround=true,previousHeld=false;
std::optional<WorldBodyQuery> inspect_body_contacts(const BodyCollisionVolume&,std::array<float,3>);
void reset_body_queries();
OriginalCollisionReaction classify_body_contact(const WorldBodyHit&,const ObstacleResponse&,const BodyCollisionVolume&,std::array<float,3>,int,const CollisionRandom&,int motion=-1);
ObstacleResponse apply_body_contact(const WorldBodyHit&,bool);RIDER_LOCAL extern bool airMotionThisTick;
RIDER_LOCAL std::optional<BodyCollisionVolume> browserBodyVolume;RIDER_LOCAL static std::optional<WorldBodyQuery> browserBodyQuery;RIDER_LOCAL static std::optional<ObstacleResponse> browserBodyResponse;RIDER_LOCAL static bool bodyResponseAir=false;
RIDER_LOCAL static std::array<float,12> posedPhysical{0,0,0,0,0,0,1,0,0,0,0,0};
RIDER_LOCAL static std::array<float,3> committedPoseTranslation{};
RIDER_LOCAL static float poses[256*7],info[19];RIDER_LOCAL static unsigned pivotBone=0;RIDER_LOCAL static std::array<float,3> currentPivot{};RIDER_LOCAL static RiderPoseContact poseContact,initialPoseContact;RIDER_LOCAL static std::array<float,17> poseControls{};RIDER_LOCAL static float legWeight=1;
RIDER_LOCAL static OriginalGrabScoreState scoring;RIDER_LOCAL static OriginalGrabScoreProfile scoringProfile;RIDER_LOCAL_LAZY static std::map<int,OriginalGrabScoreRule> scoreRules;RIDER_LOCAL static int lastScoreId=-1,banked=0;RIDER_LOCAL static OriginalGrabContext grabContext;
RIDER_LOCAL extern std::array<float,3> browserLandingTranslation;RIDER_LOCAL extern std::array<float,3> browserDeparturePush;RIDER_LOCAL extern std::optional<std::array<float,3>> browserDeparturePrePosition;RIDER_LOCAL extern bool browserGroundResetPending;RIDER_LOCAL extern bool browserGroundCrashPending;
RIDER_LOCAL extern OriginalBoostState boostState;RIDER_LOCAL extern OriginalBoostProfile boostProfile;RIDER_LOCAL extern OriginalAirTrajectory browserTrajectory;RIDER_LOCAL extern bool browserPredictionAvailable;
RIDER_LOCAL static int controlPredictionStatus=0;RIDER_LOCAL static float controlPredictedTime=0,controlPredictionElapsed=0;
// Original131870 selects the ground animation during controller dispatch, before the
// 1211F8 filter pass and 13D818 integration of the same tick. step_rider has already
// advanced both when animation_tick runs, so keep the controller-time inputs here.
RIDER_LOCAL extern uint32_t motionTick;
struct ControllerGroundInputs {bool valid=false;float turn=0,brake=0,crouch=0,boost=0;std::array<float,3> velocity{},lateral{},surfaceVelocity{};int surface=0;bool reverse=false;int controlState=0;uint32_t logicTick=0;};
RIDER_LOCAL static ControllerGroundInputs controllerGround;
// 1211F8 calls 120378 first (0x121204), after the controller: when a controller writes rider+0x1E0 (114298 takeoffs:
// charged release 12E9B8, R3 ollie 1307B8, rail jump 132048; 106848 rail attach bake; handplant entry/exit), that
// tick's secondary-motion wind is the post-controller velocity (jump-tricks 374, rail-balance-lr 673, handplant 570).
RIDER_LOCAL static std::optional<std::array<float,3>> controllerTakeoffWind;
void browser_controller_takeoff_wind(const std::array<float,3>& velocity){controllerTakeoffWind=velocity;}
// A controller-phase reverse turn (131620 / 12E9B8 -> 114CC0) toggles rider+0x320 before 120378 reads it (tech-sb-opp 498).
void browser_controller_stance(bool reverse){controllerGround.reverse=reverse;}
// Airborne motion keeps +3A0/+3B0 from the last ground integration; 11FA10's lateral
// shift reads that retained +3B0. publish_motion rebuilds a telemetry lateral in air.
RIDER_LOCAL static std::optional<std::array<float,3>> retainedGroundLateral;
void snapshot_animation_prediction(){
 controllerTakeoffWind.reset();
 controllerGround={true,physicsState.turn.current,physicsState.brake.current,physicsState.crouch.current,physicsState.boost,physicsState.velocity,physicsState.lateral,physicsState.surfaceVelocity,physicsProfile.surface.id,physicsState.reverseStance,physicsState.controlState,motionTick};
 controlPredictionStatus=browserPredictionAvailable?browserTrajectory.status:0;
 controlPredictedTime=browserTrajectory.predictedTime;controlPredictionElapsed=browserTrajectory.elapsed;
}

RIDER_LOCAL static OriginalTrickCommitProfile commitProfile;RIDER_LOCAL static OriginalTrickHistory trickHistory;RIDER_LOCAL static OriginalTrickIdentityState trickIdentity;RIDER_LOCAL static OriginalBoostAwardContext awardContext;
RIDER_LOCAL static OriginalTrickNameTables trickNameTables;RIDER_LOCAL static std::string committedTrickName;
static void setup_trick_names(const json&config){const auto&data=config.at("original_trick_names");if(data.at("version")!=1||data.at("tables").size()!=17)throw std::runtime_error("Original trick-name package");for(unsigned i=0;i<17;i++){trickNameTables[i].clear();for(const auto&t:data.at("tables").at(i)){if(t.is_null())trickNameTables[i].push_back(std::nullopt);else trickNameTables[i].push_back(t.get<std::string>());}}committedTrickName.clear();}
RIDER_LOCAL static float lastLandingSpin=0;
//11C298 builds the channel-1 bone masks per character with 310CE8: +0x8C0 (457A90 list) and +0x8C8 (457B38) each OR +0x8D0 (4A1090 "morph").
//Zoe/Sam/Griff 0x8000fffe/0x8000fff8/0x870; computer riders take theirs from npc-riders.json (Psymon/Luther bit 30, Allegra/Moby bit 33).
RIDER_LOCAL static uint64_t riderMask8C0=0x8000fffeu,riderMask8C8=0x8000fff8u,riderMask8D0=0x870u;
RIDER_LOCAL static int scoreEvent=0;RIDER_LOCAL static int idleControl=0;RIDER_LOCAL static float idleSeconds=0;RIDER_LOCAL static int upperRequest358=0;RIDER_LOCAL static int32_t upperRequestTick354=-1;/*10E028 pending upper-body reaction: kind, due logic tick*/
// 0x10E098 boost award: after the meter update, a positive award that fills the meter while the
// super timer is 0 requests the upper-body reaction 10E028(rider,5,0) when rider+0xB2C is set (3 for
// all six Snow Jam riders). Every rider gets it (kinds 1-4 are human-only; docs/ai-racers.md).
// 0x1200D0 (boost timers) runs after the controller pass whose 115B58 (131868) plays pending reactions, so its request
// is first seen on the next tick; the browser steps the boost timer before this tick's animation, hence the one-tick hold
// (PS2 uber-super-expire: the Super Uber expiry reaction 318 starts one tick after the request).
RIDER_LOCAL static int32_t upperRequestHeldTick=-1;
extern "C" void browser_upper_reaction_request(int kind){upperRequest358=kind;upperRequestTick354=int32_t(controllerGround.logicTick);upperRequestHeldTick=upperRequestTick354;}
extern "C" OriginalBoostEffects browser_boost_tick(OriginalBoostState&,const OriginalBoostProfile&,float,int,int); // web/core.cpp (C linkage block)
static OriginalBoostAwardEffects award_boost(float delta,uint32_t category=1){
 const auto e=originalBoostAward(boostState,awardContext,delta,category);
 if(e.fullMeterNotification){upperRequest358=5;upperRequestTick354=int32_t(controllerGround.logicTick);}
 if(e.meterChangeEvent)audio_event(AE_FILL_RESET); //10E130 -> 29AB08
 return e;
}RIDER_LOCAL static uint32_t animationTick=0;RIDER_LOCAL static std::array<OriginalUpperPeer,6> peers;
RIDER_LOCAL extern void (*browserRouteProgress)(float,int32_t);
RIDER_LOCAL extern bool browserResetActive;RIDER_LOCAL extern bool browserNisHold;/* web/core.cpp nis_hold (rider+0xAC4) */RIDER_LOCAL extern void (*browserResetBegin)(int);RIDER_LOCAL extern void (*browserResetDecline)();RIDER_LOCAL extern void (*browserResetRerequest)(int);RIDER_LOCAL extern bool (*browserResetControl)();RIDER_LOCAL extern void (*browserResetClear)();
RIDER_LOCAL extern std::optional<std::array<float,3>> groundUnclampedVelocity;RIDER_LOCAL extern std::optional<std::array<float,3>> groundBoardNormalBefore;RIDER_LOCAL extern bool railStepConsumed;
// 13F178 (0x13F278, word 0x4A115C) and 139C88 (0x13A734, word 0x4A1120) store 3 into the body's
// active sphere mask (+0x28) around 13F488/13AA48 and restore 0xFFFFFFFF before 105398: the ground
// and air obstacle queries test only spheres 0/1 (lower spine, head). Both words are 3 in the ELF
// and at runtime (passive-inputs 731: a thigh/arm sphere touched terrain that the PS2 never tested).
static BodyCollisionVolume originalCoreBodyQueryVolume(BodyCollisionVolume volume){volume.activeMask=3u;return volume;}
// 139C88 touchdown pushes the rider with 106538 (rider+0x110 and +0x9D0 += push, 329B40 re-offsets the body), so
// 13AA48 queries the body at the touched-down position; a crash landing keeps the air filter normal (+0x180) and
// would otherwise meet the terrain it just landed on (pipe-uber 670).
static BodyCollisionVolume offsetBodyVolume(BodyCollisionVolume volume,const std::array<float,3>& d){
 if(d[0]==0&&d[1]==0&&d[2]==0)return volume;terrain_original::Rounding rounding;
 for(unsigned k=0;k<3;k++){volume.broadCenterCm[k]=terrain_original::add(volume.broadCenterCm[k],d[k]);for(unsigned i=0;i<volume.count;i++)volume.spheres[i].centerCm[k]=terrain_original::add(volume.spheres[i].centerCm[k],d[k]);}
 return volume;}
void apply_reset_placement(const OriginalResetPlacement&,bool);void leave_reset_motion();void clear_reset_contacts();void reset_resume_velocity();void commit_rider_physics();
RIDER_LOCAL extern bool browserCrashActive,browserCrashExitFrame;RIDER_LOCAL extern int browserCrashResetReason;
RIDER_LOCAL extern void (*browserCrashReset)();RIDER_LOCAL extern bool (*browserCrashControl)(bool);RIDER_LOCAL extern void (*browserCrashMotion)();RIDER_LOCAL extern void (*browserHardCrash)(int,const OriginalCollisionEvent&);RIDER_LOCAL extern bool (*browserLandingCrash)(const OriginalLandingState&);
extern std::unique_ptr<CollisionWorld> cameraTerrain;RIDER_LOCAL extern std::unique_ptr<WorldBodyCollision> browserBodies;
RIDER_LOCAL extern std::array<OriginalGroundProfile,19> physicsMaterials;RIDER_LOCAL extern OriginalLandingProfile landingProfile;RIDER_LOCAL extern OriginalCollisionProfile collisionProfile;RIDER_LOCAL extern OriginalCollisionHistory collisionHistory;
int browser_surface_property(int);void publish_crash_actor(const OriginalCrashActorState&,int);void leave_crash_motion(OriginalCrashActorState&,int);void sync_crash_prediction(const OriginalAirTrajectory&,OriginalAirState,bool);
RIDER_LOCAL extern uint32_t motionTick,lastGroundLeave;RIDER_LOCAL extern int landed;
static void reset_boost_fx();
// Six-rider phase split of animation_tick (see animation_pose below).
RIDER_LOCAL static struct {bool pending=false;int grounded=0,jumpHeld=0;bool startFrame=false,resetFrame=false,crashFrame=false,railFrame=false,softFrame=false;std::array<float,3> prePoseUp{};std::array<float,4> presentationQuaternion{};} animationPost;
static float* animation_post_phase();
RIDER_LOCAL static int riderHostFlags=0; // web/ai-racers.js: 1 = phase hooks (the human core), 2 = rider pairs 107888 through the race host, 4 = no renderer poses (a computer rider)
EM_JS(void,js_rider_after_pose,(),{if(Module.riderHost&&Module.riderHost.afterPose)Module.riderHost.afterPose();});
EM_JS(void,js_rider_pairs,(),{if(Module.riderHost&&Module.riderHost.pairs)Module.riderHost.pairs();});
static bool rider_pairs_hosted(){return (riderHostFlags&2)!=0;}
// A terrain departure's motion switch 11FE78(1) is at 13F2CC, after 13F488/105398/107888: until this
// rider's 121750 reaches it, its motion owner is still ground (0) -- crash entries (10EB30 from 13F488)
// start sliding, and rider pairs see a grounded actor. core.cpp's step_rider has already left the ground.
RIDER_LOCAL static bool groundDeparturePending=false;
static void rider_pair_point(){if(riderHostFlags&2)js_rider_pairs();} // 0x107888 inside this rider's 121750
static void record_snow_crash(std::array<float,3>,std::array<float,3>,float,int);
#include "rail_scoring.inc"
static void release_air_control(){
 gs.controlState=5;air=originalAirControlRelease(prewind);grab={};
 if(gs.prewindStyle==3||gs.prewindStyle==4){terrain_original::Rounding rounding;auto sc=originalSinCos(-0.f);graph.defaultRoot={{0,0,0},{sc[0]*0.f,sc[0]*0.f,sc[0]*1.f,sc[1]}};} //12EA4C..12EAC0: a sideways (rail) style resets the default root (anim+0x30/+0x40) before 12EE30 plays the release clip (metro-air-tricks 981)
 originalAirReleaseGroundTargets(gp,gs,gs.velocity);
 float rate=(prewind.spin.current!=0||prewind.flip.current!=0)?originalAirReleaseAnimationRate(airProfile.trickStat,air.spinRate,air.flipRate):-1.f; //12EE30 plays with the pending animator +0x1C and sets the head rate (311B20) only for a spin/flip release (tech-oob-dance 2897)
 if(!graph.enter(originalAirReleaseAnimation(prewind.spin.current,prewind.flip.current,gs.reverseStance,gs.prewindStyle),rate,~uint64_t(0),true))throw std::runtime_error("Missing original air release animation");
 gs.prewindStyle=physicsState.prewindStyle=0; //12EAD8: the release clears rider+0x328 after 12EE30 (a held jump that left a rail released in the air kept style 4 until the crash entry: metro-air-tricks 1011)
}
RIDER_LOCAL static bool landingAirExitBaked=false;
RIDER_LOCAL static std::array<float,19> airExitInfo{};
static bool landing_air_exit(){
 if(!physicsAttached||previousGround||heldAirMode||passiveMode||browserSoftActive||physicsState.controlState==12||physicsState.controlState==6||physicsState.controlState==1)return false;
 OriginalAirExitState exit;exit.physical={physicsState.position,physicsState.quaternion};
 exit.adjustment28C=gs.adjustment28C;exit.adjustment298=gs.adjustment298;
 airExitInfo[0]+=1;for(unsigned i=0;i<3;i++){airExitInfo[1+i]=exit.physical.position[i];airExitInfo[15+i]=currentPivot[i];}
 for(unsigned i=0;i<4;i++)airExitInfo[4+i]=exit.physical.quaternion[i];
 originalAirControlExit(air,prewind,exit,currentPivot);graph.fade(1,.33000001311302185f);
 physicsState.position=exit.physical.position;physicsState.quaternion=exit.physical.quaternion;
 physicsState.physicalForward=exit.forward;physicsState.boardUp=exit.up;physicsState.manualSpin=air.spinRate;
 gs.adjustment28C=physicsState.adjustment28C=exit.adjustment28C;gs.adjustment298=physicsState.adjustment298=exit.adjustment298;
 for(unsigned i=0;i<3;i++)airExitInfo[8+i]=exit.physical.position[i];
 for(unsigned i=0;i<4;i++)airExitInfo[11+i]=exit.physical.quaternion[i];airExitInfo[18]=air.spinRate;
 landingAirExitBaked=true;return true;
}
RIDER_LOCAL_LAZY static BrowserCrashRuntime crash;
RIDER_LOCAL static std::vector<AnimationTransform> cachedCrashWorld;
RIDER_LOCAL static std::vector<AnimationTransform> resetStaleWorld;RIDER_LOCAL static AnimationTransform resetPlacedBoardRoot; // the board root of that placed pose (web/stage_teleport.inc) // cached world bones at the 11D660 placement (the original's +0x2C cache keeps the last pose)
RIDER_LOCAL static std::vector<std::array<float,16>> sourceSkinBind;
RIDER_LOCAL static std::vector<float> sourceSkinMatrices;
RIDER_LOCAL static unsigned sourceSkinMatrixTick=0xffffffffu,sourcePaletteTick=0xffffffffu;
RIDER_LOCAL static std::vector<OriginalSkinWeights> sourceSkinGroups;
RIDER_LOCAL static std::vector<uint32_t> sourceSkinIndices;
RIDER_LOCAL static std::vector<OriginalSkinMatrix> sourcePalette;RIDER_LOCAL static bool sourcePaletteFast=false;RIDER_LOCAL static std::vector<std::array<float,16>> fastSkinBones;
static void clear_skin_matrices(){sourceSkinMatrices.clear();sourceSkinMatrixTick=0xffffffffu;sourcePaletteTick=0xffffffffu;sourcePalette.clear();}
static void setup_skin_bind(const json& rig){
 cachedCrashWorld.clear();sourceSkinBind.clear();sourceSkinGroups.clear();sourceSkinIndices.clear();clear_skin_matrices();
 if(!rig.contains("source_bind_matrix_words"))return;
 if(rig.at("source_bind_matrix_space")!="source-centimeters-Z-up"||rig.at("source_bind_matrix_words").size()!=rig.at("bones").size())throw std::runtime_error("Invalid source skin bind package");
 for(const auto& row:rig.at("source_bind_matrix_words")){auto words=row.get<std::array<uint32_t,16>>();std::array<float,16> matrix;for(unsigned i=0;i<16;++i){matrix[i]=std::bit_cast<float>(words[i]);if(!std::isfinite(matrix[i]))throw std::runtime_error("Nonfinite source skin bind");}sourceSkinBind.push_back(matrix);}
 if(rig.contains("source_skin")){
  if(rig.at("source_skin_weight_units")!="integer-percent")throw std::runtime_error("Invalid source skin weight units");
  std::map<std::vector<std::pair<unsigned,int>>,uint32_t> groups;
  for(const auto& row:rig.at("source_skin")){
   if(row.empty()||row.size()>4)throw std::runtime_error("Invalid source skin influence count");
   std::vector<std::pair<unsigned,int>> key;OriginalSkinWeights weights;
   for(const auto& entry:row){unsigned bone=entry.at(0);int weight=entry.at(1);if(bone>=sourceSkinBind.size()||bone>255||weight<0||weight>32767)throw std::runtime_error("Invalid source skin influence");key.emplace_back(bone,weight);weights.push_back({int16_t(weight),uint8_t(bone)});}
   auto [at,inserted]=groups.emplace(key,uint32_t(sourceSkinGroups.size()));if(inserted)sourceSkinGroups.push_back(std::move(weights));sourceSkinIndices.push_back(at->second);
  }
 }
}
RIDER_LOCAL static int completedMainSemantic=-1;RIDER_LOCAL static bool completedMain=false;
RIDER_LOCAL static uint32_t crashSerial=0,crashObservers=0;RIDER_LOCAL static int lastCrashObserver=-1,lastCrashSemantic=-1;RIDER_LOCAL static float lastCrashImpact=0,crashPresentation=0;
RIDER_LOCAL static float tmpBegin[8]; //TMPDEBUG
RIDER_LOCAL static bool animationPostActive=false; // inside 121750: a crash entered here (13A530 touchdown) sees 106538's pending rider+0x9D0 translation
// The pair part of rider+0x9D0: 107888's 106538 pushes of this rider (web/npc_gameplay.inc pair_translate) from its motion tick on (121020
// clears the companion translation; animation_pose here) until its own 121750 commits +0x9D0 to the cached world bones (310530) with the
// contacts' translation. The next tick's rail step 13AF28 queries from that pushed board bone (PS2 DRA4 Psymon: the 1854 pair push of
// 2.05 cm is in record 1855's world bones, and the 1855 rail step slides from it). Pushes after the commit are dropped by the next 121020.
RIDER_LOCAL static std::array<float,3> pairCompanion{};
static OriginalCrashClipState crash_clip(){
 if(cachedCrashWorld.size()<24)throw std::runtime_error("Crash needs a sampled world pose");
 OriginalCrashClipState value;value.semantic=graph.requestedSemantics[2];value.animationClass=graph.currentClass(2);value.progress=graph.channelProgress(2);value.duration10=graph.channelDuration(2);value.speed90=graph.channelRate(2);
 value.complete=completedMain&&completedMainSemantic==value.semantic;for(const auto& sequence:graph.sequences)if(sequence.channel==2&&sequence.semantic==value.semantic)value.complete|=sequence.completed;
 value.primary=cachedCrashWorld[0];value.secondary=cachedCrashWorld[23];return value;
}
static void reset_crash(){auto host=std::move(crash.host);crash={};crash.host=std::move(host);crash.riderCategory=1;browserCrashActive=false;browserCrashExitFrame=false;crashSerial=crashObservers=0;lastCrashObserver=lastCrashSemantic=-1;lastCrashImpact=crashPresentation=0;}
static void detach_rail_for_crash();
static void enter_crash(int semantic,const OriginalCollisionEvent& event){
 if(crash.active)return;clear_start();
 const bool crashFromAirControl=gs.controlState==5&&!heldAirMode&&!passiveMode&&!::grounded;
 const int previousMotion=browserRailActive?4:(::grounded||groundDeparturePending)?0:1;
 const bool airborne=previousMotion==1;
 if(airborne&&!landingAirExitBaked)landing_air_exit();
 OriginalHardCrashEntryState state;state.physical={physicsState.position,physicsState.quaternion};state.physicalUp=physicsState.boardUp;state.prewindStyle328=physicsState.prewindStyle;
 OriginalHardCrashEntryCallbacks cb;
 cb.reportPeakImpact=[](float speed){collisionHistory.peakImpactCmps=std::max(collisionHistory.peakImpactCmps,speed);audio_event(AE_RUMBLE_IMPACT,speed);}; /*10EB78: rider+0x88 -> owner +0xDFC (pad rumble, web/rumble.js)*/
 cb.recordCrash=[](bool attacked){crash.penalty=score_bail(attacked);return crash.penalty;}; /*119B08*/
 cb.changeBoostMeter=[](float delta){award_boost(delta,1);physicsState.boost=boostState.amount;}; /*10EBA4: 10E098(rider,119B08 penalty,1)*/
 cb.notify=[](OriginalHardCrashObserver observer,float value){lastCrashObserver=100+int(observer);++crashObservers;grab={};scoreEvent=0;
  if(observer==OriginalHardCrashObserver::BoostPenaltyFeedback)audio_event(AE_CRASH,value,float(physicsProfile.surface.id));}; /*10EBAC..10EBEC: 296310, 29F660(0), 2961F0(penalty)*/
 cb.reportImpact=[](const OriginalCollisionEvent& impact,int){lastCrashImpact=impact.closingSpeedCmps;record_snow_crash(impact.pointCm,impact.normal,impact.closingSpeedCmps,impact.surface);};
 cb.rotateAnimationRoot=[](float angle){terrain_original::Rounding rounding;auto sc=originalSinCos(terrain_original::mul(-angle,.5f));AnimationTransform delta;delta.rotation={terrain_original::mul(sc[0],0.f),terrain_original::mul(sc[0],0.f),sc[0],sc[1]};graph.offsetSequenceRoots(delta);}; /*311B48 rotates the roots by -angle: sincos(-angle*.5) x (0,0,1,0)*/
 cb.resetAnimationRootBasis=[](const AnimationTransform& root){graph.defaultRoot=root;};
 cb.presentedRoot=[](const OriginalHardCrashEntryState& value){RiderRootPresentation p;p.turn=gs.turn.current;p.brake=gs.brake.current;p.extraLean=physicsState.extraLean.current;p.roll=physicsState.presentationRoll.current;p.liftCm=physicsState.presentationLift.current;p.lateral=physicsState.lateral;p.controlState=13;return originalRiderRootPresentation(value.physical,graph.sampledLocal->at(22).position,graph.scale,p);};
 cb.previewCrashRoot=[](int clip){auto root=graph.previewRoot(clip);if(!root)throw std::runtime_error("Missing crash preview root");return *root;};
 cb.currentScaledLocalRoot=[](){auto root=graph.scaledLocalRoot();if(!root)throw std::runtime_error("Missing sampled crash root");return *root;};
 cb.offsetAnimationRoots=[](const AnimationTransform& root){graph.offsetSequenceRoots(root);};cb.playAnimation=crash.host.play;
 cb.enterControl=[&](int control,OriginalHardCrashEntryState& value){physicsState.controlState=gs.controlState=control;if(control==13)return;
  OriginalCrashActorState actor;actor.position=value.physical.position;actor.quaternion=value.physical.rotation;actor.velocity=physicsState.velocity;actor.groundNormal=physicsState.normal; //rider+370: landing contact normal, air +180 copy, or ground normal
  actor.surfaceVelocity=physicsState.surfaceVelocity;actor.surface=physicsProfile.surface.id;actor.timeScale=physicsState.timeScale;actor.contactDistance=physicsState.distance;
  crash.beginControl(actor,animationPostActive?browserLandingTranslation:terrain_original::Vector{}); /*the control-8 entry adds the pending rider+0x9D0 (not yet committed to the cached bones) to the posed primary/secondary before 136D40 detaches the board (score-uber 480)*/tmpBegin[0]=float(animationTick);for(unsigned k=0;k<3;k++){tmpBegin[1+k]=crash.actor.detachedPosition[k];tmpBegin[4+k]=browserLandingTranslation[k];} /*TMPDEBUG*/if(crash_clip().animationClass==22)legWeight=0;boostState.window=physicsState.boostWindow=0;
 };
 cb.enterMotion=[&](int mode,OriginalHardCrashEntryState&){if(mode!=2)throw std::runtime_error("Invalid original crash motion entry");crash.beginMotion(previousMotion);if(crash.motion.submode==1)sync_crash_prediction(crash.trajectory,{crash.actor.position,crash.actor.velocity},true); /*136C40 restarts the rider's predictor (heading 0) before this tick's camera: 163E8C's landing angle is 0 (event-race 899)*/detach_rail_for_crash();browserCrashActive=true;browserSoftActive=browserSoftFrame=false;heldAirMode=passiveMode=false;if(crashFromAirControl)prewind={};air={};landingAirExitBaked=false; /*only the control-5 exit 134CB0 zeroes the prewind triplets (tech-oob-hops 3368: 12F620 rates survive a crash from control 3/4)*/
  //10EB30 clears only the 1F0/208/250 triplets (and +330); 11FE78(2) then runs the old motion's exit: 13F410 from the
  // ground re-arms the 208/2BC/2C8 decays (targets 0) and stamps the leave tick. Brake, crouch, +1FC, lift and board
  // alignment keep their values and keep decaying through the crash ticks' 1211F8 pass.
  physicsState.turn={};physicsState.extraLean={};physicsState.presentationRoll={};
  if(previousMotion==0&&!groundDeparturePending)originalLandingGroundLeave(physicsState,motionTick,lastGroundLeave); /*a departure tick already left the ground (core.cpp begin_airborne)*/
  physicsState.prewindStyle=0;physicsState.manualSpin=0;publish_crash_actor(crash.actor,crash.motion.submode);
 };
 originalHardCrashEnter(state,semantic,false,0,event,cb);lastCrashSemantic=semantic;++crashSerial;
}
static bool landing_crash(const OriginalLandingState& state){
 if(!physicsAttached)return false;
 OriginalLandingClassification input;input.animationClass=graph.currentClass(2);input.animationFlags=graph.flags(2);input.landingStat=landingProfile.landingStat;
 auto choice=originalLandingClassify(state,input);if(choice.consumedRandom){input.randomWord=rng.nextMotion();choice=originalLandingClassify(state,input);}
 if(choice.crashAnimation==438)return false;
 OriginalCollisionEvent event;event.pointCm=state.groundPoint;event.normal=state.rider.normal;event.closingSpeedCmps=state.impactNormalSpeed;event.surface=state.surface;event.surfaceProperty44=browser_surface_property(state.surface);
 auto direction=state.rider.velocity;const auto length=std::sqrt(terrain_original::dot(direction,direction));if(length>0)for(auto& value:direction)value/=length;event.incomingDirection=direction;
 enter_crash(choice.crashAnimation,event);return true;
}
struct BrowserPadCommand;static uint32_t crash_command_word(bool jump);
RIDER_LOCAL static int audioCrashPhase=-1; // crash control phase before this tick's step (audio observers)
static bool control_crash(bool jump){ControllerDraws controllerDraws;const uint32_t command=crash_command_word(jump);audioCrashPhase=crash.control.phase;
 originalBoostControl(boostState,boostProfile,false,false);physicsState.boost=boostState.amount; //12CB68 begins with 114130(rider, 0, 0): a crash stops the boost (+0x2FC 0; PS2 Gravitude Allegra 4911)
 if(crash.control.phase!=3&&(command&0x2000)){auto copy=crash.control;auto meter=originalCrashRecoveryMeter(copy,command,crash.riderCategory);audio_event(AE_CRASH_METER,meter.reachedRecovery?1.f:0.f,meter.value);} /*12CB68: 29E970 / 29E590*/
 crash.stepControl(command);if(audioCrashPhase==2&&crash.control.phase!=2)audio_event(AE_CRASH_AIR); /*12D160: both exits of the slide phase (-> 3 get-up, -> 1 air) run 296E20 + 297438*/browserCrashActive=crash.active;if(crash.active)browser_controller_takeoff_wind(crash.actor.velocity); /*12CB68 runs before 1211F8 -> 120378 (air-tricks 751)*/return crash.active;}
static OriginalPassiveAirCommand passive_command(float turn,int jumpHeld);
static void motion_crash(){
 if(!cameraTerrain||!browserBodies)throw std::runtime_error("Crash motion needs original world geometry");
 crash.stepMotion(*cameraTerrain,*browserBodies,physicsMaterials,landingProfile,browser_surface_property);
 publish_crash_actor(crash.actor,crash.motion.submode);sync_crash_prediction(crash.trajectory,{crash.actor.position,crash.actor.velocity},crash.motion.submode==1);
}
extern void request_crash_camera_shake(float);
static void setup_crash(){
 crash.host.cameraShake=request_crash_camera_shake;
 crash.host.clip=crash_clip;crash.host.play=[](int semantic){grab={};if(!graph.enter(semantic,-1,~uint64_t(0),true))throw std::runtime_error("Missing original crash animation");physicsState.animationIndex=semantic;physicsState.animationClass=graph.currentClass(2);};
 crash.host.rate=[](float rate){graph.setRate(2,rate);};crash.host.seek=[](float time){if(!graph.seekChannel(2,time))throw std::runtime_error("Crash seek without animation sequence");};
 crash.host.enterControl=[](int control){
  //111578 exits control8 through 12E690: on even logic ticks (and when rider+80's +6C0 query succeeds, assumed true) 10E028 requests reaction4 now.
  if(physicsState.controlState==8&&control!=8)audio_event(AE_CRASH_EXIT); //12E690 -> 2A02D8 wipeout speech
  if(physicsState.controlState==8&&control!=8&&(controllerGround.logicTick&1u)==0&&browserHumanRider){/*12E6BC: +6C0 isHuman*/upperRequest358=4;upperRequestTick354=int32_t(controllerGround.logicTick);}
  physicsState.controlState=gs.controlState=control;};crash.host.stanceDiffers=[](){return !physicsState.state320Equals324;};
 crash.host.leaveMotion=[](int mode){leave_crash_motion(crash.actor,mode);previousGround=mode==0;previousHeld=false;heldAirMode=passiveMode=false;prewind={};air=mode==1?originalAirControlBegin(0,0):OriginalAirControlState{};landingAirExitBaked=false;gs.controlState=mode==0?0:5;};
 crash.host.requestReset=[](int reason){if(browserResetBegin)browserResetBegin(reason);else browserCrashResetReason=reason;};crash.host.observer=[](int observer){lastCrashObserver=observer;++crashObservers;
  // 12CD20 (phase 0 -> 2) / 12D4E8 (phase 1 -> 2) slide loops, 12D160 grunt and loop stop, 12D848 get-up thud.
  if(observer==int(OriginalCrashObserver::GroundMoving)||observer==int(OriginalCrashObserver::GroundStopped))audio_event(AE_CRASH_LOOP,float(graph.currentClass(2)),float(audioCrashPhase),observer==int(OriginalCrashObserver::GroundStopped)?1.f:0.f);
  else if(observer==int(OriginalCrashObserver::SpecialRecovery))audio_event(AE_CRASH_GRUNT,1);
  else if(observer==int(OriginalCrashObserver::StopCrash)&&audioCrashPhase==3)audio_event(AE_LANDING,0,audioImpact770,2);};
 crash.host.refund=[](float quick){score_recovery(quick!=0);}; //10F280 -> 119BB0 + 10E098 (web/score_gameplay.inc)
 crash.host.collisionImpact=[](float speed){lastCrashImpact=speed;record_snow_crash(crash.actor.contactPoint,crash.actor.groundNormal,speed,crash.actor.surface);};
 crash.host.impact=[](float speed){lastCrashImpact=speed;audio_event(AE_RUMBLE_IMPACT,speed);};crash.host.presentation=[](float value){crashPresentation=value;audio_event(AE_RUMBLE_SLIDE,value);}; /*rumble: owner +0xDFC max (impacts) / +0xE00 set: the phase-2 slide 12D23C (0.5 x playback) and the phase-3 get-up 12D8E8 (2 x (1 - clip progress)); both checked against the recorded +0xE00 (local/ps2-capture/runs/rumble, web/test-rumble.mjs)*/
 crash.host.preview=[](int semantic){auto root=graph.previewRoot(semantic);if(!root)throw std::runtime_error("Missing continuation root");return *root;};crash.host.localRoot=[](){auto root=graph.scaledLocalRoot();if(!root)throw std::runtime_error("Missing crash local root");return *root;};
 crash.host.offsetRoots=[](const AnimationTransform& root){graph.offsetSequenceRoots(root);};crash.host.speedLimit=[](){return physicsProfile.speedLimit;};crash.host.random=[](){return rng.nextMotion();};crash.host.pairs=[](){rider_pair_point();};crash.riderCategory=1;
 browserCrashReset=reset_crash;browserCrashControl=control_crash;browserCrashMotion=motion_crash;browserHardCrash=enter_crash;browserLandingCrash=landing_crash;
}

RIDER_LOCAL static std::optional<OriginalStanceRestoreResult> lastStanceRestore;RIDER_LOCAL static int stanceRestoreOrder=0;
static void restore_stance(int style){
 if(style<0||style>4)throw std::runtime_error("Unsupported stance style");
 OriginalStanceRestoreState state;state.prewindStyle=style;state.motionMode=grounded?0:1;state.physical=originalOrientationBasis(physicsState.quaternion);
 physicsState.prewindStyle=style;stanceRestoreOrder=0;OriginalStanceRestoreCallbacks callbacks;
 callbacks.physicalChanged=[](const OriginalPhysicalOrientation& value){stanceRestoreOrder=stanceRestoreOrder*10+1;physicsState.quaternion=value.quaternion;physicsState.physicalForward=value.forward;physicsState.boardUp=value.up;};
 callbacks.rotateSequenceRoots=[](std::array<float,4> q){stanceRestoreOrder=stanceRestoreOrder*10+2;AnimationTransform rotation;rotation.rotation=q;for(auto& sequence:graph.sequences)sequence.root=originalAnimationCompose(rotation,sequence.root);};
 callbacks.resetDefaultRoot=[](std::array<float,3> p,std::array<float,4> q){stanceRestoreOrder=stanceRestoreOrder*10+3;graph.defaultRoot={p,q};};
 callbacks.requestAnimation=[](int semantic,float,uint32_t){stanceRestoreOrder=stanceRestoreOrder*10+4;if(graph.requestedSemantics[2]!=semantic&&!graph.enter(semantic))throw std::runtime_error("Missing stance-restoration animation");};
 lastStanceRestore=originalRestoreStance(state,callbacks);physicsState.prewindStyle=state.prewindStyle;gs.prewindStyle=state.prewindStyle;
 if(lastStanceRestore->restored)publish_motion();
}
static void reverse_roots(std::array<float,4> root){
 auto sc=originalSinCos(-1.5707963705062866f);AnimationTransform delta;delta.rotation={sc[0]*0.f,sc[0]*0.f,sc[0],sc[1]};
 for(auto& sequence:graph.sequences)sequence.root=originalAnimationCompose(delta,sequence.root);
 graph.defaultRoot={};graph.defaultRoot.rotation=root;graph.defaultMirror=physicsState.reverseStance;gs.animationTurn=physicsState.animationTurn;
}
static void reverse_animation(std::array<float,4> root){
 reverse_roots(root);
 if(!graph.enter(21))throw std::runtime_error("Missing original reverse-turn animation");
}
std::array<float,4> browser_environment_colour(int,std::array<float,4>);void browser_lighting_painter_step(float,float); // web/environment_bridge.cpp (2ED490 Lighting wrapper at rider+0x460/+0x464)
RIDER_LOCAL static board_trail::Profile trailProfile;RIDER_LOCAL_LAZY static board_trail::State trailState;
RIDER_LOCAL static board_trail::Input trailInput;RIDER_LOCAL static uint32_t trailVisualRandom=0,trailSerial=0;RIDER_LOCAL static std::array<int,5> trailBones{};
RIDER_LOCAL static std::vector<float> trailRibbon,trailRoof;
// Presentation output like the snow sprites: built from trailState when read (trail_info/trail_ribbon/trail_roof).
RIDER_LOCAL static bool trailBuffersStale=false;
static void reset_trail(){trailState=board_trail::State(trailProfile);trailRibbon.clear();trailRoof.clear();trailBuffersStale=false;++trailSerial;}
static void setup_trail(const json& config){
 const auto& t=config.at("original_board_trail");const auto& p=t.at("profile");trailProfile={};
 trailProfile.innerWidth=p.at("inner_width_scale");trailProfile.outerWidth=p.at("outer_width_scale");trailProfile.height=p.at("height_scale");trailProfile.depthHeight=p.at("depth_to_height_scale");trailProfile.innerJitter=p.at("inner_position_jitter");trailProfile.fadeSegments=p.at("fade_segments");
 trailProfile.depths={p.at("packed_depth_scale"),p.at("loose_depth_scale"),p.at("powder_depth_scale"),p.at("deep_powder_depth_scale")};trailProfile.planeOffset=p.at("plane_offset");trailProfile.backwardsOffset=p.at("backwards_offset");trailProfile.middleU=p.at("middle_texture_u");trailProfile.innerU=p.at("inner_texture_u");trailProfile.outerU=p.at("outer_texture_u");trailProfile.turnCosine=p.at("turn_cosine");trailProfile.minimumSpeed=p.at("minimum_speed");trailProfile.anglePeak=p.at("angle_peak");trailProfile.angleScale=p.at("angle_scale");trailProfile.speedScale=p.at("speed_scale");
 trailInput={};trailInput.environmentARGB=t.at("environment_argb").get<std::array<float,4>>();const auto& i=t.at("initial_context");trailInput.flagAC4=i.at("flagAC4");trailInput.flagAD0=i.at("flagAD0");trailInput.flagAFC=i.at("flagAFC");trailInput.flagB00=i.at("flagB00");
 const auto& b=t.at("bone_indices");trailBones={b.at("board"),b.at("bone8B0"),b.at("bone8B8"),b.at("bone918"),b.at("bone8E8")};trailVisualRandom=t.at("visual_rng_word");reset_trail();
}
// The rider motion mode the FX passes read: 3 during a reset and on the frozen grid (the countdown until the push-off
// 0x120378 Motion 0: 2E87E8 declines the board track, PS2 LCG trace event-race), 2 crash, 4 rail, 0 ground, 1 air.
static int rider_motion_mode(){return browserResetActive||browserStartFrozen?3:crash.active?2:browserRailActive?4: ::grounded?0:1;}
static void weather_painter_step(float x,float y); // 2ED490 Weather wrapper (+0x20), below with the breath state
static bool weather_located_step(OriginalEnvironmentTransition& s,float x,float y); // web/weather.inc: a streamed world's location record (2C0778, gp+0x770)
static void weather_region_update(int patch); // web/weather.inc: 2ED490 block 0 sets gp+0x770 after its wrappers
RIDER_LOCAL extern int browserGroundPatch; // web/core.cpp: rider+0x430 (the contact patch, -1 off the terrain)
static void weather_splash_impact(const SnowVector& position,float strength); // web/weather.inc 0x2F4260
static void weather_rider_fx_reset(); // web/weather.inc: 0x111890's 0x2C03E8 (every painter) and the splash reset of the rider's camera
RIDER_LOCAL static std::array<float,3> painterPoint{},painterTrailSeen{},painterPlacement{},painterPlacementTrail{};RIDER_LOCAL static bool painterPointSet=false,painterPlacementPending=false; // rider+0x460 (update_trail)
static void update_trail(){
 if(!physicsAttached||cachedCrashWorld.empty())return;
 const auto board=originalRiderCollisionFrame(cachedCrashWorld.at(trailBones[0]));
 trailInput.motion=rider_motion_mode();trailInput.crashSubmode=crash.motion.submode;trailInput.detached=crash.active&&crash.actor.detached;
 trailInput.surface=crash.active?crash.actor.surface:physicsProfile.surface.id;trailInput.semantic=graph.requestedSemantics[2];trailInput.marker0=graph.flags(2)&1;trailInput.marker1=graph.flags(2)&2;trailInput.reverseStance=physicsState.reverseStance;trailInput.manual=false;
 trailInput.board={board.right,board.up,board.origin};trailInput.bone8B0=cachedCrashWorld.at(trailBones[1]).position;trailInput.bone8B8=cachedCrashWorld.at(trailBones[2]).position;trailInput.bone918=cachedCrashWorld.at(trailBones[3]).position;trailInput.bone8E8=cachedCrashWorld.at(trailBones[4]).position;
 trailInput.contact=crash.active&&crash.motion.submode==0&&crash.ticks?crash.actor.contactPoint:browserTrailContact;trailInput.normal=physicsState.normal;trailInput.velocity=physicsState.velocity;trailInput.contactDistance=physicsState.distance;
 // 2ED490 steps the block's painters at rider+0x460/+0x464, the rider's last contact point: a ground contact (browserTrailContact
 // when it changes) or, in a crash, the crash body's terrain contact -- kept while the body flies and after the crash (a reset)
 // until the next ground contact or placement. The trail's fallback (the pre-crash ground point) jumped the painter distance:
 // PS2 weather/frd-regions 1925 (+181 cm, a crash bounce), weather/era5-reset 1139 (+3922 cm, a crash into a reset).
 if(!painterPointSet||browserTrailContact!=painterTrailSeen){painterPoint=browserTrailContact;painterTrailSeen=browserTrailContact;painterPointSet=true;}
 if(crash.active&&crash.ticks&&(crash.actor.contactPoint[0]!=0.f||crash.actor.contactPoint[1]!=0.f||crash.actor.contactPoint[2]!=0.f))painterPoint=crash.actor.contactPoint;
 browser_lighting_painter_step(painterPoint[0],painterPoint[1]);
 weather_painter_step(painterPoint[0],painterPoint[1]);
 // A reset placement (11D660) moves +0x460 to the placed position after this tick's painter step (PS2 weather/era5-reset 1159:
 // the painters step at the old point, the record then holds +0x460 = +0x110) -- unless a ground contact since the placement
 // has written +0x460 again (the rolling start's placement before tick 0: dbc2-weather tick 0 steps at the tick-0 contact).
 if(painterPlacementPending){if(browserTrailContact==painterPlacementTrail){painterPoint=painterPlacement;painterTrailSeen=browserTrailContact;painterPointSet=true;}painterPlacementPending=false;}
 weather_region_update(browserGroundPatch); // gp+0x770 = rider+0x430 & 0xFF (the human; after the block's wrappers)
 trailInput.environmentARGB=browser_environment_colour(trailInput.motion,trailInput.environmentARGB);board_trail::update(trailState,trailInput,trailVisualRandom,trailProfile);++trailSerial;trailBuffersStale=true;
}
static void build_trail_buffers(){
 trailBuffersStale=false;trailRibbon.clear();trailRoof.clear();const auto window=board_trail::drawWindow(trailState,trailProfile);
 auto emit=[&](std::vector<float>& data,int band,int slice){const auto& v=trailState.bands[band][(window.start+slice)%54];for(unsigned k=0;k<3;k++)data.push_back(v.position[k]);data.push_back(v.uvq[0]);data.push_back(v.uvq[1]);for(unsigned k=0;k<3;k++)data.push_back(float(v.rgba[k])/128.f);data.push_back(float(board_trail::fadedAlpha(v.rgba[3],float(slice)*window.fadeStep))/128.f);};
 auto quad=[&](auto& data,int a,int b,int i){emit(data,a,i);emit(data,b,i);emit(data,a,i+1);emit(data,a,i+1);emit(data,b,i);emit(data,b,i+1);};
 for(int band=0;band<5;band++)for(int i=0;i+1<window.count;i++)quad(trailRibbon,board_trail::bandOrder[band],board_trail::bandOrder[band+1],i);
 for(int i=0;i+1<window.count;i++)quad(trailRoof,0,1,i);
}

RIDER_LOCAL static std::array<OriginalSnowParticleProfile,10> snowParticleProfiles;
RIDER_LOCAL static std::array<float,10> snowFlipPhase{},snowFlipRate{},initialSnowFlipPhase{};RIDER_LOCAL static std::array<int,10> snowFlipCount{},snowTextureBase{};
RIDER_LOCAL static std::array<OriginalSnowChunkProfile,10> snowProfiles;
RIDER_LOCAL static std::array<std::unique_ptr<OriginalSnowParticles>,10> snowEmitters;
RIDER_LOCAL static std::array<std::vector<float>,10> snowBuffers;
// The particle sprites (the VU program's per-frame positions from the birth ring) are presentation output: nothing in
// the simulation reads them. update_snow only marks them stale (with the tick's enabled emitters); snow_info /
// snow_particles build them when read, from the same births, so a drawn frame reads exactly what the tick would have
// written and the ticks nobody reads (several ticks per drawn frame, riders not drawn) cost nothing.
RIDER_LOCAL static bool snowBuffersStale=false;RIDER_LOCAL static std::array<bool,10> snowBuffersEnabled{};
RIDER_LOCAL static OriginalSnowContextInput snowInput;RIDER_LOCAL static OriginalSnowContextState snowState;RIDER_LOCAL static OriginalSnowBodyState snowBody;
RIDER_LOCAL static float kickerBuildup=0,initialKickerBuildup=0;RIDER_LOCAL static std::array<float,19> kickerCapacity{};
RIDER_LOCAL static OriginalBreathState breathState,initialBreathState;
RIDER_LOCAL static OriginalEnvironmentTransition breathEnvironment,initialBreathEnvironment;
RIDER_LOCAL static OriginalEnvironmentRegions breathRegions;
RIDER_LOCAL static std::vector<OriginalEnvironmentPayload> breathPayloads;
RIDER_LOCAL static std::array<float,19> breathDefaults{};RIDER_LOCAL static unsigned breathHeadBone=5;
RIDER_LOCAL static OriginalRandomState snowParticleRandom;RIDER_LOCAL static std::array<OriginalSnowSurface,19> snowSurfaces;
RIDER_LOCAL static uint32_t wakeRenderSerial=0;RIDER_LOCAL static std::vector<float> wakeVertices;RIDER_LOCAL static bool wakeVerticesStale=false;/* built when read (wake_info/wake_vertices) */RIDER_LOCAL static OriginalWakePhysics wakePhysics;RIDER_LOCAL static bool wakeReady=false,wakeDeviceBound=true;RIDER_LOCAL static uint32_t wakeUpdates=0;
struct WakeSurface{float alpha=0,scale=0;bool flag=false;};RIDER_LOCAL static std::array<WakeSurface,19> wakeSurfaces;
RIDER_LOCAL static bool snowWide=true,snowReady=false,snowWakeGap=false;RIDER_LOCAL static uint32_t snowSerial=0,lastVisualTick=0xffffffffu,snowImpactSerial=0,snowImpactSeen=0;
// 2ED490 in the rider pass 0x1218D0 steps every wrapper of the rider's environment block with the contact point
// rider+0x460/+0x464, every tick (after 121818, before the FX passes), whatever the FX passes do. The Weather painter
// (type 12, block +0x20) feeds the breath (property 5) and the human's wind push 0x125970 (properties 3 / 4, read by the
// next tick's motion: web/core.cpp rider_wind_push).
RIDER_LOCAL static uint32_t weatherSteps=0;
static void weather_painter_step(float x,float y){if(!snowReady)return;if(!weather_located_step(breathEnvironment,x,y))originalEnvironmentTransitionStep(breathEnvironment,breathRegions,breathPayloads,breathDefaults,x,y);++weatherSteps;}
bool browser_weather_wind(float& speedKmh,float& directionDeg){if(!snowReady)return false;speedKmh=breathEnvironment.properties.current[4];directionDeg=breathEnvironment.properties.current[3];return true;}
// QA: [steps, distance, then the 19 current and the 19 target properties of the rider's Weather painter].
extern "C" EMSCRIPTEN_KEEPALIVE float* weather_painter_info(){RIDER_LOCAL static std::array<float,40> v{};v[0]=float(weatherSteps);v[1]=breathEnvironment.distance;for(unsigned i=0;i<19;i++){v[2+i]=breathEnvironment.properties.current[i];v[21+i]=breathEnvironment.properties.target[i];}return v.data();}
RIDER_LOCAL static SnowVector snowImpactPoint{},snowImpactNormal{};RIDER_LOCAL static float snowImpactStrength=0;RIDER_LOCAL static int snowImpactSurface=0;
static void record_snow_crash(SnowVector point,SnowVector normal,float strength,int surface){snowImpactPoint=point;snowImpactNormal=normal;snowImpactStrength=strength;snowImpactSurface=surface;++snowImpactSerial;}
static void reset_impact_fx(bool clearParticles); //web/impact_fx_gameplay.inc
RIDER_LOCAL static int sparkSurface438=0; //rider+0x438 as the rider FX passes read it (web/impact_fx_gameplay.inc): the rail surface on rails, kept in the air
// pv sprayReset (web/pv-flags.js, set per rider context by the page): 2DF3B0, the DynamicSpray reset (the constructor
// 2DE4A8 -> 2DF190 and the rider FX reset 111890 of a reset placement / location entry), writes FX+0x10 = 0: a placed
// rider has no carry-off (kicker, emitter 8) buildup. PS2 menus/fr/ctmstart f02700..f02760: FX+0x10 = 0 through the
// plane drop. Off: every reset restores the seed state's buildup (initial.json, the Snow Jam race start: 1.37), and a
// rider placed in the air sprays kicker snow that rises away from it.
RIDER_LOCAL static bool fxResetKicker=false;
extern "C" EMSCRIPTEN_KEEPALIVE void set_fx_reset_kicker(int on){fxResetKicker=on!=0;}
static void reset_snow(bool clearParticles){
 if(!snowReady)return;kickerBuildup=fxResetKicker&&!clearParticles?0.f:initialKickerBuildup;breathState=initialBreathState;breathEnvironment=initialBreathEnvironment;if(wakeReady)wakePhysics.reset(wakeDeviceBound);wakeUpdates=0;wakeVertices.clear();wakeVerticesStale=false;++wakeRenderSerial;snowState={};snowState.impact.alpha=.5f;snowState.impact.wideScatter=snowWide;snowBody={};originalSnowVisibility(snowState,snowInput.visibilityMode,snowInput.rider.trackingInhibited);snowImpactSeen=snowImpactSerial;lastVisualTick=0xffffffffu;
 if(clearParticles){for(unsigned i=0;i<10;i++){snowEmitters[i]=std::make_unique<OriginalSnowParticles>(snowParticleProfiles[i]);snowBuffers[i].clear();snowFlipPhase[i]=initialSnowFlipPhase[i];}snowBuffersStale=false;}++snowSerial;
}
// Camera target15F710 reads rider+220, the filtered controller value.
// The browser jump-button accumulator is cleared on release and is not this field.
float browser_camera_crouch(){return gs.crouch.current;}
OriginalRandomState* browser_camera_random(){return snowReady?&snowParticleRandom:nullptr;}
// The visual RNG words (PS2 0x4FF018) for capture comparison/sync (compare-ps2-capture.mjs --sync-visual-rng).
extern "C" EMSCRIPTEN_KEEPALIVE uint32_t* visual_rng_words(){return snowParticleRandom.words.data();}
RIDER_LOCAL static std::array<uint32_t,5> fxLcgMarks{}; // QA: the LCG before sparks, trail, snow, fist/boost and after (compare-ps2-capture LCG_SPAN)
// Rider FX passes (0x128F20..0x1290F0 in the rider manager, after every rider's 121818 on the PS2). Phases in the original
// pass order over all riders: 0 fx record + pickups, 1 board sparks 2DABC8, 2 board track, 3 snow FX 2DF920 (incl. the
// wake), 4 fist sparkle + boost FX; -1 = all. A host that runs several rider cores sets fx_deferred and calls fx_pass per
// phase and core with the shared visual words (0x4FF018 + gp+0xA0C), as ai-racers.js.
RIDER_LOCAL static bool fxDeferred=false,fxPending=false;
static void update_pickups();static void update_board_sparks();static void update_trail();static void update_snow();static void update_fist_sparkle();static void update_boost_fx();static void fx_record();
static void fx_pass_phase(int phase){
 if(phase<0||phase==0){fx_record();update_pickups();fxLcgMarks[0]=trailVisualRandom;}
 if(phase<0||phase==1){update_board_sparks();fxLcgMarks[1]=trailVisualRandom;}
 if(phase<0||phase==2){update_trail();fxLcgMarks[2]=trailVisualRandom;}
 if(phase<0||phase==3){update_snow();fxLcgMarks[3]=trailVisualRandom;}
 if(phase<0||phase==4){update_fist_sparkle();update_boost_fx();fxLcgMarks[4]=trailVisualRandom;}
}
extern "C" EMSCRIPTEN_KEEPALIVE void set_fx_deferred(int on){fxDeferred=on!=0;fxPending=false;}
// Runs one deferred phase (or all with -1) of this tick's FX pass; the last phase (4, or -1) completes it.
extern "C" EMSCRIPTEN_KEEPALIVE int fx_pass(int phase){if(!fxPending)return 0;fx_pass_phase(phase);if(phase<0||phase==4){fxPending=false;lastVisualTick=motionTick;}return 1;}
extern "C" EMSCRIPTEN_KEEPALIVE uint32_t* fx_lcg_marks(){return fxLcgMarks.data();}
extern "C" EMSCRIPTEN_KEEPALIVE uint32_t* visual_lcg_word(){return &trailVisualRandom;} // gp+0xA0C (0x4A3AFC): board trail, sparks, snow emission
static void setup_snow(const json& config){
 const auto& b=config.at("original_breath");const auto& bs=b.at("state");
 initialBreathState={bs.at("accumulator"),bs.at("phase"),bs.at("effort"),bs.at("clock"),bs.at("duration")};
 if(b.at("environment_index")!=0)throw std::runtime_error("Unsupported breath environment map");
 breathHeadBone=b.at("head_bone");if(breathHeadBone!=5)throw std::runtime_error("Unexpected original breath head bone");
 const auto& bp=b.at("environment_properties");initialBreathEnvironment.properties.current=bp.at("current").get<std::array<float,19>>();initialBreathEnvironment.properties.target=bp.at("target").get<std::array<float,19>>();
 initialBreathEnvironment.distance=b.at("environment_distance");initialBreathEnvironment.lastX=b.at("environment_last_coordinates").at(0);initialBreathEnvironment.lastY=b.at("environment_last_coordinates").at(1);breathDefaults=b.at("environment_defaults").get<std::array<float,19>>();
 const auto& map=b.at("regions");breathRegions.scale=map.at("scale");breathRegions.originX=map.at("origin").at(0);breathRegions.originY=map.at("origin").at(1);breathRegions.root=map.at("root");breathRegions.outside=map.at("outside");breathRegions.nodes.clear();
 for(const auto& node:map.at("nodes"))breathRegions.nodes.push_back({node.get<std::array<uint16_t,4>>()});breathPayloads.clear();for(const auto& p:map.at("payloads")){if(p.at("type")!=12)throw std::runtime_error("Unsupported environment payload type");breathPayloads.push_back({p.at("transition"),p.at("values").get<std::array<float,19>>()});}
 const auto& s=config.at("original_snow");initialSnowFlipPhase=s.at("state").at("emitter_flipbook_phases").get<std::array<float,10>>();initialKickerBuildup=s.at("state").at("kicker_buildup");wakePhysics.noise=s.at("wake_noise_table").get<OriginalWakeNoiseTable>();wakeDeviceBound=config.at("original_reset").at("device_index").get<int>()>=0;wakeReady=true;snowInput={};snowInput.suppressed=s.at("suppressed");snowInput.visibilityMode=s.at("initial_rider").at("visibility_mode");snowInput.secondaryBrake274=s.at("initial_rider").at("secondary_brake274");snowWide=s.at("state").at("impact").at("wide_scatter");
 snowParticleRandom.words=s.at("particle_random_state").get<std::array<uint32_t,6>>();if(trailVisualRandom!=s.at("shared_visual_lcg").get<uint32_t>())throw std::runtime_error("Snow and track visual seeds differ");
 for(const auto& entry:s.at("emitter_profiles")){
  unsigned index=entry.at("emitter_index");if(index>=10)throw std::runtime_error("Snow emitter index");const auto& d=entry.at("parameters");if(d.at("Duration").get<float>()>=0)throw std::runtime_error("Finite snow emitter lifetime not integrated");snowFlipCount[index]=d.at("NumFlipTextures");snowFlipRate[index]=d.at("FlipTextureRate");snowTextureBase[index]=d.at("TextureId");if(!std::isfinite(initialSnowFlipPhase[index])||initialSnowFlipPhase[index]<0||!(initialSnowFlipPhase[index]<snowFlipCount[index]))throw std::runtime_error("Invalid original snow flipbook phase");snowFlipPhase[index]=initialSnowFlipPhase[index];auto& p=snowParticleProfiles[index];p.particlesPerBirth=d.at("NumParticles");p.life=d.at("Life");p.lifeRange=d.at("LifeR");p.damping=d.at("Damp");p.size=d.at("Size");p.sizeRange=d.at("SizeR");p.finalSize=d.at("SizeFinal");
  auto xyz=[&](std::string key){return SnowVector{d.at(key+"X"),d.at(key+"Y"),d.at(key+"Z")};};auto rgba=[&](std::string key){return SnowColour{d.at(key+"R"),d.at(key+"G"),d.at(key+"B"),d.at(key+"A")};};
  p.offset=xyz("Off");p.positionRange0=xyz("R0");p.positionRange1=xyz("R1");p.velocity=xyz("Vel");p.force=xyz("Force");p.velocityRanges={xyz("R0V"),xyz("R1V"),xyz("R2V")};p.startColour=rgba("StartCol");p.endColour=rgba("EndCol");p.colourRange0=rgba("R0");p.colourRange1=rgba("R1");
  snowProfiles[index]={{d.at("VelScale"),d.at("NormScale"),d.at("NormSpeedScale")},d.at("SideScale")};
 }
 for(const auto& d:s.at("surfaces")){int id=d.at("id");if(id<0||id>=19)throw std::runtime_error("Snow surface index");const auto words=d.at("raw_words_48_to_8c").get<std::array<uint32_t,18>>();kickerCapacity[id]=std::bit_cast<float>(words[5]);wakeSurfaces[id]={std::bit_cast<float>(words[0]),std::bit_cast<float>(words[1]),words[15]!=0};auto& p=snowSurfaces[id];p.id=id;p.cloudActive=d.at("cloud_active");p.trailActive=d.at("trail_active");p.impactActive=d.at("impact_active");p.largeImpactActive=d.at("large_impact_active");p.cloudMaxHeightCm=d.at("cloud_max_height_cm");p.impactMultiplier=d.at("impact_multiplier");p.rockChance=d.at("rock_chance");p.smallChunkChance=d.at("small_chunk_chance");p.largeChunkChance=d.at("large_chunk_chance");p.chunksUseWakeVelocity=d.at("chunks_use_wake_velocity");p.minWakeVelocityScale=d.at("min_wake_velocity_scale");p.maxWakeVelocityScale=d.at("max_wake_velocity_scale");}
 snowReady=true;reset_snow(true);
}
// QA: the visual LCG after each 2DF920 call (chunky, rock, trail, breath, impacts, cloudy, body, kicker; [0] = entry).
RIDER_LOCAL static std::array<uint32_t,9> snowLcgMarks{};
extern "C" EMSCRIPTEN_KEEPALIVE uint32_t* snow_lcg_marks(){return snowLcgMarks.data();}
static void update_snow(){
 if(!snowReady||cachedCrashWorld.size()<24)return;
 auto& r=snowInput.rider;r.velocityCmps=physicsState.velocity;r.turn=gs.turn.current;r.brake=gs.brake.current;r.manualState=0;r.animationSemantic=graph.requestedSemantics[2];r.motionMode=trailInput.motion;r.controlState=gs.controlState;r.reverse=physicsState.reverseStance;
 r.trackingInhibited=trailInput.flagAC4;r.trackingAD0=trailInput.flagAD0;r.trackingAFC=trailInput.flagAFC;r.trackingB00=trailInput.flagB00;
 snowInput.board=originalSnowUnitBoardFrame(cachedCrashWorld.at(trailBones[0]));snowInput.groundNormal=physicsState.normal;snowInput.lateral=physicsState.lateral;snowInput.environmentARGB=trailInput.environmentARGB;snowInput.secondaryBrake274=physicsState.presentationRoll.current;
 if(landed)originalSnowImpactTrigger(snowState,browserTrailContact,physicsState.normal,browserLandingSpeed,physicsProfile.surface.id,false,r); //139C88 -> 10E910 passes |v| (f22), not the relative normal speed
 if(snowImpactSerial!=snowImpactSeen){originalSnowCrashImpactTrigger(snowState,snowImpactPoint,snowImpactNormal,snowImpactStrength,snowImpactSurface,r);snowImpactSeen=snowImpactSerial;}
 const int surface=sparkSurface438;if(surface<0||surface>=19)return; //2DF920 reads rider+0x438 (e.g. 10 on a rail: impacts off)snowWakeGap=snowSurfaces[surface].chunksUseWakeVelocity&&!wakeReady;
 if(wakeReady){
  OriginalWakeFrameInput input;input.normal=physicsState.normal;input.lateral=physicsState.lateral;input.velocity=physicsState.velocity;input.unitAxis0=snowInput.board.right;input.scaledAxis0=snowInput.board.right;input.unitAxis2=snowInput.board.up;input.boardPosition=snowInput.board.originCm;
  input.turn1F0=gs.turn.current;input.lean208=physicsState.extraLean.current;input.brake214=gs.brake.current;input.reverse320=physicsState.reverseStance;input.surfaceAlpha48=wakeSurfaces[surface].alpha;input.surfaceScale4C=wakeSurfaces[surface].scale;input.surfaceFlag84=wakeSurfaces[surface].flag;
  wakePhysics.step(input,r.motionMode,physicsState.presentationRoll.current,trailInput.environmentARGB);snowInput.wakeVelocity=wakePhysics.tip();++wakeUpdates;
 }else snowInput.wakeVelocity.reset();
 ++wakeRenderSerial;wakeVerticesStale=true;
 auto context=originalSnowContextStep(snowState,snowInput,snowSurfaces[surface]);OriginalSnowRandom visual{trailVisualRandom};
 auto submit=[&](OriginalSnowEmission birth,unsigned index){birth.emitter=index;const bool enabled=snowState.emitterEnabled[index];snowEmitters[index]->emit(birth,snowParticleRandom,enabled);snowFlipPhase[index]=originalSnowFlipbookStep(snowFlipPhase[index],snowFlipCount[index],snowFlipRate[index],birth.stepSeconds,enabled);};
 snowLcgMarks[0]=visual.word;for(auto birth:originalSnowChunkEmission({snowProfiles[1],snowProfiles[2]},context.chunks,visual))submit(birth,birth.emitter);snowLcgMarks[1]=visual.word;
 OriginalSnowEmission inactive;inactive.positionCm=snowInput.board.originCm;submit(originalRockSprayEmission(snowProfiles[3],context.cloud,r.brake,snowSurfaces[surface].rockChance,visual),3);snowLcgMarks[2]=visual.word;submit(originalSnowTrailEmission(snowProfiles[0],context.trail,visual),0);snowLcgMarks[3]=visual.word;
 // The Weather painter (breath value +0x30) was stepped by update_trail this tick (weather_painter_step).
 OriginalBreathContext breath;const auto head=originalSnowUnitBoardFrame(cachedCrashWorld.at(breathHeadBone));
 for(unsigned k=0;k<3;k++){breath.headMatrix[0][k]=head.right[k];breath.headMatrix[1][k]=head.forward[k];breath.headMatrix[2][k]=head.up[k];breath.headMatrix[3][k]=head.originCm[k];}breath.headMatrix[3][3]=1;
 breath.velocityCmps=physicsState.velocity;breath.groundNormal=physicsState.normal;breath.speedCmps=context.rider.speedCmps;breath.environmentValue=breathEnvironment.properties.current[5];breath.animationClass=graph.currentClass(2);
 submit(originalBreathEmission(breathState,snowProfiles[4],breath,visual),4);snowLcgMarks[4]=visual.word;
 {const SnowVector impactAt=snowState.impact.positionCm;const float impactStrength=snowState.impact.strength;const auto impacts=originalSnowImpactEmission(snowState.impact,context.impact,visual);
  if(impacts.requestSecondaryImpact)weather_splash_impact(impactAt,impactStrength); // 0x2E162C -> 0x2F4118: lens drops from a large impact
  for(auto birth:impacts.births)submit(birth,birth.emitter);}snowLcgMarks[5]=visual.word;
 submit(originalSnowCloudEmission(snowProfiles[7],context.cloud,visual),7);snowLcgMarks[6]=visual.word;
 constexpr std::array<unsigned,30> bodyBones={17,18,20,21,16,19,0,1,2,3,4,6,15,7,8,17,18,20,21,16,19,0,1,2,9,10,11,12,13,14};
 OriginalSnowBodyContext body;for(unsigned i=0;i<30;i++)body.boneOriginsCm[i]=cachedCrashWorld.at(bodyBones[i]).position;body.primaryBoneOriginCm=cachedCrashWorld.at(0).position;body.velocityCmps=physicsState.velocity;body.speedCmps=context.rider.speedCmps;body.colour=context.trail.colour;body.motionMode=r.motionMode;
 submit(originalSnowBodyEmission(snowProfiles[9],snowBody,snowState.impact,body),9);snowLcgMarks[7]=visual.word;submit(originalSnowKickerEmission(kickerBuildup,snowProfiles[8],context.trail,kickerCapacity[surface],visual),8);snowLcgMarks[8]=visual.word;trailVisualRandom=visual.word;
 for(unsigned i=0;i<10;i++)snowBuffersEnabled[i]=snowState.emitterEnabled[i];snowBuffersStale=true;++snowSerial;
}
static void build_wake_vertices(){
 wakeVerticesStale=false;wakeVertices.clear();if(wakeReady){
  const auto window=originalWakeDrawWindow(wakePhysics.cursor,wakePhysics.rows,wakePhysics.profile.lifetime);
  const auto fades=originalWakeRowFades(window);
  auto emit=[&](int band,int slice){const auto& row=wakePhysics.rows[(window.start+slice)%wakePhysics.cursor.capacity];for(float p:row.positions[band])wakeVertices.push_back(p);wakeVertices.push_back(row.textureU);wakeVertices.push_back(wakePhysics.profile.textureV[band]);for(auto c:row.rgb)wakeVertices.push_back(float(c)/128.f);wakeVertices.push_back(float(board_trail::fadedAlpha(uint32_t(row.alpha),fades[slice]))/128.f);};
  for(int band=0;band+1<wakePhysics.profile.columns;band++)for(int i=0;i+1<window.count;i++){emit(band,i);emit(band+1,i);emit(band,i+1);emit(band,i+1);emit(band+1,i);emit(band+1,i+1);}
 }
}
// Presentation fast mode (web/presentation_fast.hpp): this rider context's skin palette and snow sprites use plain
// binary32 arithmetic. Set per context by the page (web/quality.js low tier); off in every test.
RIDER_LOCAL static bool presentationFast=false;
extern "C" EMSCRIPTEN_KEEPALIVE void set_presentation_fast(int on){presentationFast=on!=0;}
static void build_snow_buffers(){
 snowBuffersStale=false;
 for(unsigned i=0;i<10;i++){auto& out=snowBuffers[i];out.clear();if(!snowBuffersEnabled[i])continue;if(presentationFast){presentation_fast::snowSprites(*snowEmitters[i],out);continue;}for(const auto& p:snowEmitters[i]->particles()){if(!p.colourGs[3])continue;for(float value:p.positionCm)out.push_back(value);out.push_back(p.halfExtentCm);for(auto value:p.colourGs)out.push_back(float(value)/128.f);}}
}

RIDER_LOCAL static std::vector<OriginalNpcPath> resetPaths;
// A streamed world's bank whose location was evicted (web/peak_world.inc drop_paths): the route stops (resetPaths empty, as
// before), but a reset before the next location's bank arrives re-attaches to it instead of failing. On the PS2 the eviction
// callback 12A490 -> 26ADA0 is `jr ra; nop`: the old bank is never cleared (a fall in a connector just after its Unload).
RIDER_LOCAL static std::vector<OriginalNpcPath> evictedResetPaths;RIDER_LOCAL static OriginalNpcRouteState evictedResetRoute;
RIDER_LOCAL static OriginalNpcRouteState resetRoute,initialResetRoute,initialEventRoute;RIDER_LOCAL static bool eventRouteLoaded=false;RIDER_LOCAL static uint32_t routeProgressUpdates=0;
RIDER_LOCAL static OriginalResetControlState resetControl;
RIDER_LOCAL static OriginalResetControlInputs resetInputs;
RIDER_LOCAL static bool resetAllowEnd=false,resetStance=false;
RIDER_LOCAL static uint32_t resetPlacements=0,resetCompletions=0,resetObservers=0;RIDER_LOCAL static int resetReason=0;
static void clear_reset(){browserResetActive=false;resetRoute=initialResetRoute;routeProgressUpdates=0;resetControl={};resetPlacements=resetCompletions=resetObservers=0;resetReason=0;}
static void score_wrong_way(); // web/score_gameplay.inc (11A088)
static void rail_reset_leave(); // web/rail_gameplay.inc: 116120 from control 7 runs the control exit 132048
RIDER_LOCAL static bool resetDecline=false; // begin_decline_reset: 1235F8's entry, not 116120's
static void begin_reset(int reason){
 if(browserResetActive)return;if(!resetDecline){audio_event(AE_RESET,float(reason)); /*116198: 29A220 (snd 0x7B, 28F108)*/if(reason==1||reason==4)score_wrong_way();} /*116120: reasons 1/4 call 11A088 first*/clear_start();if(resetPaths.empty()&&!evictedResetPaths.empty()){resetPaths=std::move(evictedResetPaths);evictedResetPaths.clear();resetRoute=evictedResetRoute;}if(resetPaths.empty())throw std::runtime_error("Original reset route data unavailable");
 if(!crash.active&&!::grounded){if(!landingAirExitBaked)landing_air_exit();commit_rider_physics();}
 if(!crash.active&&::grounded&&!browserRailActive)originalLandingGroundLeave(physicsState,motionTick,lastGroundLeave); //116120 requestMotion(3) runs the ground exit 13F410 first: +0x208/+0x2BC/+0x2C8 decay to 0 (tech-select-ground 439)
 resetReason=reason;resetControl={reason>0,0};rail_reset_leave();detach_rail_for_crash();browserResetActive=true;browserCrashActive=false;crash.active=false;
 physicsState.controlState=gs.controlState=9;graph.setRate(2,0);boostState.modifier=0;boostState.window=physicsState.boostWindow=0;
 heldAirMode=passiveMode=false;prewind={};air={};landingAirExitBaked=false;browserSoftActive=browserSoftFrame=false;
 grab={};banked=scoreEvent=0;
}
// 116120 while control 9 is already running: the reset control restarts from progress 0 (12F398 state) with the new reason.
static void rerequest_reset(int reason){if(!browserResetActive)return begin_reset(reason);resetReason=reason;resetControl={reason>0,0};}
// 1235F8(rider) (pv bcDecline; 30B658 / 30B758: a Big Challenge declined at the offer / fail / Restart Challenge? prompt, or quit):
// owner+0x350 = 1, 11FEC8(9), 11FE78(3), the start phase owner+0x290 = 0. It is 116120's reset without its +0x2E8 / +0x2EC clears,
// 11A088 (Wrong Way!), 29A220 (sound 0x7B) and the 270970 observer: the white fade, the route placement 20 ticks in and control 4
// at 41 (PS2 local/ps2-capture/ctm-decomp/bigchal/tri-offer: control 9 at 3631, placed 16.2 m away at 3650, control 4 at 3671).
static void begin_decline_reset(){
 if(browserResetActive){clear_start();resetControl={true,0};return;} // 11FEC8(9) again: 12F230 restarts the progress
 resetDecline=true;begin_reset(1);resetDecline=false;
}
// 11D660(rider, point, direction, semantic, clearance): the placement shared by the reset control (12F398, at the retained
// route point) and the location entry 11D390 -> 11DE60 (a region row of the path bank; place_rider_region).
// contactPose (0x123210, web/stage_teleport.inc): the placed pose is this tick's final one, so it gets 11EB98's board
// alignment and 11F3D8 leg solve as well (the reset control and the region placement pose again later in their tick).
static void reset_place_at(const terrain_original::Vector& point,const terrain_original::Vector& direction,float clearance,int semantic,bool contactPose=false){
  if(!cameraTerrain||!browserBodies)throw std::runtime_error("Reset placement requires original world collision");
  clear_reset_contacts();reset_body_queries();
  auto value=originalResetPlacement(point,direction,clearance,[](auto end,auto start,float preferred){return queryOriginalWorldSegment(*cameraTerrain,browserBodies.get(),end,start,0,preferred,true);});
  reset_trail();reset_snow(false);reset_impact_fx(false);reset_boost_fx();weather_rider_fx_reset();apply_reset_placement(value,resetStance);crash.actor.detached=false;painterPlacement=physicsState.position;painterPlacementTrail=browserTrailContact;painterPlacementPending=true;
  graph.sequences.clear();graph.sampledLocal.reset();graph.requestedSemantics.fill(438);graph.nextRate=1; /*311A50: animator +0x1C = 1*/graph.defaultMirror=resetStance;graph.defaultRoot={};
  auto sc=originalSinCos((resetStance?-3.1415927410125732f:-0.f)*.5f);graph.defaultRoot.rotation={0.f*sc[0],0.f*sc[0],sc[0],sc[1]};
  gs=physicsState;legWeight=1;poseContact=initialPoseContact;currentPivot={};completedMain=false;completedMainSemantic=-1;resetStaleWorld=cachedCrashWorld;cachedCrashWorld.clear();idleSeconds=0;upperRequest358=0; /*11D660 zeroes +0x35C (115D48 idle clock) and +0x358 (pending 10E028 reaction)*/
  if(!graph.enter(semantic,-1,~uint64_t(0),true))throw std::runtime_error("Missing original reset animation");{auto step=gs;step.timeScale=1;graph.advance(step,prewind.spin.current,prewind.flip.current);graph.completeSequences();} /*11D660: 3128E8, 312598, then 11EB60(f12 = 1): one full-rate step at placement, before 11DF18 (311B20) sets the rate to 0*/
  physicsState.animationIndex=semantic;physicsState.animationClass=graph.currentClass(2);++resetPlacements;
  // 11D660 -> 11EB60/11EB98: the placement poses the rider at the new spot (cached world +0x2C) before 1211F8, so this
  // tick's 120378 selects the secondary motion from the placed pose (score-uber 739: 416, not the pre-reset pose's 417).
  {const auto local=originalAnimationLocalPose(graph.rig->bones,graph.rig->clips,originalAnimationLayers(graph.sequences));RiderRootPresentation rs;rs.turn=gs.turn.current;rs.brake=gs.brake.current;rs.extraLean=physicsState.extraLean.current;rs.roll=physicsState.presentationRoll.current;rs.liftCm=physicsState.presentationLift.current;rs.lateral=physicsState.lateral;rs.controlState=physicsState.controlState;
   AnimationTransform boardRoot;const auto bodyRoot=originalRiderRootPresentation({physicsState.position,physicsState.quaternion},local.at(22).position,graph.scale,rs,&boardRoot);resetPlacedBoardRoot=boardRoot;resetStaleWorld=originalAnimationWorldPose(graph.rig->bones,local,bodyRoot,graph.scale,{bodyRoot,boardRoot});
   if(contactPose){poseContact.normal=physicsState.normal;poseContact.boardDirection=browserBoardNormalForPose;poseContact.boardLiftCm=physicsState.boardLift;poseContact.boardAlignment=physicsState.boardAlignment.current;legWeight=originalGrabLegWeight(legWeight,graph.currentClass(2));poseContact.legWeight=legWeight;originalRiderPoseContact(resetStaleWorld,local,graph.scale,poseContact);}}
}
void browser_mission_placed(); // web/mission_gameplay.inc: 3099F8 (WScript event kind 5)
static bool step_reset(){
 OriginalResetControlCallbacks cb;
 cb.refreshRoute=[](){originalResetRoute(resetPaths,resetRoute,physicsState.position,resetAllowEnd);};
 cb.routeDirection=[](){if(resetRoute.pathIndex<0)throw std::runtime_error("Reset has no retained path");return originalResetPathDirection(resetPaths.at(resetRoute.pathIndex).geometry,resetRoute.currentDistance);};
 cb.place=[](auto direction,float clearance,int semantic){reset_place_at(resetRoute.closestPoint,direction,clearance,semantic);};
 cb.postPlacement=[](){graph.setRate(2,0);reset_resume_velocity();passive={};prewind={};air={};airAnimation={};++resetObservers;browser_mission_placed();}; /*12F498: 11DF18(rider, 1) -> 3099F8 WScript event kind 5 (web/mission_gameplay.inc)*/
 cb.resetScore=[](bool penalize){grab={};++resetObservers;return score_reset(penalize);}; /*119368*/
 cb.awardBoost=[](float delta){award_boost(delta,1);physicsState.boost=boostState.amount;};
 cb.finishDeviceFade=[](){++resetObservers;};cb.clearScoringStance=[](){score_reset_stance();++resetObservers;}; /*119E38(score,0,0)*/cb.setAnimationRate=[](float rate){graph.setRate(2,rate);};
 cb.enterControl=[](int state){if(state!=4)throw std::runtime_error("Unexpected reset control handoff");physicsState.controlState=gs.controlState=4;};
 cb.enterMotion=[](int mode){if(mode!=1)throw std::runtime_error("Unexpected reset motion handoff");leave_reset_motion();previousGround=false;previousHeld=false;heldAirMode=false;passiveMode=true;originalPassiveAirBegin(passive,gs,prewind);++resetCompletions;};
 resetInputs.timeScale=physicsState.timeScale;originalResetControlStep(resetControl,resetInputs,cb);browser_controller_takeoff_wind(physicsState.velocity); /*12F398 runs before 1211F8 -> 120378: the placement tick's wind is the resumed velocity (score-uber 739)*/return browserResetActive;
}
static void follow_rider_route(float bestRemaining,int32_t tick){
 if(!physicsAttached)return;
 if(browserNisHold)return; // 121818: rider+0xAC4 set skips 112338 and the score tick 117C28 (the NIS hold)
 // 121818: the route passes (112338 / 1125C0) and then 117C28, the score tick, on every tick (rider+0xAC4 == 0): a streamed
 // location's path bank evicted before the next one is delivered leaves no route to follow here, but the score still ticks
 // (PS2 allpeak/apr-start 11879: the EBC3 bank goes at the EBC3_E crossing while the rider grinds; the grind distance kept counting).
 if(!resetPaths.empty()){
 OriginalNpcPathScoreContext context;context.position=physicsState.position;context.velocity=physicsState.velocity;context.computerControlled=false;
 originalNpcRouteProgress(resetPaths,resetRoute,context,bestRemaining,tick,[]()->uint32_t{throw std::runtime_error("Human route progress unexpectedly requested NPC randomness");});
 physicsState.headingOffset=resetRoute.heading;++routeProgressUpdates;}
 tick_rail_score();
}
// A streamed location's reset path bank (web/peak_world.inc, 12A340 -> 112180): the human's route is re-attached to the
// region row's reset path (+0xAB8): fresh cache (+0xABC +0x14 = -1), 26A638 -> closest point +0x490 and distance
// +0x4C0/+0x4C4, 26AB20 lookahead at distance + 796 cm -> +0x4A0.
bool browser_reset_replace_paths(std::vector<OriginalNpcPath> paths,int pathIndex){
 if(paths.empty()){if(!resetPaths.empty()){evictedResetPaths=std::move(resetPaths);evictedResetRoute=resetRoute;}resetPaths.clear();resetRoute.pathIndex=-1;return true;} // the bank's location was evicted (12A490)
 evictedResetPaths.clear();
 if(pathIndex<0||size_t(pathIndex)>=paths.size())throw std::runtime_error("Region row reset path outside the bank");
 resetPaths=std::move(paths);auto& r=resetRoute;r.pathIndex=pathIndex;r.cache.segment=-1;
 const auto& geometry=resetPaths[size_t(pathIndex)].geometry;const auto projection=originalRacePathProject(geometry,physicsState.position,r.cache,true);
 r.closestPoint=projection.point;r.previousDistance=r.currentDistance=projection.distance;r.lateralDistance=projection.lateralDistance;
 terrain_original::Rounding rounding;r.lookaheadPoint=originalRacePathSample(geometry,terrain_original::add(projection.distance,796.f));return true;
}
std::vector<OriginalNpcPath> browser_parse_reset_paths(const json& list){
 std::vector<OriginalNpcPath> out;
 for(const auto& p:list){OriginalNpcPath path;path.geometry.origin=p.at("origin").get<std::array<float,3>>();path.geometry.low=p.at("low").get<std::array<float,3>>();path.geometry.high=p.at("high").get<std::array<float,3>>();path.geometry.segments=p.at("segments").get<std::vector<std::array<float,4>>>();path.flags38=p.at("flags38");path.field3C=p.at("field3c");
  for(const auto& e:p.at("events"))path.geometry.events.push_back({e.at("type").get<uint32_t>(),e.at("value").get<uint32_t>(),e.at("start").get<float>(),e.at("end").get<float>()});out.push_back(std::move(path));}
 return out;
}
static OriginalNpcRouteState read_reset_route(const json& r){OriginalNpcRouteState route{};route.pathIndex=r.at("path_index");route.closestPoint=r.at("closest_point").get<std::array<float,3>>();route.lookaheadPoint=r.at("lookahead_point").get<std::array<float,3>>();route.previousDistance=r.at("previous_distance");route.currentDistance=r.at("current_distance");route.lateralDistance=r.at("lateral_distance");route.heading=r.at("heading");/*rolling starts (docs/backcountry.md): the provider steers on tick 0, before any 121818 route update*/if(r.contains("previous_lookahead_point"))route.previousLookaheadPoint=r.at("previous_lookahead_point").get<std::array<float,3>>();const auto& c=r.at("cache");route.cache={c.at("origin").get<std::array<float,3>>(),c.at("distance"),c.at("segment")};return route;}
static void setup_reset(const json& config){
 const auto& data=config.at("original_reset");resetPaths.clear();evictedResetPaths.clear();
 for(const auto& p:data.at("paths")){OriginalNpcPath path;path.geometry.origin=p.at("origin").get<std::array<float,3>>();path.geometry.low=p.at("low").get<std::array<float,3>>();path.geometry.high=p.at("high").get<std::array<float,3>>();path.geometry.segments=p.at("segments").get<std::vector<std::array<float,4>>>();path.flags38=p.at("flags38");path.field3C=p.at("field3c");for(const auto&e:p.at("events"))path.geometry.events.push_back({e.at("type"),e.at("value"),e.at("start"),e.at("end")});resetPaths.push_back(std::move(path));}
 auto readRoute=read_reset_route;
 eventRouteLoaded=data.contains("event_route");if(eventRouteLoaded)initialEventRoute=readRoute(data.at("event_route"));
 const auto&r=data.at("route");initialResetRoute={};initialResetRoute.pathIndex=r.at("path_index");initialResetRoute.closestPoint=r.at("closest_point").get<std::array<float,3>>();initialResetRoute.lookaheadPoint=r.at("lookahead_point").get<std::array<float,3>>();initialResetRoute.previousDistance=r.at("previous_distance");initialResetRoute.currentDistance=r.at("current_distance");initialResetRoute.lateralDistance=r.at("lateral_distance");initialResetRoute.heading=r.at("heading");const auto& c=r.at("cache");initialResetRoute.cache={c.at("origin").get<std::array<float,3>>(),c.at("distance"),c.at("segment")};
 if(resetPaths.empty()||resetPaths.size()>200||initialResetRoute.pathIndex<0||size_t(initialResetRoute.pathIndex)>=resetPaths.size())throw std::runtime_error("Invalid reset path bank");
 resetAllowEnd=data.at("allow_path_end");resetStance=data.at("stance").get<int>()!=0;resetInputs.eventModeActive=data.at("event_mode_active");resetInputs.eventVariant=data.at("event_variant");resetInputs.deviceIndex=data.at("device_index");resetInputs.deviceEnabled=data.at("device_enabled");
 browserRouteProgress=follow_rider_route;browserResetBegin=begin_reset;browserResetDecline=begin_decline_reset;browserResetRerequest=rerequest_reset;browserResetControl=step_reset;browserResetClear=clear_reset;clear_reset();
}

// The glide checkpoint's live sequence lists (probe_rider_pose.py layers), seeded
// exactly like makeOriginalRiderAnimation. Starting from a fresh semantic5 would
// drop HEADCHECK_TS, the cycle phase and the SH_RIGHT hair channel.
RIDER_LOCAL static std::vector<OriginalAnimationSequence> initialSequences;RIDER_LOCAL static std::array<int,6> initialRequested{438,438,438,438,438,438};
static void setup_initial_sequences(const json& source){
 initialSequences.clear();std::map<uint64_t,size_t> groups;
 for(const auto& l:source.at("layers")){const uint64_t id=l.at("sequence");auto found=groups.find(id);size_t index;
  if(found==groups.end()){index=initialSequences.size();groups[id]=index;OriginalAnimationSequence s;s.semantic=l.at("semantic");s.channel=l.at("channel");s.priority=l.at("priority");s.mask=l.at("mask");s.rate=l.value("sequence_speed",1.f);s.weight=l.value("sequence_weight",1.f);s.targetWeight=l.value("fade_target",1.f);s.fadeRemaining=l.value("fade_remaining",0.f);s.stopWhenFaded=l.at("stop_on_fade");s.completionEnabled=!s.stopWhenFaded;s.completed=l.at("completed");s.flags=l.at("sequence_flags");s.raisedFlags=l.at("raised_flags");s.seekPending=l.at("seek_pending");s.mirror=l.at("mirror");s.root.position=l.at("root_position").get<std::array<float,3>>();s.root.rotation=l.at("root_rotation").get<std::array<float,4>>();s.startFadeIn=l.value("fade_in",0.f);s.endFadeOut=l.value("fade_out",0.f);initialSequences.push_back(s);}
  else index=found->second;
  auto& s=initialSequences[index];const unsigned slot=l.at("slot");if(slot>=3)throw std::runtime_error("Invalid original sequence slot");if(s.slots.size()<=slot)s.slots.resize(slot+1);
  s.slots[slot]={l.at("clip"),l.at("time"),l.at("duration"),l.value("speed",1.f),l.value("slot_weight",l.value("base_weight",l.value("weight",1.f))),l.at("loop"),true};
 }
 const auto ids=source.at("current_semantics").get<std::vector<int>>();if(ids.size()!=6)throw std::runtime_error("Expected six requested animation channels");for(unsigned c=0;c<6;++c)initialRequested[c]=ids[c];
 for(const auto& s:initialSequences){if(s.slots.empty())throw std::runtime_error("Empty original sequence");for(const auto& slot:s.slots)if(!slot.enabled)throw std::runtime_error("Sparse original sequence slots");}
}
RIDER_LOCAL static std::optional<OriginalSecondaryMotionProfile> secondaryProfile;RIDER_LOCAL static std::array<int,3> secondaryBones{};
static void setup_secondary_motion(const json& source){
 secondaryProfile.reset();if(!source.contains("secondary_motion"))return;const auto& d=source.at("secondary_motion");OriginalSecondaryMotionProfile p;p.slot=d.at("slot");
 for(unsigned c=0;c<3;++c){p.enabled[c]=d.at("enabled")[c];p.bone[c]=d.at("bones")[c];p.forward[c]=d.at("forward")[c].get<std::array<float,4>>();p.reverse[c]=d.at("reverse")[c].get<std::array<float,4>>();
  // Compiled world slots: body 0..23 match; 24/25 are the inactive eye bones; 26.. are the imported hair bones.
  const int slot=p.bone[c];secondaryBones[c]=slot<24?slot:slot>=26?slot-2:-1;if(p.enabled[c]&&secondaryBones[c]<0)throw std::runtime_error("Secondary motion bone is not imported");}
 secondaryProfile=p;
}
//120378 runs before this tick's pose: it reads the pre-motion wind and the cached world pose.
static void step_secondary_motion(){
 if(!secondaryProfile||!controllerGround.valid)return;
 OriginalSecondaryMotionInput in;for(unsigned k=0;k<3;++k){in.velocity[k]=controllerTakeoffWind?(*controllerTakeoffWind)[k]:controllerGround.velocity[k];in.surfaceVelocity[k]=controllerGround.surfaceVelocity[k];}
 in.reverse=controllerGround.reverse;in.controlState=controllerGround.controlState;in.logicTick=controllerGround.logicTick;
 const auto& boneWorld=!cachedCrashWorld.empty()?cachedCrashWorld:resetStaleWorld; //reset placement tick: the placed pose (step_reset place)
 for(unsigned c=0;c<3;++c){in.requested[c]=graph.requestedSemantics[3+c];if(secondaryProfile->enabled[c]&&!boneWorld.empty())in.boneRotation[c]=boneWorld.at(secondaryBones[c]).rotation;}
 const auto out=originalSecondaryMotion(*secondaryProfile,in);
 for(unsigned c=0;c<3;++c)graph.setRate(3+c,out.rate);
 // Without a cached pose (first tick after a reset) there is no bone frame to select from; the still
 // semantic (0x19B+7c) ignores the bone, so the event's first tick (logic tick 0) still requests it.
 for(unsigned c=0;c<3;++c)if(out.request[c]>=0&&(!boneWorld.empty()||out.request[c]==0x19B+7*int(c))&&!graph.enter(out.request[c]))throw std::runtime_error("Missing original secondary-motion semantic");
 if(!cachedCrashWorld.empty())resetStaleWorld.clear();
}
static void seed_initial_sequences(){graph.sequences=initialSequences;graph.requestedSemantics=initialRequested;}
// Original131870/12E9B8 choose the main animation during controller dispatch, so the
// same tick's 13D818 integration (lift/alignment suppression by class) sees it.
RIDER_LOCAL static bool groundControllerRan=false;RIDER_LOCAL static int passiveDeparture=0;
// The CTM plane drop (web/plane-drop.js, pv dropPose) as the PS2 has it 10 ticks after its placement (menus/fr/ctmstart f02700): in the
// air controller (control 5; +0xDE4 = 5 through the fall) with its air clip 287 on channel 2 and 416 on channel 3 (the sequences JSON).
// The port's run starts from a grounded seed, so its first air tick would take a passive departure instead (control 4, clip 10 for
// up to 16 ticks) and the air clip would start late: the body sat 28 cm off the PS2's through the fall. As the crash handoff to air.
extern "C" EMSCRIPTEN_KEEPALIVE void drop_air_seed(const char* sequences){
 previousGround=false;previousHeld=false;heldAirMode=passiveMode=false;passiveDeparture=0;prewind={};air=originalAirControlBegin(0,0);landingAirExitBaked=false;gs.controlState=5;
 // 11D660's state reset (web/core.cpp apply_reset_placement): the ground-control triplets +0x1F0..+0x2A4 are 0 at f02700; the port's
 // seed carried a presentation lift of 8.5 cm and a lean of 0.79 into the fall (the body 18 cm off the board).
 for(auto* v:{&physicsState.turn,&physicsState.brake,&physicsState.crouch,&physicsState.presentationLift,&physicsState.animationTurn,&physicsState.extraLean,&physicsState.boardAlignment,&physicsState.presentationRoll,&physicsState.balance280,&physicsState.adjustment28C,&physicsState.adjustment298})*v={};
 if(physicsAttached){physicsState.controlState=5;physicsState.manualSpin=air.spinRate;}commit_rider_physics();
 if(sequences){const auto keepSequences=initialSequences;const auto keepRequested=initialRequested;setup_initial_sequences(json::parse(sequences));graph.sequences=initialSequences;graph.requestedSemantics=initialRequested;initialSequences=keepSequences;initialRequested=keepRequested;}}
static void finish_reset(); // web/finish_gameplay.inc
static bool npc_obstacle_query_enabled(bool air); // web/npc_gameplay.inc (13F4CC on-route skip)
RIDER_LOCAL extern int browserPrewindBranch; // web/core.cpp: 12E9B8 held branch (1 class 10, 2 requested 21, 3 reverse turn played 21)
int browser_channel2_class(){return graph.currentClass(2);}
int browser_channel2_semantic(){return graph.requestedSemantics[2];}
static void select_ground_animation(bool jumpHeld,float turn,float charge,float braking,float prewindSpin,float prewindFlip){
 if(jumpHeld&&gs.controlState==2&&browserPrewindBranch==3)return; //114CC0 reversed: 12E9B8 returns after playing 21
 const bool prewindUpdate=jumpHeld&&gs.controlState==2; //12E9B8 runs (targets below); on the entry tick only 12E980 (+204/+200, then 12EE30 selection) runs
 if(jumpHeld&&gs.controlState!=2){gs.animationTurn.target=0;gs.animationTurn.rate=std::bit_cast<float>(0x3d088889u);} //12E980 control2 entry: +204=0, +200 rate; 1211F8 approaches it this tick
 gs.controlState=jumpHeld?2:0;gs.animationIndex=graph.requestedSemantics[2];gs.animationClass=graph.currentClass(2);RiderInput input;input.turn=turn;input.crouch=charge;input.brake=braking;
 // 12E9B8 sets prewind targets and selects from the retained currents; 1211F8
 // approaches them afterwards, before the local pose samples driver7.
 if(prewindUpdate){OriginalAirPrewindContext context{0,gs.animationClass,gs.animationIndex,0,false};originalAirPrewindTargets(prewind,prewindSpin,prewindFlip,context);} //not on the 12E980 entry tick (tech-oob-hops 3018: class-10 landing clip, targets untouched)
 if(prewindUpdate&&(browserPrewindBranch==1||browserPrewindBranch==2))return; //class 10 / requested 21: no 12EE30 selection
 if(physicsAttached&&controllerGround.valid&&gs.controlState==0){auto selection=gs;auto profile=gp;selection.turn.current=controllerGround.turn;selection.brake.current=controllerGround.brake;selection.crouch.current=controllerGround.crouch;selection.boost=controllerGround.boost;selection.velocity=controllerGround.velocity;selection.lateral=controllerGround.lateral;profile.surface.id=controllerGround.surface;
  originalSelectGroundAnimation(profile,selection,input,prewind.spin.current,prewind.flip.current,[](){return rng.next();});gs.animationIndex=selection.animationIndex;gs.animationClass=selection.animationClass;gs.animationSelectionSupported=selection.animationSelectionSupported;gs.animationTurn=selection.animationTurn;
  if(groundControllerRan){physicsState.brake.target=selection.brake.target;physicsState.brake.rate=selection.brake.rate;physicsState.crouch.target=selection.crouch.target;physicsState.crouch.rate=selection.crouch.rate;}}
 else originalSelectGroundAnimation(gp,gs,input,prewind.spin.current,prewind.flip.current,[](){return rng.next();});
 if(gs.animationIndex!=graph.requestedSemantics[2])graph.enter(gs.animationIndex);
}
// Called by step_rider's grounded controller after its turn/crouch/brake targets and
// reverse-turn check, before the1211F8 filter pass and ground integration.
void browser_ground_controller_animation(int jumpHeld,float turn,float braking,float charge,float prewindSpin,float prewindFlip){
 ControllerDraws controllerDraws;
 if(!physicsAttached||!previousGround||browserResetActive||crash.active)return;
 if(previousHeld&&!jumpHeld)return; // control2 release: 12E9B8 requests control5 instead (release_air_control).
 groundControllerRan=true;
 controllerGround.boost=physicsState.boost; //131620 runs 114130 (boost press sets +0x2FC) at 0x131844 before the 131878 selection reads +0x2FC (metro-event-race 1094: tuck + boost press -> 8)
 gs.reverseStance=physicsState.reverseStance;gs.state320Equals324=physicsState.state320Equals324;gs.prewindStyle=physicsState.prewindStyle;gs.manualSpin=physicsState.manualSpin;
 gs.turn=physicsState.turn;gs.brake=physicsState.brake;gs.crouch=physicsState.crouch;gs.velocity=physicsState.velocity;gs.forward=physicsState.forward;gs.physicalForward=physicsState.physicalForward;gs.lateral=physicsState.lateral;gs.normal=physicsState.normal;gs.boost=controllerGround.boost;gp.surface=physicsProfile.surface;
 //115B58 (131868) plays a pending 10E028 reaction on channel1 with the rider+8C8 mask, then 115D48.
 if(!jumpHeld&&upperRequest358&&upperRequestHeldTick!=int32_t(controllerGround.logicTick)){const int mainClass=graph.currentClass(2);
  if(mainClass!=5&&graph.currentClass(1)==0&&mainClass!=10){const int32_t age=std::bit_cast<int32_t>(controllerGround.logicTick-uint32_t(upperRequestTick354));
   if(age>=0){if(age<180){const int kind=upperRequest358;if(kind==1||kind==2)audio_event(AE_TRICK_SPEECH,float(kind-1)); /*115C44/115C90: 29FF80(rider, 0 easy / 1 difficult)*/int semantic=kind>=1&&kind<=3?315:kind==4?314:(kind==5||kind==6)&&std::abs(physicsState.animationTurn.current)<.75f&&physicsState.controlState==0?318:-1;
     // 115CE0: kinds 5/6 (full meter, Tricky/Super Uber timer out) need 11FEE8 == 0 (control 0) and play 318 with the animator's
     // current mask (+0x20 = -1, all bones): only kinds 1..4 load the rider+0x8C8 mask (PS2 uber-super-expire 460).
     if(semantic>=0&&!graph.enter(semantic,-1,semantic==318?~uint64_t(0):riderMask8C8))throw std::runtime_error("Missing upper reaction request animation");}
    upperRequest358=0;upperRequestTick354=-1;}}}
 //115D48 runs at131870 before main-animation selection; both draw from the shared RNG.
 if(!jumpHeld){OriginalUpperReactionContext context{graph.currentClass(1),physicsState.physicalForward,physicsState.reverseStance,animationTick+1,riderMask8C0,riderMask8D0};auto reaction=originalUpperReaction(idleSeconds,peers,context,[](){return rng.next();});if(reaction.semantic>=0)graph.enter(reaction.semantic,-1,reaction.mask);}
 select_ground_animation(jumpHeld,turn,charge,braking,prewindSpin,prewindFlip);
 physicsState.animationIndex=graph.requestedSemantics[2];physicsState.animationClass=graph.currentClass(2);
}
static bool board_press_owned();static bool board_press_landing(); //web/boardpress_gameplay.inc
static void finish_landing(bool jumpHeld,float impact){
const bool softLanding=browserSoftActive; /*control 3 (12E778) touching down: 11FEC8(0) runs no exit for control 3 (0x456B90[3]), so +0x2DC (the soft entry's spin), the +0x2A4/+0x2B0 triplets and +0x360 stay; 131608 enters control 0*/const bool exitAirControl=!heldAirMode&&!passiveMode&&!softLanding;const bool heldLanding=heldAirMode&&jumpHeld;const bool passiveLanding=passiveMode||softLanding;heldAirMode=false;passiveMode=false;if(scoring.holdSeconds40>=0&&lastScoreId>=0)originalGrabScoreEnd(scoring,lastScoreId,boostState.tier);
 //10E910: 119D40 commit/reset/seed on the raw score object, Uber progression, 10E098 (web/score_gameplay.inc).
 auto award=score_landing();audio_event(AE_LANDING,award.meterDelta,audioImpact770,0);audio_event(AE_RUMBLE_IMPACT,browserLandingSpeed*0.5f); /*10EAD8: rider+0x88 -> owner +0xDFC = max(v0, 0.5 x the 10E910 speed) (pad rumble)*/ /*10EAF4: 2948D0(rider, 119D40 value)*/banked=std::bit_cast<int32_t>(uint32_t(banked)+uint32_t(award.points));if(award.points>0)committedTrickName=originalTrickName(award.identity,trickNameTables);
 grab={};if(exitAirControl&&!landingAirExitBaked)originalAirExitRates(air);landingAirExitBaked=false;float spin=passiveLanding&&physicsAttached?physicsState.manualSpin:air.spinRate; /*a passive flight (12F730) or a soft collision (12E778) leaves +2DC alone: 13A968 reads the retained manual spin*/
 const bool boardPressAir=board_press_owned();if(!heldLanding&&physicsAttached&&board_press_landing()){if(!boardPressAir){physicsState.manualSpin=spin;lastLandingSpin=spin;prewind={};air={};}return;} //0x13A5D4: +0x330 re-enters control 1
 // 139C88 0x13A5A8 / 0x13A644: every landing but the board-press one (+0x330) runs 115640 before 11FEC8(0) and 114CC0, so a
 // stance kept through the air is restored at the touchdown (PS2 ESS3 Moby 815: a soft collision off a rail keeps style 4, +0x328,
 // to the landing, which turns the physical frame 90 degrees and clears it). A no-op for style 0.
 if(physicsAttached&&physicsState.prewindStyle!=0)restore_stance(physicsState.prewindStyle);
 OriginalReverseTurnResult reverse;
 if(physicsAttached&&!heldLanding){reverse=originalReverseTurn(physicsState,physicsState.balance280);if(reverse.reversed){++reverseTurnSerial;reverse_roots(reverse.animationRootQuaternion);publish_motion();}gs.reverseStance=physicsState.reverseStance;gs.state320Equals324=physicsState.state320Equals324;}
 int landing=reverse.reversed?originalReverseLandingAnimation(impact,spin):originalLandingAnimation(impact,spin);if(physicsAttached)physicsState.manualSpin=spin;lastLandingSpin=spin;if(!heldLanding||impact< -1388.888916015625f)graph.enter(landing,-1,~uint64_t(0),false); /*13A968/13A8F8 play with a2 = 0: 311F00 revives a still-fading copy of the same landing clip*/if(exitAirControl)prewind={};air={}; /*134CB0 (control-5 exit) zeroes the +0x2A4/+0x2B0 triplets; a passive (control 4, entry 12F620 rate 1/30) or held landing keeps them (handplant-spring 664)*/
 if(physicsAttached)physicsState.controlState=heldLanding?2:0;gs.controlState=heldLanding?2:0;
 if(softLanding){browserSoftActive=false;prewind.jumpGate=0;} //139C88 0x13A650 11FEC8(0) replaces the soft controller (next tick runs 131620); 131608 clears +0x360
}
#include "rail_gameplay.inc"
#include "pickup_gameplay.inc"
#include "instance_contact_gameplay.inc"
#include "boost_gameplay.inc"
#include "start_gameplay.inc"
#include "input_bridge.inc"
#include "handplant_gameplay.inc"
#include "attack_gameplay.inc"
#include "boardpress_gameplay.inc"
#include "npc_gameplay.inc"
#include "finish_gameplay.inc"
#include "score_gameplay.inc"
#include "weather.inc" // camera Weather painter, snowfall layers, camera splash, lightning (docs/weather.md)
#include "stage_script_gameplay.inc"
#include "presentation_core.inc" // cutscene stage calls, the painter region for located Fog / Lighting (docs/presentation.md)
#include "impact_fx_gameplay.inc"
#include "shared_world.inc"
#include "audio_events.inc"
#include "fx_puppet.inc"
// pv animationPrepare (web/main.js prepareRiderAnimation, web/ai-racers.js configureRider): a document of init_animation / init_race
// parsed ahead in a call of its own (a load-screen frame each: the three parses were most of init_animation's time), kept here by its
// text until the call that needs it takes it. A taken document is that parse itself, so the loaded state is the same; a text that
// was not prepared is parsed in the call as before. Shared by the rider contexts (a prepare is taken by the next call).
namespace {
struct PreparedDocument {std::string key;json doc;};
std::vector<PreparedDocument> preparedDocuments;
std::string prepared_key(const char* text){uint32_t h=0x811c9dc5u;size_t n=0;for(const char* c=text;*c;++c,++n){h^=uint8_t(*c);h*=0x01000193u;}return std::to_string(n)+":"+std::to_string(h);}
}
json animation_document(const char* text){
 if(!preparedDocuments.empty()){const auto key=prepared_key(text);
  for(auto it=preparedDocuments.begin();it!=preparedDocuments.end();++it)if(it->key==key){json doc=std::move(it->doc);preparedDocuments.erase(it);return doc;}}
 return json::parse(text);
}
extern "C" {
EMSCRIPTEN_KEEPALIVE void animation_prepare(const char* text){preparedDocuments.push_back({prepared_key(text),json::parse(text)});if(preparedDocuments.size()>12)preparedDocuments.erase(preparedDocuments.begin());}
EMSCRIPTEN_KEEPALIVE int animation_prepared(){return int(preparedDocuments.size());}
RIDER_LOCAL extern float browserHumanBodyScale; // web/attribute_bridge.cpp
RIDER_LOCAL static std::optional<OriginalGroundState> humanEventSeed;
// A character's countdown state (settings original_event_start, tools/export_characters.py) was recorded on Snow Jam.
// On another course it is carried over relative to that course's compiled Zoe seed: the spot's offset from Zoe's in
// Snow Jam's gate frame (forward, lateral, normal; it moves along the gate's forward with the body scale, lineups.json
// grid) re-applied in this gate's frame, the ground depths as deltas, and the character's stance / lift / filter
// scalars where they differ from Zoe's; the surface vectors and orientation stay this course's. Snow Jam: unchanged.
static OriginalGroundState human_event_seed_for_course(const OriginalGroundState& ch){
 if(std::string_view(browserEventLocation)=="ARA1")return ch;
 const auto zoe=browser_start_ARA1::browserEventGroundState();auto out=browserEventGroundState();
 terrain_original::Rounding rounding;using terrain_original::add;using terrain_original::sub;using terrain_original::mul;
 const auto dot3=[&](const auto& a,const auto& b){return add(add(mul(a[0],b[0]),mul(a[1],b[1])),mul(a[2],b[2]));};
 std::array<float,3> delta{};for(int k=0;k<3;k++)delta[k]=sub(ch.position[k],zoe.position[k]);
 const float f=dot3(delta,zoe.forward),l=dot3(delta,zoe.lateral),n=dot3(delta,zoe.normal);
 for(int k=0;k<3;k++)out.position[k]=add(out.position[k],add(add(mul(out.forward[k],f),mul(out.lateral[k],l)),mul(out.normal[k],n)));
 out.depth1=add(out.depth1,sub(ch.depth1,zoe.depth1));out.depth3=add(out.depth3,sub(ch.depth3,zoe.depth3));out.distance=add(out.distance,sub(ch.distance,zoe.distance));
 const auto take=[](auto& dst,const auto& c,const auto& z){static_assert(std::is_trivially_copyable_v<std::decay_t<decltype(c)>>);if(std::memcmp(&c,&z,sizeof c)!=0)dst=c;}; // plain data: bitwise
 take(out.reverseStance,ch.reverseStance,zoe.reverseStance);take(out.boardLift,ch.boardLift,zoe.boardLift);take(out.boardBouncePhase,ch.boardBouncePhase,zoe.boardBouncePhase);
 take(out.manualSpin,ch.manualSpin,zoe.manualSpin);take(out.presentationLift,ch.presentationLift,zoe.presentationLift);take(out.presentationRoll,ch.presentationRoll,zoe.presentationRoll);
 take(out.extraLean,ch.extraLean,zoe.extraLean);take(out.boardAlignment,ch.boardAlignment,zoe.boardAlignment);take(out.animationTurn,ch.animationTurn,zoe.animationTurn);
 take(out.turn,ch.turn,zoe.turn);take(out.brake,ch.brake,zoe.brake);take(out.crouch,ch.crouch,zoe.crouch);take(out.balance280,ch.balance280,zoe.balance280);
 take(out.adjustment28C,ch.adjustment28C,zoe.adjustment28C);take(out.adjustment298,ch.adjustment298,zoe.adjustment298);take(out.riderType,ch.riderType,zoe.riderType);
 take(out.animationIndex,ch.animationIndex,zoe.animationIndex);take(out.animationClass,ch.animationClass,zoe.animationClass);take(out.prewindStyle,ch.prewindStyle,zoe.prewindStyle);
 take(out.flags308,ch.flags308,zoe.flags308);take(out.boostSpeedFloor,ch.boostSpeedFloor,zoe.boostSpeedFloor);take(out.state320Equals324,ch.state320Equals324,zoe.state320Equals324);
 return out;
}
// begin_event_rider (web/core.cpp) applied Zoe's compiled grid seed; a selected human replaces the rider-specific state.
static void human_event_seed_apply(){if(!humanEventSeed)return;physicsState=human_event_seed_for_course(*humanEventSeed);const auto actor=OriginalAirState{physicsState.position,physicsState.velocity};position=actor.nativePosition();velocity={};normal={physicsState.normal[0],physicsState.normal[2],-physicsState.normal[1]};physicsState.controlState=6;publish_motion();}
static void human_event_seed_restore(){browserEventRiderSeed=humanEventSeed?human_event_seed_apply:nullptr;}
EMSCRIPTEN_KEEPALIVE void init_animation(const char* metadata,const char* skeleton,const char* settings,const uint8_t* packets,int packetBytes){
 setup_crash();setup_rails();setup_start();browserLandingAirExit=landing_air_exit;browserRestoreStance=restore_stance;browserReverseAnimation=reverse_animation;
 auto m=animation_document(metadata),r=animation_document(skeleton),config=animation_document(settings);setup_skin_bind(r);graph={};graph.scale=config["original_animation"]["scale"].get<std::array<float,3>>();for(float value:graph.scale)if(!std::isfinite(value)||value<=0)throw std::runtime_error("Invalid authored rider scale");graph.rig=std::make_shared<BrowserRig>();graph.variantFlags=config["original_animation"].value("variant_flags",0u);graph.defaultMirror=config["original_animation"].value("default_mirror",false);graph.defaultRoot.position=config["original_animation"]["default_root_position"].get<std::array<float,3>>();graph.defaultRoot.rotation=config["original_animation"]["default_root_rotation"].get<std::array<float,4>>();initialDefaultRoot=graph.defaultRoot;initialDefaultMirror=graph.defaultMirror;{/* channel-1 masks rider+0x8C0/+0x8C8/+0x8D0 (11C298/310CE8, per character): a selected human's own (web/character-roster.js, tools/export_characters.py), else Zoe's; npc_configure sets the computer riders' */const auto id=config.value("original_rider_identity",json::object());auto mask=[&](const char* key,uint64_t fallback){return id.contains(key)?std::stoull(id.at(key).get<std::string>(),nullptr,16):fallback;};riderMask8C0=mask("upper_mask8c0",0x8000fffeu);riderMask8C8=mask("upper_mask8c8",0x8000fff8u);riderMask8D0=mask("upper_mask8d0",0x870u);}{/* the selected human's grid spot and body scale (its own countdown actor: the spot follows the scale, reverse stance the base rider); none = Zoe's compiled seed */const auto start=config.value("original_event_start",json());humanEventSeed.reset();browserHumanBodyScale=0;if(start.is_object()){humanEventSeed=browserJsonGroundState(start.at("state"));browserHumanBodyScale=start.at("body_scale").get<float>();}if(humanEventSeed)browserEventRiderSeed=human_event_seed_apply;else if(browserEventRiderSeed==human_event_seed_apply)browserEventRiderSeed=nullptr;}setup_reset(config);setup_trick_names(config);setup_rail_context(config);setup_pickups(config);crash.host.recoveryInputs=[](){OriginalCrashRecoveryInputs in;in.deviceIndex870=resetInputs.deviceIndex;in.deviceEnabled87C=resetInputs.deviceEnabled;return in;};setup_trail(config);setup_snow(config);setup_impact_fx(config);reset_boost_fx();
 for(auto&b:r["bones"]){AnimationBone bone;bone.parent=b["parent"];bone.part=b["file"];bone.translationChannel=b["animation_translation_channel"];bone.rotationChannel=b["animation_rotation_channel"];auto t=b["translation"].get<std::array<double,3>>();auto q=b["rotation"].get<std::array<float,4>>();bone.bind={{float(t[0]*100),float(-t[2]*100),float(t[1]*100)},{q[0],-q[2],q[1],q[3]}};bone.mirrorQuaternion=b["mirror_quaternion_map"].get<std::array<uint8_t,4>>();for(int k=0;k<3;k++)bone.mirrorTranslation[k]=b["mirror_translation_scale"][k];graph.rig->bones.push_back(bone);}
 for(unsigned i=0;i<r["bones"].size();i++)for(unsigned j=0;j<r["bones"].size();j++)if(r["bones"][i]["file"]==r["bones"][j]["file"]&&r["bones"][i]["mirror_index"]==r["bones"][j]["index"])graph.rig->bones[i].mirrorSource=j;
 for(auto&c:m["clips"]){AnimationClip clip;clip.id=c["id"];clip.duration=c["duration"];clip.eventTimes=c["event_times"].get<std::vector<float>>();for(auto&p:c["packets"]){size_t offset=p["offset"],size=p["size"];if(offset+size>size_t(packetBytes))throw std::runtime_error("Animation packet bounds");clip.segments.push_back({p["part"],AnimationPacket({packets+offset,size},p["frames"])});}graph.rig->clips.push_back(std::move(clip));}
 for(auto collection:{"state_properties","state_definitions"})for(auto&d:m[collection]){AnimationStateDefinition s;s.semantic=d["semantic"];s.animationClass=d["animation_class"];s.kind=d["kind"];s.channel=d["channel"];s.completionKind=d["completion_kind"];s.blendSeconds=d["blend_seconds"];s.firstFadeIn=d["first_fade_in"];s.endFadeOut=d["end_fade_out"];s.initialClip=d.value("initial_clip",0xffffffffu);s.followupClip=d.value("followup_clip",0u);graph.rig->stateDefinitions[s.semantic]=s;}
 for(auto&[key,d]:m["cycle_maps"].items())graph.rig->cycleMaps[std::stoi(key)]=d.get<std::array<uint32_t,5>>();
 for(auto&[key,d]:m["three_way_maps"].items())graph.rig->threeWayMaps[std::stoi(key)]=d.get<std::array<uint32_t,3>>();
 for(auto&[key,records]:m["animation_variants"].items())for(auto&d:records){OriginalAnimationVariant v{d["leaf"],d["weight"],d["allowed_flags"]};graph.rig->animationVariants[std::stoi(key)].push_back(v);if(v.leaf!=519)graph.rig->variantClips[v.leaf]=d["clip"];}
 rng.words=config["original_animation"]["random_state"].get<std::array<uint32_t,6>>();graph.variantRandom=[](){return rng.next();};
 pivotBone=config["original_animation"]["pivot_bone"];if(pivotBone>=graph.rig->bones.size())throw std::runtime_error("Air pivot outside skeleton");
 auto p=config["original_grab_control"]["profile"];grabProfile.playbackRate=originalGrabPlaybackRate(p["grab_stat"]);grabProfile.extendedDefinitions=p["extended_definitions"];
 auto def=[](json d){return OriginalGrabDefinition{d["semantic"],d["upper_semantic"],d["score_id"],d["begin_points"],d["hold_points"]};};
 for(int i=0;i<15;i++){grabProfile.grabs[i]=def(p["grabs"][i]);grabProfile.tweak[i]=def(p["tweak"][i]);for(int t=0;t<2;t++)grabProfile.uber[t][i]=def(p["uber"][t][i]);}
 auto tc=config["original_trick_identity"];auto tp=tc["profile"];auto cp=tc["commit_profile"];
 commitProfile.identity.spinDegrees=tp["spin_degrees"];commitProfile.identity.flipDegrees=tp["flip_degrees"];
 commitProfile.identity.ordinary=tp["ordinary"].get<std::array<uint8_t,319>>();commitProfile.identity.alternate=tp["alternate"].get<std::array<uint8_t,319>>();
 for(unsigned i=0;i<24;i++){auto row=tc["named_tricks"][i];commitProfile.named[i]={row["id"],row["points"],row["identity_fields"].get<std::array<uint8_t,7>>()};}
 commitProfile.namedPointScale=cp["named_point_scale"];commitProfile.scoreScale=cp["score_scale"];commitProfile.spinScale=cp["spin_scale"];commitProfile.flipScale=cp["flip_scale"];
 auto ac=config["original_boost"]["award_context"];awardContext={ac["reward_mask"],ac["enable_tricky"]};trickHistory={};trickIdentity={};
 scoring={};reset_rail_score_tracking();clear_trick_scoring();score_init();lastScoreId=-1;banked=0;idleSeconds=0;idleControl=0;upperRequest358=0;upperRequestTick354=-1;animationTick=0;peers={};passiveMode=false;heldAirMode=false;passive={};prewind={};air={};airAnimation={};airProfile.trickStat=browserAttributesSet?browserAttributeStat(4):0.0909090936f;
 for(auto&d:grabProfile.grabs)scoreRules[d.scoreId]={d.scoreId,d.beginPoints,d.holdPoints};for(auto&d:grabProfile.tweak)scoreRules[d.scoreId]={d.scoreId,d.beginPoints,d.holdPoints};for(auto&t:grabProfile.uber)for(auto&d:t)scoreRules[d.scoreId]={d.scoreId,d.beginPoints,d.holdPoints};
 for(int i=0;i<15;i++)scoringProfile.normal[i]=scoreRules.at(grabProfile.grabs[i].scoreId);
 for(int i=0;i<4;i++){auto t=config["original_grab_score"]["profile"]["hold_thresholds"][i];scoringProfile.holdThresholds[i]={t["seconds"],t["points"]};}
 auto contact=config["original_animation"]["contact"];poseContact={};poseContact.normal=poseContact.boardDirection={0,0,1};poseContact.boardAlignment=contact["board_alignment"];poseContact.boardLiftCm=contact["board_lift_cm"];poseContact.legWeight=legWeight=contact["leg_weight"];for(int i=0;i<2;i++){auto l=contact["legs"][i];poseContact.legs[i]={l["bones"][0],l["bones"][1],l["bones"][2],{l["position"].get<std::array<float,3>>(),l["rotation"].get<std::array<float,4>>()}};}
 initialPoseContact=poseContact;
 grabContext={config["original_boost"]["state"]["super_time"],config["original_boost"]["state"]["tier"],awardContext.enableTricky};
gs={};gs.timeScale=1;gs.animationIndex=5;gs.animationClass=7;gp.surface.id=0;grab={};previousGround=true;previousHeld=false;setup_initial_sequences(config["original_animation"]);setup_secondary_motion(config["original_animation"]);seed_initial_sequences();
}
EMSCRIPTEN_KEEPALIVE void animation_use_physics(int enabled){physicsAttached=enabled;browserPosedLandingEnabled=enabled;}
EMSCRIPTEN_KEEPALIVE void restore_rider_stance(int style){restore_stance(style);}
EMSCRIPTEN_KEEPALIVE float* rider_stance_info(){RIDER_LOCAL static float values[9];values[0]=physicsState.reverseStance;values[1]=graph.defaultMirror;values[2]=physicsState.state320Equals324;values[3]=physicsState.prewindStyle;for(unsigned i=0;i<4;i++)values[4+i]=graph.defaultRoot.rotation[i];values[8]=reverseTurnSerial;return values;}
EMSCRIPTEN_KEEPALIVE float* stance_restore_info(){RIDER_LOCAL static float values[6];values[0]=lastStanceRestore&&lastStanceRestore->restored;values[1]=lastStanceRestore&&lastStanceRestore->physicalRotated;values[2]=lastStanceRestore&&lastStanceRestore->sequenceRootsRotated;values[3]=lastStanceRestore&&lastStanceRestore->defaultRootReset;values[4]=lastStanceRestore?lastStanceRestore->animationSemantic:-1;values[5]=stanceRestoreOrder;return values;}
EMSCRIPTEN_KEEPALIVE void soft_collision_begin(int semantic,float spin){
 if(physicsState.controlState==0&&idleControl!=0)idleSeconds=0; //the tick's touchdown / 12E778 exit entered control 0 first: 131608 zeroes +0x35C before 108388 (tech-oob-hops 2846)
 physicsState.animationTurn=gs.animationTurn;
 if(!begin_soft_control(semantic,spin))throw std::runtime_error("Unsupported original soft collision entry");
 passiveDeparture=0;passiveMode=heldAirMode=false; //11FEC8(3) replaces the controller: a departure this tick (13F178 -> 105398 -> 108388) no longer runs 131620 in the air next tick (control-4 request) or 12E9B8 (control 2); 12E778 runs instead
 audio_event(AE_SOFT,(semantic==0x3A||semantic==0x3C)?1.f:0.f); //108680: 2A0E70(rider, speech flag): grunt 2 + Post_Object_Collision after clips 0x3A/0x3C
 gs.controlState=3; // 108388 -> 11FEC8(rider,3): the next input tick decodes as control 3 (carve 408 word check).
 if(browserRailActive){auto view=rail_view();originalRailControlLeave(view);railBalance=view.balance;railHeldJump=railPreviousJump=false;}
 if(!graph.enter(semantic))throw std::runtime_error("Missing soft collision animation"); /*108388 plays through 3128E8(anim,semantic,0,-1): an ordinary play, so a fading-out copy of the same clip is inherited (311F00), not restarted (metro-glide-carve 1208)*/grab={};heldAirMode=passiveMode=false;
}
EMSCRIPTEN_KEEPALIVE void reset_animation(){finish_reset();retainedGroundLateral.reset();clear_skin_matrices();committedPoseTranslation={};clear_start();lastVisualTick=0xffffffffu;reset_boost_fx();committedTrickName.clear();clear_rails();reset_snow(true);reset_impact_fx(true);reset_trail();clear_reset();reset_crash();cachedCrashWorld.clear();completedMain=false;completedMainSemantic=-1;landingAirExitBaked=false;airExitInfo={};poseContact=initialPoseContact;poseControls={};legWeight=initialPoseContact.legWeight;graph.sequences.clear();graph.sampledLocal.reset();graph.nextRate=1;graph.defaultRoot=initialDefaultRoot;graph.defaultMirror=initialDefaultMirror;graph.requestedSemantics.fill(438);posedPhysical[7]=0;browserBodyVolume.reset();browserBodyQuery.reset();browserBodyResponse.reset();reset_body_queries();lastLandingSpin=0;trickHistory={};trickIdentity={};scoring={};reset_rail_score_tracking();clear_trick_scoring();score_init();lastScoreId=-1;banked=0;idleSeconds=0;idleControl=0;upperRequest358=0;upperRequestTick354=-1;animationTick=0;peers={};passiveMode=false;heldAirMode=false;passive={};prewind={};air={};airAnimation={};airProfile.trickStat=browserAttributesSet?browserAttributeStat(4):0.0909090936f;gs={};gs.timeScale=1;gs.animationIndex=5;gs.animationClass=7;grab={};previousGround=true;previousHeld=false;seed_initial_sequences();}
// QA (compare-ps2-capture.mjs): a mid-run baseline seeds the 115D48 idle clock rider+0x35C from its first record.
EMSCRIPTEN_KEEPALIVE void idle_clock_seed(float seconds){idleSeconds=seconds;}
// 0x135BE0 in-flight stance switch (engine/air_switch.hpp), called from inside the control-5 update.
// The original runs that update before air motion; the browser translated already, so a switch
// restarts the flight from the pre-motion state and translates again (core.cpp).
RIDER_LOCAL static std::array<float,3> presentationForward170{0,1,0}; //+0x170 with +0x180: the last pose's board-root frame
static void set_presented_frame(const OriginalCollisionFrame& f){physicsState.presentationUp=f.up;presentationForward170=f.forward;} // a placement's cached frame (web/mission_gameplay.inc)
extern "C++" {bool browser_air_motion_start(std::array<float,3>&,std::array<float,3>&);void browser_air_switch_redo(const std::array<float,3>&);const OriginalAirTrajectory* air_motion_start_trajectory();}
RIDER_LOCAL static unsigned airSwitchSerial=0;
static bool browser_air_switch(OriginalAirControlState& control){
 std::array<float,3> start,startVelocity;if(!physicsAttached||!graph.sampledLocal||!browser_air_motion_start(start,startVelocity))return false;
 OriginalAirSwitchPrediction p;
 if(const auto* t=air_motion_start_trajectory()){p.status=t->status;p.surface=t->surface;p.surfaceProperty44=browser_surface_property(t->surface);p.heading={t->heading[0],t->heading[1],t->heading[2],0};p.normal={t->normal[0],t->normal[1],t->normal[2],0};}
 OriginalAirSwitchRider r;r.position={start[0],start[1],start[2],0};r.quaternion=physicsState.quaternion;r.velocity={startVelocity[0],startVelocity[1],startVelocity[2],0};
 const auto& up=physicsState.presentationUp;r.frameUp={up[0],up[1],up[2],0};r.frameForward={presentationForward170[0],presentationForward170[1],presentationForward170[2],0};
 const auto& pivot=graph.sampledLocal->at(pivotBone).position;r.pivot={pivot[0],pivot[1],pivot[2],0};r.scale={graph.scale[0],graph.scale[1],graph.scale[2],1};
 if(!originalAirSwitchRequired(r,p))return false;
 r.surfaceForward={physicsState.forward[0],physicsState.forward[1],physicsState.forward[2],0};r.lateral={physicsState.lateral[0],physicsState.lateral[1],physicsState.lateral[2],0};
 r.turn=physicsState.turn;r.animationTurn=physicsState.animationTurn;r.extraLean=physicsState.extraLean;r.brake=physicsState.brake;r.balance=physicsState.balance280;r.reverseStance=physicsState.reverseStance;
 std::array<float,4> root{0,0,0,1};std::array<float,3> moved{};OriginalAirSwitchAccess a;
 a.rotateAnimation=[](float){};a.setAnimationMirror=[](int){}; //reverse_roots: 0x311B48(pi), mirror = +0x320
 a.setAnimationRoot=[&](AirSwitchQuad,AirSwitchQuad q){root=q;};a.beginFlight=[&](AirSwitchQuad position,AirSwitchQuad,float){moved={position[0],position[1],position[2]};};
 a.play=[](int semantic){graph.nextRate=airAnimation.nextRate;if(!graph.enter(semantic))throw std::runtime_error("Missing air switch semantic");}; //135BE0's 3128E8 plays with animator +0x1C and leaves it pending (tech-oob-dance 3064)
 if(!originalAirSwitch(r,p,a))throw std::runtime_error("Air switch test disagreed");
 physicsState.quaternion=r.quaternion;physicsState.physicalForward={r.forward[0],r.forward[1],r.forward[2]};physicsState.boardUp={r.up[0],r.up[1],r.up[2]};
 physicsState.reverseStance=r.reverseStance;physicsState.state320Equals324=!physicsState.state320Equals324;
 physicsState.forward={r.surfaceForward[0],r.surfaceForward[1],r.surfaceForward[2]};physicsState.lateral={r.lateral[0],r.lateral[1],r.lateral[2]};
 physicsState.turn=r.turn;physicsState.animationTurn=r.animationTurn;physicsState.extraLean=r.extraLean;physicsState.brake=r.brake;physicsState.balance280=r.balance;
 for(auto* v:{&gs.turn,&gs.brake,&gs.extraLean,&gs.balance280}){v->current=-v->current;v->target=-v->target;} //the animation copies of the same +0x1F0/+0x214/+0x208/+0x280
 gs.reverseStance=physicsState.reverseStance;gs.state320Equals324=physicsState.state320Equals324;gs.forward=physicsState.forward;gs.lateral=physicsState.lateral;gs.physicalForward=physicsState.physicalForward;gs.boardUp=physicsState.boardUp;
 ++reverseTurnSerial;++airSwitchSerial;reverse_roots(root);controllerGround.reverse=physicsState.reverseStance; //120378 runs after the controller and reads the switched +0x320 (rnb-event-tuck 1561)
 browser_air_switch_redo(moved);
 originalAirSwitchControl(control,a);return true;
}
// Six-rider phases (web/ai-racers.js): 0x128AF0 runs every rider's pose passes (121700/121728, incl.
// 11EB98 -> 106828 AA0 spheres) before any rider's second motion phase 121750. animation_pose is this
// rider's tick up to and including the pose and its collision body; animation_post is its 121750
// (touchdown, 13F488/13AA48, 105398, rider pairs 107888, the 13F358 clamp, rail/crash posts).
// animation_tick runs both, with the race host's hook (the other riders' passes) in between.
RIDER_LOCAL static std::vector<AnimationTransform> tmpPreContact; //TMPDEBUG
RIDER_LOCAL static float tmpDebug[24]; RIDER_LOCAL static float tmpDebug2[8]; //TMPDEBUG
EMSCRIPTEN_KEEPALIVE void animation_pose(float speed,float turn,float braking,float charge,int grounded,int jumpHeld,int mask,int tweak,int boost,float predictedLanding,float impact,float flip){
 if(browserNisHold)return; // 121700 / 121728: rider+0xAC4 set skips 11EB60 / 11EB98; motion 3's second phase 136978 is empty
 pairCompanion={}; // 121020 (this rider's motion tick): the companion translation starts again
 browserLandingTranslation={};committedPoseTranslation={};
 scoreEvent=0;banked=deferredScore;deferredScore=0;++animationTick;board_press_filters();const bool airAdjustWasLive=airAdjustLive;airAdjustLive=false;gs.velocity={speed*100,0,0};if(!physicsAttached){gs.turn.current=turn;gs.animationTurn.current=turn;gs.brake.current=braking;}if(grounded)gs.crouch.current=charge;gs.boost=boost?1:0;if(physicsAttached){grabContext.superTime=boostState.superTime;grabContext.boostTier=boostState.tier;gs.reverseStance=physicsState.reverseStance;gs.state320Equals324=physicsState.state320Equals324;gs.prewindStyle=physicsState.prewindStyle;gs.boost=physicsState.boost;gs.manualSpin=physicsState.manualSpin;gs.velocity=physicsState.velocity;if(grounded||previousGround){gs.turn=physicsState.turn;gs.brake=physicsState.brake;gs.crouch=physicsState.crouch;}gs.forward=physicsState.forward;gs.physicalForward=physicsState.physicalForward;gs.lateral=physicsState.lateral;gs.normal=physicsState.normal;gp.surface=physicsProfile.surface;}
 const int softExit=physicsAttached?take_soft_transition():-1;
 if(softExit==0)prewind.jumpGate=0;
 if(softExit==4){passiveMode=true;heldAirMode=false;gs.controlState=4;originalPassiveAirBegin(passive,gs,prewind);}
 bool passiveEntryApproached=false; //control 4 requested control 5 this tick and already approached the triplets
 const bool startFrame=browserStartFrame;const bool resetFrame=browserResetActive;const bool crashFrame=crash.active;const bool railFrame=railOwned||handplant_owned();
 const bool softFrame=physicsAttached&&(browserSoftActive||browserSoftFrame);
 // Ground motion 13D818 ran this tick and then left the ground (passive or jump-held
 // departure; a charged release takes off in the controller instead).
 const bool groundMotionDeparture=physicsAttached&&!grounded&&previousGround&&!(previousHeld&&!jumpHeld)&&!startFrame&&!resetFrame&&!crashFrame&&!railFrame&&!softFrame;
 groundDeparturePending=groundMotionDeparture;
 tmpDebug[0]=groundMotionDeparture;for(unsigned k=0;k<3;k++)tmpDebug[1+k]=browserDeparturePush[k];tmpDebug[4]=browserDeparturePrePosition.has_value(); //TMPDEBUG
 if(startFrame||resetFrame||crashFrame||railFrame||railReleaseFrame||browserCrashExitFrame){gs.controlState=physicsState.controlState;gs.turn=physicsState.turn;gs.brake=physicsState.brake;gs.crouch=physicsState.crouch;gs.animationTurn=physicsState.animationTurn;if(railFrame){gs.adjustment28C=physicsState.adjustment28C;gs.adjustment298=physicsState.adjustment298;}} //rail 0x1211F8 keeps approaching +0x28C/+0x298 for a fading kind-11 air adjust
 else if(softFrame){gs.animationTurn=physicsState.animationTurn;gs.controlState=physicsState.controlState;gs.turn=physicsState.turn;gs.brake=physicsState.brake;gs.crouch=physicsState.crouch;gs.manualSpin=physicsState.manualSpin;}
 else if(grounded){if(!previousGround){finish_landing(jumpHeld,impact);}else if(groundControllerRan){gs.controlState=browserBoardPressFrame?physicsState.controlState:jumpHeld?2:0;}else select_ground_animation(jumpHeld,turn,charge,braking,rideLatched?rideSpin:turn,rideLatched?rideFlip:flip);}
 else {ControllerDraws controllerDraws;
  // Control2's release requests control5; its first 12E?? air-control step (grab
  // lifecycle, adjust spin, air animation selection) runs on the next tick.
  bool airReleaseTick=false;
  if(bpAirTransition||board_press_air_frame()){passiveDeparture=0;} //0x1307B8/0x12FFF8 requested control 5 (0x133308 runs next tick) or control 1 is airborne
  else if(previousGround){grab={};if(previousHeld&&!jumpHeld){release_air_control();airReleaseTick=true;}else if(jumpHeld&&(previousHeld||groundControllerRan)){heldAirMode=true;passiveMode=false;gs.controlState=2;air={};} /*a held jump in control 0 requests control 2 (1162C8) even on the departure tick itself (backcountry rolling start with Cross held: PS2 bc-race-idle control 2 from tick 1)*/else {passiveMode=true;passiveDeparture=1;}}
  // Passive departure: the ground controller already ran before 13F194 left the ground.
  // Next tick control0 runs in the air and only requests control4 (12F730 entry); the
  // first passive step is the tick after that.
  else if(passiveDeparture==1){passiveDeparture=2;gs.controlState=4;originalPassiveAirBegin(passive,gs,prewind);air={};}
  else passiveDeparture=0;
  if(heldAirMode&&!jumpHeld){heldAirMode=false;release_air_control();airReleaseTick=true;}
  if(heldAirMode){gs.controlState=2;gs.animationIndex=graph.requestedSemantics[2];gs.animationClass=graph.currentClass(2);
   //12E9B8 in the air (control 2 after a crouched departure): 113F88(1, 0), prewind targets, 113E80(PrewindTurn), 12EE30; then 1211F8 approaches turn/crouch/brake (tech-oob-hops 3149)
   const bool heldAirUpdate=!airReleaseTick&&!previousGround;
   if(heldAirUpdate&&physicsAttached){gs.velocity=physicsState.velocity;gs.forward=physicsState.forward;groundCrouchBrakeTargets(gs.crouch,gs.brake,1,0,terrain_original::dot(physicsState.velocity,physicsState.forward),gs.turn.current);}
   OriginalAirPrewindContext context{0,gs.animationClass,gs.animationIndex,0,false};originalAirPrewindTargets(prewind,rideLatched?rideSpin:turn,rideLatched?rideFlip:flip,context);
   if(heldAirUpdate&&physicsAttached)groundTurnTarget(gs.turn,turn,physicsState.velocity,physicsProfile.surface.id);
   RiderInput input;input.turn=turn;input.crouch=1;input.brake=braking;originalSelectGroundAnimation(gp,gs,input,prewind.spin.current,prewind.flip.current,[](){return rng.next();});if(gs.animationIndex!=graph.requestedSemantics[2])graph.enter(gs.animationIndex);
   if(heldAirUpdate&&physicsAttached){groundControlApproach(gs.turn);groundControlApproach(gs.crouch);groundControlApproach(gs.brake);}}

  bool airEntryTick=false;
  if(passiveMode&&passiveDeparture==0){OriginalPassiveAirAccess access;access.upper=attack_passive_upper;access.rail=[](){return false;};access.handplant=[](bool){return false;}; /*0x107578 already ran before rail attach*/access.stopBoost=[](){gs.boost=0;};access.mainAnimation=[](){return OriginalPassiveAirAnimation{graph.requestedSemantics[2],graph.currentClass(2)};};access.requestAnimation=[](int semantic,float,uint32_t){if(!graph.enter(semantic))throw std::runtime_error("Missing passive air animation");};access.requestControl=[](int control){if(control==5){passiveMode=false;gs.controlState=5;air=originalAirControlBegin(0,0);}};tmpDebug[16]=gs.turn.current;tmpDebug[17]=gs.turn.rate;tmpDebug[18]=gs.turn.target;tmpDebug[19]=gs.crouch.current;tmpDebug[20]=gs.brake.current;tmpDebug[21]=float(animationTick); /*TMPDEBUG*/originalPassiveAirStep(passive,gs,passive_command(turn,jumpHeld),access);airEntryTick=!passiveMode;passiveEntryApproached=airEntryTick;groundControlApproach(gs.turn);groundControlApproach(gs.crouch);groundControlApproach(gs.brake);groundControlApproach(gs.animationTurn);}
  if(passiveDeparture==2){groundControlApproach(gs.turn);groundControlApproach(gs.crouch);groundControlApproach(gs.brake);groundControlApproach(gs.animationTurn);passiveDeparture=0;} /*1211F8 approaches +1FC too (the kind-4 cycle blend reads it)*/ //1211F8 still runs while airborne control0 requests control4
  if(!passiveMode&&!heldAirMode&&!airReleaseTick&&!airEntryTick&&!passiveDeparture&&!bpAirTransition&&!board_press_air_frame()){
  constexpr std::array<int,15> masks={1,2,4,8,3,5,9,6,10,12,7,11,13,14,15};int index=-1;for(int i=0;i<15;i++)if(masks[i]==mask)index=i;
  OriginalGrabAnimationAccess access;access.mainClass=[](){return graph.currentClass(2);};access.mainFlags=[](){return graph.flags(2);};access.play=[](int s,bool force){if(!graph.enter(s,-1,~uint64_t(0),force))throw std::runtime_error("Missing original grab semantic");};access.setRate=[](unsigned c,float r){graph.setRate(c,r);};access.fade=[](unsigned c,float t){graph.fade(c,t);};access.mappedScore=[](int id,bool begin){scoreEvent=id+1;lastScoreId=id;if(begin)originalGrabScoreBegin(scoring,scoreRules.at(id));else originalGrabScoreEnd(scoring,id,grabContext.boostTier);};access.advancedStarted=[](){audio_event(AE_GRAB);}; /*1352A8 -> 29A530*/
  auto result=originalGrabLifecycle(grab,grabProfile,index,tweak,access,grabContext);
  board_press_air_style(int(rideLatched?rideBoard:0),air.adjustFlip); //0x133590..0x133634
  RiderInput input;input.spin=rideLatched?rideSpin:turn;input.flip=rideLatched?rideFlip:flip;input.lateSpin=rideLatched&&rideLateSpin;input.airAdjustLR=turn;input.airAdjustFB=rideLatched?rideAxes[1]-rideAxes[2]:flip;input.boardPress=rideLatched?rideBoard:0;airProfile.boostModifier=physicsAttached&&boostState.modifier>0;airProfile.grabLifecycleResolved=true;airProfile.grabActive=result.active;airProfile.landingAnimation=graph.requestedSemantics[2]==288;airProfile.airSwitch=browser_air_switch; //312AA0 == 288; 0x135BE0
  OriginalAirControlFrame frame;originalAirControlStep(air,input,airProfile,&frame);if(frame.trickStarted)score_trick_start();originalTrickRotationScore(scoring,trickIdentity,commitProfile,(physicsState.state320Equals324?physicsState.reverseStance:!physicsState.reverseStance)?-air.scoredSpin:air.scoredSpin,air.scoredFlip); /*134334: rider+0x324 set negates the spin 119898 stores in score+0x34 (Psymon: -0.0)*/
 OriginalAirAnimationContext context{graph.requestedSemantics[2],graph.currentClass(2),false,gs.reverseStance,result.active,airProfile.trickStat,0,1,predictedLanding,0};
 if(physicsAttached){context.boostModifier=boostState.modifier;context.trajectoryStatus=controlPredictionStatus;context.predictedTime=controlPredictedTime;context.elapsed=controlPredictionElapsed;}
 for(auto&s:graph.sequences)if(s.channel==2)context.mainCompleted=s.completed;
 OriginalAirAnimationAccess aa;aa.duration=[](int semantic){return graph.semanticDuration(semantic);};aa.setNextRate=[](float rate){graph.nextRate=rate;};aa.play=[](int semantic,float){if(!graph.enter(semantic))throw std::runtime_error("Missing air animation semantic");}; /*the selector writes animator +0x1C (0x3158E0) and plays with it; a rate left pending also drives the next non-selector play (tech-oob-dance 2837 landing 66 at 0.7026)*/if(!airAdjustWasLive){airAnimation.adjustment28C=physicsState.adjustment28C;airAnimation.adjustment298=physicsState.adjustment298;} //one rider+0x28C/+0x298 pair: continue the ground/passive values
 originalSelectAirAnimation(air,frame,airAnimation,context,aa);groundControlApproach(airAnimation.adjustment28C);groundControlApproach(airAnimation.adjustment298); //0x1211F8 after 0x133308's targets, before the pose
 gs.adjustment28C=physicsState.adjustment28C=airAnimation.adjustment28C;gs.adjustment298=physicsState.adjustment298=airAnimation.adjustment298;airAdjustLive=true;
  }
 }
 // Control 7 (0x131D30) runs 0x115B58/0x115D48 whenever its update reaches them, including the tick motion 4 loses the rail.
 if(!groundControllerRan&&!startFrame&&!resetFrame&&!crashFrame&&!browserCrashExitFrame&&!softFrame&&(railOwned&&physicsState.controlState==7?railUpperReactions:grounded&&(!physicsAttached||previousGround))&&!jumpHeld){ControllerDraws controllerDraws; /*115D48 is controller-phase (pass 121068)*/OriginalUpperReactionContext context{graph.currentClass(1),physicsState.physicalForward,physicsState.reverseStance,animationTick,riderMask8C0,riderMask8D0};auto reaction=originalUpperReaction(idleSeconds,peers,context,[](){return rng.next();});if(reaction.semantic>=0)graph.enter(reaction.semantic,-1,reaction.mask);}
 // 139A20's orientation tail (0x139A64) runs in every air motion tick, whatever the controller: a rider that left a rail and is
 // still in control 7 (0x132770 waits for the rotation clip) turns toward its trajectory too (PS2 The Throne Psymon 2925).
 if(physicsAttached&&(!railFrame||(physicsState.controlState==12&&!browserRailActive)||(physicsState.controlState==7&&!browserRailActive&&!railStepConsumed))&&!resetFrame&&!crashFrame&&!grounded){align_air_orientation(startFrame?physicsState.controlState:physicsState.controlState==12?12:physicsState.controlState==7&&railFrame?7:softFrame?3:board_press_air_frame()?1:heldAirMode?2:passiveMode?4:5,air.adjustSpin);gs.forward=physicsState.forward;gs.physicalForward=physicsState.physicalForward;gs.lateral=physicsState.lateral;gs.boardUp=physicsState.boardUp;}
 // Active air control owns the single1211F8 filter tick, including charged release.
 if(physicsAttached&&!railFrame&&!resetFrame&&!browserCrashExitFrame&&!crashFrame&&!softFrame&&!grounded&&!heldAirMode&&!passiveMode&&!passiveEntryApproached){groundControlApproach(gs.turn);groundControlApproach(gs.crouch);groundControlApproach(gs.brake);} /*a 12F730 -> control 5 request already had its 1211F8 pass above (pipe-uber 601)*/
 if(physicsAttached&&!(startFrame&&browserStartFrozen)&&!railFrame&&!resetFrame&&!browserCrashExitFrame&&!crashFrame&&(!passiveMode||passiveDeparture==1)&&!softFrame)groundControlApproach(gs.animationTurn); /*passive departure tick: control0 already ran, 1211F8 advances +1FC*/if(physicsAttached)step_secondary_motion();
 if(!railFrame)originalAirPrewindApproach(prewind); //1211F8 approaches the prewind pair every tick, after controller selection.
 attack_control_changes(browser_control_state()); //0x131C30/0x12FB68 upper-attack exit
 if(physicsAttached){gs.adjustment28C=physicsState.adjustment28C;gs.adjustment298=physicsState.adjustment298;} //1043F8 (kind 11) reads rider+0x28C/+0x298, also while a landed air adjust fades out
 graph.advance(gs,prewind.spin.current,prewind.flip.current);audio_animation_events(graph);auto layers=originalAnimationLayers(graph.sequences);auto local=originalAnimationLocalPose(graph.rig->bones,graph.rig->clips,layers);graph.sampledLocal=local;currentPivot=(startFrame||railFrame||grounded||heldAirMode||passiveMode)?std::array<float,3>{}:local.at(pivotBone).position;{terrain_original::Rounding rounding;for(unsigned k=0;k<3;++k)currentPivot[k]=terrain_original::mul(currentPivot[k],graph.scale[k]);} /*134DD0 scales the pivot with EE mul.s (chop)*/const bool airControlPose=!(softFrame||board_press_air_frame()); /*11EB98 applies 134DD0 only while 11FEE8 reports control 5 (not soft control 3 or board-press control 1 in the air)*/auto presentation=(startFrame||resetFrame||crashFrame||railFrame||grounded||heldAirMode||passiveMode||!airControlPose)?originalAirPresentationCurrent(air,{{0,0,0},{0,0,0,1}},currentPivot):originalAirPresentation(air,{{0,0,0},{0,0,0,1}},currentPivot);for(unsigned i=0;i<3;i++)info[16+i]=presentation.position[i];RiderRootPresentation rootState;rootState.controlState=(startFrame||resetFrame||crashFrame||railFrame||browserCrashExitFrame)?physicsState.controlState:softFrame?physicsState.controlState:grounded?(jumpHeld?2:0):heldAirMode?2:passiveMode?4:5;rootState.turn=gs.turn.current;rootState.brake=gs.brake.current;rootState.lateral={1,0,0};poseContact=initialPoseContact;
 // 11EB98 runs FK from the physical root in world space (control5 first applies 134DD0
 // around the animated pivot), with world-space lateral/contact vectors. Building the
 // pose relative to the presented root and composing afterwards differs by several ulps.
 AnimationTransform poseRoot{},presentedFrame{};
 if(physicsAttached){
  AnimationTransform physical{physicsState.position,physicsState.quaternion};
  const bool groundMotionLeft=groundMotionDeparture||(!grounded&&browserDeparturePrePosition.has_value()); /*13D818 ran this tick and 13F194 left the ground after the pose (also soft control 3)*/
  if(groundMotionLeft){if(browserDeparturePrePosition)physical.position=*browserDeparturePrePosition;else{terrain_original::Rounding rounding;for(unsigned k=0;k<3;k++)physical.position[k]=terrain_original::sub(physical.position[k],browserDeparturePush[k]);}} //13F194 pushes after the pose: the pose reads the position before it
  presentedFrame=originalAnimationCompose(physical,{presentation.position,presentation.quaternion});
  const bool airPresentedRoot=airControlPose&&!(startFrame||resetFrame||crashFrame||railFrame||grounded||heldAirMode||passiveMode);
  poseRoot=physical;if(airPresentedRoot){auto root=originalAirPresentationCurrent(air,{physical.position,physical.rotation},currentPivot);poseRoot={root.position,root.quaternion};}
  rootState.extraLean=physicsState.extraLean.current;rootState.roll=physicsState.presentationRoll.current;rootState.liftCm=physicsState.presentationLift.current;
  rootState.lateral=physicsState.lateral; /*11FA10 reads +3B0, which airborne motion leaves at the departure tick's ground-contact tangent*/
  // 139A70 copies the retained presentation up (+180, the previous pose's board-root up)
  // into +370 before airborne motion; 11EB98's board lift then reads that normal.
  // A passive departure tick ran ground motion 13D818 (13F194 leaves afterwards): its
  // pose still reads the ground contact normal and the pre-13F2E4 board normal.
  const bool groundMotionPose=grounded||groundMotionLeft;
  poseContact.normal=!groundMotionPose&&airMotionThisTick&&!railFrame&&!crashFrame&&!resetFrame&&!startFrame?physicsState.presentationUp:physicsState.normal;
  poseContact.boardDirection=groundMotionPose?browserBoardNormalForPose:physicsState.boardNormal;
  poseContact.boardLiftCm=physicsState.boardLift;poseContact.boardAlignment=physicsState.boardAlignment.current;
 }
 poseControls={rootState.turn,rootState.brake,rootState.liftCm,rootState.extraLean,rootState.roll,poseContact.boardLiftCm,poseContact.boardAlignment};
 for(unsigned i=0;i<3;i++){poseControls[7+i]=rootState.lateral[i];poseControls[10+i]=poseContact.normal[i];poseControls[13+i]=poseContact.boardDirection[i];}
 AnimationTransform boardRoot;auto bodyRoot=originalRiderRootPresentation(poseRoot,local.at(22).position,graph.scale,rootState,&boardRoot);
 // 11EB98 with rider+0x150 (136D40 detached the board): after 11FA10 the local board root/child (+0x8A0/+0x8A4) become
 // identity and the second FK root is rider+0x130/+0x140, so bone 22 is the free board itself (board alignment still runs on it).
 const bool detachedBoardPose=physicsAttached&&crash.actor.detached; //+0x150 stays set through the reset control 9 until 11D660 places the rider
 // The rider frame +0x160..+0x190 (the air probe's board-root frame, 1057B8's up) is 11FA10's board root with that identity local
 // board: the physical transform (PS2 peak2/dbc2-race-tuck 3934.., +0x180 = +0x120's up, +0x190 = +0x110), not the free board.
 AnimationTransform frameBoardRoot=boardRoot;if(detachedBoardPose)originalRiderRootPresentation(poseRoot,{0,0,0},graph.scale,rootState,&frameBoardRoot);
 if(detachedBoardPose){for(unsigned b:{22u,23u}){local.at(b)={{0,0,0},{0,0,0,1}};if(graph.sampledLocal)graph.sampledLocal->at(b)=local.at(b);}boardRoot={crash.actor.detachedPosition,crash.actor.detachedQuaternion};}for(unsigned k=0;k<3;k++)tmpDebug[5+k]=boardRoot.position[k]; //TMPDEBUG
auto worldPose=originalAnimationWorldPose(graph.rig->bones,local,bodyRoot,graph.scale,{bodyRoot,boardRoot});legWeight=originalGrabLegWeight(legWeight,graph.currentClass(2));poseContact.legWeight=legWeight;poseControls[16]=legWeight;tmpPreContact=worldPose;originalRiderPoseContact(worldPose,local,graph.scale,poseContact);if(crashFrame&&crash.actor.detached&&!physicsAttached){{const auto&q=physicsState.quaternion;AnimationTransform inverse;inverse.rotation={-q[0],-q[1],-q[2],q[3]};auto delta=crash.actor.detachedPosition;for(unsigned k=0;k<3;k++)delta[k]-=physicsState.position[k];worldPose.at(23)=originalAnimationCompose(inverse,{delta,crash.actor.detachedQuaternion});}}
 for(unsigned k=0;k<3;k++){tmpDebug[8+k]=crash.actor.detachedPosition[k];tmpDebug[11+k]=crash.motion.detachedVelocity[k];}tmpDebug[14]=crash.actor.detached;tmpDebug[15]=crashFrame; //TMPDEBUG
completedMain=false;completedMainSemantic=graph.requestedSemantics[2];for(const auto& sequence:graph.sequences)if(sequence.channel==2&&sequence.semantic==completedMainSemantic)completedMain|=sequence.completed;
// rider_host bit 4 (web/ai-racers.js, computer riders): nothing reads this context's renderer poses (its renderer draws the
// skin palette, which reads cachedCrashWorld), so the poses below (a compose per bone, pure, writing only `poses`) are skipped
// and animation_post's return keeps the last ones written (docs/sim-performance.md "Renderer poses").
if(!(riderHostFlags&4)){AnimationTransform toPresented;if(physicsAttached){const auto&q=presentedFrame.rotation;toPresented.rotation={-q[0],-q[1],-q[2],q[3]};const auto&t=presentedFrame.position;toPresented.position=originalAnimationCompose({{0,0,0},toPresented.rotation},{{-t[0],-t[1],-t[2]},{0,0,0,1}}).position;}
  // Renderer poses stay relative to the presented physical root.
  for(unsigned i=0;i<worldPose.size();i++){const auto bone=physicsAttached?originalAnimationCompose(toPresented,worldPose[i]):worldPose[i];for(int k=0;k<3;k++)poses[i*7+k]=bone.position[k];for(int k=0;k<4;k++)poses[i*7+3+k]=bone.rotation[k];}}if(physicsAttached&&browserSoftActive){bool complete=false;for(const auto& sequence:graph.sequences)if(sequence.channel==2&&sequence.semantic==physicsState.animationIndex){complete=sequence.completed;break;}soft_animation_complete(complete);} /*312AE8: the FIRST channel-2 sequence (a new play is inserted first): a back-to-back soft collision's clip is not complete because the previous soft's same clip, completed and still fading behind it, is (course-limits/p3b-right3000 14776, docs/obstacle-collision.md)*/graph.completeSequences();previousGround=grounded;previousHeld=jumpHeld;
 info[0]=graph.requestedSemantics[2];info[1]=graph.currentClass(2);info[2]=grab.state;info[3]=scoreEvent;info[4]=graph.rig->bones.size();info[5]=graph.flags(2)&0xffffff;
 if(physicsAttached){
  // These are rider+208/+214/+220 in the original, shared by air and ground.
  // Commit air filters before contacts can switch back to the ground owner.
  if(!grounded&&!railFrame&&!resetFrame&&!crashFrame&&!softFrame&&!browserCrashExitFrame){physicsState.turn=gs.turn;physicsState.brake=gs.brake;physicsState.crouch=gs.crouch;}
  physicsState.animationTurn=gs.animationTurn;physicsState.animationIndex=graph.requestedSemantics[2];physicsState.animationClass=graph.currentClass(2);
  const AnimationTransform physical{physicsState.position,physicsState.quaternion};
  cachedCrashWorld=worldPose;
  const auto worldHead=worldPose.at(0).position;
  for(unsigned i=0;i<3;i++){posedPhysical[i]=physical.position[i];posedPhysical[9+i]=worldHead[i];}
  for(unsigned i=0;i<4;i++)posedPhysical[3+i]=physical.rotation[i];posedPhysical[7]=1;posedPhysical[8]=grounded;

  std::array<terrain_original::Vector,22> centers;
  for(unsigned i=0;i<centers.size();i++)centers[i]=worldPose.at(i).position;
  // Original collision radii use the rider body scale, independently of bone posing.
  auto volume=bodyVolumeFromBones(centers,physicsProfile.bodyScale);
  // The air probe is 139C88: presentation runs on the physical pose, then the board
  // root is built in that frame -- the same world-space FK as the cached pose.
  const AnimationTransform probeBoard=frameBoardRoot;
  const auto probeCenter=worldPose.at(poseContact.boardRoot).position;
  const auto prePoseUp=physicsState.presentationUp;volume.reactionFrame=originalRiderCollisionFrame(probeBoard);physicsState.presentationUp=volume.reactionFrame->up;presentationForward170=volume.reactionFrame->forward;
  volume.landingCenterCm=probeCenter;
  if(!grounded&&!heldAirMode&&!passiveMode)volume.airPivotCm=currentPivot;
  for(const auto& sequence:graph.sequences)if(sequence.channel==2&&sequence.semantic==graph.requestedSemantics[2]){volume.mainAnimation=BodyAnimationState{sequence.semantic,graph.currentClass(2),sequence.flags,sequence.completed};break;}
  browserBodyVolume=std::move(volume);animationPost.prePoseUp=prePoseUp;
 }
 animationPost.pending=true;animationPost.grounded=grounded;animationPost.jumpHeld=jumpHeld;animationPost.startFrame=startFrame;animationPost.resetFrame=resetFrame;animationPost.crashFrame=crashFrame;animationPost.railFrame=railFrame;animationPost.softFrame=softFrame;for(unsigned i=0;i<4;i++)animationPost.presentationQuaternion[i]=presentation.quaternion[i];
}
static float* animation_post_phase(){
 if(!animationPost.pending)return poses;animationPost.pending=false;struct PostScope{PostScope(){animationPostActive=true;}~PostScope(){animationPostActive=false;}} postScope;
 const int grounded=animationPost.grounded,jumpHeld=animationPost.jumpHeld;const bool startFrame=animationPost.startFrame,resetFrame=animationPost.resetFrame,crashFrame=animationPost.crashFrame,railFrame=animationPost.railFrame,softFrame=animationPost.softFrame;
 bool poseLanded=false;std::array<float,3> bodyPoseTranslation{};
 const int postMotion=npc_motion_mode(); // the 1114A0 post stage this rider runs (0 ground 13F178, 1 air 139C88; 2/4 below)
 // 13F178's board normal update (13F2E0: +0x390 = unit(+0x390 + 0.5 +0x370)) belongs to the ground post. A rider an earlier rider's
 // 107888 crashed after its ground motion runs the crash post instead and keeps +0x390 (PS2 Gravitude Mac 873, Nate's pair).
 if(postMotion==2&&!crashFrame&&groundBoardNormalBefore)physicsState.boardNormal=*groundBoardNormalBefore;
 if(physicsAttached){
  // 139D78 touchdown resolves before the second-phase body query 13AA48; after a touchdown the
  // query filters against the new ground normal and uses the ground response.
  if((!railFrame||(physicsState.controlState==12&&!browserRailActive)||handplant_air_frame())&&!grounded&&resolve_posed_landing(*browserBodyVolume)){
   if(physicsState.controlState==12){auto award=rail_score_entry(!physicsState.state320Equals324,0,0);award_rail_meter(award.meterDelta);}else finish_landing(jumpHeld,output[15]);previousGround=true;poseLanded=true;
   physicsState.animationIndex=graph.requestedSemantics[2];physicsState.animationClass=graph.currentClass(2);
   info[0]=graph.requestedSemantics[2];info[1]=graph.currentClass(2);info[2]=grab.state;info[5]=graph.flags(2)&0xffffff;
  }
  // 13F1C8..13F22C: a grounded 13F178 on surface 18 enters the hard crash 10EB30(rider, 360, 0, +0x438, {+0x110, +0x1E0 unit,
  // +0x370, closing 0}) with the unclamped velocity, before its 13F488 / 105398 / 107888 (they then see the ragdoll) and the
  // 13F358 clamp (below: on the crash actor, the rider's own +0x1E0). 11FEC8(13) replaces the soft controller (control 3 has
  // no exit). PS2 peak2/dbc2-race-tuck 3910: the soft-collision slide onto patch 0x4DB (surface 18) crashes at once.
  const bool surfaceCrash=browserGroundCrashPending&&grounded&&!crash.active;browserGroundCrashPending=false;
  if(surfaceCrash){if(groundUnclampedVelocity)physicsState.velocity=*groundUnclampedVelocity;
   OriginalCollisionEvent event;event.pointCm=physicsState.position;event.normal=physicsState.normal;event.closingSpeedCmps=0;event.surface=18;event.surfaceProperty44=browser_surface_property(18);
   auto direction=physicsState.velocity;const auto length=std::sqrt(terrain_original::dot(direction,direction));if(length>0)for(auto& value:direction)value/=length;event.incomingDirection=direction;
   browserSoftActive=false;enter_crash(360,event);}
  if(browserGroundResetPending&&grounded&&!crash.active){browserGroundResetPending=false;if(browserResetBegin)browserResetBegin(1);} //13F260 116120(rider,0,1) (core.cpp)
  // 13F4CC skips only a computer rider's 13F488 query near its route; 105398 (and 107888) still run.
  const bool postContacts=!((startFrame&&browserStartFrozen)||resetFrame||crashFrame||(railFrame&&(browserRailActive||(physicsState.controlState!=12&&!handplant_air_frame()))));
  // 139C88 runs 13AA48 after its touchdown too (13A718 -> 11E150 -> 13AA48 at 13A744, also after a landing crash): the query
  // keeps the air filter (query+0x10 = rider+0x180, not the new ground normal +0x370) and 13AA48's own response (no ground
  // projection or steering); only 105398 after it sees the ground motion 11FE78(0) set (PS2 allpeak/apr-start 3581: a landing
  // on a steep wall patch, whose triangle the ground-normal filter would drop, pushes the rider 18 cm and plays a soft clip).
  // The filter is this tick's +0x180: the rider manager poses (121728 -> 11EB98 -> 11FA10, the frame +0x160..+0x190) before it
  // moves (121750 -> 1114A0 -> 139C88 -> 13AA48), so the air query reads the frame of the pose just made, not the previous
  // one (PS2 ESS3 Moby 839: a slope triangle at dot 0.7979 with the old up, 0.8003 with the new, is dropped; ARA1 six riders).
  const bool airQuery=airMotionThisTick;
  browserBodyQuery=(!postContacts||!npc_obstacle_query_enabled(airQuery))?std::nullopt:inspect_body_contacts(originalCoreBodyQueryVolume(offsetBodyVolume(*browserBodyVolume,browserLandingTranslation)),airQuery?physicsState.presentationUp:physicsState.normal);
  browserBodyResponse.reset();bodyResponseAir=airQuery;
  // 13F178: 13F488 and 105398 (106F78 first) respond before the 13F358 speed clamp, which core.cpp
  // applied early; give them the unclamped velocity and clamp afterwards (idempotent without contacts).
  const bool pairPost=rider_pairs_hosted()&&(postMotion==0||postMotion==1)&&!crashFrame&&!resetFrame&&!(railFrame&&browserRailActive);
  // A 13F178 departure (13F194 -> 114298) runs the same post stage: the contacts, then 11FE78(1) seeds the flight (1135B8) at
  // 13F2CC with the unclamped velocity, then 13F358 clamps the rider (PS2 ARA1 Luther 3799: the first air tick is 2 ulp faster).
  const bool departureClamp=!grounded&&browserDeparturePrePosition.has_value();
  const bool groundClampAfterContacts=groundUnclampedVelocity&&(postContacts||pairPost)&&!bodyResponseAir&&(grounded||departureClamp)&&!crash.active;
  const auto groundClampedVelocity=physicsState.velocity;if(groundClampAfterContacts)physicsState.velocity=*groundUnclampedVelocity;
  // A touchdown that entered the crash (landing crash, 13A530) before this 13AA48: the rest of 139C88 runs with the
  // rider as the ragdoll (motion 2) -- the response moves the crash actor and 105D98 dispatches as a ragdoll impact.
  const bool crashInPost=crash.active&&!crashFrame;
  if(browserBodyQuery&&browserBodyQuery->best.hit){const auto incoming=physicsState.velocity;browserBodyResponse=apply_body_contact(browserBodyQuery->best,bodyResponseAir);if(browserBodyResponse->accepted){bodyPoseTranslation=browserBodyResponse->translationCm;terrain_original::Rounding rounding;for(unsigned k=0;k<3;k++){browserBodyVolume->broadCenterCm[k]=terrain_original::add(browserBodyVolume->broadCenterCm[k],browserBodyResponse->translationCm[k]);for(unsigned i=0;i<browserBodyVolume->count;i++)browserBodyVolume->spheres[i].centerCm[k]=terrain_original::add(browserBodyVolume->spheres[i].centerCm[k],browserBodyResponse->translationCm[k]);}if(crashInPost){crash.actor.position=physicsState.position;crash.actor.velocity=physicsState.velocity;} /*the rider is the ragdoll now*/if(browserBodyResponse->bounced){if(crashInPost)crash.impactReaction(browserBodyQuery->best,*browserBodyResponse,incoming,browser_surface_property,collisionProfile,collisionHistory);else{if(grounded)audio_event(AE_OBSTACLE); /*13F810: 2989A8 after the 13F488 105D98 dispatch*/auto reaction=classify_body_contact(browserBodyQuery->best,*browserBodyResponse,*browserBodyVolume,incoming,softFrame&&browserSoftActive?3:heldAirMode?2:passiveMode?4:(grounded||poseLanded)?(jumpHeld?2:0):5,[](){return rng.nextMotion();},poseLanded?0:-1); /*a soft clip that ended in this tick's 12E778 already requested control 0: 105D98 sees 0 and 108388 re-enters control 3 (full-course 3334)*/if(reaction.kind==OriginalCollisionReactionKind::Soft&&(grounded||poseLanded||groundDeparturePending)&&!reaction.cancelControlOne&&(physicsState.controlState==0||physicsState.controlState==2))soft_collision_begin(reaction.animation,reaction.manualSpin);}}}} /*108388 gates on motion 0/4: a 13F194 departure keeps motion 0 until 13F2CC, so a body contact in the departure tick still enters control 3 (air-release/handplant-invert 503)*/
  // Original 105398 follows 13F488 on the ground (a1=0) and 13AA48 in the air (a1 =
  // the bounced hit normal). It owns this tick's filter-1 query and pickup copy.
  if(postContacts){advance_pickup_timers(motionTick);instanceContactTickShared=motionTick;
   const std::optional<std::array<float,3>> caller=bodyResponseAir&&!poseLanded&&browserBodyResponse&&browserBodyResponse->bounced?std::optional(browserBodyResponse->normal):std::nullopt; //after a touchdown 105398 projects on the ground normal (motion 0) whatever a1 is
   const int eventControl=(!airMotionThisTick&&!grounded)?physicsState.controlState:(softFrame&&browserSoftActive?3:heldAirMode?2:passiveMode?4:(grounded||poseLanded)?(jumpHeld?2:0):5); //a touchdown earlier in this post stage (13A968) already requested control 0: 108388 accepts (tech-oob-hops 2846) //a soft clip that ended in this tick's 12E778 left control 0: 105398's 108388 re-plays it (full-course 5535)
   const auto moved=run_rider_instance_contacts(*browserBodyVolume,browserLandingTranslation,caller?&*caller:nullptr,bodyResponseAir&&!poseLanded?1:0,eventControl,{});
   terrain_original::Rounding rounding;for(unsigned k=0;k<3;k++)bodyPoseTranslation[k]=terrain_original::add(bodyPoseTranslation[k],moved[k]);}
  if(pairPost)rider_pair_point(); // 13F2A8 / 13A784: 107888 after 105398, before the 13F358 clamp
  groundDeparturePending=false; // 13F2CC: 11FE78(1) switches a departed rider to air motion
  if(groundClampAfterContacts){
   if(departureClamp)begin_prediction({physicsState.position,physicsState.velocity});
   const auto unclamped=physicsState.velocity;originalGroundClampSpeed(physicsState,physicsProfile.speedLimit);
   if(crash.active&&crash.actor.velocity==unclamped)crash.actor.velocity=physicsState.velocity; // a contact entered the crash: the rider is the crash actor
   if(physicsState.velocity!=groundClampedVelocity)commit_rider_physics();
  }

  if(surfaceCrash&&crash.active&&groundUnclampedVelocity){auto clamp=physicsState;clamp.velocity=crash.actor.velocity;originalGroundClampSpeed(clamp,physicsProfile.speedLimit); //13F358 after a surface-18 crash
   if(clamp.velocity!=crash.actor.velocity){crash.actor.velocity=clamp.velocity;publish_crash_actor(crash.actor,crash.motion.submode);}}
  if(railFrame){const auto moved=rail_motion_post();terrain_original::Rounding rounding;for(unsigned k=0;k<3;k++)bodyPoseTranslation[k]=terrain_original::add(bodyPoseTranslation[k],moved[k]);} //13BFA8 motion-4 post
  if(crashFrame){crash.contacts(*browserBodyVolume,*cameraTerrain,*browserBodies,landingProfile,browser_surface_property,collisionProfile,collisionHistory);{terrain_original::Rounding rounding;for(unsigned k=0;k<3;k++)bodyPoseTranslation[k]=terrain_original::add(bodyPoseTranslation[k],crash.contactTranslation[k]);}publish_crash_actor(crash.actor,crash.motion.submode);sync_crash_prediction(crash.trajectory,{crash.actor.position,crash.actor.velocity},crash.motion.submode==1);}
 }
//121750/310530 translates cached world geometry after second motion/contact.
// Keep the sampled rotations and animation clocks; only commit the accumulated displacement.
if(physicsAttached){
 terrain_original::Rounding rounding;
 for(unsigned k=0;k<3;++k){
  const float delta=committedPoseTranslation[k]=terrain_original::add(terrain_original::add(bodyPoseTranslation[k],pairCompanion[k]),browserLandingTranslation[k]);
  if(delta!=0){posedPhysical[k]=terrain_original::add(posedPhysical[k],delta);posedPhysical[9+k]=terrain_original::add(posedPhysical[9+k],delta);for(auto& bone:cachedCrashWorld)bone.position[k]=terrain_original::add(bone.position[k],delta);}
  if(browserBodyVolume){
   auto& volume=*browserBodyVolume;
   // Body response already translated spheres; landing adds its own106538 displacement.
   volume.broadCenterCm[k]=terrain_original::add(volume.broadCenterCm[k],browserLandingTranslation[k]);
   for(unsigned i=0;i<volume.count;++i)volume.spheres[i].centerCm[k]=terrain_original::add(volume.spheres[i].centerCm[k],browserLandingTranslation[k]);
   if(volume.landingCenterCm)(*volume.landingCenterCm)[k]=terrain_original::add((*volume.landingCenterCm)[k],delta);
  }
 }
}
if(physicsAttached){if(grounded&&!railFrame&&!crashFrame&&!resetFrame)retainedGroundLateral=physicsState.lateral;else if(railFrame||crashFrame||resetFrame)retainedGroundLateral.reset();}
if(crash.active){info[0]=graph.requestedSemantics[2];info[1]=graph.currentClass(2);info[2]=grab.state;info[5]=graph.flags(2)&0xffffff;}
if(physicsAttached&&lastVisualTick!=motionTick){if(fxDeferred)fxPending=true;else{lastVisualTick=motionTick;fx_pass_phase(-1);}} // the rider FX passes (deferred: the host runs fx_pass after every rider's 121818)
for(int i=0;i<4;i++)info[6+i]=animationPost.presentationQuaternion[i];info[10]=air.totalSpin;info[11]=air.totalFlip;info[12]=originalCurrentTrickPoints(scoring);info[13]=banked;info[14]=lastScoreId;info[15]=(browserStartFrame||browserResetActive||crash.active||railFrame||railReleaseFrame||browserCrashExitFrame||board_press_owned())?physicsState.controlState:poseLanded?physicsState.controlState:physicsAttached&&(browserSoftActive||browserSoftFrame)?physicsState.controlState:heldAirMode?2:passiveMode?4:(grounded?(jumpHeld?2:0):5);rideLatched=false;groundControllerRan=false;
 //131608 (control0 entry via 11FEC8/111538) zeroes the 115D48 idle clock +35C; entries happen after that tick's controller.
 groundDeparturePending=false;
 if(physicsAttached){const int control=int(info[15]);if(control==0&&idleControl!=0)idleSeconds=0;idleControl=control;}
 return poses;
}
EMSCRIPTEN_KEEPALIVE float* animation_tick(float speed,float turn,float braking,float charge,int grounded,int jumpHeld,int mask,int tweak,int boost,float predictedLanding,float impact,float flip){
 animation_pose(speed,turn,braking,charge,grounded,jumpHeld,mask,tweak,boost,predictedLanding,impact,flip);
 if(riderHostFlags&1)js_rider_after_pose(); // the other riders' passes up to their poses (web/ai-racers.js)
 return animation_post_phase();
}
EMSCRIPTEN_KEEPALIVE float* animation_post(){return animation_post_phase();}
EMSCRIPTEN_KEEPALIVE void rider_host(int flags){riderHostFlags=flags;}
EMSCRIPTEN_KEEPALIVE int animation_post_pending(){return animationPost.pending;}
EMSCRIPTEN_KEEPALIVE int animation_audit(){int count=0;for(auto&clip:graph.rig->clips){for(float time:{0.f,clip.duration*.5f,clip.duration}){AnimationLayer layer;layer.clip=clip.id;layer.time=time;auto pose=originalAnimationLocalPose(graph.rig->bones,graph.rig->clips,{layer});for(auto&b:pose){for(float v:b.position)if(!std::isfinite(v))return -int(clip.id);for(float v:b.rotation)if(!std::isfinite(v))return -int(clip.id);}}++count;}return count;}
EMSCRIPTEN_KEEPALIVE float* crash_root_preview(int semantic){
 RIDER_LOCAL static float values[7];auto root=graph.previewRoot(semantic);if(!root)return nullptr;
 for(unsigned i=0;i<3;i++)values[i]=root->position[i];for(unsigned i=0;i<4;i++)values[3+i]=root->rotation[i];return values;
}
EMSCRIPTEN_KEEPALIVE float* sampled_local_root(){
 RIDER_LOCAL static float values[7];auto root=graph.scaledLocalRoot();if(!root)return nullptr;
 for(unsigned i=0;i<3;i++)values[i]=root->position[i];for(unsigned i=0;i<4;i++)values[3+i]=root->rotation[i];return values;
}
EMSCRIPTEN_KEEPALIVE void hard_crash_begin(int semantic,float closingSpeed){if(semantic<328||semantic>361||!std::isfinite(closingSpeed)||closingSpeed<0)throw std::runtime_error("Invalid original hard-crash event");OriginalCollisionEvent event;event.pointCm=physicsState.position;event.normal=physicsState.normal;event.closingSpeedCmps=closingSpeed;event.surface=physicsProfile.surface.id;event.surfaceProperty44=browser_surface_property(event.surface);enter_crash(semantic,event);}
EMSCRIPTEN_KEEPALIVE float* wake_info(){RIDER_LOCAL static float v[13];if(wakeVerticesStale)build_wake_vertices();v[0]=wakeReady;v[1]=wakePhysics.cursor.count;v[2]=wakePhysics.cursor.head;v[3]=wakePhysics.control.active3C;v[4]=wakePhysics.control.amplitude90;v[5]=wakePhysics.control.alpha94;v[6]=wakePhysics.cursor.phase;auto tip=wakePhysics.tip();for(unsigned k=0;k<3;k++)v[7+k]=tip?(*tip)[k]:0;v[10]=wakeUpdates;v[11]=wakeVertices.size()/9;v[12]=wakeRenderSerial;return v;}
EMSCRIPTEN_KEEPALIVE float* wake_vertices(){if(wakeVerticesStale)build_wake_vertices();return wakeVertices.data();}
EMSCRIPTEN_KEEPALIVE float* snow_flipbook_info(){RIDER_LOCAL static float values[20];for(unsigned i=0;i<10;++i){values[i]=snowTextureBase[i]+int(snowFlipPhase[i]);values[10+i]=snowFlipPhase[i];}return values;}
// 111890 (the rider FX reset) without a placement: a free-ride start that is not a region placement (the CTM plane drop,
// web/main.js resetPhysics, pv sprayReset). The same FX set as reset_place_at.
EMSCRIPTEN_KEEPALIVE void rider_fx_reset(){reset_trail();reset_snow(false);reset_impact_fx(false);reset_boost_fx();weather_rider_fx_reset();}
EMSCRIPTEN_KEEPALIVE float* snow_info(){RIDER_LOCAL static float v[23];if(snowBuffersStale)build_snow_buffers();for(unsigned i=0;i<10;i++){v[i]=snowBuffers[i].size()/8;v[10+i]=snowEmitters[i]?snowEmitters[i]->parameters().birthCapacity*snowEmitters[i]->parameters().particlesPerBirth:0;}v[20]=snowSerial;v[21]=snowWakeGap;v[22]=snowState.impact.strength;return v;}
EMSCRIPTEN_KEEPALIVE float* snow_particles(unsigned emitter){if(snowBuffersStale)build_snow_buffers();return emitter<10?snowBuffers[emitter].data():nullptr;}
EMSCRIPTEN_KEEPALIVE float* trail_info(){RIDER_LOCAL static float v[16];if(trailBuffersStale)build_trail_buffers();v[0]=trailRibbon.size()/9;v[1]=trailRoof.size()/9;v[2]=trailSerial;v[3]=trailState.head;v[4]=trailState.count;v[5]=trailState.phase;v[6]=trailState.cooldown;v[7]=trailInput.motion;for(unsigned k=0;k<3;k++){v[8+k]=trailInput.contact[k];v[11+k]=trailInput.velocity[k];}v[14]=trailInput.surface;v[15]=trailState.parity;return v;}
EMSCRIPTEN_KEEPALIVE float* trail_ribbon(){if(trailBuffersStale)build_trail_buffers();return trailRibbon.data();}
EMSCRIPTEN_KEEPALIVE float* trail_roof(){if(trailBuffersStale)build_trail_buffers();return trailRoof.data();}
// QA (compare-ps2-capture.mjs --peak-run): the human's route words of a PS2 record: +0x490 closest point, +0x4A0 lookahead,
// +0x4B0 previous lookahead, +0x4C0/+0x4C4 distances, +0x4C8 lateral, +0x4CC heading (13C948 reads it as the fall line).
EMSCRIPTEN_KEEPALIVE void reset_route_seed(const float* w){auto& r=resetRoute;for(unsigned k=0;k<3;k++){r.closestPoint[k]=w[k];r.lookaheadPoint[k]=w[3+k];r.previousLookaheadPoint[k]=w[6+k];}r.previousDistance=w[9];r.currentDistance=w[10];r.lateralDistance=w[11];r.heading=physicsState.headingOffset=w[12];}
// QA (compare-ps2-capture.mjs --peak-arrival): the words a Transport arrival carries from the ride before it, which the
// placement 11D390 and the route re-attach 112180 do not write: +0x4CC the route heading (13C948 reads it as the fall line on
// the placement tick, before 121818 updates it) and +0x434 the location id (1218D0 writes it after the motion). In the browser
// the rider keeps its own; a capture replay starts from a course seed, so it copies the placement record's.
EMSCRIPTEN_KEEPALIVE void arrival_carry_seed(float heading,int location){resetRoute.heading=physicsState.headingOffset=heading;physicsState.riderType=location;gs.headingOffset=heading;gs.riderType=location;}
// A location entry's placement (11D390 in free ride and the peak runs, kinds 4..6; web/free-ride.js spawnFor/arrivalFor).
// 11D390: 117540 (score run reset), then 11DE60(rider, index, kind) at the path-bank region row's position/direction (PS2 cm):
// 119368(score, 1), 11FEC8 control 0, 11FE78 motion 0 (its entry 13C7A8 runs BEFORE the placement: +0x390 = the old +0x370,
// the controller depths of the old +0x438, bounce phase 0, focus tick), 11D660(semantic 5, clearance 0) (writes +0x370 and
// +0x438 = 0; keeps +0x380/+0x390); then 11DF18(rider, 0): animator rate 0 (311B20), velocity = +0x1B0 x 833.333 (VU, chop)
// with z = 0, 111890. transport = 1: the rider comes from the Transport loop's limbo placement (PS2 peak1-arrive-*: control
// 13 / motion 3 with +0x370 = +0x380 = (0, 0, 1) on surface 0), as 236960 -> 123F38 -> 11D390 does.
void start_ground_motion(); // web/core.cpp: 13C7A8
// The rider's setup record (0x535B20 + i x 0x1C through 0x5305B0[rider+0x86C], getter 14A080: +0x11 the base character, +0x12 the
// cheat id, +0x10 flags), what 11D390's free-ride branch and the stat getters read. A race's rider has its own character's record;
// the port carries the words it feeds as the rider's document (the reset stance = its CHARDB stance, the body scale = 14EFA8's size).
// CHARDB.DBL (loader 0x149C84 -> 0x530970, 10 rows of 0x88): +0x44 stance (1 goofy), +0x48 model size (14EFA8: x gp-0x6DF4 = 0.01).
static constexpr int chardbStance[10]={1,0,1,0,0,0,1,0,1,0},chardbSize[10]={94,80,83,85,85,70,96,100,92,89};
RIDER_LOCAL static float setupModelSize=-1.f; // 14EFA8(slot) when the record changed after the rider was made (-1: its own body scale)
// 149A88(., slot) (0x149A88..0x149AE8): the slot's setup record cleared, +0x10 |= 2 and +0x11 = 4 (Zoe). PS2 c0a-ret3 (watch of
// 0x535B20): slot 1 turns from Psymon to 4 at WS15's record; from then on 11D390's +0x324 is CHARDB[4].stance (0x11D3F8..0x11D424:
// 14EF70 -> 14A080), 115B08 / 115AB0 read CHARDB[4]'s size, and the three stat getters 1494C0 / 1493D8 / 148D80 return 0.5 (1477E8, the
// player setup: 0x149508 / 0x149420 / 0x148DC8; PS2 c0a-ret8 probe of 0x11B434). Which WS15 code calls it is unconfirmed (indirect).
EMSCRIPTEN_KEEPALIVE void rider_setup_player_reset(){
 constexpr int character=4;resetStance=chardbStance[character]!=0;
 {terrain_original::Rounding rounding;setupModelSize=terrain_original::mul(float(chardbSize[character]),0.00999999977648258209f);} // cvt.s.w x gp-0x6DF4 (0x14F0D8)
 npc_player_setup_stats();
}
static float setup_model_size(){return setupModelSize>=0?setupModelSize:physicsProfile.bodyScale;}
#include "../engine/race_event.hpp"
extern "C++" bool browser_race_replace_paths(std::vector<ssx::OriginalRacePath>,int); // web/race_bridge.cpp (this part of the file is inside extern "C")
// 112180(rider, place) (docs/ctm-events-in-world.md stage 5). bank: the location's paths.json variant; slot: rider+0x86C.
// - 26B5E0(0x4D33A0, type 1, rider+0x86C) at 0x1121AC: the exported kind-0 row of this slot (the start line), else the bank's first row;
// - place (0x1121B8: its second argument): the start-row placement (0x1121C0..0x112248): point = row + dir x (90 x 14EFA8(slot) - 90)
//   (115B08; VU vmulx / vadd), then 11D660(point, dir, 5, 0): its ground probe's normal is the +0x370 that a following 11DE60's 13C7A8
//   copies into +0x390 (PS2 c0a-ret3: every computer rider's +0x390 = the start pad's (-0.0001, 0, 1) at WS15's record);
// - the route caches: +0xAB4 = the row's race path, 26A638 -> +0x4D0 = +0x4D4 (0x1122A0); +0xAB8 = its reset path, 26A638 -> +0x490,
//   +0x4C0 / +0x4C4 / +0x4C8, 26AB20 -> +0x4A0 at + 796 (0x1122E4..0x112318). +0x4CC is not written.
static int start_row_reattach(const char* bankText,int slot,bool place){
 const auto bank=json::parse(bankText);
 const json* row=nullptr;for(const auto& r:bank.at("regions"))if(r.at("kind")==0&&r.at("index")==slot){row=&r;break;}
 if(!row&&!bank.at("regions").empty())row=&bank.at("regions").at(0);
 if(!row)return 0;
 const auto p=row->at("position").get<std::array<float,3>>(),d=row->at("direction").get<std::array<float,3>>();
 terrain_original::Vector point,dir{d[0],d[1],d[2]};
 {terrain_original::Rounding rounding;const float offset=terrain_original::sub(terrain_original::mul(setup_model_size(),90.f),90.f);
  for(unsigned k=0;k<3;++k)point[k]=terrain_original::add(p[k],terrain_original::mul(d[k],offset));}
 if(place)reset_place_at(point,dir,0.f,5);
 std::vector<ssx::OriginalRacePath> race;
 for(const auto& q:bank.at("race_paths")){ssx::OriginalRacePath path;path.origin=q.at("origin").get<std::array<float,3>>();path.low=q.at("low").get<std::array<float,3>>();path.high=q.at("high").get<std::array<float,3>>();path.remainingAtOrigin=q.at("remaining_at_origin");path.segments=q.at("segments").get<std::vector<std::array<float,4>>>();
  for(const auto& e:q.at("events"))path.events.push_back({e.at("type").get<uint32_t>(),e.at("value").get<uint32_t>(),e.at("start").get<float>(),e.at("end").get<float>()});race.push_back(std::move(path));}
 browser_race_replace_paths(std::move(race),row->at("race_path").get<int>());
 browser_reset_replace_paths(browser_parse_reset_paths(bank.at("reset_paths")),row->at("reset_path").get<int>());
 return 1;
}
// 129160 (the rider manager's pose pass, 0x129160..0x1291DC; its one caller is world state 1's exit 0x234750 at 0x23488C): each
// listed rider gets 11EB60(rider, 1.0) (one full-rate step: advance, local pose, completion batch; as 11D660's placement step),
// 11EB98 and 3103F0(rider+0x780), with no provider, controller or physics pass. After a WS15 return that exit follows the riders'
// removal (12B030) in the same frame, so only the human is listed (PS2 c0a-ret11: its 3135B0 slot advances run twice in the WS3
// tick-0 window, its controllers' 311B20 once: c0a-ret10). 11EB98 / 3103F0's pose is the next tick's here (the gate is exact).
EMSCRIPTEN_KEEPALIVE void rider_pose_step(){
 if(browserNisHold)return;
 {auto step=gs;step.timeScale=1;graph.advance(step,prewind.spin.current,prewind.flip.current);graph.completeSequences();}
}
EMSCRIPTEN_KEEPALIVE void place_rider_region(float x,float y,float z,float dx,float dy,float dz,int transport);
// 11D390(rider)'s free-ride branch (the location entry of 1297C8(C, 1), pv eventReturnInWorld: WS15's 230180): 112180(rider, 1) then
// 11DE60 / 11DF18 at the location's row (place_rider_region). Today's other place_rider_region callers keep it without 112180.
EMSCRIPTEN_KEEPALIVE int location_entry_place(const char* bankText,int slot,float x,float y,float z,float dx,float dy,float dz){
 if(!start_row_reattach(bankText,slot,true))return 0;place_rider_region(x,y,z,dx,dy,dz,0);return 1;
}
// World state 14's enter 0x236250 with arg 2 (the results' Transport; pv eventReturnInWorld), the human's part: the boost meter +0x2F8
// = 0 (0x2362B4), then 11D390(human) (0x2363DC) while the event kind still holds, i.e. its event branch: 112180(human, 1) on the start
// row and the grid hold (11FE78(3) / 11FEC8(6)). PS2 c0a-ret7: +0xAB8 -> the bank's path 2 before the WS14 frame's 112338; c0a-ret6b:
// that frame's 1125C0 then writes +0x4CC = -2.9563, the heading WS15's record shows.
EMSCRIPTEN_KEEPALIVE int transport_map_enter(const char* bankText){
 boostState.meter=0;if(!start_row_reattach(bankText,0,true))return 0;grid_hold_enter();return 1;
}
EMSCRIPTEN_KEEPALIVE void place_rider_region(float x,float y,float z,float dx,float dy,float dz,int transport){
 browserNisHold=false; // 123B48 releases the NIS hold before 11D390
 if(browserResetActive)clear_reset();
 // 11DE60 is straight-line code: every caller runs its 11FEC8(0) / 11FE78(0) (0x11DED8 / 0x11DEE4), so a running crash ends here.
 // Control 8's exit (table 0x456B90 -> 12E690) is the wipeout speech 2A02D8 plus, on an even logic tick, 10E028(reaction 4), which
 // 11D660 below clears again (+0x358 = 0). Motion 2's exit (table 0x456B10 -> 136F28) is empty. PS2 c0a-ret record 3031 -> 3032:
 // the WS15 return's 1297C8 -> 11D390 of a rider mid-crash (2/8) gives 0/0 at the location row.
 if(physicsState.controlState==8)audio_event(AE_CRASH_EXIT);
 if(crash.active){crash.active=false;browserCrashActive=false;browserCrashExitFrame=false;}
 // ... and a finished rider's control 10 (the finish stop, finish_gameplay.inc) ends with it: 11FEC8(0) replaces the controller (control
 // 10 has no exit: 0x456B90[10] -> 0x111624). PS2 c0a-ret3: the human, in control 10 since the Give Up, is in control 2 on WS15's tick 1.
 // So does the grid hold's start controller (control 6, no exit either: world state 14's hold before a WS15 return, transport_map_enter).
 finish_reset();if(browserStarting)clear_start();
 if(transport){physicsState.normal=physicsState.previousNormal={0,0,1};const float limit=physicsProfile.speedLimit;physicsProfile=physicsMaterials[0];physicsProfile.speedLimit=limit;}
 push_score();originalScoreRunReset(scoreObject);pull_score();grab={};score_reset(true);
 start_ground_motion();const float depth1=physicsState.depth1,depth3=physicsState.depth3;
 reset_place_at({x,y,z},{dx,dy,dz},0.f,5);
 {const float limit=physicsProfile.speedLimit;physicsProfile=physicsMaterials[0];physicsProfile.speedLimit=limit;}physicsState.depth1=depth1;physicsState.depth3=depth3;
 {terrain_original::Rounding rounding;graph.setRate(2,0);reset_resume_velocity();}passive={};prewind={};air={};airAnimation={};
 boostState.window=physicsState.boostWindow=0;boostState.modifier=0;boostState.superTime=0;boostState.tier=physicsState.boostTierCounter=0; // 11D390 tail: +0x2E8, +0x2EC, +0x2F0, +0x2F4 = 0
 physicsState.controlState=gs.controlState=0;gs=physicsState;::grounded=true;previousGround=true;previousHeld=false;heldAirMode=passiveMode=false;
}
// A CTM event run in the streamed world (pv eventInWorld, docs/ctm-events-in-world.md stage 3): the race start's grid placement
// on the rider as free ride and WS1's hold left it. PS2: WS1's last tick 1289F0 = 1297C8(C, 0) places every rider, 11D390's event
// branch (0x11D564: 11FE78(3), 11FEC8(6), the tail +0x470 -1, +0x2E8 / +0x2EC / +0x2F0 / +0x2F4 / +0x3FC / +0x474.. 0, the vt call,
// 111890, 125038) does not call 11DE60 / 11DF18, and the Continue's 129768 -> 1297C8(C, 1) places them again with C+8 = 0.
// PS2 c0a-full (the Snow Jam first heat ridden in from free ride), rider +0x100..+0xB40 at the countdown against the Single Event
// anchor's (event-race record 18): equal except the words neither the hold nor the placement writes. The port's grid state is
// the anchor's (start_event), so this runs it and keeps those words from the current rider:
// - the motion-0 object's stamps (owner +0x10 ground focus, +0x14 last leave: the hold's motion-0 exit): 13C7A8 at the push-off
//   scales the velocity by 0.7 + 0.01 x (tick - leave - 40) (<= 1); the game tick restarts at 0, so a CTM start keeps 0.7
//   (c0a-race 182: the anchor's leave 0 gave 1, 160 cm/s fast);
// - the boost meter +0x2F8, amount +0x2FC and drain +0x304 (11D390 zeroes only +0x2E8..+0x2F4; c0c-race carries the qualifier's
//   0.621: WS13 has no cGame_restart);
// - +0x380 / +0x390 (the last contact normal and the board normal).
// - the retained speed limit +0x2E4 (after WS1's ~500 motion-3 ticks it is 11B3F8's motion-3 fixed point, the anchor's too).
// The stage world is kept too (start_event without browser_reset_pickups): no world reset at an offline event start.
// It resets the race session itself (reset_race, keeping the world): the page calls it instead of reset_race + reset_rider +
// start_event, after event_route_seed.
// Not carried (not modelled, or not read before they are rewritten in these runs): +0x360 (1 in CTM; the port has no cruise
// latch 0x1162C8), +0x3C0/C4, +0x3F8, the race-path cache +0x4DC..+0x4E8, +0x5B4,
// +0x764, +0x770, +0x9E0..+0xAE0. Used by the page instead of reset_rider + start_event; Single Event keeps start_event.
extern "C" void reset_rider(float,float,float,float); // web/core.cpp
extern "C" void reset_race(); // web/race_bridge.cpp
extern "C" void speed_limit_seed(float); // web/core.cpp
// The event's grid route (rider +0x490..+0x4CC): 11D390's 112180(rider, 1) re-attaches it at the course's grid row of the
// location's bank; the result is the event document's original_reset.event_route (the anchor's words: PS2 c0a-full's countdown
// +0x490 / +0x4C8 / +0x4CC equal ANIMATIONS/initial.json's). A streamed world's own document (init_animation) holds free ride's,
// so the in-world event loads its course's before event_grid_start (start_event copies it into the route).
EMSCRIPTEN_KEEPALIVE int event_route_seed(const char* text){const auto doc=animation_document(text);const auto& data=doc.at("original_reset");if(!data.contains("event_route"))return 0;
 initialEventRoute=read_reset_route(data.at("event_route"));eventRouteLoaded=true;return 1;}
EMSCRIPTEN_KEEPALIVE void event_grid_start(float x,float y,float z,float angle){
 const uint32_t leave=lastGroundLeave,focus=groundFocusTick;const auto meter=boostState.meter,amount=boostState.amount;const auto drain=boostState.drainEnabled;
 const auto previousNormal=physicsState.previousNormal,boardNormal=physicsState.boardNormal;const float limit=physicsProfile.speedLimit;
 browserEventWorldKept=true;reset_race();reset_rider(x,y,z,angle);start_event();browserEventWorldKept=false; // the race session too (the page calls no reset_race first)
 // The grid is on the ground (motion 3 over the anchor's contact). reset_rider's snap probes the course collision world, which a
 // streamed world does not load (a miss leaves grounded false: the animation side then ran 1211F8 a second time, crouch 0.2 at GO).
 ::grounded=true;airMotionThisTick=false;
 speed_limit_seed(limit);
 lastGroundLeave=leave;groundFocusTick=focus;boostState.meter=meter;boostState.amount=amount;boostState.drainEnabled=drain;physicsState.boost=amount;
 physicsState.previousNormal=previousNormal;physicsState.boardNormal=boardNormal;gs.previousNormal=previousNormal;gs.boardNormal=boardNormal;gs.boost=amount;
}
// pv eventInWorldAi (docs/ctm-events-in-world.md stage 4): a CTM event's computer rider at the countdown. The PS2 builds it fresh at
// gate + 2 (129E20), the approach NIS carries it (placed at its actor, then ticked held: +0x2E4 ramps under motion 3, +0x380 is the
// held contact's normal; PS2 c0a-full-ai 3130..3370), 1297C8(C, 0) at WS1's last tick and (C, 1) at the Continue place it on its grid
// row with 11D390, which keeps those words. So, as event_grid_start for the human: the Single Event start (npc_start_event: the anchor's
// grid seed) with the carried words put back. Single Event keeps npc_start_event.
EMSCRIPTEN_KEEPALIVE void npc_grid_start(){
 const uint32_t leave=lastGroundLeave,focus=groundFocusTick;const auto meter=boostState.meter,amount=boostState.amount;const auto drain=boostState.drainEnabled;
 const auto previousNormal=physicsState.previousNormal,boardNormal=physicsState.boardNormal;const float limit=physicsProfile.speedLimit;
 npc_start_event();
 ::grounded=true;airMotionThisTick=false;
 speed_limit_seed(limit);
 lastGroundLeave=leave;groundFocusTick=focus;boostState.meter=meter;boostState.amount=amount;boostState.drainEnabled=drain;physicsState.boost=amount;
 physicsState.previousNormal=previousNormal;physicsState.boardNormal=boardNormal;gs.previousNormal=previousNormal;gs.boardNormal=boardNormal;gs.boost=amount;
}
// A world start's fresh rider (docs/peak-mountain.md "Fresh rider at a world start"): the world load builds the rider (constructor
// 0x125EB8 over zeroed memory), so before its first placement 11D390 its +0x2E4 speed limit, +0x4CC route heading, +0x438 surface,
// +0x380 previous normal, +0x244 lean triplet and boost words +0x2E8..+0x2FC are 0, drain +0x304 1 (PS2 start-lodge, the state
// before tick 0). The port's rider starts from the world's glide seed (reset_rider: browserGroundState / browserBoostState), so a
// free-ride world start calls this after its last reset_rider, right before place_rider_region (web/free-ride.js placeRegion, pv
// freshRider). 11B3F8 then ramps the limit from 0 (L += 0.1 x (target - L): 220.9, 419.3, ...; 11DF18's 833.333 cm/s push is held
// to it), 13C948 reads heading 0 on tick 0, and the placement's 13C7A8 sets the controller depths +4/+8 from material 0 (bodyScale x
// 0.5 / 2.5042). Green Station with a neutral pad: exact through the lodge door (tick 393; capture peak1-green-start).
extern "C" void speed_limit_seed(float); // web/core.cpp
extern "C" void boost_state_seed(float,float,float,int,float,float,int); // web/score_gameplay.inc
EMSCRIPTEN_KEEPALIVE void fresh_rider_start(){
 browserNisHold=false; // a world start's rider is not held
 physicsProfile=physicsMaterials[0];speed_limit_seed(0.f);
 resetRoute.heading=physicsState.headingOffset=0;physicsState.previousNormal={0,0,0};hpLean244={};
 boost_state_seed(0,0,0,0,0,0,1); // +0x2E8..+0x2FC 0 and the +0x304 drain word 1 (11D390 keeps the meter and amount)
 gs=physicsState;
}
// pv eventInWorldAi: a CTM computer rider built at gate + 2 (129E20, constructor 0x125EB8 over zeroed memory): fresh_rider_start's words
// and the board normal +0x390 zero too (PS2 c0a-full-ai: 0 in every rider from 3131 to the race, 11D390 does not write it).
EMSCRIPTEN_KEEPALIVE void npc_fresh_rider(){fresh_rider_start();physicsState.boardNormal={0,0,0};gs.boardNormal={0,0,0};}
EMSCRIPTEN_KEEPALIVE float* route_info(){RIDER_LOCAL static float v[14];v[0]=resetRoute.pathIndex;v[1]=resetRoute.previousDistance;v[2]=resetRoute.currentDistance;v[3]=resetRoute.lateralDistance;v[4]=resetRoute.heading;v[5]=routeProgressUpdates;v[6]=resetRoute.cache.segment;v[7]=resetRoute.cache.distance;for(unsigned i=0;i<3;i++){v[8+i]=resetRoute.closestPoint[i];v[11+i]=resetRoute.lookaheadPoint[i];}return v;}
EMSCRIPTEN_KEEPALIVE float* reset_info(){RIDER_LOCAL static float v[9];v[0]=browserResetActive;v[1]=resetControl.progress;v[2]=resetPlacements;v[3]=resetCompletions;v[4]=resetReason;v[5]=resetRoute.pathIndex;v[6]=resetObservers;v[7]=resetRoute.currentDistance;v[8]=browserResetActive&&resetInputs.deviceEnabled&&resetInputs.deviceIndex>=0?originalResetFadeAlpha(resetControl.progress):0;return v;}
EMSCRIPTEN_KEEPALIVE float* upper_request_info(){RIDER_LOCAL static float v[12];v[6]=float(riderMask8C0>>16);v[7]=float(riderMask8C0&0xffff);v[8]=float(riderMask8C8>>16);v[9]=float(riderMask8C8&0xffff);v[10]=float(riderMask8D0);v[0]=upperRequest358;v[1]=float(upperRequestTick354);v[2]=float(controllerGround.logicTick);v[3]=idleSeconds;v[4]=graph.requestedSemantics[1];v[5]=graph.currentClass(1);return v;} // QA: 10E028 pending kind/tick, logic tick, 115D48 clock; [6..10] channel-1 masks +8C0/+8C8 (hi16, lo16), +8D0
EMSCRIPTEN_KEEPALIVE float* crash_air_contact_info(){return crash.lastAirContact.data();}
// QA: the ragdoll's air predictor (rider+0x788 layout order): hit xyz, heading xyz, normal xyz, patch, prediction pos/vel, integrated pos/vel, predicted/apex/elapsed/integrated time, limit, status.
EMSCRIPTEN_KEEPALIVE float* crash_trajectory_info(){RIDER_LOCAL static float v[32];const auto& t=crash.trajectory;unsigned n=0;auto put=[&](const std::array<float,3>& a){for(float x:a)v[n++]=x;};
 put(t.hitPosition);put(t.heading);put(t.normal);v[n++]=float(t.patchId);put(t.prediction.position);put(t.prediction.velocity);put(t.integrated.position);put(t.integrated.velocity);
 v[n++]=t.predictedTime;v[n++]=t.apexTime;v[n++]=t.elapsed;v[n++]=t.integratedTime;v[n++]=t.speedLimit;v[n++]=float(t.status);return v;}
EMSCRIPTEN_KEEPALIVE float* crash_info(){RIDER_LOCAL static float v[12];v[0]=crash.active;v[1]=crash.control.phase;v[2]=crash.motion.submode;v[3]=lastCrashSemantic;v[4]=crash.ticks;v[5]=crashSerial;v[6]=crash.actor.detached;v[7]=crash.control.recovery70;v[8]=browserCrashResetReason;v[9]=crashObservers;v[10]=lastCrashObserver;v[11]=lastCrashImpact;return v;}
EMSCRIPTEN_KEEPALIVE float* air_control_info(){RIDER_LOCAL static float v[22];v[0]=air.mode;v[1]=air.phase;v[2]=air.extended;v[3]=air.targetSpin;v[4]=air.targetFlip;v[5]=air.progressSpin;v[6]=air.progressFlip;v[7]=air.totalSpin;v[8]=air.totalFlip;v[9]=air.adjustSpin;v[10]=air.adjustFlip;v[11]=air.scoredSpin;v[12]=air.scoredFlip;v[13]=air.maxSpin;v[14]=air.maxFlip;v[15]=air.axisBlend;v[16]=air.holdSpin;v[17]=air.holdFlip;v[18]=air.inputAngle;v[19]=air.idleTime;v[20]=air.spinRate;v[21]=air.flipRate;return v;}
EMSCRIPTEN_KEEPALIVE float* air_exit_info(){return airExitInfo.data();}
EMSCRIPTEN_KEEPALIVE float* pose_controls_info(){return poseControls.data();}
EMSCRIPTEN_KEEPALIVE float* pose_translation(){return committedPoseTranslation.data();}
// Full source-space pose*bind matrices. Called only after animation/contact;
// caching avoids recomputing the same matrices for repeated render frames.
EMSCRIPTEN_KEEPALIVE int rider_skin_matrix_count(){return !sourceSkinBind.empty()&&sourceSkinBind.size()==cachedCrashWorld.size()?int(sourceSkinBind.size()):0;}
EMSCRIPTEN_KEEPALIVE float* rider_skin_matrices(){
 const unsigned count=rider_skin_matrix_count();if(!count)return nullptr;
 if(sourceSkinMatrixTick==animationTick&&sourceSkinMatrices.size()==count*16)return sourceSkinMatrices.data();
 sourceSkinMatrices.resize(count*16);const std::array<float,4> scale{graph.scale[0],graph.scale[1],graph.scale[2],1};
 for(unsigned i=0;i<count;++i){const auto& bone=cachedCrashWorld[i];const std::array<float,4> position{bone.position[0],bone.position[1],bone.position[2],1};auto pose=originalPoseMatrices(position,bone.rotation,scale);auto matrix=originalSkinPoseMatrix(pose.scaled,sourceSkinBind[i]);std::copy(matrix.begin(),matrix.end(),sourceSkinMatrices.begin()+i*16);}
 sourceSkinMatrixTick=animationTick;return sourceSkinMatrices.data();
}
// Online races (web/net/pose-codec.js): the authored body scale rider_skin_matrices applies (graph.scale), so a
// receiver rebuilds the palette from world_pose_bones exactly.
EMSCRIPTEN_KEEPALIVE float* rider_skin_scale(){RIDER_LOCAL static float v[3];for(unsigned i=0;i<3;++i)v[i]=graph.scale[i];return v;}
EMSCRIPTEN_KEEPALIVE int rider_skin_palette_count(){return rider_skin_matrix_count()?int(sourceSkinGroups.size()):0;}
EMSCRIPTEN_KEEPALIVE uint32_t* rider_skin_palette_indices(){return sourceSkinIndices.empty()?nullptr:sourceSkinIndices.data();}
EMSCRIPTEN_KEEPALIVE float* rider_skin_palette(){
 if(!rider_skin_palette_count())return nullptr;
 if(presentationFast){ // web/presentation_fast.hpp: the same pose x bind x weights, plain arithmetic
  if(sourcePaletteTick!=animationTick||sourcePalette.empty()||!sourcePaletteFast){
   const unsigned count=rider_skin_matrix_count();fastSkinBones.resize(count);const std::array<float,4> scale{graph.scale[0],graph.scale[1],graph.scale[2],1};
   for(unsigned i=0;i<count;++i){const auto& bone=cachedCrashWorld[i];fastSkinBones[i]=presentation_fast::skinMatrix({bone.position[0],bone.position[1],bone.position[2],1},bone.rotation,scale,sourceSkinBind[i]);}
   presentation_fast::skinPalette(fastSkinBones,sourceSkinGroups,sourcePalette);sourcePaletteTick=animationTick;sourcePaletteFast=true;
  }
  return sourcePalette[0].data();
 }
 if(sourcePaletteTick!=animationTick||sourcePalette.empty()||sourcePaletteFast){
  const auto* matrices=rider_skin_matrices();std::vector<OriginalSkinMatrix> bones(sourceSkinBind.size());std::memcpy(bones.data(),matrices,bones.size()*sizeof(OriginalSkinMatrix));
  sourcePalette=originalSkinPalette(bones,sourceSkinGroups);sourcePaletteTick=animationTick;sourcePaletteFast=false;
 }
 return sourcePalette[0].data();
}
EMSCRIPTEN_KEEPALIVE float* pose_physical(){return posedPhysical.data();}
EMSCRIPTEN_KEEPALIVE float* body_response_info(){
 RIDER_LOCAL static float values[11];std::fill(std::begin(values),std::end(values),0);if(!browserBodyResponse)return values;const auto&r=*browserBodyResponse;values[0]=r.accepted;values[1]=r.moved;values[2]=r.bounced;values[3]=bodyResponseAir;for(unsigned i=0;i<3;i++){values[4+i]=r.translationCm[i];values[7+i]=r.velocityCmps[i];}values[10]=r.closingSpeedCmps;return values;
}
EMSCRIPTEN_KEEPALIVE float* body_query_info(){
 RIDER_LOCAL static float values[14];std::fill(std::begin(values),std::end(values),0);if(!browserBodyQuery)return values;
 const auto& q=*browserBodyQuery;values[0]=1;values[1]=q.complete();values[2]=q.best.hit;values[3]=q.best.penetrationCm;values[4]=q.contacts;values[5]=q.candidates;values[6]=q.unsupportedInstances.size();values[7]=q.candidateLimitExceeded;
 for(unsigned i=0;i<3;i++){values[8+i]=q.best.normal[i];values[11+i]=q.best.pointCm[i];}return values;
}
EMSCRIPTEN_KEEPALIVE float* body_volume_info(){
 RIDER_LOCAL static float values[108];std::fill(std::begin(values),std::end(values),0);
 if(!browserBodyVolume)return values;const auto& v=*browserBodyVolume;
 values[0]=v.count;values[1]=v.broadRadiusCm;
 for(unsigned i=0;i<3;i++){values[2+i]=v.broadCenterCm[i];if(v.landingCenterCm)values[5+i]=(*v.landingCenterCm)[i];}
 for(unsigned i=0;i<v.count;i++){values[8+i*5]=v.spheres[i].bone;values[9+i*5]=v.spheres[i].radiusCm;for(unsigned k=0;k<3;k++)values[10+i*5+k]=v.spheres[i].centerCm[k];}
 return values;
}
EMSCRIPTEN_KEEPALIVE float* animation_inputs(){
 RIDER_LOCAL static float values[17];for(unsigned i=0;i<3;i++){values[i]=gs.velocity[i];values[i+3]=gs.normal[i];}
 values[6]=gs.turn.current;values[7]=gs.turn.target;values[8]=gs.brake.current;values[9]=gs.crouch.current;values[10]=air.axisBlend;for(unsigned i=0;i<3;i++)values[14+i]=currentPivot[i];values[12]=air.spinRate;values[13]=lastLandingSpin;values[11]=0;for(const auto& sequence:graph.sequences)if(sequence.channel==2&&sequence.semantic==graph.requestedSemantics[2]){values[11]=sequence.rate;break;}return values;
}
EMSCRIPTEN_KEEPALIVE float* animation_info(){return info;}
}

extern "C" {
// Debug view of the live sequence lists: count, then per sequence
// channel,semantic,priority,rate,weight,target,fadeRemaining,stop,completed,seek,slots and 3x(clip,time,duration,weight,loop).
EMSCRIPTEN_KEEPALIVE float* animation_sequences_info(){
 RIDER_LOCAL static std::vector<float> values;values.assign(1,0);
 for(const auto& q:graph.sequences){values.insert(values.end(),{float(q.channel),float(q.semantic),float(q.priority),q.rate,q.weight,q.targetWeight,q.fadeRemaining,float(q.stopWhenFaded),float(q.completed),float(q.seekPending),float(q.slots.size())});for(unsigned k=0;k<3;++k){if(k<q.slots.size()){const auto&l=q.slots[k];values.insert(values.end(),{float(l.clip),l.time,l.duration,l.weight,float(l.loop)});}else values.insert(values.end(),{0,0,0,0,0});}}
 values[0]=float(values.size()-1);return values.data();
}
EMSCRIPTEN_KEEPALIVE float* tmp_debug(){for(int k=0;k<8;k++)tmpDebug2[k]=tmpBegin[k];return tmpDebug;} //TMPDEBUG
EMSCRIPTEN_KEEPALIVE float* tmp_begin(){return tmpBegin;} //TMPDEBUG
EMSCRIPTEN_KEEPALIVE float* tmp_pre_contact(){RIDER_LOCAL static std::vector<float> v;v.clear();for(auto&b:tmpPreContact){v.insert(v.end(),b.position.begin(),b.position.end());v.insert(v.end(),b.rotation.begin(),b.rotation.end());}return v.data();} //TMPDEBUG
// QA only: per-sequence root (+0x60/+0x70) and mirror (+0x80) as [channel, semantic, px,py,pz, qx,qy,qz,qw, mirror] (compare-ps2-capture SEQ_ROOTS).
EMSCRIPTEN_KEEPALIVE float* animation_sequence_roots(){
 RIDER_LOCAL static std::vector<float> values;values.assign(1,0);
 for(const auto& q:graph.sequences){values.insert(values.end(),{float(q.channel),float(q.semantic)});values.insert(values.end(),q.root.position.begin(),q.root.position.end());values.insert(values.end(),q.root.rotation.begin(),q.root.rotation.end());values.push_back(float(q.mirror));}
 values[0]=float(values.size()-1);return values.data();
}
// QA only: the shared original RNG words (0x4FF030 layout) for capture audits.
EMSCRIPTEN_KEEPALIVE uint32_t* animation_rng_words(){return rng.words.data();}
EMSCRIPTEN_KEEPALIVE void animation_rng_defer(int draws){rng.deferred=std::max(draws,0);}
// Split cursors for the six-rider world (ai-racers.js): enable, then [controller draws, motion draws, motion started].
EMSCRIPTEN_KEEPALIVE void animation_rng_split(int enabled){rng.split=enabled!=0;rng.motionStarted=false;rng.controllerCount=rng.motionCount=0;}
EMSCRIPTEN_KEEPALIVE uint32_t* animation_rng_split_info(){RIDER_LOCAL static uint32_t v[3];v[0]=rng.controllerCount;v[1]=rng.motionCount;v[2]=rng.motionStarted;return v;}EMSCRIPTEN_KEEPALIVE int animation_rng_pending(){return rng.deferred;}
EMSCRIPTEN_KEEPALIVE float* sampled_local_pose(){
 RIDER_LOCAL static std::vector<float> values;values.clear();
 if(graph.sampledLocal)for(const auto& bone:*graph.sampledLocal){values.insert(values.end(),bone.position.begin(),bone.position.end());values.insert(values.end(),bone.rotation.begin(),bone.rotation.end());}
 return values.data();
}
}

extern "C" {
EMSCRIPTEN_KEEPALIVE float* source_motion_audit(){
 RIDER_LOCAL static float values[20];
 for(unsigned i=0;i<3;i++){values[i]=physicsState.position[i];values[3+i]=physicsState.velocity[i];values[10+i]=crash.actor.position[i];values[13+i]=crash.actor.velocity[i];}
 for(unsigned i=0;i<4;i++){values[6+i]=physicsState.quaternion[i];values[16+i]=crash.actor.quaternion[i];}
 return values;
}
}

extern "C" { EMSCRIPTEN_KEEPALIVE float* rail_gameplay_info(){RIDER_LOCAL static float v[8];v[0]=browserRailActive;v[1]=railOwned;v[2]=railAttachments;v[3]=railExits;v[4]=railTicks;v[5]=railMotion.balance;v[6]=railMotion.railId;v[7]=railObservers;return v;} }

extern "C" { EMSCRIPTEN_KEEPALIVE void rail_preinput(float flip){railFlipInput=flip;} EMSCRIPTEN_KEEPALIVE float* rail_jump_info(){RIDER_LOCAL static float v[3];v[0]=railHeldJump;v[1]=railReleaseFrame;v[2]=railJumps;return v;} }

extern "C" { EMSCRIPTEN_KEEPALIVE void rail_rotation_input(float value){if(!std::isfinite(value))throw std::runtime_error("Nonfinite rail rotation input");railRotationInput=std::clamp(value,-1.f,1.f);} EMSCRIPTEN_KEEPALIVE float* rail_rotation_info(){RIDER_LOCAL static float v[4];v[0]=railRotations;v[1]=physicsState.prewindStyle;v[2]=railControl.spin;v[3]=graph.defaultMirror;return v;} }

extern "C" { EMSCRIPTEN_KEEPALIVE float* rail_score_info(){RIDER_LOCAL static float v[8];v[0]=trickIdentity.time24;v[1]=scoring.accumulated14;v[2]=scoreExtras.inverted1C;v[3]=railBankedTotal;v[4]=railScoreCommits;v[5]=railScoreBonuses;v[6]=railAwardedMeter;v[7]=deferredScore;return v;} }

extern "C" { EMSCRIPTEN_KEEPALIVE float* boost_hud_info(){RIDER_LOCAL static float v[13];v[0]=boostHudDisplay.preview;v[1]=boostHudDisplay.stored;v[2]=boostLetterDisplay.count;v[3]=boostLetterDisplay.fraction;v[4]=boostLetterDisplay.removed;v[5]=boostPendingDisplay.count;v[6]=boostPendingPhase;v[7]=boostPendingDisplay.removed;v[8]=boostFlashPhase;v[9]=boostFlashDisplay.fraction;v[10]=boostFlashDisplay.present;v[11]=boostPendingDisplay.fraction;v[12]=boostFlashDisplay.paletteTier;return v;} }

extern "C" EMSCRIPTEN_KEEPALIVE const char* trick_name(){
 RIDER_LOCAL static std::string preview;
 if(originalCurrentTrickPoints(scoring)<=0)return committedTrickName.c_str();
 auto copy=trickIdentity;copy.grabs60=scoring.history60;
 OriginalTrickIdentityInput input{!physicsState.state320Equals324,false,0,0,reference_stance()};
 auto result=originalTrickIdentity(copy,commitProfile.identity,input);
 if(!result.valid){preview.clear();return preview.c_str();}
 originalNamedTrickBonus(result.identity,commitProfile.named);preview=originalTrickName(result.identity,trickNameTables);return preview.c_str();
}

//Isolated QA of the production animation graph; it never changes the rider graph.
extern "C" EMSCRIPTEN_KEEPALIVE float* rail_uber_balance_probe(int semantic,float amount,int reverse){
 if(semantic!=218&&semantic!=226&&semantic!=234&&semantic!=242)throw std::runtime_error("Not an authored Uber balance semantic");
 auto copy=graph;auto state=gs;state.reverseStance=reverse!=0;copy.railBalance=amount;
 if(!copy.enter(semantic,1,~uint64_t(0),true))throw std::runtime_error("Uber balance driver cannot enter");
 auto selected=[&]() -> OriginalAnimationSequence& {auto found=std::find_if(copy.sequences.begin(),copy.sequences.end(),[&](const auto&s){return s.semantic==semantic;});if(found==copy.sequences.end())throw std::runtime_error("Missing Uber probe sequence");return *found;};
 copy.advance(state,0,0);auto&sequence=selected();auto first=sequence.slots.front();
 RIDER_LOCAL static float result[7];result[0]=first.clip;result[1]=first.time;result[2]=first.duration;result[3]=first.weight;result[4]=sequence.seekPending;
 copy.advance(state,0,0);result[5]=selected().slots.front().time;result[6]=selected().slots.front().clip;return result;
}

extern "C" EMSCRIPTEN_KEEPALIVE float* start_animation_probe(float pose){
 auto copy=graph;auto state=gs;copy.startPose=pose;
 if(!copy.enter(0,1,~uint64_t(0),true))throw std::runtime_error("Original start animation missing");
 auto selected=[&]() -> OriginalAnimationSequence& {auto found=std::find_if(copy.sequences.begin(),copy.sequences.end(),[](const auto&s){return s.semantic==0;});if(found==copy.sequences.end())throw std::runtime_error("Start sequence missing");return *found;};
 copy.advance(state,0,0);auto first=selected().slots.front();RIDER_LOCAL static float out[4];out[0]=first.clip;out[1]=first.time;out[2]=first.duration;copy.advance(state,0,0);out[3]=selected().slots.front().time;return out;
}
// Career attributes (web/rider_attributes.hpp): 0x1495A8 spin -> air control, 0x149690 tricks -> 0x120038 grab rate.
void browser_apply_animation_attributes(){if(!browserAttributesSet)return;airProfile.trickStat=browserAttributeStat(4);grabProfile.playbackRate=originalGrabPlaybackRate(browserAttributeStat(2));}

// QA (ground_state_dump): controller triplets owned by the animation side, as (rider offset, value) pairs.
static void animation_state_dump(std::vector<float>& out){
 auto t3=[&](unsigned off,const GroundControlValue& t){out.push_back(float(off));out.push_back(t.current);out.push_back(float(off+4));out.push_back(t.rate);out.push_back(float(off+8));out.push_back(t.target);};
 t3(0x244,hpLean244);t3(0x2A4,prewind.spin);t3(0x2B0,prewind.flip);
}
// A computer rider's relationship row (0x155B50 levels for 10DBF0 attacks) after an in-race 0x155BF0 change (web/lineup.js).
extern "C" EMSCRIPTEN_KEEPALIVE void npc_set_relationships(const float* row){for(unsigned k=0;k<6;++k)npc.relationship[k]=int(row[k]);}
// Static-initialisation work of this translation unit, per rider context (web/rider_local.hpp, web/rider_context.cpp):
// construct its RIDER_LOCAL_LAZY state (graph, crash, trail, sparks, containers; the first use runs them all, in
// declaration order) and register the controller callbacks, in their original order.
void rider_statics_animation(){
 rider_touch(&graph);
 crash.host.instanceContacts=crash_instance_contacts;setup_handplant();setup_attack();setup_board_press();browserFinishStep=finish_step;
}
static const bool riderStaticsAnimationReady=(rider_statics_animation(),true);
// ---- 1057B8's air landing on a scenery top (web/instance_contact_gameplay.inc) --------------------------------------------
// Motion 1, channel-2 class 1/2/9/11, presentation up . n > 0.3: after the tangential velocity rebuild and the peak impact,
// unless clip 268 already plays past 0.2: 10E910(rider, 0, 0, 0, hit, speed), 119E38(score, stanceDiffers, 0) (its meter
// return is dropped), 11FEC8(13) (the old control's exit), 11FEC8(5) (133128: +0x2DC / +0x2E0 from the prewind +0x2A4 /
// +0x2B0), 3128E8(anim, 268, 1, -1). PS2 peak3/fr-throne-unload 15035: a lost rail's passive exit (132770 -> control 4) lands
// on the ice rail's scenery the same tick and enters control 5, so the 15037 re-attach runs 134CB0; peak3/the-throne-tuck 4903:
// from control 5, whose exit 134CB0 bakes the presented spin into the root.
static float surface_landing_channel_time(){ // 312AB0(anim, 2): the channel's newest sequence, time / duration (EE div.s)
 for(const auto& s:graph.sequences)if(s.channel==2&&!s.slots.empty())return collision_scalar::divide(s.slots[0].time,s.slots[0].duration);
 return 0.f;
}
static void surface_landing_award(float speed,const OriginalInstanceContactPacket&){
 auto award=score_landing_args(0,0,0);audio_event(AE_LANDING,award.meterDelta,audioImpact770,0);audio_event(AE_RUMBLE_IMPACT,speed*0.5f); /*10EAD8 rumble*/
 banked=std::bit_cast<int32_t>(uint32_t(banked)+uint32_t(award.points));if(award.points>0)committedTrickName=originalTrickName(award.identity,trickNameTables); /*the landing HUD, as 139C88's*/
}
static void surface_landing_boundary(bool stanceDiffers){
 auto state=capture_score();auto result=originalScoreTakeoffBoundary(state,int(stanceDiffers),0,score_commit_raw);apply_score(state);queue_rail_score(result);
}
static void surface_landing_control(int control){
 if(control==13){ // 111538: the old control's exit first: control 5's 134CB0 (the pivot bake, prewind reset), controls 0 / 4 131C30 / 12FB68
  if(gs.controlState==5&&!passiveMode&&!heldAirMode&&landing_air_exit())landingAirExitBaked=false;
  attack_control_changes(13);gs.controlState=13;if(physicsAttached)physicsState.controlState=13;return;}
 if(control!=5)throw std::runtime_error("Unexpected surface landing control");
 passiveMode=heldAirMode=false;passiveDeparture=0;gs.controlState=5;air=originalAirControlBegin(prewind.spin.current,prewind.flip.current);
 if(physicsAttached){physicsState.controlState=5;physicsState.manualSpin=air.spinRate;}
 attack_control_changes(5);
 // The pose of this tick (11E150) ran before this post stage in the old control: the next 134CB0 / 134DD0 bake (a re-attach
 // or landing from control 5) turns about that pose's animated pivot, which the passive / held pose left at zero here
 // (PS2 fr-throne-unload 15036: the attach bake moves the root 1 ulp).
 if(graph.sampledLocal&&graph.sampledLocal->size()>pivotBone){currentPivot=graph.sampledLocal->at(pivotBone).position;terrain_original::Rounding rounding;for(unsigned k=0;k<3;++k)currentPivot[k]=terrain_original::mul(currentPivot[k],graph.scale[k]);}
}
// ---- the game tick 1298C8 (rider manager +8) and a peak run's location-crossing restart ----------------------------------
// motionTick is the port's 1298C8 (the ground leave / focus stamps, 13C7A8's landing speed scale, the scope refresh every
// third tick, the streamers' even frames). The original restarts it at a Rival Challenge peak run's location crossing: the
// Unload's 22DF50 requests world state 11 and the background state 10 (231278), whose update 235080 waits for the location's
// NIS script read (278D58), then reloads the rider list (sub-states 1..3) and in sub-state 3 calls 128A10 (event type 5 in
// Conquer the Mountain; 1289F0 for type 0) -> 1297C8(rider manager, 0): +8 = 0 (the race clock +0xC runs on). PS2: ARMSX2
// entry probe of 1297C8 on apr-full's crossing state (ra 0x128A28, stack 0x235504), docs/peak3.md.
void browser_section_tick_restart(uint32_t value); // web/section_gameplay.inc
extern "C" void browser_race_total_ticks_restart(int32_t value); // web/race_bridge.cpp (inside its extern "C" block): the race clock's copy of the same word
void browser_game_tick_restart(uint32_t value){motionTick=value;browser_section_tick_restart(value);browser_race_total_ticks_restart(int32_t(value));}
extern "C" EMSCRIPTEN_KEEPALIVE void game_tick_restart(uint32_t value){browser_game_tick_restart(value);}
extern "C" EMSCRIPTEN_KEEPALIVE uint32_t game_tick(){return motionTick;}
#include "stage_teleport.inc" // stage builtin 34 rider side 0x123210 (docs/stage-teleport.md; after the pose state it writes)
// QA: one instance's collision / stage state by resource: [stage flags known, stage flags, body runtime flags set,
// body runtime flags, body authored flags, stage entity type] (the body's eventRuntimeFlags route the collision queries).
extern "C" EMSCRIPTEN_KEEPALIVE uint32_t* stage_instance_flags(uint32_t resource){
 RIDER_LOCAL static uint32_t v[6];std::fill(std::begin(v),std::end(v),0u);
 if(const auto* st=stage_instance(resource)){v[0]=st->flagsKnown;v[1]=st->flags;}
 if(auto* w=set_piece_instance_at(resource)){v[2]=w->eventRuntimeFlags.has_value();v[3]=w->eventRuntimeFlags.value_or(0);v[4]=w->flags;}
 v[5]=uint32_t(stage_entity_type(resource));return v;
}
#ifdef SSX_SNAPSHOT_REGISTRY // the rider-context snapshot's registry (web/generate-snapshot-registry.mjs, docs/replay.md §2a)
#include "generated/snapshot/animation_bridge.inc"
#endif
