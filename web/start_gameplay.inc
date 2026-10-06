#include "../engine/start_control.hpp"
#include "generated/event_start_seed.hpp"
extern void apply_snow_jam_dead_nodes();
RIDER_LOCAL extern bool browserStarting,browserStartFrame,browserStartFrozen;
RIDER_LOCAL extern bool (*browserStartControl)();RIDER_LOCAL extern void (*browserStartClear)();
extern "C" {void begin_event_clock();int event_phase();int event_ticks();void begin_event_rider();void start_ground_motion();void reset_animation();}
RIDER_LOCAL static OriginalStartControlState startControl;RIDER_LOCAL static float startAxis=0,startTilt=0;
// The provider's control-6 word0 (127848 0x1285F4..0x128618: the raw GateAnticipate axis x 31, cvt.w, bits 12..17) when the pad provider
// produced this tick's command (web/input_bridge.inc). Re-encoding the decoded axis instead gives 30 for a full stick (31 x 1/31 is one ULP
// under 1); the start controller reads only its sign, so the two agree on everything but the word.
RIDER_LOCAL static uint32_t startCommand=0;
RIDER_LOCAL static bool startCommandFromPad=false;
static void clear_start(){browserStarting=browserStartFrame=browserStartFrozen=false;startControl={};startAxis=0;startCommand=0;startCommandFromPad=false;graph.startPose=0;}
static bool step_start(){
 OriginalRounding rounding;OriginalStartControlInput in;
 if(startCommandFromPad)in.command=startCommand&0x3f000u;
 else in.command=(uint32_t(int32_t(terrain_original::mul(std::clamp(startAxis,-1.f,1.f),31.f)))&63)<<12;
 in.gamePhase=event_phase();in.raceTicks=event_ticks();in.delaySeconds=browserEventStartDelay;
 in.motion=browserStartFrozen?3: ::grounded?0:1;in.semantic=graph.requestedSemantics[2];in.mainTime=graph.channelProgress(2);in.tilt=startTilt;in.forward=physicsState.physicalForward;
 for(const auto&a:originalStartControlStep(startControl,in)){
  switch(a.kind){
   case OriginalStartAction::StopBoost:originalBoostControl(boostState,boostProfile,false,false);break;
   case OriginalStartAction::Tilt:physicsState.presentationLift={a.amount,0,a.amount};break;
   case OriginalStartAction::Crouch:groundCrouchBrakeTargets(physicsState.crouch,physicsState.brake,a.amount,0,terrain_original::dot(physicsState.velocity,physicsState.forward),physicsState.turn.current);break;
   case OriginalStartAction::Velocity:physicsState.velocity=a.vector;controllerGround.velocity=a.vector;commit_rider_physics();break; //120378 runs after the controller: the push-off wind picks the hair semantic
   case OriginalStartAction::Motion:if(a.value!=0)throw std::runtime_error("Unsupported start motion");start_ground_motion();break;
   case OriginalStartAction::Play:if(!graph.enter(a.value,-1,~uint64_t(0),true))throw std::runtime_error("Missing start continuation animation");break;
   case OriginalStartAction::Control:physicsState.controlState=gs.controlState=a.value;browserStarting=false;heldAirMode=false;passiveMode=a.value==4;if(passiveMode){gs=physicsState;originalPassiveAirBegin(passive,gs,prewind);}break;
  }
 }
 graph.startPose=startControl.pose;return browserStarting;
}
static void setup_start(){browserStartControl=step_start;browserStartClear=clear_start;clear_start();}
extern "C" EMSCRIPTEN_KEEPALIVE void start_input(float axis){if(!std::isfinite(axis))throw std::runtime_error("Invalid start input");startAxis=axis;startCommandFromPad=false;}
void stage_event_visual_seed_apply(); // web/stage_world.inc: ready-state visual words, LCG and breath
extern "C" void place_rider_region(float,float,float,float,float,float,int); // web/animation_bridge.cpp (11D390)
void weather_event_seed_apply(); // web/weather.inc: the ready state's Weather painters, snowfall layers, camera splash
void weather_event_contact_seed(); // web/weather.inc: the grid contact rider+0x460 of the ready state (the painters' point)
// event_grid_start (web/animation_bridge.cpp, a CTM event in the streamed world) runs this with the world kept: no world reset at an
// offline event start (2300F0 is online only), so the stage world (the Big Challenge markers' Hide nodes of 308DB8, pickups,
// crashbag rollers) stays as free ride left it (PS2 c0a-full: flg_ARA1_BigCFlag_1002 a type-16 node from 2066 through the race).
RIDER_LOCAL bool browserEventWorldKept=false;
// 11D390's event branch (0x11D564) on the rider as it stands, without the Single Event anchor's seed (pv eventReturnInWorld: world
// state 14's enter 0x236250 runs 11D390(human) at 0x2363DC after a results Transport, before the map, while the event kind still
// holds; PS2 c0a-ret7: its 112180 re-attaches the human's route; core transport_map_enter): 11FE78(3) and 11FEC8(6), the grid hold's
// control entry (the start controller, as start_event enters it) and the tail's +0x2E8 / +0x2EC / +0x2F0 / +0x2F4 = 0. The rider's
// own words stay (its presentation lift, limit, normals).
extern "C" void grid_hold_enter(){
 boostState.window=physicsState.boostWindow=0;boostState.modifier=0;boostState.superTime=0;boostState.tier=physicsState.boostTierCounter=0;
 startControl={};const auto entry=originalStartControlEnter(startControl,physicsState.reverseStance);startTilt=physicsState.presentationLift.current;
 graph.sequences.clear();graph.sampledLocal.reset();graph.requestedSemantics.fill(438);graph.defaultMirror=entry.mirror;graph.defaultRoot={};const auto sc=originalSinCos(entry.rootHalfAngle);graph.defaultRoot.rotation={0,0,sc[0],sc[1]};
 if(!graph.enter(entry.semantic,-1,~uint64_t(0),true))throw std::runtime_error("Missing original grid animation");
 graph.startPose=startControl.pose;physicsState.controlState=6;gs=physicsState;physicsState.animationIndex=entry.semantic;physicsState.animationClass=graph.currentClass(2);previousGround=true;previousHeld=false;browserStarting=browserStartFrame=browserStartFrozen=true;
}
extern "C" EMSCRIPTEN_KEEPALIVE void start_event(){
 apply_snow_jam_dead_nodes();reset_animation();stage_event_visual_seed_apply();weather_event_seed_apply();if(!browserEventWorldKept)browser_reset_pickups();begin_event_clock();begin_event_rider();if(!eventRouteLoaded)throw std::runtime_error("Missing event route seed");resetRoute=initialEventRoute;/*grid anchor retains path 2 at distance 0 (rider+490..4CC)*/
 // Backcountry rival events (docs/backcountry.md): no countdown and no start controller. 234AD0 sends event kinds 4..6 to
 // 233AA0; the riders keep the ready state (control 0, motion 0, start velocity) and ride from the first tick.
 // The Continue re-places the riders before tick 0 (rider manager 0x1297C8 -> 11D390 while rider+0x880 == 7): 11D660
 // semantic 5 at the course's region row (26B5E0 runtime kind 1, the player's slot) -- the placement animation entered and
 // stepped once -- then 11DF18 (animator rate 0, velocity = +0x1B0 x 833.333, z 0). Without it the rolling start poses from
 // reset_animation's graph: the posed root (the camera's head input) is 1.2 cm (EBC3) .. 23 cm (DBC2) off on ticks 1..13.
 // place_rider_region reproduces the ready state bit for bit (physics, score, boost; all 29 bones: PS2 dbc2-weather 204,
 // the-throne-tuck 699). The camera it rebuilds is replaced by the event seed + the two set-targets (web/core.cpp).
 if(browserEventRolling){startControl={};browserStarting=browserStartFrame=browserStartFrozen=false;
  if(browserHumanRider&&cameraTerrain&&browserBodies&&browserEventRollingPlacement){ /*the human only: npc_start_event runs start_event too (a rival has its own slot row)*/const auto& r=*browserEventRollingPlacement;place_rider_region(r[0],r[1],r[2],r[3],r[4],r[5],0);
   weather_event_seed_apply();publish_motion();} /*the placement ends grounded (11DF18, control 0): publish it, as begin_event_rider does*/ /*the placement's 0x111890 only sets the painters' +0 = -99999 (reset_snow put the glide seed's rider painter back): the ready state's Weather state again, with the rolling start's resets*/
  gs=physicsState;previousGround=true;previousHeld=false;return;}
 weather_event_contact_seed();startControl={};const auto entry=originalStartControlEnter(startControl,physicsState.reverseStance);startTilt=physicsState.presentationLift.current;
 graph.sequences.clear();graph.sampledLocal.reset();graph.requestedSemantics.fill(438);graph.defaultMirror=entry.mirror;graph.defaultRoot={};const auto sc=originalSinCos(entry.rootHalfAngle);graph.defaultRoot.rotation={0,0,sc[0],sc[1]};
 if(!graph.enter(entry.semantic,-1,~uint64_t(0),true))throw std::runtime_error("Missing original grid animation");
 graph.startPose=startControl.pose;gs=physicsState;physicsState.animationIndex=entry.semantic;physicsState.animationClass=graph.currentClass(2);previousGround=true;previousHeld=false;browserStarting=browserStartFrame=browserStartFrozen=true;
}
extern "C" EMSCRIPTEN_KEEPALIVE float* start_info(){RIDER_LOCAL static float v[7];v[0]=browserStarting;v[1]=browserStartFrozen;v[2]=startControl.phase;v[3]=startControl.pose;v[4]=startControl.high;v[5]=event_phase();v[6]=event_ticks();return v;}
