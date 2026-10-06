#include "rider_local.hpp"
#include "rail_bridge.hpp"
#include "../engine/collision.hpp"
#include <emscripten/emscripten.h>
#include <memory>
#include "generated/physics_seed.hpp"
#include "generated/event_start_seed.hpp"
#include "rider_attributes.hpp" // career attributes -> stat fields (web/attribute_bridge.cpp)
#include "../engine/jump_motion.hpp"
#include "../engine/pickup_reward.hpp"
#include "../engine/air_motion.hpp"
#include "../engine/orientation_motion.hpp"
#include "../engine/original_camera.hpp"
#include "../engine/original_camera_director.hpp"
#include "../engine/original_camera_words.hpp" // DEFAULT_3 0x390-byte object <-> words (capture seeding/compare)
#include "../engine/camera_transform.hpp"
#include "../engine/rider_query_bounds.hpp"
#include "../engine/air_alignment.hpp"
#include "../engine/ground_pose_motion.hpp"
#include "../engine/ground_surface.hpp"
#include "../engine/collision_event.hpp"
#include "../engine/soft_collision_control.hpp"
#include "../engine/air_entry.hpp"
#include "../engine/landing_contact.hpp"
#include "../engine/crash_motion.hpp"
#include "../engine/reset_placement.hpp"
#include "../engine/original_input.hpp"
#include "../engine/patch_state.hpp"
#include "../engine/wind_push.hpp"
using namespace ssx;
#include "audio_events.hpp" // audio observers (web/audio_events.inc)
RIDER_LOCAL extern OriginalAirTrajectory browserTrajectory;RIDER_LOCAL extern bool browserPredictionAvailable;
void reseed_prediction(OriginalAirState);
void reset_prediction();void begin_prediction(OriginalAirState);bool advance_prediction(OriginalAirState&);
void browser_score_takeoff(bool ramp); // 114298 -> 119E38 + 10E098 (web/score_gameplay.inc)
std::unique_ptr<CollisionWorld> world;
std::unique_ptr<CollisionWorld> cameraTerrain;
RIDER_LOCAL Vec3 position,velocity,normal{0,1,0},spawn;
RIDER_LOCAL double yaw=0,startYaw=0,charge=0,airtime=0,distanceRun=0;
RIDER_LOCAL std::optional<std::array<float,3>> groundUnclampedVelocity; // this tick's ground velocity before 13F358
RIDER_LOCAL std::optional<std::array<float,3>> groundBoardNormalBefore; // +0x390 before this tick's 13F2E0 update (the post stage may undo it)
RIDER_LOCAL bool railStepConsumed=false;
RIDER_LOCAL bool grounded=true,held=false,airbornePrewind=false;RIDER_LOCAL bool airMotionThisTick=false; RIDER_LOCAL int landed=0,respawns=0;
RIDER_LOCAL float output[16];
RIDER_LOCAL OriginalJumpState lastChargedTakeoff;
RIDER_LOCAL std::array<float,3> browserLandingTranslation{};
RIDER_LOCAL float physicsInfo[7];RIDER_LOCAL float effectiveSpeedLimit=0;RIDER_LOCAL static bool speedLimitHeld=false;
// Computed initial values (the course seeds) are assigned by rider_statics_core() below, for every rider context.
RIDER_LOCAL OriginalGroundProfile physicsProfile{};
RIDER_LOCAL decltype(browserGroundMaterials()) physicsMaterials{};
RIDER_LOCAL OriginalGroundState physicsState{};
RIDER_LOCAL int browserGroundPatch=-1;RIDER_LOCAL float browserGroundU=0,browserGroundV=0;
// rider+0x2D4: runtime flags (patch+0xA = authored|0x40) of the last contacted terrain patch, written by the
// ground contact 13D1B8 (13D604). 0x20 keeps a vertical-wall takeoff at full speed (114A5C), 0x10 forces heading boost (13C9CC).
RIDER_LOCAL uint16_t browserPatchFlags=0x49;
RIDER_LOCAL std::array<float,3> browserDeparturePush{}; // this tick's 114298 passive-departure position change (post-pose 13F178)
RIDER_LOCAL std::optional<std::array<float,3>> browserDeparturePrePosition; // rider+0x110 before that change: the pose reads it (sub(new, push) can miss it by an ulp)
// 13F178 (post-pose ground stage) without a departure: surface property +0x44 != 0 (13F23C) or patch flag 0x2 in
// rider+0x2D4 (13F248) requests the reset 116120(rider,0,1) before 13F488/105398 (The Junction 0xB/0x3 patches).
RIDER_LOCAL bool browserGroundResetPending=false;RIDER_LOCAL bool browserGroundCrashPending=false; // 13F1C8: a ground contact on surface 18 (web/animation_bridge.cpp post)
RIDER_LOCAL static bool airControl0Request=false; // first airborne tick of control 0 (it only requests control 4)
RIDER_LOCAL static bool wasAirControl0=false; // step_rider's control 0 in the air last tick (file scope: the rider-context snapshot restores it)
RIDER_LOCAL static bool releasedLastTick=false; // last tick released a charged jump on the ground (12E9B8 -> 114298, control 5 requested)
RIDER_LOCAL bool browserCrashAir=false; // crash motion 2 submode 1 (publish_crash_actor), for the 1210B0 crash-air timers
RIDER_LOCAL int browserCrashSurface=0; // crash actor surface: motion 2 contacts write rider+0x438 (camera surfaceId, pipe-air 793)
// rider+0x434 (physicsState.riderType): location id of the contacted patch. 1218D0 (per-rider pass from 128AC0, after
// motion) calls 22E0E0 with the track byte of rider+0x430 (= patch+0x150 = rid<<8|track, the web patch resource): the index
// of the streaming-table 0x442168 row whose +4 (loaded track slot) matches, 50 when none. The index is the ELF location
// table 0x43E250 id (ARA1 0, BRA2 1, BHP1 11 .. 13 half pipes, 17+ hubs/connectors). 0x31 = constructor 11B718 default.
// 13C948 (>=17 halves auto boost, 11..13 scales it by |normal.z|) and 114298 (11..13 skip the ground-focus scale) read it.
static const char* const browserLocationNames[50]={"ARA1","BRA2","CRA3","DRA4","ERA5","ASS1","DSS2","ESS3","ABA1","CBA2","EBA3","BHP1","CHP2","EHP3","ABC1","DBC2","EBC3","A","B","C","D","E","EBC3_E","E_ERA5","E_ESS3","E_EHP3","E_EBA3","ERA5_C","C_CRA3","C_CHP2","C_CBA2","CRA3_D","DBC2_D","D_DRA4","D_DSS2","DRA4_A","ABC1_A","A_ARA1","A_ASS1","A_ABA1","ARA1_B","B_BRA2","B_BHP1","TRANSP","ASKY","BSKY","CSKY","DSKY","ESKY","dbg"};
RIDER_LOCAL static std::array<int32_t,kOriginalStreamingRows> browserStreamingTracks=[]{std::array<int32_t,kOriginalStreamingRows> a;a.fill(-1);return a;}(); // 0x442168 row +4
void browser_set_track_location(int track,const std::string& name){
 if(track<0||track>255)throw std::runtime_error("Track slot out of range");
 for(int k=0;k<50;k++)if(name==browserLocationNames[k]){browserStreamingTracks[k]=track;return;}
 throw std::runtime_error("Unknown location "+name);
}
void browser_clear_track_locations(){browserStreamingTracks.fill(-1);}
// A streamed location left the world (web/peak_world.inc): its 0x442168 row no longer holds a track.
void browser_unset_track_location(const std::string& name){for(int k=0;k<50;k++)if(name==browserLocationNames[k]){browserStreamingTracks[k]=-1;return;}throw std::runtime_error("Unknown location "+name);}
RIDER_LOCAL std::array<float,3> browserTrailContact{};
RIDER_LOCAL float browserLandingSpeed=0; // 139D54: |rider+0x1E0| before the landing response, the 10E910 -> 111AA0 snow impact strength
RIDER_LOCAL std::array<float,3> browserBoardNormalForPose{};
RIDER_LOCAL OriginalBoostProfile boostProfile{};
RIDER_LOCAL OriginalBoostState boostState{};
RIDER_LOCAL bool boostHeld=false;RIDER_LOCAL float boostInfo[8];
RIDER_LOCAL OriginalLandingProfile landingProfile{};
RIDER_LOCAL uint32_t motionTick=0,lastGroundLeave=0;
// Host-only: the unposed headless landing fallback waits ten ticks after any departure.
RIDER_LOCAL uint32_t hostDepartureTick=0;
RIDER_LOCAL float landingContactInfo[14]{};RIDER_LOCAL float landingInfo[6]{};RIDER_LOCAL float groundContactInfo[6]{};RIDER_LOCAL terrain_original::ContactCache groundCache;
// Rider query scope (rider+0x860). 12B788 -> 120E50 -> 332DB8 rebuilds it from the query bounds +0x400/+0x410
// on the world job, once every three game ticks: the refresh follows each tick whose record tick is a multiple of 3
// (rider+0x79C.. light list changes land only on those records in every capture) and uses that tick's completed
// bounds. The rider's queries in between see the older scope; an inverted rider's landing probe (200cm on the head
// side) leaves it (pipe-tricks 1361..1362). Empty until the first refresh after a reset (whole world).
RIDER_LOCAL std::optional<terrain_original::RiderScope> riderScope;
void refresh_rider_scope(){const auto basis=originalOrientationBasis(physicsState.quaternion);const auto& p=physicsState.position;const auto& r=basis.right;const auto& f=basis.forward;const auto& u=basis.up;
 const auto b=originalRiderQueryBounds({p[0],p[1],p[2],1},{r[0],r[1],r[2],0},{f[0],f[1],f[2],0},{u[0],u[1],u[2],0});riderScope=terrain_original::RiderScope{{b.minimum[0],b.minimum[1],b.minimum[2]},{b.maximum[0],b.maximum[1],b.maximum[2]}};}
// World state 14's rider actor hold (123640 -> 123B48; docs/ctm-parity.md "The NIS rider hold"): rider+0xAC4. nis_hold below.
RIDER_LOCAL bool browserNisHold=false;
RIDER_LOCAL bool nisProbeContact=false;RIDER_LOCAL float nisProbeSpeed=0;RIDER_LOCAL int nisProbeMode=0;RIDER_LOCAL std::array<float,3> nisProbeBone{}; // rider+0xB00 / +0xB04: 1242B0's contact flag and last speed (nis_hold_ground_probe)
RIDER_LOCAL bool browserResetActive=false;RIDER_LOCAL bool browserResetFromController=false;RIDER_LOCAL void (*browserResetBegin)(int)=nullptr;RIDER_LOCAL void (*browserResetDecline)()=nullptr;RIDER_LOCAL void (*browserResetRerequest)(int)=nullptr;RIDER_LOCAL bool (*browserResetControl)()=nullptr;RIDER_LOCAL void (*browserResetClear)()=nullptr;
RIDER_LOCAL bool browserPosedLandingEnabled=false,browserCrashActive=false,browserCrashExitFrame=false;RIDER_LOCAL int browserCrashResetReason=0;
RIDER_LOCAL void (*browserCrashReset)()=nullptr;RIDER_LOCAL bool (*browserCrashControl)(bool)=nullptr;RIDER_LOCAL void (*browserCrashMotion)()=nullptr;
// 10EB30(rider, semantic, attacked, impact type, event): `attacked` is 107E70's a3 (only the 107888 attack branch passes 1).
RIDER_LOCAL void (*browserHardCrash)(int,const OriginalCollisionEvent&,bool attacked)=nullptr;
RIDER_LOCAL bool (*browserLandingCrash)(const OriginalLandingState&)=nullptr;
RIDER_LOCAL extern std::unique_ptr<WorldBodyCollision> browserBodies;
RIDER_LOCAL static std::array<float,17> landingProbeInfo{};RIDER_LOCAL static uint32_t lastPosedContactTick=0xffffffffu;
RIDER_LOCAL bool (*browserLandingAirExit)()=nullptr;
RIDER_LOCAL void (*browserRestoreStance)(int)=nullptr;RIDER_LOCAL void (*browserReverseAnimation)(std::array<float,4>)=nullptr;RIDER_LOCAL uint32_t reverseTurnSerial=0;
RIDER_LOCAL bool browserSoftActive=false,browserSoftFrame=false;RIDER_LOCAL static bool softComplete=false;RIDER_LOCAL static int softTransition=-1;
RIDER_LOCAL OriginalCollisionProfile collisionProfile;RIDER_LOCAL OriginalCollisionHistory collisionHistory;RIDER_LOCAL std::optional<OriginalCollisionReaction> collisionReaction;RIDER_LOCAL uint32_t collisionSerial=0;RIDER_LOCAL bool collisionRecoveryRequested=false;
RIDER_LOCAL uint32_t groundFocusTick=0; //motion owner+0x10, stamped by ground enter 13C7A8
// The computed initial values of this file's rider state, in declaration order (they were dynamic initialisers of plain
// globals). Run for the human's context at static initialisation (below) and for every new rider context by
// web/rider_context.cpp, whose fresh course seeds (browserGlideLocation "ARA1") give a fresh core's values.
void rider_statics_core(){
 physicsProfile=browserGroundProfile();physicsMaterials=browserGroundMaterials();physicsState=browserGroundState();
 browserBoardNormalForPose=physicsState.boardNormal;boostProfile=browserBoostProfile();boostState=browserBoostState();landingProfile=browserLandingProfile();
 motionTick=browserLandingTick;lastGroundLeave=browserLastGroundLeave;groundFocusTick=browserGroundFocusTick;
}
static const bool riderStaticsCoreReady=(rider_statics_core(),true);
// Pause Options camera select persists for the session (gp+0x1CE0); 0x3D Mid is the boot default.
RIDER_LOCAL int cameraViewType=0x3D,pendingCameraView=0;
// Camera director (0x161BB8) + outer compositor (0x15E668): DEFAULT_2/3/4 chase and POST_RACE_1 fades.
RIDER_LOCAL OriginalDirectedCameraState chase;RIDER_LOCAL bool chaseReady=false,pendingCameraFinish=false;RIDER_LOCAL static bool eventCameraSeedArmed=false;static void arm_event_camera_seed(); // event camera seed
// Camera cuts requested by the rider code (15CCF0 set-target on every director node), applied at the next camera step.
RIDER_LOCAL static int cameraSetTargets=0;void browser_camera_set_targets(int count){if(count>0)cameraSetTargets+=count;}
RIDER_LOCAL int32_t cameraRaceRiders=1;
RIDER_LOCAL_LAZY static OriginalCameraInput cameraLastInput;RIDER_LOCAL static bool cameraLastInputSet=false; // this tick's camera input, for the replay view (web/replay_camera.inc)
RIDER_LOCAL float cameraOut[9];RIDER_LOCAL std::array<float,16> cameraRenderView{};RIDER_LOCAL float cameraInputs[22];RIDER_LOCAL float cameraSourceInput[38];RIDER_LOCAL float cameraLaunchValue=0;RIDER_LOCAL std::array<float,3> cameraWallNormal{};
void retain_jump_camera(const OriginalJumpState& jump){cameraLaunchValue=jump.cameraLaunchValue;cameraWallNormal=jump.cameraWallNormal;}
RIDER_LOCAL float pendingCrashCameraShake=0;RIDER_LOCAL unsigned crashCameraShakeRequests=0;
extern OriginalRandomState* browser_camera_random();
void browser_section_viewer(float x,float y,float z,float far); // web/section_gameplay.inc: texture-chunk viewer (outer camera +0x20 eye, +0x08 far)
extern float browser_camera_crouch();
extern "C" float browser_finish_elapsed(); // web/race_bridge.cpp: rider+0x470
extern void snapshot_animation_prediction();void browser_ground_controller_animation(int,float,float,float,float,float);void browser_ground_upper_reactions(int);int browser_channel2_class();int browser_channel2_semantic();RIDER_LOCAL int browserPrewindBranch=0; // 12E9B8 held branch (animation_bridge.cpp select_ground_animation)
extern void browser_fog_step(float,float,float,float);void browser_weather_camera_step(float,float,float); // web/weather.inc (0x15EBCC: the camera's Weather painter, block 6)
// Rider finish routine 0x125108 -> 0x162258 (race_bridge.cpp): applied before this tick's camera update.
void browser_camera_finish(int32_t raceRiders){pendingCameraFinish=true;cameraRaceRiders=raceRiders;}
void browser_camera_race_reset(){pendingCameraFinish=false;}
static const OriginalChaseAlgorithmState& cameraHead(){return chase.director.nodes.front().algorithm;}
void request_crash_camera_shake(float strength){pendingCrashCameraShake=std::max(pendingCrashCameraShake,strength);++crashCameraShakeRequests;}
// Stage builtin 91 (0x3050F0, web/stage_script_gameplay.inc): 15E360(camera, type, scale, fade) from the entity pass, in call order,
// after the rider pass's crash request and before the camera update (15E460 shakeTrigger).
RIDER_LOCAL std::vector<std::array<float,3>> pendingStageCameraShakes;RIDER_LOCAL unsigned stageCameraShakeRequests=0;
void request_stage_camera_shake(int type,float scale,float fade){if(pendingStageCameraShakes.size()<64)pendingStageCameraShakes.push_back({float(type),scale,fade});++stageCameraShakeRequests;}
extern "C" EMSCRIPTEN_KEEPALIVE float* stage_camera_shake_info(){RIDER_LOCAL static float v[4];v[0]=float(stageCameraShakeRequests);v[1]=float(pendingStageCameraShakes.size());v[2]=pendingStageCameraShakes.empty()?-1.f:pendingStageCameraShakes.back()[2];v[3]=float(crashCameraShakeRequests);return v;}
auto sourceVector(Vec3 p){return std::array<float,3>{float(p.x*100),float(-p.z*100),float(p.y*100)};}
auto sourceDirection(Vec3 p){return std::array<float,3>{float(p.x),float(-p.z),float(p.y)};}
RayHit terrain_down(Vec3 origin,double distance){
 if(cameraTerrain){
  auto authored=cameraTerrain->sourceTerrainSegment(sourceVector(origin),sourceVector(origin+Vec3{0,-distance,0}),0,true);
  if(authored.hit){RayHit h;h.hit=true;h.distance=authored.fraction*distance;
   h.position={authored.pointCm[0]/100.,authored.pointCm[2]/100.,-authored.pointCm[1]/100.};
   h.normal={authored.normal[0],authored.normal[2],-authored.normal[1]};h.surface=authored.surface;
   h.analytic=true;h.resource=authored.resource;h.u=authored.u;h.v=authored.v;h.terrainQuery=3;return h;
  }
 }
 return world->raycast(origin,{0,-1,0},distance);
}
void begin_airborne(bool keepPrewind=false){
 airbornePrewind=keepPrewind;
 originalLandingGroundLeave(physicsState,motionTick,lastGroundLeave);hostDepartureTick=motionTick;grounded=false;airtime=0;begin_prediction(OriginalAirState::fromNative(position,velocity));
 audio_event(AE_TAKEOFF,audioSpeed75C,float(browserTrajectory.status==1||browserTrajectory.status==3),browserTrajectory.predictedTime); //294170 after 11FE78(1): 0x75C of the last ground tick
}
void retain_jump_camera(const OriginalJumpState&);
void browser_controller_takeoff_wind(const std::array<float,3>&); // web/animation_bridge.cpp: 120378 reads the takeoff +0x1E0
void browser_controller_stance(bool); // web/animation_bridge.cpp: 120378 reads the post-controller +0x320
void browser_rail_idle_approach(); // web/rail_gameplay.inc: 1211F8 approaches +0x22C/+0x238/+0x25C in every control
// 0x114298(rider,charge) requested by a controller (board-press R3 ollie 0x1307B8, charge 1),
// then motion 1 (0x13F410 ground leave, 0x1399E0 enter).
void browser_controller_takeoff(float takeoffCharge){
 OriginalJumpState takeoff;takeoff.position=physicsState.position;takeoff.velocity=physicsState.velocity;
 takeoff.normal=physicsState.normal;takeoff.takeoffNormal=physicsState.previousNormal;takeoff.forward=physicsState.forward;
 takeoff.boardUp=physicsState.boardUp;takeoff.speedLimit=physicsProfile.speedLimit;takeoff.charge=takeoffCharge;
 takeoff.ticksSinceGroundFocus=motionTick-groundFocusTick;takeoff.riderState=physicsState.riderType;takeoff.flags=browserPatchFlags;
 originalJumpTakeoff(takeoff);retain_jump_camera(takeoff);browser_score_takeoff(takeoff.rampTakeoff);browser_controller_takeoff_wind(takeoff.velocity);
 physicsState.position=takeoff.position;physicsState.velocity=takeoff.velocity;
 const OriginalAirState launched{takeoff.position,takeoff.velocity};position=launched.nativePosition();velocity=launched.nativeVelocity();
 begin_airborne();charge=0;
}
bool browser_rail_uber_control();
RIDER_LOCAL bool browserStarting=false,browserStartFrame=false,browserStartFrozen=false;RIDER_LOCAL bool (*browserStartControl)()=nullptr;RIDER_LOCAL void (*browserStartClear)()=nullptr;
RIDER_LOCAL void (*browserRailTickBegin)()=nullptr; // clears the rail step's per-tick flags whether or not it runs (web/rail_gameplay.inc)
// 12E9B8's jump release left a rail this tick: it returns before 114130, so the boost runs on into the first air tick (web/rail_gameplay.inc)
RIDER_LOCAL bool browserRailJumpRelease=false;
// the rail controller (control 7) ran this tick's 114130 and then handed the tick to the air (web/rail_gameplay.inc Stop::Airborne)
RIDER_LOCAL bool browserRailControllerRan=false;
// The +0x28C / +0x298 values before this tick's presentation-filter pass (1211F8), for the first 133308 air-animation tick
// (web/animation_bridge.cpp): its own 1211F8 approach after the new targets is the tick's only one.
RIDER_LOCAL bool browserAdjustFiltersApproached=false;
RIDER_LOCAL GroundControlValue browserAdjustBefore28C{},browserAdjustBefore298{};
RIDER_LOCAL bool browserRailActive=false;RIDER_LOCAL bool browserRailBoostTicked=false; // the rail step ran this tick's 114130 / 1200D0 and handed the tick back (web/rail_gameplay.inc)
RIDER_LOCAL bool (*browserRailStep)(float,int,int,int)=nullptr;RIDER_LOCAL void (*browserRailReset)()=nullptr;
// Handplant control11/motion5 (web/handplant_gameplay.inc): runs before rail attach; a failed cruise attempt edits the cruise inputs and skips 0x106848.
// Attacks 0x1163B0 (web/attack_gameplay.inc): natural air runs it before 0x107578/0x106848; cruise clamps its turn to +-0.5.
RIDER_LOCAL bool (*browserAttackAir)()=nullptr;RIDER_LOCAL void (*browserAttackCruise)(float&)=nullptr;RIDER_LOCAL void (*browserAttackReset)()=nullptr;
RIDER_LOCAL bool (*browserHandplantStep)(float&,float&,float&,bool&)=nullptr;RIDER_LOCAL void (*browserHandplantReset)()=nullptr;
// Board press control 1 (web/boardpress_gameplay.inc): 0x1161D0 cruise entry / 0x12FC80 update in the controller slot; the tick skips the cruise controller.
RIDER_LOCAL int (*browserFinishStep)()=nullptr; // web/finish_gameplay.inc: control 10 (0x116378 entry, 0x12C678)
RIDER_LOCAL int (*browserBoardPressStep)()=nullptr;RIDER_LOCAL void (*browserBoardPressReset)()=nullptr;
// 108388's control-1 cancel 131348 (web/boardpress_gameplay.inc board_press_soft_cancel): +0x320 after it, -1 without a live control 1
RIDER_LOCAL int (*browserBoardPressCancel)()=nullptr;
RIDER_LOCAL bool browserBoardPressFrame=false;
RIDER_LOCAL int browserAirModeFlag=0;RIDER_LOCAL bool browserHandplantMotion5=false; //air motion+0: entered from motion5 (0x1399E0)
struct RideCommand{bool active=false;float turn=0,crouch=0,brake=0,board=0,spin=0,flip=0;int jump=0,boost=0,grab=0,applyTargets=1;};
RIDER_LOCAL RideCommand rideCommand;RIDER_LOCAL float rideAxes[12]{};
RIDER_LOCAL float rideBoard=0,rideSpin=0,rideFlip=0;RIDER_LOCAL bool rideLatched=false;
void refresh_surface_frame(){
 const auto basis=originalOrientationBasis(physicsState.quaternion);
 physicsState.physicalForward=basis.forward;physicsState.boardUp=basis.up;
 const float distance=physicsState.distance;
 originalGroundContact(physicsState,physicsState.position,sourceDirection(normal),physicsState.surfaceVelocity);
 physicsState.distance=distance;
}
void publish_motion(){
 // Source centimeters stay authoritative. Rebuilding them from metres is not
 // an identity on binary32 and walks the rider off the original line.
 OriginalRounding hostRounding(FE_TONEAREST);
 auto resolved=OriginalAirState{physicsState.position,physicsState.velocity};
 position=resolved.nativePosition();velocity=resolved.nativeVelocity();
 physicsState.normal=sourceDirection(normal);
 auto basis=originalOrientationBasis(physicsState.quaternion);
 physicsState.physicalForward=basis.forward;physicsState.boardUp=basis.up;
 auto resolvedForward=Vec3{basis.forward[0],basis.forward[2],-basis.forward[1]};
 yaw=std::atan2(resolvedForward.x,resolvedForward.z);
 // Ground contact owns the retained surface basis; telemetry must not rebuild it.
 // Airborne motion keeps the departure tick's surface tangents (rider+3A0/3B0): the original
 // builds them in ground contact and never rebuilds them from the board in the air.

 output[0]=position.x;output[1]=position.y;output[2]=position.z;output[3]=yaw;
 output[4]=normal.x;output[5]=normal.y;output[6]=normal.z;output[7]=std::sqrt(dot(velocity,velocity));
 output[8]=grounded;output[9]=charge;output[10]=airtime;output[11]=landed;output[12]=distanceRun;output[13]=respawns;output[14]=velocity.y;
}
int browser_surface_property(int surface){return surface>=0&&surface<19?browserSurfaceProperties44[surface]:0;}
void apply_reset_placement(const OriginalResetPlacement& value,bool stance){
 physicsState.position=value.position;physicsState.quaternion=value.physical.quaternion;physicsState.velocity={};physicsState.normal=value.normal; // 11D660 writes +0x370, not +0x380/+0x390 (both keep their pre-reset values)
 physicsState.forward=value.forward;physicsState.lateral=value.lateral;physicsState.physicalForward=value.physical.forward;physicsState.boardUp=value.physical.up;
 physicsState.surfaceVelocity={};physicsState.presentationUp={0,0,1};physicsState.depth1=physicsState.depth3=physicsState.distance=physicsState.contactClearance=0;
 for(auto* v:{&physicsState.turn,&physicsState.brake,&physicsState.crouch,&physicsState.presentationLift,&physicsState.animationTurn,&physicsState.extraLean,&physicsState.boardAlignment,&physicsState.presentationRoll,&physicsState.balance280,&physicsState.adjustment28C,&physicsState.adjustment298})*v={};
 physicsState.timeScale=1;physicsState.manualSpin=physicsState.boost=physicsState.boardLift=0;physicsState.prewindStyle=0;physicsState.reverseStance=stance;physicsState.state320Equals324=true;
 collisionHistory={};collisionRecoveryRequested=false;boostState.amount=0;charge=airtime=0;held=airbornePrewind=false;grounded=false;physicsState.controlState=9;
 auto actor=OriginalAirState{value.position,{}};position=actor.nativePosition();velocity={};normal={value.normal[0],value.normal[2],-value.normal[1]};groundCache={};browserBoardNormalForPose=physicsState.boardNormal;chaseReady=false;reset_prediction();publish_motion();
 refresh_rider_scope(); // 11D660 -> 11E150 -> 332DB8: the placement rebuilds the scope list from the placed bounds (a teleport's next ground query sees the new spot)
}
void reset_resume_velocity(){for(unsigned k=0;k<3;k++)physicsState.velocity[k]=terrain_original::mul(physicsState.physicalForward[k],833.3333740234375f);physicsState.velocity[2]=0;velocity=OriginalAirState{physicsState.position,physicsState.velocity}.nativeVelocity();publish_motion();}
void commit_rider_physics(){auto value=OriginalAirState{physicsState.position,physicsState.velocity};position=value.nativePosition();velocity=value.nativeVelocity();publish_motion();}
void publish_rail_rider(const OriginalRailRider&r,bool onRail){physicsState.position=r.position;physicsState.velocity=r.velocity;physicsState.normal=r.contactNormal;position=OriginalAirState{r.position,r.velocity}.nativePosition();velocity=OriginalAirState{r.position,r.velocity}.nativeVelocity();normal={r.contactNormal[0],r.contactNormal[2],-r.contactNormal[1]};grounded=onRail||r.motionMode==0;airMotionThisTick=!onRail;charge=0;held=false;browserTrailContact=r.contactPoint;publish_motion();}
void retain_rail_crouch(float value){charge=value;held=true;airbornePrewind=true;output[9]=value;}
void publish_rail_charge(float value){charge=value;output[9]=value;}
// Motion4 exits through 13C5A0 (rail tolerance only), not the ground exit 13F410.
void leave_rail_motion(){hostDepartureTick=motionTick;grounded=false;held=airbornePrewind=false;charge=airtime=0;begin_prediction({physicsState.position,physicsState.velocity});publish_motion();}
void leave_reset_motion(){browserResetActive=false;grounded=false;physicsState.controlState=4;airtime=charge=0;held=airbornePrewind=false;begin_prediction({physicsState.position,physicsState.velocity});publish_motion();}
void clear_reset_contacts(){groundCache={};}
void publish_crash_actor(const OriginalCrashActorState& actor,int submode){
 held=false;charge=0;airbornePrewind=false;physicsState.position=actor.position;physicsState.velocity=actor.velocity;physicsState.normal=actor.groundNormal;auto value=OriginalAirState{actor.position,actor.velocity};position=value.nativePosition();velocity=value.nativeVelocity();normal={actor.groundNormal[0],actor.groundNormal[2],-actor.groundNormal[1]};
 physicsState.quaternion=actor.quaternion;physicsState.surfaceVelocity=actor.surfaceVelocity;physicsState.distance=actor.contactDistance;physicsState.controlState=8;grounded=submode==0;airMotionThisTick=submode==1;browserCrashAir=submode==1;browserCrashSurface=actor.surface;publish_motion();
 // Motion2 writes only +370/+3D0 (sliding contact 137D18, air body landing 137860); +380 and the +3A0/+3B0 tangents keep their entry values until 13C7A8/ground motion.
}
void leave_crash_motion(OriginalCrashActorState& actor,int mode){
 publish_crash_actor(actor,mode);browserCrashActive=false;physicsState.controlState=mode==0?0:5;
 if(mode==0){const int surface=std::clamp(actor.surface,0,18);const float limit=physicsProfile.speedLimit;physicsProfile=physicsMaterials[surface];physicsProfile.speedLimit=limit;originalLandingGroundEnter(physicsState,landingProfile.materials[surface],landingProfile.bodyScale,motionTick,lastGroundLeave);velocity=OriginalAirState{physicsState.position,physicsState.velocity}.nativeVelocity();groundFocusTick=motionTick;browserTrajectory.predictedTime=0;}
 else begin_prediction({actor.position,actor.velocity});
 held=false;charge=0;airtime=0;airbornePrewind=false;publish_motion();
}
// Contact acceptance and resolution are shared with the scalar diagnostic path.
// The gameplay caller supplies the original posed-board query after world pose.
bool resolve_touchdown(const OriginalWorldSegmentHit& contact,uint32_t tick,int queryKind){
 if(!contact.complete||contact.surface<0||contact.surface>=19)return false;
 OriginalLandingState touchdown;touchdown.rider=physicsState;
 auto impact=originalLandingImpact(touchdown,contact);if(!impact.contact)return false;
 if(browserLandingAirExit)browserLandingAirExit();
 touchdown.rider=physicsState;touchdown.physicalRight=originalOrientationBasis(physicsState.quaternion).right;
 {terrain_original::Rounding rounding;const auto& v=physicsState.velocity;float square=terrain_original::mul(v[0],v[0]);square=terrain_original::add(square,terrain_original::mul(v[1],v[1]));square=terrain_original::add(square,terrain_original::mul(v[2],v[2]));browserLandingSpeed=terrain_original::sqrt(terrain_original::add(square,0.f));}
 const auto& material=landingProfile.materials.at(contact.surface);
 for(unsigned i=0;i<3;i++){landingContactInfo[i]=physicsState.position[i];landingContactInfo[3+i]=contact.position[i];landingContactInfo[6+i]=contact.normal[i];}landingContactInfo[9]=material.depth3;
 if(!originalLandingResolveContact(touchdown,contact,material,impact.relativeNormalSpeed)){collisionRecoveryRequested=true;if(browserResetBegin)browserResetBegin(1);return false;}
 browserLandingTranslation=touchdown.translationCm;
 browserTrajectory.predictedTime=touchdown.trajectoryPredictionTime; //139C88 clears predictor+98 at contact.
 if(contact.surface!=physicsProfile.surface.id){const float limit=physicsProfile.speedLimit;physicsProfile=physicsMaterials[contact.surface];physicsProfile.speedLimit=limit;}
 browserGroundPatch=contact.hasPatch?contact.patchId:-1;browserGroundU=contact.patchU;browserGroundV=contact.patchV;browserTrailContact=contact.position;physicsState=touchdown.rider;output[15]=impact.relativeNormalSpeed;audioImpact770=-impact.relativeNormalSpeed; /*139C88: rider+0x770*/
 if(browserLandingCrash&&browserLandingCrash(touchdown))return false;
 for(unsigned i=0;i<3;i++)landingContactInfo[10+i]=physicsState.position[i];landingContactInfo[13]=physicsState.distance;
 landingInfo[4]=impact.relativeNormalSpeed;landingInfo[5]=terrain_original::dot(physicsState.velocity,physicsState.normal);
 landingInfo[3]=queryKind;landingInfo[0]=std::sqrt(terrain_original::dot(physicsState.velocity,physicsState.velocity))/100.f;landingInfo[2]=uint32_t(tick-lastGroundLeave);
 originalLandingGroundEnter(physicsState,material,landingProfile.bodyScale,tick,lastGroundLeave);
 auto resolved=OriginalAirState{physicsState.position,physicsState.velocity};position=resolved.nativePosition();velocity=resolved.nativeVelocity();normal={contact.normal[0],contact.normal[2],-contact.normal[1]};
 landingInfo[1]=std::sqrt(dot(velocity,velocity));landed=1;grounded=true;airbornePrewind=false;airtime=0;groundFocusTick=tick;
 publish_motion();return true;
}
bool resolve_posed_landing(const BodyCollisionVolume& volume){
 if(!browserPosedLandingEnabled||browserCrashActive||browserResetActive||grounded||!airMotionThisTick||lastPosedContactTick==motionTick)return false;
 lastPosedContactTick=motionTick;landingProbeInfo={};
 if(!cameraTerrain||!browserBodies||!volume.landingCenterCm||!volume.reactionFrame)return false;
 const OriginalLandingProbe probe{*volume.landingCenterCm,volume.reactionFrame->up};
 const auto hit=originalLandingContact(*cameraTerrain,browserBodies.get(),probe,&groundCache,riderScope?&*riderScope:nullptr);
 landingProbeInfo[0]=1;landingProbeInfo[1]=hit.complete;landingProbeInfo[2]=hit.fraction;landingProbeInfo[16]=hit.surface;
 for(unsigned k=0;k<3;k++){landingProbeInfo[4+k]=probe.centerCm[k];landingProbeInfo[7+k]=probe.presentationUp[k];landingProbeInfo[10+k]=hit.position[k];landingProbeInfo[13+k]=hit.normal[k];}
 const bool accepted=resolve_touchdown(hit,motionTick-1,4);landingProbeInfo[3]=accepted;return accepted;
}
// Wind push 0x125970, first in the ground motion 13D818 and the air motion 139A20 (engine/wind_push.hpp): only the human
// (rider+0x874), from its own Weather painter (web/animation_bridge.cpp weather_painter_step, stepped after last tick's
// motion); 0x1250A8 adds the change and, in the air (11FE98 == 1), restarts the predictor with 0x1135B8.
extern "C" {RIDER_LOCAL extern bool browserHumanRider;} // web/world_bridge.cpp
bool browser_weather_wind(float& speedKmh,float& directionDeg); // web/animation_bridge.cpp
RIDER_LOCAL static uint32_t windPushes=0;RIDER_LOCAL static std::array<float,4> windLastPush{};RIDER_LOCAL static int32_t windLastTick=-1;
void browser_wind_push(bool air){
 if(!browserHumanRider)return;float kmh=0,deg=0;if(!browser_weather_wind(kmh,deg))return;
 const auto dv=originalWindPush(kmh,deg,{physicsState.velocity[0],physicsState.velocity[1],physicsState.velocity[2],0.f});if(!dv)return;
 const auto v=originalBoostAddVelocity({physicsState.velocity[0],physicsState.velocity[1],physicsState.velocity[2],0.f},*dv);physicsState.velocity={v[0],v[1],v[2]};
 if(air)begin_prediction({physicsState.position,physicsState.velocity});
 ++windPushes;windLastPush=*dv;windLastTick=int32_t(motionTick);
}
// QA: [pushes, last push tick, last dv x y z].
extern "C" EMSCRIPTEN_KEEPALIVE float* wind_push_info(){RIDER_LOCAL static std::array<float,5> v{};v={float(windPushes),float(windLastTick),windLastPush[0],windLastPush[1],windLastPush[2]};return v.data();}
// Air motion 139A20: predictor step (113648) or plain integration, then 139A80/139A8C.
static void translate_air_motion(){auto airborne=OriginalAirState{physicsState.position,physicsState.velocity};effectiveSpeedLimit=3333.33349609375f;if(!advance_prediction(airborne))airborne.step(effectiveSpeedLimit);physicsState.position=airborne.position;physicsState.velocity=airborne.velocity;position=airborne.nativePosition();velocity=airborne.nativeVelocity();
  /*139A80/139A8C: air motion copies presentation up (+180) into the contact normal (+370) and clears surface velocity (+3D0 = 4FF120 zero) every tick*/physicsState.normal=physicsState.presentationUp;normal={physicsState.normal[0],physicsState.normal[2],-physicsState.normal[1]};physicsState.surfaceVelocity={0,0,0};}
// 0x114DB8 (in-flight stance switch) belongs to the control-5 update, which the original runs before
// this translation; the browser runs it later (animation_tick). Keep the pre-motion state so a switch
// can restart the flight from it and translate again (browser_air_switch_redo).
struct AirMotionStart {bool valid=false;std::array<float,3> position{},velocity{},normal{},surfaceVelocity{};};
RIDER_LOCAL static AirMotionStart airMotionStart;
void save_air_motion_prediction();void restore_air_motion_prediction();
bool browser_air_motion_start(std::array<float,3>& position,std::array<float,3>& velocity){if(!airMotionStart.valid)return false;position=airMotionStart.position;velocity=airMotionStart.velocity;return true;}
void browser_air_switch_redo(const std::array<float,3>& position){
 if(!airMotionStart.valid)throw std::runtime_error("Air switch without this tick's air motion");
 physicsState.position=position;physicsState.velocity=airMotionStart.velocity;physicsState.normal=airMotionStart.normal;physicsState.surfaceVelocity=airMotionStart.surfaceVelocity;
 restore_air_motion_prediction();begin_prediction({physicsState.position,physicsState.velocity}); //0x1135B8(pred,+0x110,+0x1E0,3333.33)
 browser_wind_push(true);translate_air_motion();publish_motion(); // 139A20 again: 125970 first
}
void align_air_orientation(int controlState,float adjustSpin){
 if(grounded||!browserPredictionAvailable)return;
 OriginalAirAlignmentContext context;context.normal=browserTrajectory.normal;context.heading=browserTrajectory.heading;
 context.physicalForward=physicsState.physicalForward;context.predictedTime=browserTrajectory.predictedTime;context.elapsedTime=browserTrajectory.elapsed;
 context.timeScale=physicsState.timeScale;context.adjustSpin=adjustSpin;context.controlState=controlState;context.airModeFlag=browserAirModeFlag;
 context.trajectoryStatus=browserTrajectory.status;context.surfaceIndex=browserTrajectory.surface;context.surfaceFlags=browserTrajectory.patchFlags;
 if(context.surfaceIndex>=0&&context.surfaceIndex<19)context.surfaceProperty44=browserSurfaceProperties44[context.surfaceIndex];
 physicsState.quaternion=originalAirAlignmentStage(physicsState.quaternion,context).quaternion;
 publish_motion();
}
static bool getenv_legacy_tangent(){return false;}
ObstacleResponse apply_body_contact(const WorldBodyHit& hit,bool airborne){
 terrain_original::Rounding rounding;
 auto response=airborne?originalAirObstacleResponse(hit.penetrationCm,hit.normal,physicsState.velocity):originalObstacleResponse(hit.penetrationCm,physicsState.normal,hit.normal,physicsState.velocity,hit.surfaceVelocityCmps);
 if(!response.accepted)return response;
 for(unsigned i=0;i<3;i++)physicsState.position[i]=terrain_original::add(physicsState.position[i],response.translationCm[i]);
 physicsState.velocity=response.velocityCmps;
 if(response.bounced){
  auto q=physicsState.quaternion;
  if(!airborne)q=originalObstacleOrientation(q,physicsState.boardUp,physicsState.physicalForward,response.normal);
  auto rebuilt=originalRebuildOrientation(q);
  physicsState.quaternion=rebuilt.quaternion;physicsState.physicalForward=rebuilt.forward;physicsState.boardUp=rebuilt.up;
  // 13F488 (1065B0 orientation, 11E098 rebuild) does not rewrite the +0x3A0/+0x3B0 surface tangents: they keep this tick's
  // 13C7A8 contact values (projected from the pre-bounce facing) until the next ground contact (full-course 3334).
  if(!airborne&&getenv_legacy_tangent()){const float distance=physicsState.distance;originalGroundContact(physicsState,physicsState.position,physicsState.normal,physicsState.surfaceVelocity);physicsState.distance=distance;}
 }
 OriginalAirState corrected{physicsState.position,physicsState.velocity};position=corrected.nativePosition();velocity=corrected.nativeVelocity();
 if(!grounded&&!airborne)begin_prediction(corrected); // Pending passive takeoff seeds after ground body response.
 publish_motion();return response;
}
OriginalCollisionReaction dispatch_body_event(const OriginalCollisionEvent& event,const BodyCollisionVolume& volume,std::array<float,3> eventVelocity,int motion,int controlState,const CollisionRandom& random){
 terrain_original::Rounding rounding;
 auto physical=originalOrientationBasis(physicsState.quaternion);OriginalCollisionContext context;
 if(volume.reactionFrame)context.presentation=*volume.reactionFrame;
 context.physical={physical.right,physical.forward,physical.up,physicsState.position};context.velocityCmps=eventVelocity;
 context.motionMode=motion;context.controlState=controlState;
 if(controlState==1&&browserBoardPressCancel)context.cancelControlOne=browserBoardPressCancel;
 context.animationClass=volume.mainAnimation?volume.mainAnimation->animationClass:physicsState.animationClass;context.reverseStance=physicsState.reverseStance;context.manualSpin=physicsState.manualSpin;
 auto result=originalCollisionReaction(collisionProfile,collisionHistory,context,event,random);collisionReaction=result;++collisionSerial;if(context.controlState!=9&&result.kind!=OriginalCollisionReactionKind::SurfaceReset)audio_event(AE_RUMBLE_IMPACT,event.closingSpeedCmps); /*105E7C: owner +0xDFC rumble*/
 // 105D98 (105EEC/105F50) restarts the flight with 1135B8 = 113198 (clears hit/heading/normal/times) + 113618, not 113618 alone.
 if(result.resetPredictor&&!grounded)begin_prediction({physicsState.position,physicsState.velocity});
 if(result.kind==OriginalCollisionReactionKind::SurfaceReset&&browserResetBegin)browserResetBegin(1);
 // 105D98's crash 10EB30 at 0x1064E4 passes attacked = 0.
 if(result.kind==OriginalCollisionReactionKind::Crash&&browserHardCrash)browserHardCrash(result.animation,event,false);
 return result;
}
OriginalCollisionReaction classify_body_contact(const WorldBodyHit& hit,const ObstacleResponse& response,const BodyCollisionVolume& volume,std::array<float,3> incoming,int controlState,const CollisionRandom& random,int motion){ // motion: 11FE98 when the caller knows it (0 after a touchdown in 139C88), else from this tick's motion
 terrain_original::Rounding rounding;OriginalCollisionEvent event;
 event.pointCm=hit.pointCm;event.normal=response.normal;event.closingSpeedCmps=response.closingSpeedCmps;event.surface=hit.surface;
 if(hit.surface>=0&&hit.surface<19)event.surfaceProperty44=browserSurfaceProperties44[hit.surface];
 float length=std::sqrt(incoming[0]*incoming[0]+incoming[1]*incoming[1]+incoming[2]*incoming[2]);if(length>0)for(auto& v:incoming)v/=length;event.incomingDirection=incoming;
 return dispatch_body_event(event,volume,response.velocityCmps,motion>=0?motion:airMotionThisTick?1:0,(!airMotionThisTick&&!grounded)?physicsState.controlState:controlState,random);
}

bool begin_soft_control(int semantic,float spin){
 // Callers gate on the motion mode (108388); a passive departure this tick has already cleared `grounded`. Control 1 comes only
 // through web/boardpress_gameplay.inc board_press_soft_enter, after 108388's cancel 131348 ran.
 const int c=physicsState.controlState;
 if((c!=0&&c!=1&&c!=2&&!(browserRailActive&&c==7))||semantic<55||semantic>60||!std::isfinite(spin))return false;
 browserSoftActive=true;softComplete=false;softTransition=-1;held=false;charge=0;
 physicsState.controlState=3;physicsState.animationIndex=semantic;physicsState.animationClass=6;physicsState.manualSpin=spin;return true;
}
bool soft_animation_finished(){return softComplete;}
void soft_animation_complete(bool complete){if(browserSoftActive)softComplete=complete;}
int take_soft_transition(){int value=softTransition;softTransition=-1;return value;}
std::array<float,4> browser_environment_colour(int,std::array<float,4>); // web/environment_bridge.cpp (2ED490)
// pv readyLight (web/main.js readyView): the rival card (world state 2) draws the rider lit by the environment its load frames
// settled at the start spot (PS2 the-throne-ready: rider irradiance row 0 (0.220, 0.307, 0.508) = 0.95 dark + 0.05 bright bank,
// rows 1..9 the same blend). The port's first update runs on race tick 0 and its ground patch is unknown until then: the card's
// ground query (a copy of the contact cache), then the per-tick update n times; the placed state keeps its patch fields.
static void environment_settle_at_rest(int n){
 const int keepPatch=browserGroundPatch;const float keepU=browserGroundU,keepV=browserGroundV;
 if(cameraTerrain){auto cache=groundCache;Vec3 lateral{physicsState.lateral[0],physicsState.lateral[2],-physicsState.lateral[1]};
  auto h=cameraTerrain->sourceGroundContact(position,normal,lateral,physicsState.turn.current,physicsProfile.bodyScale,&cache);
  if(h.hit&&h.analytic){browserGroundPatch=int(h.resource);browserGroundU=h.u;browserGroundV=h.v;}}
 std::array<float,4> colour{};for(int k=0;k<n;++k)colour=browser_environment_colour(0,colour);
 browserGroundPatch=keepPatch;browserGroundU=keepU;browserGroundV=keepV;}
extern "C" {
EMSCRIPTEN_KEEPALIVE void init_world(float* data,int count) {
 std::vector<Triangle> triangles;triangles.reserve(count/9);
 for(int i=0;i<count;i+=9)triangles.push_back({{data[i],data[i+1],data[i+2]},{data[i+3],data[i+4],data[i+5]},{data[i+6],data[i+7],data[i+8]},uint32_t(i/9)});
 world=std::make_unique<CollisionWorld>(std::move(triangles));
}
// pv eventSlices (web/load-slices.js initWorldStepped): init_world's tree built across load-screen frames. begin copies the triangles
// (the caller may free `data` after it); step builds until about `budget` triangles were scanned and returns 1 when the world is
// complete, which then replaces the loaded one (nothing queries the triangle world during a load).
static std::unique_ptr<CollisionWorld> pendingWorld;
EMSCRIPTEN_KEEPALIVE void init_world_begin(float* data,int count) {
 std::vector<Triangle> triangles;triangles.reserve(count/9);
 for(int i=0;i<count;i+=9)triangles.push_back({{data[i],data[i+1],data[i+2]},{data[i+3],data[i+4],data[i+5]},{data[i+6],data[i+7],data[i+8]},uint32_t(i/9)});
 pendingWorld=std::make_unique<CollisionWorld>(std::move(triangles),CollisionWorld::DeferredBuild{});
}
EMSCRIPTEN_KEEPALIVE int init_world_step(int budget) {
 if(!pendingWorld)return 1;
 if(!pendingWorld->buildStep(size_t(std::max(1,budget))))return 0;
 world=std::move(pendingWorld);return 1;
}
// Tests (web/test-event-slices.mjs): FNV-1a over the triangle world and the camera terrain (init_world / init_terrain state).
EMSCRIPTEN_KEEPALIVE uint32_t world_load_hash() {
 uint32_t h=0x811c9dc5u;auto mix=[&](const void* p,size_t n){const auto* b=static_cast<const uint8_t*>(p);for(size_t i=0;i<n;++i){h^=b[i];h*=0x01000193u;}};
 const uint8_t present[2]={uint8_t(world!=nullptr),uint8_t(cameraTerrain!=nullptr)};mix(present,2);
 if(world)world->hashState(mix);if(cameraTerrain)cameraTerrain->hashState(mix);
 return h;
}
EMSCRIPTEN_KEEPALIVE float height_at(float x,float y,float z) {auto h=terrain_down({x,y,z},100);return h.hit?h.position.y:-1e9;}
// World state 14's rider actor (web/main.js, pv nisTick; docs/ctm-parity.md "The NIS rider hold"). on = 1 is 123640's rider part: the
// running control's / motion's exits (11FEC8(13), 11FE78(3); the same clears as reset_rider), control 13 / motion 3, velocity 0 (0x4FF120),
// the rider at world cm (x, y, z) (the anchor's ground-snapped point; NaN z keeps the rider's) with +0x120 = qZ(atan2(fy, fx) - pi/2), so its forward row
// is (fx, fy, 0) (PS2 fr-booth2 record 2644: (0, 0, 0.4279, 0.9038) at mdl_A_NIS_Transport_0). Then step_rider runs motion 3's tick only
// and animation_pose / follow_rider_route skip (121700 / 121728 / 121818 test +0xAC4). on = 0 is 123B48 without its 11D390: the caller
// places (reset_rider, place_rider_region and fresh_rider_start release first). Not modelled: the actor's per-tick moves (124788) and
// the animator's semantic 5 (311A50, 3128E8): the lodge / booth actors stand still, the pose is not drawn under the cut, and the
// release's placement (11D390 -> 11D660 semantic 5, 11DF18) rewrites the velocity and the animation.
EMSCRIPTEN_KEEPALIVE void nis_hold(int on,float x,float y,float z,float fx,float fy){
 if(!on){browserNisHold=false;return;}
 if(!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(fx)||!std::isfinite(fy)||(fx==0&&fy==0))throw std::runtime_error("Invalid NIS hold");
 if(browserStartClear)browserStartClear();if(browserRailReset)browserRailReset();if(browserHandplantReset)browserHandplantReset();if(browserBoardPressReset)browserBoardPressReset();if(browserAttackReset)browserAttackReset();if(browserResetClear)browserResetClear();if(browserCrashReset)browserCrashReset();
 browserCrashActive=false;browserCrashExitFrame=false;browserCrashResetReason=0;browserSoftActive=browserSoftFrame=softComplete=false;softTransition=-1;
 if(!std::isfinite(z))z=physicsState.position[2]; // the caller snaps (main.js: the cut engine's 3369D8 ground snap of the anchor)
 const float half=(std::atan2(fy,fx)-1.5707963705062866f)*.5f;physicsState.quaternion={0.f,0.f,std::sin(half),std::cos(half)};
 physicsState.position={x,y,z};physicsState.velocity={0.f,0.f,0.f};physicsState.controlState=13;airMotionThisTick=false;reset_prediction();
 if(!browserNisHold){nisProbeContact=true;nisProbeSpeed=0;nisProbeMode=0;} // 123640 at the hold's entry: +0xB00 = 1, +0xB04 = 0; the probe off until the caller's nis_hold_probe
 browserNisHold=true;publish_motion();
}
// The hold's 120F20 re-probe (1242B0, nis_hold_ground_probe) runs only for a hold that opts in, after nis_hold, per hold entry:
// mode 1 = an actor with +0xAFC 0 (the NIS record's byte 7, 123640): the probe from +0x110, which the caller's placement gives
// (the CTM approach's computer riders: PS2 c0a-ws13 +0xAFC 0); mode 2 = +0xAFC 1: the probe from the posed board-root bone
// (+0x780->+0x2C[+0x8A0], index 22) at (x, y, z) source cm, and 1242B0's hit after a miss fires the 111AA0 snow impact; 0 = none
// (every other hold: the lodge door, the booth, the Transport's steps, the human's CTM approach and the gondola run as before
// 1242B0 was ported; the page does not pose the rider under an NIS, so it cannot give mode 2's bone). A new hold entry clears it
// (123B48 clears +0xAFC).
EMSCRIPTEN_KEEPALIVE void nis_hold_probe(int mode,float x,float y,float z){if(mode<0||mode>2)throw std::runtime_error("Invalid NIS probe mode");nisProbeMode=browserNisHold?mode:0;nisProbeBone={x,y,z};}
void browser_cruise_enter(); // web/input_bridge.inc
EMSCRIPTEN_KEEPALIVE void reset_rider(float x,float y,float z,float angle) {
 browserNisHold=false; // 123B48 releases the NIS hold before 11D390 places
 browser_cruise_enter(); // a placement enters control 0 again (131608 clears the +0x360 jump latch; web/input_bridge.inc)
 eventCameraSeedArmed=false;
 browserPatchFlags=0x49;
 riderScope.reset(); // no scope list yet: the first queries are unscoped until the slot's refresh (as on a fresh load, so a restart rides the same)
 if(browserStartClear)browserStartClear();if(browserRailReset)browserRailReset();if(browserHandplantReset)browserHandplantReset();if(browserBoardPressReset)browserBoardPressReset();if(browserAttackReset)browserAttackReset();if(browserResetClear)browserResetClear();if(browserCrashReset)browserCrashReset();browserCrashActive=false;browserCrashExitFrame=false;browserCrashResetReason=0;landingProbeInfo={};lastPosedContactTick=0xffffffffu;std::fill(std::begin(landingContactInfo),std::end(landingContactInfo),0);reverseTurnSerial=0;browserSoftActive=browserSoftFrame=softComplete=false;softTransition=-1;collisionHistory={};collisionReaction.reset();collisionSerial=0;collisionRecoveryRequested=false;physicsProfile=browserGroundProfile();browser_apply_ground_attributes();effectiveSpeedLimit=physicsProfile.speedLimit;physicsState=browserGroundState();browserBoardNormalForPose=physicsState.boardNormal;reset_prediction();groundCache={};riderScope.reset();motionTick=browserLandingTick;lastGroundLeave=browserLastGroundLeave;landingInfo[0]=landingInfo[1]=landingInfo[2]=landingInfo[3]=landingInfo[4]=landingInfo[5]=0;boostState=browserBoostState();cameraLaunchValue=0;cameraWallNormal={};lastChargedTakeoff={};pendingCrashCameraShake=0;crashCameraShakeRequests=0;pendingStageCameraShakes.clear();stageCameraShakeRequests=0;boostHeld=false;groundFocusTick=browserGroundFocusTick;chaseReady=false;spawn={x,y,z};startYaw=yaw=angle;position=spawn;velocity=OriginalAirState{physicsState.position,physicsState.velocity}.nativeVelocity();charge=airtime=distanceRun=0;respawns=0;grounded=true;held=false;airbornePrewind=false;landed=0;normal={0,1,0};output[15]=0;
 const auto seedPosition=OriginalAirState{physicsState.position,physicsState.velocity}.nativePosition();
 const bool seededPosition=x==float(seedPosition.x)&&y==float(seedPosition.y)&&z==float(seedPosition.z);
 if(seededPosition){position=seedPosition;normal={physicsState.normal[0],physicsState.normal[2],-physicsState.normal[1]};}
 else {auto h=world->raycast(position+Vec3{0,5,0},{0,-1,0},30);if(h.hit){position=h.position;normal=h.normal;if(normal.y<0)normal=normal*-1;}else grounded=false;physicsState.position=sourceVector(position);if(grounded)physicsState.normal=sourceDirection(normal);}
 auto initial=originalOrientationBasis(physicsState.quaternion);float initialYaw=std::atan2(initial.forward[0],-initial.forward[1]);
 // start.json headings are rounded through JSON/double atan2: within float precision of the seed means the seed orientation.
 const bool seededHeading=angle==initialYaw||std::abs(angle-initialYaw)<2e-6f;
 if(!seededHeading)physicsState.quaternion=originalRebuildOrientation(originalRotateOrientation(physicsState.quaternion,{0,0,1},angle-initialYaw)).quaternion;
 browserGroundPatch=-1;browserGroundU=browserGroundV=0;browserTrailContact=sourceVector(position);airMotionThisTick=!grounded;if(airMotionThisTick)begin_prediction(OriginalAirState::fromNative(position,velocity));
 if(grounded&&(!seededPosition||!seededHeading))refresh_surface_frame();
 publish_motion();
}
RIDER_LOCAL extern bool browserHumanRider; // web/world_bridge.cpp
extern "C++" int npc_motion_mode(); // web/npc_gameplay.inc: owner +0xDE0 (C++ linkage inside this C block)
extern "C++" unsigned npc_rider_slot(); // web/npc_gameplay.inc: the computer rider's slot 1..5, 0 for the human
void browser_upper_reaction_request(int kind); // web/animation_bridge.cpp: 10E028(rider,kind,0)
// 0x1200D0 boost timers: the super timer reaching 0 requests 10E028(rider,6,0) for every rider.
OriginalBoostEffects browser_boost_tick(OriginalBoostState& s,const OriginalBoostProfile& p,float timeScale,int motionMode,int controlState){
 const auto e=originalBoostTick(s,p,timeScale,motionMode,controlState);if(e.timerExpired)browser_upper_reaction_request(6);return e;}
RIDER_LOCAL void (*browserEventRiderSeed)()=nullptr; // computer-rider core instances replace the human grid seed (web/npc_gameplay.inc)
void begin_event_rider(){auto seed=browserEventGroundState();auto actor=OriginalAirState{seed.position,seed.velocity};auto p=actor.nativePosition();auto basis=originalOrientationBasis(seed.quaternion);reset_rider(p.x,p.y,p.z,std::atan2(basis.forward[0],-basis.forward[1]));physicsState=seed;physicsProfile=browserEventGroundProfile();browser_apply_ground_attributes();position=p;velocity={};normal={seed.normal[0],seed.normal[2],-seed.normal[1]};motionTick=0;lastGroundLeave=0;groundFocusTick=0;/*countdown anchor: motion owner+0x10/+0x14 are both 0*/if(!browserEventRolling){physicsState.controlState=6;browserStarting=browserStartFrozen=true;}/*backcountry rolling start (docs/backcountry.md): the ready state's control 0 / motion 0 with its start velocity*/chaseReady=false;arm_event_camera_seed();publish_motion();if(browserEventRiderSeed)browserEventRiderSeed();}
void start_ground_motion(){browserStartFrozen=false;grounded=true;originalLandingGroundEnter(physicsState,landingProfile.materials.at(physicsProfile.surface.id),physicsProfile.bodyScale,motionTick,lastGroundLeave);groundFocusTick=motionTick;commit_rider_physics();}
EMSCRIPTEN_KEEPALIVE void set_rider_velocity(float x,float y,float z){if(!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(z))throw std::runtime_error("Nonfinite fixture velocity");physicsState.velocity={x,y,z};commit_rider_physics();if(!grounded)begin_prediction({physicsState.position,physicsState.velocity});}
EMSCRIPTEN_KEEPALIVE float* rider_query_bounds(){
 RIDER_LOCAL static OriginalRiderQueryBounds result;const auto basis=originalOrientationBasis(physicsState.quaternion);
 const auto& p=physicsState.position;const auto& r=basis.right;const auto& f=basis.forward;const auto& u=basis.up;
 result=originalRiderQueryBounds({p[0],p[1],p[2],1},{r[0],r[1],r[2],0},{f[0],f[1],f[2],0},{u[0],u[1],u[2],0});return result.minimum.data();
}
EMSCRIPTEN_KEEPALIVE float* rider_orientation(){return physicsState.quaternion.data();}
EMSCRIPTEN_KEEPALIVE float* collision_reaction_info(){
 RIDER_LOCAL static float values[10];values[0]=collisionSerial;values[1]=collisionReaction?int(collisionReaction->kind):-1;values[2]=collisionReaction?collisionReaction->animation:-1;values[3]=collisionReaction?collisionReaction->nextControlState:-1;values[4]=collisionReaction&&collisionReaction->resetPredictor;values[5]=collisionReaction?collisionReaction->manualSpin:0;values[6]=collisionReaction?collisionReaction->randomDraws:0;values[7]=collisionHistory.directionChanges;values[8]=collisionHistory.peakImpactCmps;values[9]=collisionRecoveryRequested;return values;
}
// Reward endpoint for source-authored boost pickup actions; contact dispatch is separate.
EMSCRIPTEN_KEEPALIVE void award_boost_pickup(int type,float amount){
 if(!std::isfinite(amount)||(type!=1&&type!=2))throw std::runtime_error("Unsupported boost pickup action");
 if(type==1){boostState.window=originalPickupCounterAward(boostState.window,amount);physicsState.boostWindow=boostState.window;}
 else boostState.modifier=originalPickupCounterAward(boostState.modifier,amount);
 audio_event(AE_PICKUP,type==1?0.f:1.f,1); //10E7A0 / 10E800: 29CED8(0|1, 1.0) + 2A3B18(1|2)
}
EMSCRIPTEN_KEEPALIVE float* rider_state(){return output;}
EMSCRIPTEN_KEEPALIVE void environment_settle(int n){environment_settle_at_rest(n);}
EMSCRIPTEN_KEEPALIVE void request_rider_reset(int reason){if(reason<0||reason>4)throw std::runtime_error("Invalid original reset reason");browserCrashResetReason=reason?reason:-1;}
EMSCRIPTEN_KEEPALIVE float* landing_probe_info(){return landingProbeInfo.data();}
EMSCRIPTEN_KEEPALIVE float* landing_contact_info(){return landingContactInfo;}
EMSCRIPTEN_KEEPALIVE float* landing_info(){return landingInfo;}
// Read-only contact provenance for render/collision diagnostics.
EMSCRIPTEN_KEEPALIVE float* terrain_contact_info(){RIDER_LOCAL static float values[12];values[0]=browserGroundPatch;values[1]=browserGroundU;values[2]=browserGroundV;for(unsigned i=0;i<3;++i){values[3+i]=browserTrailContact[i];values[6+i]=physicsState.normal[i];}values[9]=grounded;values[10]=physicsState.depth1;values[11]=physicsState.depth3;return values;}
EMSCRIPTEN_KEEPALIVE float* ground_contact_info(){groundContactInfo[5]=physicsState.distance;return groundContactInfo;}
RIDER_LOCAL static std::optional<std::pair<std::array<uint32_t,271>,std::array<uint8_t,271>>> pendingCameraSeed;static void apply_camera_seed();
// Event start: the countdown anchor savestate's camera words (generated/event_instance_seed.hpp) are the PS2 camera
// state at game tick cameraAnchorTick (18); the camera the browser builds at start_event is replaced by them before
// the camera step that follows that tick, so from the anchor on the event camera matches the original word for word.
static void arm_event_camera_seed(){eventCameraSeedArmed=browserEventCamera!=nullptr;}
// The camera algorithm input of this tick from the rider as it is now (also the teleport cuts of 0x123210, web/stage_teleport.inc).
static OriginalCameraInput camera_input_for_head(Vec3 head){
 auto basis=originalOrientationBasis(physicsState.quaternion);Vec3 f{basis.forward[0],basis.forward[2],-basis.forward[1]};
 OriginalCameraInput input;auto h=sourceVector(head),sv=sourceVector(velocity),sf=sourceDirection(f),sn=physicsState.previousNormal;input.headPosition={h[0],h[1],h[2],1};input.velocity={sv[0],sv[1],sv[2],0};input.riderForward={sf[0],sf[1],sf[2],0};input.previousContactNormal={sn[0],sn[1],sn[2],0};input.motionMode=(browserStartFrozen||browserResetActive)?3:browserCrashActive?2:browserRailActive?4:browserHandplantMotion5?5:grounded?0:1;input.raceRiderCount=cameraRaceRiders;input.jumpCharge=browser_camera_crouch();input.launchValue=cameraLaunchValue;input.wallNormal={cameraWallNormal[0],cameraWallNormal[1],cameraWallNormal[2],0};input.riderType=physicsState.riderType;input.surfaceId=browserCrashActive?browserCrashSurface:physicsProfile.surface.id;input.tick=motionTick;input.boostLevel=physicsState.boost;input.farCap=30000;
 if(browserPredictionAvailable){input.trajectoryStatusActive=browserTrajectory.status==1||browserTrajectory.status==3;input.predictedAirTime=browserTrajectory.predictedTime;input.trajectoryHeading={browserTrajectory.heading[0],browserTrajectory.heading[1],browserTrajectory.heading[2],0};input.trajectoryNormal={browserTrajectory.normal[0],browserTrajectory.normal[1],browserTrajectory.normal[2],0};}
 input.terrainProbe=[](const OriginalCameraQuad&a,const OriginalCameraQuad&b)->std::optional<OriginalCameraProbeHit>{if(cameraTerrain){auto hit=cameraTerrain->sourceTerrainSegment({a[0],a[1],a[2]},{b[0],b[1],b[2]},original_camera::originalCameraTerrainPreferredFraction,true);if(!hit.hit)return {};return OriginalCameraProbeHit{hit.fraction,{hit.normal[0],hit.normal[1],hit.normal[2],0}};}Vec3 p{a[0]*.01,a[2]*.01,-a[1]*.01},q{b[0]*.01,b[2]*.01,-b[1]*.01};auto d=q-p;double length=std::sqrt(dot(d,d));if(length<.001)return {};auto hit=world->raycast(p,d,length);if(!hit.hit)return {};return OriginalCameraProbeHit{float(hit.distance/length),{float(hit.normal.x),float(-hit.normal.z),float(hit.normal.y),0}};};
 return input;
}
static float* camera_for_head(Vec3 head){
 auto input=camera_input_for_head(head);
 // Complete algorithm input snapshot for cross-runtime camera replay (source units).
 const OriginalCameraQuad* traceQuads[]={&input.headPosition,&input.riderForward,&input.velocity,&input.previousContactNormal,&input.wallNormal,&input.trajectoryHeading,&input.trajectoryNormal};
 for(unsigned q=0;q<7;++q)for(unsigned k=0;k<4;++k)cameraSourceInput[q*4+k]=(*traceQuads[q])[k];
 const float traceScalars[]={float(input.motionMode),input.boostLevel,input.jumpCharge,float(input.surfaceId),float(input.riderType),input.launchValue,float(input.proximityFlag),float(input.trajectoryStatusActive),input.predictedAirTime,float(input.tick)};
 std::copy(std::begin(traceScalars),std::end(traceScalars),cameraSourceInput+28);
 cameraInputs[21]=input.jumpCharge;
 cameraInputs[0]=input.trajectoryStatusActive;cameraInputs[1]=input.predictedAirTime;
 for(unsigned i=0;i<3;i++){cameraInputs[2+i]=input.trajectoryHeading[i];cameraInputs[5+i]=input.trajectoryNormal[i];cameraInputs[8+i]=input.previousContactNormal[i];}cameraInputs[11]=input.tick;cameraInputs[12]=input.motionMode;cameraInputs[13]=input.launchValue;for(unsigned i=0;i<3;i++)cameraInputs[18+i]=input.wallNormal[i];
 if(!chaseReady){chase={};originalDirectedCameraBegin(chase,input,cameraViewType);chaseReady=true;apply_camera_seed();cameraSetTargets=0;}
 else if(pendingCameraView){originalDirectedCameraSelect(chase,input,pendingCameraView);}
 // Director vt+0x24 = 15CCF0 (a teleport's camera cut, e.g. the Big Challenge start 1234D0: 11D660's tail and 1234D0's end):
 // DEFAULT_3 set-target 0x176FE0 on every node (no rider check), once per request, with the rider as placed.
 for(;cameraSetTargets>0;--cameraSetTargets){original_camera::Rounding rounding;for(auto& node:chase.director.nodes)original_camera::setTarget(node.algorithm,input);}
 pendingCameraView=0;
 if(pendingCameraFinish){originalDirectedCameraFinish(chase,input);pendingCameraFinish=false;}
 if(eventCameraSeedArmed&&uint32_t(motionTick)>browserEventCameraAnchorTick){ /*motionTick counts this tick: the anchor words are the state after anchor-tick steps*/eventCameraSeedArmed=false;std::array<uint8_t,271> all;all.fill(1);pendingCameraSeed={*browserEventCamera,all};apply_camera_seed();
  // Rolling start (anchor 0 = the ready savestate): the Continue re-places every rider before race tick 0 (rider manager
  // 0x1297C8 -> 11D390: 112180 -> 11D660 and 11DE60 -> 11D660; 11D390 runs while rider+0x880 == 7, as in the ready states).
  // Each 11D660 tail (the human: rider+0x870 < 2) is director vt+0x24 15CCF0 -> 166F28 -> DEFAULT_3 set-target 0x176FE0, so
  // the camera takes two set-targets between the savestate and tick 0's camera update. The target is the placed rider:
  // velocity 0 (11D660 zeroes +0x1E0; 11DF18 writes the start velocity after both), motion 0, crouch 0, the ready head and
  // forward (the seed's +0xA0 head copy and +0x1B0 = rider +0x1B0 of the ready state, bit-equal). PS2 record 0 of
  // weather/dbc2-weather, peak3/the-throne-tuck and bc/bc-race-idle = the savestate words + these two set-targets on every
  // decoded word of +0x000..+0x15C except the pitch +0x54 (2e-5..1.6e-4 rad, gone by tick 6; never reaches the eye).
  if(browserEventRolling){original_camera::Rounding rounding;
   for(auto& node:chase.director.nodes){auto& a=node.algorithm;OriginalCameraInput placed;
    placed.headPosition={a.headCopy[0],a.headCopy[1],a.headCopy[2],1};placed.riderForward={a.lastVelocity[0],a.lastVelocity[1],a.lastVelocity[2],0};
    placed.previousContactNormal=input.previousContactNormal;placed.surfaceId=browserEventGroundProfile().surface.id;placed.riderType=input.riderType;placed.raceRiderCount=input.raceRiderCount;placed.farCap=input.farCap;
    original_camera::setTarget(a,placed);original_camera::setTarget(a,placed);}}
 }
 input.visualRandom=browser_camera_random();
 if(pendingCrashCameraShake>0){original_camera::Rounding rounding;original_camera::requestShake(chase.compositor,input,4,pendingCrashCameraShake,0);pendingCrashCameraShake=0;}
 if(!pendingStageCameraShakes.empty()){original_camera::Rounding rounding;for(const auto& r:pendingStageCameraShakes)original_camera::requestShake(chase.compositor,input,int(r[0]),r[1],r[2]);pendingStageCameraShakes.clear();}
 original_camera_director::Frame directorFrame;directorFrame.raceStateActive=input.raceStateSuppressesShake;auto sourceCamera=originalDirectedCameraStep(chase,input,directorFrame);auto transform=originalCameraTransform(sourceCamera.eye,sourceCamera.yaw,sourceCamera.pitch);cameraRenderView=originalCameraRenderView(transform.position,transform.quaternion);auto view=nativeCameraView(sourceCamera);browser_fog_step(chase.compositor.eye[0],chase.compositor.eye[1],chase.compositor.near,chase.compositor.far);browser_weather_camera_step(chase.compositor.eye[0],chase.compositor.eye[1],chase.compositor.eye[2]);browser_section_viewer(chase.compositor.eye[0],chase.compositor.eye[1],chase.compositor.eye[2],chase.compositor.far);cameraInputs[14]=chase.compositor.shakeIndex;cameraInputs[15]=chase.compositor.shake.active;cameraInputs[16]=chase.compositor.shake.fadeTimer;cameraInputs[17]=crashCameraShakeRequests;for(int i=0;i<3;i++){cameraOut[i]=view.eye[i];cameraOut[i+3]=view.lookAt[i];}cameraOut[6]=view.fovRadians;cameraOut[7]=view.nearMeters;cameraOut[8]=view.farMeters;cameraLastInput=input;cameraLastInputSet=true;return cameraOut;
}
EMSCRIPTEN_KEEPALIVE float* step_camera(float rootX,float rootY,float rootZ){
 auto basis=originalOrientationBasis(physicsState.quaternion);
 Vec3 f{basis.forward[0],basis.forward[2],-basis.forward[1]},right{basis.right[0],basis.right[2],-basis.right[1]},up{basis.up[0],basis.up[2],-basis.up[1]};
 return camera_for_head(position+right*rootX+up*rootY-f*rootZ);
}
EMSCRIPTEN_KEEPALIVE float* step_camera_head(float x,float y,float z){return camera_for_head({x/100.,z/100.,-y/100.});}
// 0x123210 (stage builtin 34, web/stage_teleport.inc) camera cuts at the moment they happen: director vt+0x1C 0x15CC70 before
// the placement and 11D660's tail vt+0x24 0x15CCF0 after it: DEFAULT_3 set-target 0x176FE0 on every node with the rider as it
// is then (head in source cm, as step_camera_head takes it). No camera yet: nothing (the first camera step builds it).
void browser_camera_teleport_cut(float x,float y,float z){if(!chaseReady)return;const auto input=camera_input_for_head({x/100.,z/100.,-y/100.});original_camera::Rounding rounding;for(auto& node:chase.director.nodes)original_camera::setTarget(node.algorithm,input);}
EMSCRIPTEN_KEEPALIVE float* camera_source_input(){return cameraSourceInput;}
EMSCRIPTEN_KEEPALIVE float* camera_render_view(){return cameraRenderView.data();}
// The ready state's camera: the event camera seed's compositor eye (outer+0x100) and look-at (outer+0xE0) as native metres,
// [eye, lookAt] like step_camera_head; null without a seed. A rolling start's objectives card shows it unchanged (WS2 does
// not step the camera: PS2 happiness-ready, the same eye over 900 card frames), main.js readyView. Read-only.
EMSCRIPTEN_KEEPALIVE float* camera_event_seed_view(){RIDER_LOCAL static float v[6];if(!browserEventCamera)return nullptr;const auto& w=*browserEventCamera;
 auto f=[&](unsigned i){return std::bit_cast<float>(w[228+i]);};v[0]=f(4)*.01f;v[1]=f(6)*.01f;v[2]=-f(5)*.01f;v[3]=f(0)*.01f;v[4]=f(2)*.01f;v[5]=-f(1)*.01f;return v;}
// Source centimetres, before compositor terrain lift and shake. Read-only audit.
EMSCRIPTEN_KEEPALIVE float* camera_algorithm_info(){RIDER_LOCAL static float values[6];for(unsigned i=0;i<3;++i){values[i]=cameraHead().outputEye[i];values[i+3]=cameraHead().lookAt[i];}return values;}
EMSCRIPTEN_KEEPALIVE float* camera_inputs(){return cameraInputs;}
// PS2 word view of the live DEFAULT_3 algorithm (228 words = +0x000..+0x38C) followed by the 43 compositor words at the
// outer-camera offsets original_camera_words::compositorOffsets, for word-level comparison with captures (compare-ps2-capture.mjs).
EMSCRIPTEN_KEEPALIVE uint32_t* camera_state_words(){RIDER_LOCAL static uint32_t out[228+43];std::fill(std::begin(out),std::end(out),0u);if(!chaseReady)return out;auto w=original_camera_words::toWords(cameraHead());std::copy(w.begin(),w.end(),out);auto c=original_camera_words::compositorWords(chase.compositor);std::copy(c.begin(),c.end(),out+228);return out;}
// Seed the live DEFAULT_3 algorithm and compositor from captured words (same layout; mask[i]!=0 selects the captured
// words, the rest keep their constructed values), as the camera oracles do; applied after construction.  words==nullptr clears.
static void apply_camera_seed(){if(!pendingCameraSeed||!chaseReady)return;const auto& [in,mask]=*pendingCameraSeed;auto& a=chase.director.nodes.front().algorithm;auto w=original_camera_words::toWords(a);for(unsigned i=0;i<228;++i)if(mask[i])w[i]=in[i];original_camera_words::assignWords(a,w);original_camera_words::assignCompositorWords(chase.compositor,in.data()+228,mask.data()+228);chase.compositor.firstFrameAfterReset=0;pendingCameraSeed.reset();}
EMSCRIPTEN_KEEPALIVE void camera_seed_words(const uint32_t* words,const uint8_t* mask){if(!words){pendingCameraSeed.reset();return;}std::array<uint32_t,271> w;std::array<uint8_t,271> m;std::copy(words,words+271,w.begin());std::copy(mask,mask+271,m.begin());pendingCameraSeed={w,m};apply_camera_seed();}
// QA only: swap the live chase variant without the set-target cut (matches a patched original vtable).
EMSCRIPTEN_KEEPALIVE void camera_variant_qa(int type){(void)original_camera::default3::originalChaseVariant(type);cameraViewType=type;chase.director.preferredType=type;for(auto& node:chase.director.nodes)if(node.algorithm.variantType!=0x44)node.algorithm.variantType=type;}
// Director state for QA: node count, head type, head weight/smoothed, second weight/smoothed, finish pending.
EMSCRIPTEN_KEEPALIVE float* camera_director_info(){RIDER_LOCAL static float values[8];std::fill(std::begin(values),std::end(values),0.f);if(!chaseReady)return values;const auto& d=chase.director;values[0]=float(d.nodes.size());values[1]=float(d.currentType);for(unsigned i=0;i<2&&i<d.nodes.size();++i){values[2+2*i]=d.nodes[i].weight;values[3+2*i]=d.nodes[i].smoothed;}values[6]=pendingCameraFinish;values[7]=float(d.preferredType);return values;}
EMSCRIPTEN_KEEPALIVE int set_camera_view(int type){if(type!=0x3C&&type!=0x3D&&type!=0x3E)throw std::runtime_error("Unknown original camera view");if(type!=cameraViewType){cameraViewType=type;if(chaseReady)pendingCameraView=type;}return cameraViewType;}
EMSCRIPTEN_KEEPALIVE float* jump_takeoff_info(){
 RIDER_LOCAL static float values[21];unsigned at=0;
 for(const auto& v:{lastChargedTakeoff.position,lastChargedTakeoff.velocity,lastChargedTakeoff.normal,lastChargedTakeoff.takeoffNormal,lastChargedTakeoff.forward})for(float x:v)values[at++]=x;
 values[15]=lastChargedTakeoff.charge;values[16]=lastChargedTakeoff.speedLimit;
 values[17]=lastChargedTakeoff.ticksSinceGroundFocus;values[18]=lastChargedTakeoff.riderState;
 values[19]=lastChargedTakeoff.flags;values[20]=lastChargedTakeoff.motionMode;return values;
}
// QA (compare-ps2-capture.mjs): a mid-run baseline seeds the retained speed limit rider+0x2E4 (11B3F8 approaches it).
EMSCRIPTEN_KEEPALIVE void speed_limit_seed(float cm){physicsProfile.speedLimit=cm;effectiveSpeedLimit=cm;speedLimitHeld=false;}
// QA (--seed-limit): a record's +0x2E4 is the limit its own tick uses (11B3F8 ran before the capture's provider hook), so the next
// frame begin keeps it instead of stepping it again.
EMSCRIPTEN_KEEPALIVE void speed_limit_seed_held(float cm){speed_limit_seed(cm);speedLimitHeld=true;}
// QA (compare-ps2-capture.mjs --carry-seed): the motion-0 object's tick stamps (the motion owner +0x10 ground focus, +0x14 last
// ground leave; 13C7A8 scales the velocity by 0.7 + 0.01 x (tick - leave - 40), capped at 1, when motion 0 is entered).
EMSCRIPTEN_KEEPALIVE void ground_tick_seed(uint32_t focus,uint32_t leave){groundFocusTick=focus;lastGroundLeave=leave;}
// Peak runs (modes 6..11; docs/peak-mountain.md): the run's setup placed the rider on the grid slot (11DE60 -> 11D660), and
// the frames before the objectives card's Continue ran in the placement's reset motion: 11B3F8 in motion 1 retained
// +0x2E4 = 0x45505556 and the ground queries left +0x380 = +0x370. Continue places it again at the same slot (11D660 writes
// +0x370 only, 11DF18 the forward x 833.333 velocity), so tick 0 starts from these (PS2 peak1-race-objectives.p2s).
EMSCRIPTEN_KEEPALIVE void peak_run_start_seed(){physicsState.previousNormal=physicsState.normal;physicsProfile.speedLimit=effectiveSpeedLimit=std::bit_cast<float>(0x45505556u);}
EMSCRIPTEN_KEEPALIVE float* physics_info(){
 physicsInfo[0]=physicsProfile.speedLimit/100.f;
 physicsInfo[1]=physicsProfile.surface.id;
 physicsInfo[2]=physicsState.crouch.current;
 physicsInfo[3]=physicsState.boost;
 physicsInfo[4]=grounded?0:1;physicsInfo[5]=effectiveSpeedLimit/100.f;
 physicsInfo[6]=physicsProfile.speedLimit; // rider+0x2E4 in cm/s, exact (QA: compare-ps2-capture.mjs; [0] is m/s and rounds)
 return physicsInfo;
}
EMSCRIPTEN_KEEPALIVE float* boost_info(){
 boostInfo[0]=boostState.meter;boostInfo[1]=boostState.amount;
 boostInfo[2]=boostState.window;boostInfo[3]=boostState.tier;
 boostInfo[4]=boostState.modifier;boostInfo[5]=boostState.superTime;
 boostInfo[6]=boostState.drainEnabled;boostInfo[7]=boostState.feedbackFlags;
 return boostInfo;
}
EMSCRIPTEN_KEEPALIVE float original_axis(float value){if(!std::isfinite(value))throw std::runtime_error("Nonfinite axis");return originalQuantizeAxis(std::clamp(value,-1.f,1.f));}
EMSCRIPTEN_KEEPALIVE void ride_command(float turn,float crouch,float brake,float board,float spin,float flip,int jump,int boost,int grab,int applyTargets){
 if(!std::isfinite(turn)||!std::isfinite(crouch)||!std::isfinite(brake)||!std::isfinite(board)||!std::isfinite(spin)||!std::isfinite(flip))throw std::runtime_error("Nonfinite ride command");
 if(jump<0||jump>1||boost<0||boost>1||grab<0||grab>15||applyTargets<0||applyTargets>1)throw std::runtime_error("Invalid ride command");
 rideCommand={true,turn,crouch,brake,board,spin,flip,jump,boost,grab,applyTargets};
}
EMSCRIPTEN_KEEPALIVE float* ride_axes(){return rideAxes;}
EMSCRIPTEN_KEEPALIVE float* reference_motion(){
 RIDER_LOCAL static float values[20];
 values[0]=physicsState.position[0];values[1]=physicsState.position[1];values[2]=physicsState.position[2];
 values[3]=physicsState.velocity[0];values[4]=physicsState.velocity[1];values[5]=physicsState.velocity[2];
 for(int i=0;i<4;i++)values[6+i]=physicsState.quaternion[i];
 values[10]=grounded;values[11]=physicsState.controlState;values[12]=physicsState.turn.current;values[13]=physicsState.brake.current;values[14]=physicsState.crouch.current;
 values[15]=boostState.meter;values[16]=boostState.amount;values[17]=float(boostState.tier);values[18]=boostState.superTime;values[19]=physicsState.manualSpin;
 return values;
}
// 120F20 under the NIS hold (rider+0xAC4 set, 0x120FD4): 1242B0, the playback ground re-probe, before the motion. Every write:
// - +0x380 = +0x370 (always, 0x12442C);
// - the query: 32E100 over the 13D818 contact's segment (center = +0x110 + +0x3B0 x (+0x780+0x140 x +0x1F0 x 45), from center
//   - 100 x +0x370 to center + 200 x +0x370, preferred fraction 0.5), 3342D0 on the rider's world (+0x860, no cache);
// - distance > 0 with a patch: +0x430 = patch+0x150, +0xAAC/+0xAB0 = the hit's u / v, +0x2D4 = patch+0xA; else +0x430 = -1.
//   1218D0 after the motion sets +0x434 from the track byte when +0x430 != -1 (22E0E0);
// - a miss (distance < 0): +0xB00 = 0, +0x370 = (0, 0, 1) (0x4FF160), +0x3D0 = 0, +0x3A0 = |+0x1B0 - n (n . +0x1B0)|, +0x3B0 =
//   n x +0x3A0, +0xB04 = |+0x1E0|, +0x75C / +0x438 / +0x750 / +0x754 / +0x758 = 0 (+0x454 / +0x460 kept);
// - a hit: (+0xAFC set and +0xB00 0: 111AA0 snow impact at the hit, strength |surface velocity|), +0xB00 = 1, +0x438 = the
//   surface (-1 -> 0), +0x370 = the hit normal, +0x460 = the hit point, +0x3D0 = the surface velocity, +0x3A0 / +0x3B0 as above,
//   +0x454 = (center - hit) . n, +0x750 = min(-(+0xB04 - speed) x 0.0036, 1) when +0xB04 < speed else 0, +0x75C = min(speed x
//   0.00072, 1), +0xB04 = speed, +0x758 = 0.3, +0x754 = 0 (gp-0x789C / -0x7898 / -0x7894). speed = |+0x1E0| (+0x6C0 vt+0x14).
// 123640 (nis_hold) sets +0xB00 = 1 and +0xB04 = 0. The web's terrain is static (surface velocity 0, as the 13D818 contact). It runs
// only for a hold that opts in (nis_hold_probe): mode 1 from +0x110 for an actor with +0xAFC 0, mode 2 from the caller's board-root
// bone for +0xAFC 1 (the probe point and the impact). PS2 +0xAFC: 0 for the CTM approach's computer riders (c0a-ws13 1091..1332) and
// the Transport arrivals' stage holds (peak1-arrive-*, a miss every tick); 1 for the human's CTM approach (c0a-ws13 814..1332, a hit
// every tick), the gondola (14017..14136, a miss every tick) and the booth (fr-booth2 2644: a miss, then hits from 2646). A mesh
// (non-analytic) hit is +0x430 -1 here, as in the contact path.
// PS2 c0a-ws13: the first heat's riders take location 0 on their first held tick at gate + 2 (record 1092); the semi's, placed on the
// grid without a hold, keep the constructor's -1 / 0x31 to the push-off.
// QA: [rider+0x430, +0x434, +0xB00, the surface +0x438]
EMSCRIPTEN_KEEPALIVE int32_t* rider_patch_info(){RIDER_LOCAL static int32_t v[4];v[0]=browserGroundPatch;v[1]=physicsState.riderType;v[2]=nisProbeContact;v[3]=physicsProfile.surface.id;return v;}
// QA: rider+0x460 (the last contact point the painters step at)
EMSCRIPTEN_KEEPALIVE float* rider_contact_point(){return browserTrailContact.data();}
void browser_snow_impact(std::array<float,3> point,std::array<float,3> normal,float strength,int surface); // web/animation_bridge.cpp (111AA0 -> 2E23E0)
static void nis_hold_select_surface(int surface){if(surface!=physicsProfile.surface.id){const float limit=physicsProfile.speedLimit;physicsProfile=physicsMaterials.at(surface);physicsProfile.speedLimit=limit;}}
static void nis_hold_ground_probe(){
 terrain_original::Rounding rounding;using terrain_original::add;using terrain_original::mul;using terrain_original::sub;
 physicsState.previousNormal=physicsState.normal;
 const auto& v=physicsState.velocity;const float speed=terrain_original::sqrt(add(add(mul(v[0],v[0]),mul(v[1],v[1])),mul(v[2],v[2])));
 const float offset=mul(physicsProfile.bodyScale,mul(physicsState.turn.current,45.f));std::array<float,3> center;
 const auto& p=nisProbeMode==2?nisProbeBone:physicsState.position; // 120F20: +0xAFC selects the board-root bone
 for(unsigned k=0;k<3;k++)center[k]=add(p[k],mul(physicsState.lateral[k],offset));
 const Vec3 at{p[0]/100.,p[2]/100.,-p[1]/100.};
 const Vec3 n{physicsState.normal[0],physicsState.normal[2],-physicsState.normal[1]},l{physicsState.lateral[0],physicsState.lateral[2],-physicsState.lateral[1]};
 RayHit h;
 if(cameraTerrain)h=cameraTerrain->sourceGroundContact(at,n,l,physicsState.turn.current,physicsProfile.bodyScale,nullptr,riderScope?&*riderScope:nullptr); // 3342D0 without a cache
 else {h=world->raycast(at+n*2,n*-1,3);}
 if(h.hit&&h.analytic){browserGroundPatch=int(h.resource);browserGroundU=h.u;browserGroundV=h.v;browserPatchFlags=uint16_t(h.patchFlags|0x40);physicsState.forceHeadingBoost=(browserPatchFlags&0x10)!=0;
  physicsState.riderType=originalStreamingLocationIndex(browserStreamingTracks,int32_t(h.resource&0xFF));}
 else browserGroundPatch=-1;
 if(!h.hit){
  nisProbeContact=false;const float distance=physicsState.distance;originalGroundContact(physicsState,physicsState.position,{0,0,1},{});physicsState.distance=distance;
  nisProbeSpeed=speed;audioSpeed75C=0;nis_hold_select_surface(0);audioBrake750=0;audioTurn754=0;audioCompression758=0;
 }else{
  const auto point=sourceVector(h.position),hn=sourceDirection(h.normal);
  if(nisProbeMode==2&&!nisProbeContact)browser_snow_impact(point,hn,0.f,originalGroundSurfaceId(true,h.surface)); // 111AA0: strength |surface velocity|, 0 on the static terrain
  nisProbeContact=true;nis_hold_select_surface(originalGroundSurfaceId(true,h.surface));browserTrailContact=point;
  originalGroundContact(physicsState,point,hn,{});
  physicsState.distance=terrain_original::dot(terrain_original::difference(center,point),hn);
  const float gap=sub(nisProbeSpeed,speed);float brake=0;if(gap<0){brake=mul(-gap,0.0036000001709908247f);if(brake>1)brake=1;}audioBrake750=brake;
  float s75c=mul(speed,0.0007200000109151006f);if(s75c>1)s75c=1;audioSpeed75C=s75c;nisProbeSpeed=speed;audioCompression758=0.30000001192092896f;audioTurn754=0;
 }
 normal={physicsState.normal[0],physicsState.normal[2],-physicsState.normal[1]};
}
EMSCRIPTEN_KEEPALIVE float* step_rider(float steering,int jump,int brake,int boost) {
 physicsState.modeTiming=browser_finish_elapsed(); //rider+0x470 finish marker: 13C948 drops the forward drive and 12E778 hands over to control 10 once it is >= 0 (pipe-run-event 2213)
 // The world job refreshes one rider's scope a tick, in roster order: rider k after the ticks whose record tick is k mod 3 (PS2
 // metro-scope watches of all six rider+0xB50 lists: the human 0, computer riders 1..5 at 1, 2, 0, 1, 2).
 if(motionTick%3==npc_rider_slot()%3)refresh_rider_scope();
 // Air animation selection belongs to controller dispatch, before motion predicts again.
 snapshot_animation_prediction();groundUnclampedVelocity.reset();groundBoardNormalBefore.reset();browserSoftFrame=false; /*set below when 12E778 runs this tick: a tick that returns early (rail, crash) must not keep the last soft tick's flag (PS2 CRA3 Moby 1091: a rail after a soft exit kept 115D48 off)*/browserDeparturePush={};browserDeparturePrePosition.reset();browserGroundResetPending=browserGroundCrashPending=false;
 browserBoardNormalForPose=physicsState.boardNormal; // 11EB98 reads +0x390 as it is; only a ground motion 13D818 tick replaces it (13F2E4) after the pose (rail attach 106848: rail-balance-lr 901)
 float crouchTarget=jump?1.f:0.f,brakeTarget=brake?1.f:0.f;
 bool applyTargets=true;
 rideLatched=false;
 if(rideCommand.active){
  steering=rideCommand.turn;crouchTarget=rideCommand.crouch;brakeTarget=rideCommand.brake;
  rideBoard=rideCommand.board;rideSpin=rideCommand.spin;rideFlip=rideCommand.flip;
  jump=rideCommand.jump;boost=rideCommand.boost;applyTargets=rideCommand.applyTargets!=0;
  rideLatched=true;rideCommand.active=false;
 }
 rideAxes[0]=steering;rideAxes[1]=crouchTarget;rideAxes[2]=brakeTarget;rideAxes[3]=rideBoard;rideAxes[4]=rideSpin;rideAxes[5]=rideFlip;rideAxes[6]=jump;rideAxes[7]=boost;rideAxes[8]=rideCommand.grab;rideAxes[9]=applyTargets;
 if(browserCrashResetReason==-2){browserCrashResetReason=0;if(!browserResetDecline)throw std::runtime_error("Reset controller unavailable");browserResetDecline();} // 1235F8 (web/mission_gameplay.inc mission_rider_decline): entered between ticks, stepped from this tick
 // 11B3F8 is frame-begin work (120F20 -> 0x121024, the first rider pass), including crash/recovery frames: it sees the motion
 // owner +0xDE0 (crash 2, reset/grid 3, rail 4, handplant 5) before this tick's 116120. The provider's ResetPath (Select), the
 // 1210B0 collision timers and the controllers request the reset later in the tick (PS2 tech-select-ground 438 / -air 428,
 // tech-oob-dance 3297: +0x2E4 from motion 0 / 1 on the request tick). A reset entered between ticks (the decline above) is
 // already motion 3; the requests deferred from the previous tick (rail recovery, crash) leave motion 4 / 2, which 11B3F8
 // treats as motion 3. speed_limit_seed_held: a mid-run seed's record already holds this tick's value.
 if(speedLimitHeld)speedLimitHeld=false;
 else physicsProfile.speedLimit=originalGroundSpeedLimit(physicsProfile,physicsState,npc_motion_mode());
 effectiveSpeedLimit=physicsProfile.speedLimit;
 if(browserCrashResetReason){if(!browserResetBegin)throw std::runtime_error("Reset controller unavailable"); /*116120 leaves a rail through the control-7 exit 132048 and the motion-4 exit 13C5A0 (begin_reset), not a full rail reset (tech-select-rail 925)*/browserResetBegin(browserCrashResetReason<0?0:browserCrashResetReason);browserCrashResetReason=0;}
 browserCrashExitFrame=false;
 // 1210B0 (collision timers, before this tick's motion): +0x3F0 direction changes > 4.5 after the decay, or in the air
 // (motion 1) / crash air (motion 2, submode 1) with a live prediction (status 1/3) more than 45 s left (pred+98 -
 // pred+A0), or the air bounce counter +0x3F4 > 5, request 116120(rider,0,2). The reset begins at once: that tick's
 // motion is already the frozen reset motion 3 (tech-oob-dance 3298, tech-oob-hops 3430 in crash air).
 {const bool decayed=originalCollisionHistoryDecay(collisionProfile,collisionHistory);if(decayed)collisionRecoveryRequested=true;
  const bool crashAir=browserCrashActive&&browserCrashAir;bool request=decayed;
  // The air test is 11FE98 == 1 only: a handplant (motion 5), a rail (4) or the reset hold (3) is not air, whatever the retained
  // prediction says (a handplant stall kept status 1 with > 45 s left and re-requested the reset every tick: ARA1 boost 7792).
  if(!request&&((!grounded&&!browserCrashActive&&!browserResetActive&&!browserHandplantMotion5&&!browserRailActive)||crashAir))request=originalCollisionTimerReset(collisionHistory,crashAir?2:1,crashAir,browserTrajectory.status,browserTrajectory.predictedTime,browserTrajectory.elapsed);
  // 1210B0 runs after the controller dispatch: the reset controller 12F398 first steps in the next tick. A request while the
  // reset is already running (3F0 still above 4.5) re-enters it with the progress at 0 (tech-oob-dance: placement 3321).
  if(request&&!browserStarting){if(!browserResetBegin||!browserResetRerequest)throw std::runtime_error("Reset controller unavailable");if(browserResetActive)browserResetRerequest(2);else browserResetBegin(2);browserResetFromController=true;}}
 constexpr double dt=1./60;landingProbeInfo={};landed=0;output[15]=0;Vec3 old=position;
 auto resetMotion=[&](){
  browser_boost_tick(boostState,boostProfile,physicsState.timeScale,3,9);physicsState.boost=boostState.amount;physicsState.boostWindow=boostState.window;physicsState.boostTierCounter=boostState.tier;
  for(auto* value:{&physicsState.turn,&physicsState.brake,&physicsState.crouch,&physicsState.presentationLift,&physicsState.animationTurn,&physicsState.extraLean,&physicsState.boardAlignment,&physicsState.presentationRoll,&physicsState.balance280,&physicsState.adjustment28C,&physicsState.adjustment298})groundControlApproach(*value);
  browser_rail_idle_approach();
  physicsState.quaternion=originalRebuildOrientation(physicsState.quaternion).quaternion;physicsState.controlState=9;airMotionThisTick=false;++motionTick;publish_motion();
 };
 // pv nisTick: world state 14's rider actor holds the rider (control 13 / motion 3, 123640). The pipeline's 1210B0 (the collision timers
 // above, the boost tick 1200D0) and the 1211F8 filters run; control 13 has no tick (0x456C10) and motion 3 only 11E098 (136958).
 if(browserNisHold){
  if(nisProbeMode)nis_hold_ground_probe(); // 120F20 at the start of the rider pass (before the motion; nis_hold_probe)
  browser_boost_tick(boostState,boostProfile,physicsState.timeScale,3,13);physicsState.boost=boostState.amount;physicsState.boostWindow=boostState.window;physicsState.boostTierCounter=boostState.tier;
  for(auto* value:{&physicsState.turn,&physicsState.brake,&physicsState.crouch,&physicsState.presentationLift,&physicsState.animationTurn,&physicsState.extraLean,&physicsState.boardAlignment,&physicsState.presentationRoll,&physicsState.balance280,&physicsState.adjustment28C,&physicsState.adjustment298})groundControlApproach(*value);
  browser_rail_idle_approach();
  physicsState.quaternion=originalRebuildOrientation(physicsState.quaternion).quaternion;physicsState.velocity={0.f,0.f,0.f};physicsState.controlState=13;airMotionThisTick=false;++motionTick;publish_motion();return output;
 }
 // A controller's ResetPath (bit 0x1000 -> 0x116120: requestControl(9), requestMotion(3)) switches
 // this tick; the reset controller 0x12F398 first updates in the next tick's controller dispatch.
 if(browserResetActive&&browserResetFromController){browserResetFromController=false;resetMotion();return output;}
 if(browserResetActive){if(!browserResetControl)throw std::runtime_error("Reset update unavailable");if(browserResetControl()){resetMotion();return output;}browserCrashExitFrame=true;}
 if(browserCrashActive&&browserCrashControl){
  browserCrashExitFrame=!browserCrashControl(jump);
  if(browserResetActive){resetMotion();return output;}
  if(browserCrashActive){
   browser_boost_tick(boostState,boostProfile,physicsState.timeScale,2,8);
   physicsState.boost=boostState.amount;physicsState.boostWindow=boostState.window;physicsState.boostTierCounter=boostState.tier;
   for(auto* value:{&physicsState.turn,&physicsState.brake,&physicsState.crouch,&physicsState.presentationLift,&physicsState.animationTurn,&physicsState.extraLean,&physicsState.boardAlignment,&physicsState.presentationRoll,&physicsState.balance280,&physicsState.adjustment28C,&physicsState.adjustment298})groundControlApproach(*value);
   browser_rail_idle_approach();
   if(!browserCrashMotion)throw std::runtime_error("Missing crash motion stage");browserCrashMotion();
   ++motionTick;distanceRun+=std::hypot(position.x-old.x,position.z-old.z);publish_motion();return output;
  }
  // The source control8 handler returns after recovery. Control0/5 must not
  // dispatch again, but the newly selected motion still advances this tick.
  groundControlApproach(physicsState.animationTurn);
 }

 // a tick the rail step does not reach (an attack held in passive air, 12F7AC) is not a rail release tick: the flag set by the
 // previous tick's Stop::Airborne kept animation_tick from running 12F730 for the rest of the flight (PS2 hl-rail-15 721: control 5)
 if(browserRailTickBegin)browserRailTickBegin();
 bool handplantSkipsRail=false;
 const bool attackHoldsAir=browserAttackAir&&!browserCrashExitFrame&&browserAttackAir(); //12F7AC returns before 0x107578/0x106848
 if(!attackHoldsAir&&browserHandplantStep&&!browserCrashExitFrame&&browserHandplantStep(steering,crouchTarget,brakeTarget,handplantSkipsRail)){++motionTick;distanceRun+=std::hypot(position.x-old.x,position.z-old.z);publish_motion();return output;}
 railStepConsumed=false; // the rail motion (motion 4) owned this tick, including the tick it loses the rail
 browserRailBoostTicked=false;
 if(browserRailStep&&!browserCrashExitFrame&&!handplantSkipsRail&&!attackHoldsAir&&browserRailStep(steering,jump,brake,boost)){railStepConsumed=true;++motionTick;distanceRun+=std::hypot(position.x-old.x,position.z-old.z);publish_motion();return output;}
 // 1211F8 approaches the rail triplets +0x22C / +0x238 / +0x25C every tick, also when 12F730 returned early for an attack hold
 // (12F7AC): a fading rail cycle 18..20 keeps reading +0x238 (PS2 hl2/rail-rnb-s1 739: L1+R1 held after leaving the rail).
 if(attackHoldsAir&&browserRailStep&&!browserCrashExitFrame&&!handplantSkipsRail)browser_rail_idle_approach();
 browserBoardPressFrame=(browserFinishStep&&!browserCrashExitFrame&&!attackHoldsAir&&browserFinishStep()!=0)||(browserBoardPressStep&&!browserCrashExitFrame&&!attackHoldsAir&&browserBoardPressStep()!=0); /*control 10 also owns the controller slot*/ //0x1161D0 entry or 0x12FC80 (runs its own 0x114130)
 if(browserBoardPressFrame&&browserCrashActive){ //0x130DD0 -> 0x10EB30 crash in the controller slot: 0x1200D0, 0x1211F8, then motion 2 this tick
  browser_boost_tick(boostState,boostProfile,physicsState.timeScale,2,8);physicsState.boost=boostState.amount;physicsState.boostWindow=boostState.window;physicsState.boostTierCounter=boostState.tier;
  for(auto* value:{&physicsState.turn,&physicsState.brake,&physicsState.crouch,&physicsState.presentationLift,&physicsState.animationTurn,&physicsState.extraLean,&physicsState.boardAlignment,&physicsState.presentationRoll,&physicsState.balance280,&physicsState.adjustment28C,&physicsState.adjustment298})groundControlApproach(*value);
  if(!browserCrashMotion)throw std::runtime_error("Missing crash motion stage");browserCrashMotion();
  ++motionTick;distanceRun+=std::hypot(position.x-old.x,position.z-old.z);publish_motion();return output;
 }
 const bool uberFrame=browser_rail_uber_control();
 browserStartFrame=browserStarting;
 if(browserStarting){if(!browserStartControl)throw std::runtime_error("Start controller unavailable");browserStartControl();
  if(browserStartFrozen){browser_boost_tick(boostState,boostProfile,physicsState.timeScale,3,6);for(auto* v:{&physicsState.turn,&physicsState.brake,&physicsState.crouch,&physicsState.presentationLift,&physicsState.animationTurn,&physicsState.extraLean,&physicsState.boardAlignment,&physicsState.presentationRoll})groundControlApproach(*v);charge=physicsState.crouch.current;
   // motion 3's tick 136958 is 11E098: the quaternion is renormalised every held tick (an identity in mode-1 arithmetic, not on the
   // console's: docs/ps2-float.md)
   physicsState.quaternion=originalRebuildOrientation(physicsState.quaternion).quaternion;
  airMotionThisTick=false;++motionTick;publish_motion();return output;}
 }
 browserSoftFrame=browserSoftActive;
 if(browserSoftFrame){
  auto result=originalSoftControlStep(physicsProfile,physicsState,{grounded?0:1,softComplete,false},{grounded?steering:-steering,0,bool(boost&&!boostHeld),bool(boost),false});
  if(result.requestBoost)originalBoostControl(boostState,boostProfile,result.boostHeld,result.boostPressed);
  if(result.restoreStance&&physicsState.prewindStyle!=0){if(!browserRestoreStance)throw std::runtime_error("Missing stance-restoration callback");browserRestoreStance(physicsState.prewindStyle);}
  if(result.nextControl==0||result.nextControl==4){browserSoftActive=false;physicsState.controlState=result.nextControl;softTransition=result.nextControl;held=false;charge=0;}
  if(result.stop==OriginalSoftControlResult::Stop::Recovery)collisionRecoveryRequested=true;
 }
 if(browserSoftFrame){
  groundControlApproach(physicsState.animationTurn);
  if(!grounded){groundControlApproach(physicsState.turn);groundControlApproach(physicsState.crouch);groundControlApproach(physicsState.brake);}
 }
 // Original1211F8 also advances presentation triplets once per tick.
 const bool chargedRelease=grounded&&held&&!jump&&!browserCrashExitFrame&&!browserSoftFrame&&!uberFrame&&!browserStartFrame;
 auto advancePresentationFilters=[&](){
  browserAdjustBefore28C=physicsState.adjustment28C;
  browserAdjustBefore298=physicsState.adjustment298;
  browserAdjustFiltersApproached=true;
  for(auto* value:{&physicsState.presentationLift,&physicsState.extraLean,&physicsState.boardAlignment,&physicsState.presentationRoll,&physicsState.adjustment28C,&physicsState.adjustment298})groundControlApproach(*value);
 };
 browserAdjustFiltersApproached=false;
 if(!chargedRelease)advancePresentationFilters();
 // 114130 controller dispatch precedes the 1200D0 timer stage. Cruise
 // jump-entry and crouch-release return before boost dispatch; airborne
 // Square is a tweak input, not permission to thrust.
 // The first air tick after a charged release runs control 5 (133308, which stops the boost), not a ride-off's control 0 (the
 // port's controlState still reads 0 there; PS2 hl-sj-2 913: +0x2FC 0 on the first air tick).
 {const bool airControl0=!grounded&&physicsState.controlState==0;airControl0Request=airControl0&&!wasAirControl0&&!releasedLastTick;wasAirControl0=airControl0;}
 releasedLastTick=chargedRelease;
 // 12E9B8 (control 2) with JumpHeld (word0 0x2000) calls 114130(rider, BoostHeld, 0) at 0x12EB58 whatever the motion: Cross held
 // through a passive departure keeps the boost (and its drain) in the air (PS2 hl-glide-3 1430). Released, it never calls 114130.
 const bool airCrouch=!grounded&&physicsState.controlState==2&&held;
 // a controller that already ran 114130 this tick or returned before it: control 7 leaving for the air (its Stop::Airborne), or 12F730
 // returning at 12F7AC with an attack held (PS2 hl2/rail-rnb-s1b 3057, hl2/uber-row8 883: +0x2FC stays for that tick)
 if(browserRailBoostTicked||browserRailControllerRan||attackHoldsAir){}
 else if(!browserCrashExitFrame&&!browserSoftFrame&&!uberFrame&&!browserStartFrame&&grounded&&bool(jump)==held&&!browserBoardPressFrame)
  originalBoostControl(boostState,boostProfile,boost,!held&&boost&&!boostHeld);
 else if(!browserCrashExitFrame&&!browserSoftFrame&&!uberFrame&&!browserStartFrame&&!browserBoardPressFrame&&airCrouch){if(jump)originalBoostControl(boostState,boostProfile,boost,false);}
 // In the air, control 0 (131620 on the ride-off tick) only requests control 4 and returns before its boost dispatch; the
 // passive controller 12F730 stops the boost in the next tick (tech-speedcap-groomed 421: amount still 1 on the PS2).
 else if(!browserCrashExitFrame&&!browserSoftFrame&&!uberFrame&&!browserStartFrame&&!browserBoardPressFrame&&!grounded&&!airControl0Request&&!browserRailJumpRelease) originalBoostControl(boostState,boostProfile,false,false); /*PS2 hl2/rail-slide 737: +0x2FC still 0.25 on a rail jump release*/
 boostHeld=boost;
 if(!browserRailBoostTicked)browser_boost_tick(boostState,boostProfile,physicsState.timeScale,grounded?0:1,(browserStartFrame||uberFrame||browserSoftFrame||browserCrashExitFrame||browserBoardPressFrame)?physicsState.controlState:grounded?(jump?2:0):5);
 physicsState.boost=boostState.amount;physicsState.boostWindow=boostState.window;
 physicsState.boostTierCounter=boostState.tier;
 Vec3 f={0,0,1};
 if(grounded){
  //13D818 consumes the surface frame retained by the preceding contact.
  // Reprojecting the newly aligned physical forward here changes next-frame forces.
  f={physicsState.forward[0],physicsState.forward[2],-physicsState.forward[1]};
  const int controlAtTickStart=physicsState.controlState; //the controller that runs this tick (a held Cross after a touchdown first runs 131620, which requests control 2)
  if(browserHumanRider)physicsState.timeScale=1; /*+0x300: written only by the NPC provider (120090) and reset 11D660*/if(!browserCrashExitFrame&&!browserSoftFrame&&!uberFrame&&!browserStartFrame&&!browserBoardPressFrame)physicsState.controlState=jump?2:0;
  if(browserAttackCruise&&applyTargets&&!browserCrashExitFrame&&!browserSoftFrame&&!uberFrame&&!browserStartFrame&&!browserBoardPressFrame)browserAttackCruise(steering); //1317FC..13182C
  const bool heldPrewind=!browserCrashExitFrame&&!browserSoftFrame&&!uberFrame&&!browserStartFrame&&!browserBoardPressFrame&&jump&&held&&controlAtTickStart==2; //12E9B8 with JumpHeld (control 2 continuing; tech-oob-hops 3018: Cross held through a touchdown runs 131620 first)
  if(applyTargets&&!heldPrewind&&!browserCrashExitFrame&&!browserSoftFrame&&!uberFrame&&!browserStartFrame&&!browserBoardPressFrame)groundTurnTarget(physicsState.turn,steering,physicsState.velocity,physicsProfile.surface.id);
  if(applyTargets&&!browserCrashExitFrame&&!browserSoftFrame&&!uberFrame&&!browserStartFrame&&!browserBoardPressFrame&&!(jump&&!held))groundCrouchBrakeTargets(physicsState.crouch,physicsState.brake,crouchTarget,brakeTarget,float(dot(velocity,f)*100),physicsState.turn.current);
  // 12E9B8 held path after 113F88(1,0): channel-2 class 10 -> 113E80(PrewindTurn), prewind targets 0; requested 21 (the reverse
  // clip) -> 113E80(0), prewind targets 0; else with rider+0x328 == 0 and +0x2DC == 0 the 114CC0 reverse turn (riding backwards
  // after a crouched spin landing, tech-reprewind 497) plays 21 (3128E8 -1, a2 0) and returns; else prewind targets, 113E80, 12EE30.
  browserPrewindBranch=0;
  if(heldPrewind){
   const int prewindClass=browser_channel2_class(),prewindSemantic=browser_channel2_semantic();
   if(prewindClass==10)browserPrewindBranch=1;else if(prewindSemantic==21)browserPrewindBranch=2;
   else if(physicsState.prewindStyle==0&&physicsState.manualSpin==0){auto reverse=originalReverseTurn(physicsState,physicsState.balance280);
    if(reverse.reversed){browserPrewindBranch=3;++reverseTurnSerial;physicsState.animationIndex=21;physicsState.animationClass=7;if(browserReverseAnimation)browserReverseAnimation(reverse.animationRootQuaternion);browser_controller_stance(physicsState.reverseStance);}}
   if(applyTargets&&browserPrewindBranch!=3)groundTurnTarget(physicsState.turn,browserPrewindBranch==2?0.f:steering,physicsState.velocity,physicsProfile.surface.id);
  }
  if(!browserCrashExitFrame&&!browserSoftFrame&&!uberFrame&&!browserStartFrame&&!browserBoardPressFrame&&!jump&&!held)browser_ground_upper_reactions(0); // 115B58 / 115D48 before 114CC0 (animation_bridge.cpp)
  if(!browserCrashExitFrame&&!browserSoftFrame&&!uberFrame&&!browserStartFrame&&!browserBoardPressFrame&&!jump&&!held&&physicsState.animationIndex!=22&&physicsState.animationIndex!=21&&physicsState.manualSpin==0){
   auto reverse=originalReverseTurn(physicsState,physicsState.balance280);
   if(reverse.reversed){++reverseTurnSerial;physicsState.animationIndex=21;physicsState.animationClass=7;if(browserReverseAnimation)browserReverseAnimation(reverse.animationRootQuaternion);browser_controller_stance(physicsState.reverseStance);}
  }
  if(physicsState.animationIndex==21&&!browserBoardPressFrame&&!heldPrewind){physicsState.animationTurn.target=physicsState.brake.target=0;physicsState.animationTurn.rate=physicsState.brake.rate=0.03333333507180214f;} //131620 (control 0) only
  // 131870 main-animation selection precedes 1211F8/13D818 (animation_bridge.cpp).
  if(!browserCrashExitFrame&&!browserSoftFrame&&!uberFrame&&!browserStartFrame&&(!browserBoardPressFrame||physicsState.controlState==2))browser_ground_controller_animation(jump,steering,brakeTarget,jump?1.f:crouchTarget,rideLatched?rideSpin:steering,rideLatched?rideFlip:0.f); //131620 f22: crouch axis, 1 while jump is held
  // Controller takeoff reads retained crouch before1211F8 advances filters.
  const float takeoffCharge=physicsState.crouch.current;
  // Charged release sets air targets before its one filter tick in animation_tick.
  if(!(held&&!jump&&!browserCrashExitFrame&&!browserSoftFrame&&!uberFrame&&!browserStartFrame)){groundControlApproach(physicsState.turn);groundControlApproach(physicsState.crouch);groundControlApproach(physicsState.brake);}
  if(browserSoftFrame||!(held&&!jump)){
  browser_wind_push(false); // 13D818 begins with 125970
  auto diag=originalGroundIntegrate(physicsProfile,physicsState);audioCompression758=diag.compression; /*13C878 -> rider+0x758*/browserBoardNormalForPose=diag.boardNormalForPose;originalGroundPresentationTarget(physicsProfile,physicsState,diag);OriginalHeadingState heading;heading.relativeVelocity=diag.relativeVelocity;heading.normal=physicsState.normal;heading.forward=physicsState.forward;heading.lateral=physicsState.lateral;heading.bodyForward=physicsState.physicalForward;heading.turn=physicsState.turn.current;heading.charge=physicsState.crouch.current;heading.dt=diag.stepTime;heading.controlState=physicsState.controlState;heading.manualSpin=physicsState.manualSpin;heading.reverseStance=physicsState.reverseStance;const float headingDelta=originalGroundHeading(physicsProfile.headingProfile,heading);physicsState.manualSpin=heading.manualSpin;if(headingDelta!=0){auto rebuilt=originalRebuildOrientation(originalRotateOrientation(physicsState.quaternion,physicsState.normal,headingDelta));physicsState.quaternion=rebuilt.quaternion;physicsState.physicalForward=rebuilt.forward;physicsState.boardUp=rebuilt.up;}
  OriginalAirState physical{physicsState.position,physicsState.velocity};position=physical.nativePosition();velocity=physical.nativeVelocity();
  }
  if(!browserCrashExitFrame&&!browserSoftFrame&&!uberFrame&&!browserStartFrame&&held&&!jump){
   OriginalJumpState takeoff;takeoff.position=physicsState.position;takeoff.velocity=physicsState.velocity;
   takeoff.normal=physicsState.normal;takeoff.takeoffNormal=physicsState.previousNormal;takeoff.forward=physicsState.forward;
   takeoff.boardUp=physicsState.boardUp;takeoff.speedLimit=physicsProfile.speedLimit;takeoff.charge=takeoffCharge;
   takeoff.ticksSinceGroundFocus=motionTick-groundFocusTick;takeoff.riderState=physicsState.riderType;takeoff.flags=browserPatchFlags;
   lastChargedTakeoff=takeoff;
   originalJumpTakeoff(takeoff);retain_jump_camera(takeoff);browser_score_takeoff(takeoff.rampTakeoff);browser_controller_takeoff_wind(takeoff.velocity);
   const OriginalAirState launched{takeoff.position,takeoff.velocity};position=launched.nativePosition();velocity=launched.nativeVelocity();
   begin_airborne();advancePresentationFilters();charge=0;
  }
  if(!browserCrashExitFrame)charge=originalJumpChargeStep(charge,!browserSoftFrame&&!uberFrame&&!browserStartFrame&&jump);
 }else {airtime+=dt;if(airbornePrewind&&jump)charge=originalJumpChargeStep(charge,true);else{charge=0;airbornePrewind=false;}}
 if(!browserCrashExitFrame)held=!browserSoftFrame&&!uberFrame&&!browserStartFrame&&jump;
 airMotionThisTick=!grounded;
 if(uberFrame&&!grounded){groundControlApproach(physicsState.turn);groundControlApproach(physicsState.crouch);groundControlApproach(physicsState.brake);}
 if(uberFrame)groundControlApproach(physicsState.animationTurn);
 if(!grounded){airMotionStart={true,physicsState.position,physicsState.velocity,physicsState.normal,physicsState.surfaceVelocity};save_air_motion_prediction();browser_wind_push(true);translate_air_motion();}else airMotionStart.valid=false;
 RayHit h;
 if(grounded){
  // Ground contact retains old+370 into+380 after controller takeoff.
  physicsState.previousNormal=physicsState.normal;
  const auto previousSurfaceVelocity=physicsState.surfaceVelocity;
  Vec3 lateral{physicsState.lateral[0],physicsState.lateral[2],-physicsState.lateral[1]};
  if(cameraTerrain)h=cameraTerrain->sourceGroundContact(position,normal,lateral,physicsState.turn.current,physicsProfile.bodyScale,&groundCache,riderScope?&*riderScope:nullptr); // 13D818's ground query sees the rider's scope list
  else {h=world->raycast(position+normal*2,normal*-1,3);if(h.hit){auto offset=position-h.position;h.contactSignedDistanceCm=dot(offset,h.normal)*100;auto tangent=offset-h.normal*dot(offset,h.normal);h.contactLateralDistanceCm=std::sqrt(dot(tangent,tangent))*100;}}
  const int selected=originalGroundSurfaceId(h.hit,h.surface);
  bool leave=!h.hit;
  if(h.hit){browserGroundPatch=h.analytic?int(h.resource):-1;if(h.analytic){browserPatchFlags=uint16_t(h.patchFlags|0x40);physicsState.forceHeadingBoost=(browserPatchFlags&0x10)!=0;physicsState.riderType=originalStreamingLocationIndex(browserStreamingTracks,int32_t(h.resource&0xFF));}browserGroundU=h.u;browserGroundV=h.v;browserTrailContact=sourceVector(h.position);originalGroundContact(physicsState,sourceVector(h.position),sourceDirection(h.normal),{});physicsState.distance=h.contactSignedDistanceCm;physicsState.contactClearance=h.contactLateralDistanceCm;leave=physicsProfile.airHeight>0&&physicsState.flags308&&physicsState.distance>physicsProfile.airHeight;}
  else {const float distance=physicsState.distance;originalGroundContact(physicsState,physicsState.position,{0,0,1},{});physicsState.distance=distance;physicsState.contactClearance=0;}
  normal={physicsState.normal[0],physicsState.normal[2],-physicsState.normal[1]};
  groundContactInfo[0]=std::sqrt(dot(velocity,velocity));groundContactInfo[2]=dot(velocity,normal)*100;groundContactInfo[4]=selected;
  originalGroundVelocityContact(physicsState,selected,previousSurfaceVelocity);
  bool rotated=false;auto q=physicsState.quaternion;
  if(!leave)q=originalGroundAlignment(q,physicsState.normal,physicsState.boardUp,physicsState.contactClearance,physicsProfile.alignmentRate,&rotated);
  if(rotated)q=originalRebuildOrientation(q).quaternion;
  auto basis=originalRebuildOrientation(q);physicsState.quaternion=basis.quaternion;physicsState.physicalForward=basis.forward;physicsState.boardUp=basis.up;
  if(selected!=physicsProfile.surface.id){const float limit=physicsProfile.speedLimit;physicsProfile=physicsMaterials.at(selected);physicsProfile.speedLimit=limit;}
  auto relative=terrain_original::difference(physicsState.velocity,physicsState.surfaceVelocity);
#if SSX_PS2_EXACT_FPU
  // 13D8F0: dt = mul.s rider+0x300 (fs) x gp-0x7064 1/60 (ft); on the console 1.0 as fs comes back one ULP low (docs/ps2-float.md).
  originalGroundVisualTargets(physicsProfile,physicsState,software_float::exactArithmetic?ps2fpu::mul(physicsState.timeScale,std::bit_cast<float>(0x3c888889u)):physicsState.timeScale/60.f,relative);
#else
  originalGroundVisualTargets(physicsProfile,physicsState,physicsState.timeScale/60.f,relative);
#endif
  if(!leave)originalGroundBoardLift(physicsState,relative);
  groundBoardNormalBefore=physicsState.boardNormal;originalGroundBoardNormal(physicsState);
  // 13F358 is after the contact velocity correction. Clamping first changes
  // the non-scaling vertical correction once the rider is on the speed limit.
  // On a departure tick 13F178 calls 114298 first (13F194) and clamps at 13F358 afterwards.
  // 13F178 runs 13F488 and 105398 (with 106F78) before this clamp: the post-pose contact phase
  // (animation_bridge) restores the unclamped velocity for its responses and clamps afterwards.
  if(!leave){groundUnclampedVelocity=physicsState.velocity;audio_ground_speed(physicsState.velocity);originalGroundClampSpeed(physicsState,physicsProfile.speedLimit);}
  velocity=OriginalAirState{physicsState.position,physicsState.velocity}.nativeVelocity();
  groundContactInfo[1]=std::sqrt(dot(velocity,velocity));groundContactInfo[3]=dot(velocity,normal)*100;
  // 13F178 (no departure): surface 18 (+0x438) is a hard crash (13F1C8 -> 10EB30), else a surface with table+0x44 or a patch
  // flag 2 is the 116120 reset (13F23C). Both run in the post stage (animation_bridge.cpp).
  if(!leave&&h.hit&&!browserResetActive&&selected==18)browserGroundCrashPending=true;
  else if(!leave&&h.hit&&h.analytic&&!browserResetActive&&(browser_surface_property(selected)!=0||(browserPatchFlags&2)))browserGroundResetPending=true;
  if(leave){
   //13F194..13F1A0: terrain departure calls114298 with charge -1 before air motion.
   OriginalJumpState passive;passive.position=physicsState.position;passive.velocity=physicsState.velocity;
   passive.normal=physicsState.normal;passive.takeoffNormal=physicsState.previousNormal;passive.forward=physicsState.forward;
   passive.boardUp=physicsState.boardUp;passive.speedLimit=physicsProfile.speedLimit;passive.charge=-1;
   passive.ticksSinceGroundFocus=motionTick-groundFocusTick;passive.riderState=physicsState.riderType;passive.flags=browserPatchFlags;
   originalJumpTakeoff(passive);retain_jump_camera(passive);browser_score_takeoff(passive.rampTakeoff);if(passive.groundLeaveSentinel)lastGroundLeave=0xffffffffu;
   // 114298 writes rider+0x110/+0x1E0 directly; keep the source state authoritative through the departure tick.
   // 13F178 runs after the pose: the near-vertical takeoff push (+4 cm along the wall normal, 114A58) is not in
   // this tick's pose (animation_bridge builds it from position - browserDeparturePush).
   {terrain_original::Rounding rounding;for(unsigned k=0;k<3;k++)browserDeparturePush[k]=terrain_original::sub(passive.position[k],physicsState.position[k]);}browserDeparturePrePosition=physicsState.position;
   physicsState.position=passive.position;physicsState.velocity=passive.velocity;
   // 13F488 / 105398 / 107888 and 11FE78(1) -> 1399E0 -> 1135B8 (the flight's seed) still see this velocity: the post stage restores
   // it (animation_bridge.cpp groundClampAfterContacts), seeds the predictor, then clamps as 13F358 does.
   groundUnclampedVelocity=physicsState.velocity;
   audio_ground_speed(physicsState.velocity);originalGroundClampSpeed(physicsState,physicsProfile.speedLimit); //13F358 after the departure call
   passive.velocity=physicsState.velocity;
   position={passive.position[0]/100.,passive.position[2]/100.,-passive.position[1]/100.};
   velocity=OriginalAirState{passive.position,passive.velocity}.nativeVelocity();
   begin_airborne(!browserSoftFrame&&jump);
  }
 }else if(!browserPosedLandingEnabled&&motionTick-hostDepartureTick>10){
  // Headless diagnostics have no posed board. The origin stays inside this
  // slope's vertical band through takeoff, so those frames are not a landing.
  // Powder depth is centimeters; a rider who leaves inside it sinks about one
  // extra depth during the ten skipped ticks and is still in that column.
  const double ceiling=std::max(old.y,position.y)+1.3;h=terrain_down({position.x,ceiling,position.z},30);
  if(h.hit){if(h.normal.y<0)h.normal=h.normal*-1;
   const int surface=std::clamp(h.surface,0,18);
   const double depthM=std::max(0.0,double(landingProfile.materials.at(surface).depth3)*double(landingProfile.bodyScale)*0.01);
   const double window=std::max(0.15,depthM+depthM);
   if(h.normal.y>.35&&dot(velocity,h.normal)<0&&position.y<=h.position.y+.025&&old.y>=h.position.y-window){
    physicsState.position=sourceVector(position);physicsState.velocity=sourceVector(velocity);
    OriginalWorldSegmentHit contact;contact.fraction=.5f;contact.position=sourceVector(h.position);contact.normal=sourceDirection(h.normal);contact.surface=h.surface;
    resolve_touchdown(contact,motionTick,h.terrainQuery);
   }
  }
 }
 if(browserStartFrame)charge=physicsState.crouch.current;
 ++motionTick;
 distanceRun+=std::hypot(position.x-old.x,position.z-old.z);
 // Host failsafe for a non-finite position only. No air-time or height limit: the original resets long airs itself
 // (1210B0: a prediction with more than 45 s left, or more than 5 air bounces; originalCollisionTimerReset above), and a
 // super-big Snow Jam air (charged kicker + trick boost) stays up well past 6 s. Snow Jam's finish lies >1800 m below the grid.
 if(!std::isfinite(position.y)){const int next=respawns+1;reset_rider(spawn.x,spawn.y,spawn.z,startYaw);respawns=next;}
 publish_motion();return output;
}
}
#include "replay_camera.inc" // the replay view (web/replay.js, docs/replay.md)
#ifdef SSX_SNAPSHOT_REGISTRY // the rider-context snapshot's registry (web/generate-snapshot-registry.mjs, docs/replay.md §2a)
#include "generated/snapshot/core.inc"
#endif

#if SSX_PS2_EXACT_FPU
// The arithmetic profile (docs/ps2-float.md): the capture comparers run their setup in mode 1 and the capture on the console model.
extern "C" EMSCRIPTEN_KEEPALIVE void ps2_arith_exact(int on){ssx::software_float::exactArithmetic=on!=0;}
#endif
