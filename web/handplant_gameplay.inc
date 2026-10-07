// Original handplant for the browser: control 11 "Handplant" + motion 5 "Handplant".
// The recovered routines live in engine/handplant.hpp (docs/handplant-recovery.md);
// this adapter binds them to the browser rider, animation graph, rail catalog,
// trajectory predictor and score state. Included after input_bridge.inc.
//
// Tick order follows the original stage list 0x128C00: controller update
// (entry 0x107578 from cruise 0x131620 / natural air 0x12F730 / spin 0x133308,
// or control 11 0x132A30), boost timers 0x1200D0, filter approach 0x1211F8, then
// the motion update (motion 5 0x1391A8, or air 0x139A20 while control 11 owns
// the airborne exit). Pose, contacts and landing stay in animation_tick.
#include "../engine/handplant.hpp"
RIDER_LOCAL extern bool (*browserHandplantStep)(float&,float&,float&,bool&);RIDER_LOCAL extern void (*browserHandplantReset)();RIDER_LOCAL extern int browserAirModeFlag;RIDER_LOCAL extern bool browserHandplantMotion5;
RIDER_LOCAL extern float effectiveSpeedLimit;RIDER_LOCAL extern double airtime,charge;RIDER_LOCAL extern bool held,airbornePrewind;
void begin_prediction(OriginalAirState);bool advance_prediction(OriginalAirState&);
bool advance_prediction_unscaled(OriginalAirState&);
void browser_wind_push(bool air); // web/core.cpp 0x125970

RIDER_LOCAL static OriginalHandplantMotionState hpMotion;RIDER_LOCAL static OriginalHandplantControlState hpControl;
// rider+0x244 triplet. A new rider has all three words 0 (PS2 zoe-race pass 186: +0x248 is 0 in every slot); 132F98 (handplant
// exit, rate 0x3D4CCCCE from gp-0x754C, a .sdata constant) and the phase-2 seek (rate dt x 5) write the rate together with the target.
RIDER_LOCAL static GroundControlValue hpLean244{};
RIDER_LOCAL static bool hpActive=false,hpTick=false,hpAirTick=false;RIDER_LOCAL static int hpPhase=0,hpMotionMode=0,hpClip=438,hpEntryControl=-1;
RIDER_LOCAL static unsigned hpAttempts=0,hpEntries=0,hpLaunches=0,hpAttaches=0,hpStats=0,hpFailedCruise=0,hpLastExitPhase=0;RIDER_LOCAL static int hpLastScoreKind=0,hpLastReject=0;
RIDER_LOCAL static float hpLastAward=0;

static bool handplant_owned(){return hpTick;}
static bool handplant_air_frame(){return hpTick&&hpAirTick;}
void browser_handplant_tick_begin(){hpTick=hpAirTick=false;}
static void clear_handplant(){hpMotion={};hpControl={};hpLean244={};hpActive=hpTick=hpAirTick=false;hpPhase=0;hpMotionMode=0;hpClip=438;hpEntryControl=-1;
 hpAttempts=hpEntries=hpLaunches=hpAttaches=hpStats=hpFailedCruise=hpLastExitPhase=0;hpLastScoreKind=hpLastReject=0;hpLastAward=0;browserAirModeFlag=0;browserHandplantMotion5=false;graph.handplantBalance=0;}

static OriginalHandplantRider hp_view(){
 OriginalHandplantRider r;r.position=physicsState.position;r.quaternion=physicsState.quaternion;
 const auto basis=originalOrientationBasis(r.quaternion);r.right=basis.right;r.forward=basis.forward;r.up=basis.up;
 r.velocity=physicsState.velocity;r.timeScale=physicsState.timeScale;r.reverseStance=physicsState.reverseStance;r.phase=hpPhase;
 r.presentedUpZ=browserBodyVolume&&browserBodyVolume->reactionFrame?browserBodyVolume->reactionFrame->up[2]:basis.up[2];
 r.turn=physicsState.turn;r.animationTurn=physicsState.animationTurn;r.extraLean=physicsState.extraLean;r.roll250=physicsState.presentationRoll;r.lean244=hpLean244;
 //0x138BA0 reads bone rider+0x8A8 (=5) of the previous tick's posed world bones.
 if(cachedCrashWorld.size()>5)r.handBone=cachedCrashWorld[5].position;else r.handBone=r.position;
 r.motionMode=hpActive?hpMotionMode:browserRailActive?4: ::grounded?0:1;r.channel2Class=graph.currentClass(2);
 return r;
}
static void hp_apply(const OriginalHandplantRider& r){
 physicsState.position=r.position;physicsState.quaternion=r.quaternion;physicsState.physicalForward=r.forward;physicsState.boardUp=r.up;physicsState.velocity=r.velocity;
 physicsState.turn=r.turn;physicsState.animationTurn=r.animationTurn;physicsState.extraLean=r.extraLean;physicsState.presentationRoll=r.roll250;hpLean244=r.lean244;hpPhase=r.phase;
}
static bool hp_channel2_complete(){
 bool complete=completedMain&&completedMainSemantic==hpClip;
 for(const auto& s:graph.sequences)if(s.channel==2&&s.semantic==hpClip)complete|=s.completed;
 return complete;
}
static void hp_play(int semantic,float rate){if(!graph.enter(semantic,rate))throw std::runtime_error("Missing original handplant animation");hpClip=semantic;}
//0x131C30 cruise exit / 0x12FB68 natural-air exit (next control is 13, not 4).
static void hp_upper_exit(){const int upper=graph.currentClass(1);if(upper==3||upper==13){graph.fade(0,.1f);graph.setRate(1,1);}}
// Leave control 11 (0x132F98) for another controller.
static void hp_end(OriginalHandplantRider& r){originalHandplantControlExit(r);hp_apply(r);hpActive=false;hpMotionMode=0;hpClip=438;graph.handplantBalance=hpLean244.current;}
//0x11FA10(rider,rider+0x110): bake the presented root, then 0x11E098.
// 116120 (a reset requested in a handplant): 11FEC8(9) runs control 11's exit 132F98 (phase 0), then 11FE78(3) runs motion 5's
// exit 139178: 139548 launches from phase 0 (no launch: +0x10 stays 0) and +0x1E0 = +0x10, so the reset starts from rest
// (fuzz r5-0165-min 544: the PS2's +0x1E0 is 0 after a Select in control 11 / motion 5).
static void handplant_reset_exit(){
 if(!hpActive)return;
 auto r=hp_view();
 originalHandplantControlExit(r);
 if(hpMotionMode==5)originalHandplantMotionExit(hpMotion,r);
 hp_apply(r);
 hpActive=false;hpMotionMode=0;hpClip=438;browserHandplantMotion5=false;graph.handplantBalance=hpLean244.current;
}
static void hp_bake(OriginalHandplantRider& r){
 if(!graph.sampledLocal||graph.sampledLocal->size()<=22)throw std::runtime_error("Handplant root bake needs a sampled pose");
 RiderRootPresentation p;p.turn=r.turn.current;p.brake=physicsState.brake.current;p.extraLean=r.extraLean.current;p.roll=r.roll250.current;p.liftCm=physicsState.presentationLift.current;p.lateral=physicsState.lateral;p.controlState=13;
 auto presented=originalRiderRootPresentation({r.position,r.quaternion},graph.sampledLocal->at(22).position,graph.scale,p);
 r.position=presented.position;auto basis=originalRebuildOrientation(presented.rotation);r.quaternion=basis.quaternion;r.right=basis.right;r.forward=basis.forward;r.up=basis.up;
}
static bool hp_step_predictor(HandplantQuad& p,HandplantQuad& v){
 OriginalAirState s{{p[0],p[1],p[2]},{v[0],v[1],v[2]}};
 // 0x13940C / 0x1394F4: the motion-5 steps pass 1/60 itself, not scaled by the time scale (web/prediction_bridge.cpp).
 if(!advance_prediction_unscaled(s))throw std::runtime_error("Handplant launch prediction unavailable");
 for(unsigned k=0;k<3;k++){p[k]=s.position[k];v[k]=s.velocity[k];}return true;
}
// Motion 1 entered from motion 5 (0x139178 exit, then 0x1399E0 enter).
static void hp_launch(OriginalHandplantRider& r){
 originalHandplantMotionExit(hpMotion,r);hp_apply(r);hpMotionMode=1;++hpLaunches;
 browserAirModeFlag=1;::grounded=false;airtime=0;charge=0;held=airbornePrewind=false;
 begin_prediction({physicsState.position,physicsState.velocity});commit_rider_physics();
}

static OriginalHandplantAccess hp_access(OriginalHandplantRider& r,bool& attached){
 OriginalHandplantAccess a;
 a.query=browserHandplantQuery;a.markAttempt=[](){++hpAttempts;};
 a.motionMode=[](){return hpMotionMode;};
 a.semanticDuration=[](int semantic){return graph.semanticDuration(semantic);};
 a.play=hp_play;a.fade=[](unsigned channel,float seconds){graph.fade(channel,seconds);};
 // 0x311B48 rotates the roots by -angle: neg.s, mul.s(-angle, 0.5), sincos 0x31BE50, then sin x the axis (0,0,1) at 0x4FF160
 // (fs = sin), composed onto every root by 0x311BF0. The handplant's pi gives (-0, -0, -1, cos), as web/animation_bridge.cpp's
 // rotateAnimationRoot does.
 a.rotateAnimation=[](float angle){
  terrain_original::Rounding rounding;
  auto sc=originalSinCos(terrain_original::mul(-angle,.5f));
  AnimationTransform delta;
  delta.rotation={terrain_original::mul(sc[0],0.f),terrain_original::mul(sc[0],0.f),terrain_original::mul(sc[0],1.f),sc[1]};
  graph.offsetSequenceRoots(delta);
 };
 a.stat=[](){++hpStats;audio_event(AE_HANDPLANT);}; /*1328F8 -> 29DC48*/
 // Select.pressed is already routed to request_rider_reset by pad_tick; 0x116120 then returns 1.
 a.resetPath=[](bool requested){return requested;};
 a.channel2Complete=hp_channel2_complete;
 a.channel2Event=[](unsigned bit){return bool(graph.flags(2)&(uint64_t(1)<<bit));};
 //0x311B20(anim,2): the first channel-2 sequence; +0x08 time, +0x10 duration, +0x90 rate.
 a.channel2Clock=[](){for(const auto& s:graph.sequences)if(s.channel==2&&!s.slots.empty())return std::array<float,2>{s.slots.front().time,s.slots.front().duration};return std::array<float,2>{0,0};};
 a.setChannel2Rate=[](float rate){graph.setRate(2,rate);};
 a.scoreBegin=[](int kind){auto s=capture_score();float v=originalHandplantScoreBegin(s,kind);apply_score(s);hpLastScoreKind=kind;return v;};
 a.scoreLaunch=[](){auto s=capture_score();auto result=originalHandplantScoreLaunch(s,commit_score_state);apply_score(s);queue_rail_score(result);grab={};lastScoreId=-1;return result.meterDelta;};
 a.award=[](float value){hpLastAward=value;award_rail_meter(value);};
 a.bakeRoot=hp_bake;
 a.predictorStatus=[](){return browserTrajectory.status;};
 a.predictorRemaining=[](){return originalScalarSubtract(browserTrajectory.predictedTime,browserTrajectory.elapsed);};
 a.predictorBegin=[](HandplantQuad p,HandplantQuad v){begin_prediction({{p[0],p[1],p[2]},{v[0],v[1],v[2]}});};
 a.predictorStep=[](HandplantQuad& p,HandplantQuad& v){hp_step_predictor(p,v);};
 a.predictorNormal=[](){return browserTrajectory.normal;};a.predictorHeading=[](){return browserTrajectory.heading;};
 a.railAttach=[&r,&attached](){
  hp_apply(r);physicsState.controlState=gs.controlState=11;
  if(!step_rails(0,0,0,0))return false;
  attached=true;++hpAttaches;
  //0x106848 requested control 13 then 7: control 11's exit ran first.
  OriginalHandplantRider exit=hp_view();originalHandplantControlExit(exit);hpLean244=exit.lean244;hpPhase=0;hpActive=false;hpMotionMode=0;hpClip=438;return true;
 };
 return a;
}

// Controller-stage entry attempt. Returns true when control 11 took over this tick.
static bool hp_try_entry(float& steering,float& crouch,float& brake,bool& skipRail){
 if(!padCommand.active||!padCommand.fields.handplant||railOwned||browserSoftActive||browserStarting)return false;
 const int control=padCommand.control;
 if(control==0){if(!::grounded||physicsState.controlState!=0)return false;}
 else if(control==4){if(::grounded||!passiveMode)return false;}
 else if(control==5){if(::grounded||passiveMode||heldAirMode||air.phase!=3)return false;} //133370: spin controller must be idle
 else return false;
 auto r=hp_view();bool attached=false;auto a=hp_access(r,attached);
 a.requestControl=[&](int next){
  if(next==13){ //old controller exit
   if(control==5){hp_apply(r);if(landing_air_exit())landingAirExitBaked=false;r=hp_view();}
   else hp_upper_exit();
  } else if(next==11){
   hpActive=true;hpEntryControl=control;originalHandplantControlEnter(hpControl,r,a);physicsState.controlState=gs.controlState=11;
   // 1211F8's 120378 reads owner+0xDE4 after the controller: control 11 already (its +1000 cm/s wind term; PS2 hl2/plant-pipe-a 900:
   // the hair request 414 on the plant tick)
   controllerGround.controlState=11;
  }
  else throw std::runtime_error("Unexpected handplant entry control request");
 };
 a.requestMotion=[&](int next){
  if(next!=5)throw std::runtime_error("Unexpected handplant entry motion request");
  if(::grounded){hp_apply(r);originalLandingGroundLeave(physicsState,motionTick,lastGroundLeave);r.turn=physicsState.turn;r.animationTurn=physicsState.animationTurn;r.extraLean=physicsState.extraLean;r.roll250=physicsState.presentationRoll;} //0x13F410
  originalHandplantMotionEnter(hpMotion,r);hpMotionMode=5;
 };
 auto test=originalHandplantEnter(r,hpMotion,hpControl,a);
 hpLastReject=int(test.reject);
 if(!test.accepted){
  if(control==0){ //131744..131780: failed cruise attempt
   steering=-.5f<=steering?(steering<.5f?steering:.5f):-.5f;crouch=1;brake=0;skipRail=true;++hpFailedCruise;
  }
  return false;
 }
 ++hpEntries;hp_apply(r);
 ::grounded=false;airMotionThisTick=false;charge=0;held=airbornePrewind=false;
 passiveMode=heldAirMode=false;grab={};prewind.jumpGate=0;
 return true;
}

void browser_air_contact_frame(); // web/core.cpp (139A80 / 139A8C)
static bool step_handplant_tick(float& steering,float& crouch,float& brake,bool& skipRail);
//11B3F8 runs at frame begin with the motion that the previous tick left (motion 5 is not motion 1).
static bool step_handplant(float& steering,float& crouch,float& brake,bool& skipRail){const bool consumed=step_handplant_tick(steering,crouch,brake,skipRail);browserHandplantMotion5=hpActive&&hpMotionMode==5;return consumed;}
static bool step_handplant_tick(float& steering,float& crouch,float& brake,bool& skipRail){
 hpTick=hpAirTick=false;
 if(hpActive&&(crash.active||browserResetActive||physicsState.controlState!=11)){auto r=hp_view();hp_end(r);}
 if(!hpActive&&(::grounded||browserRailActive||crash.active||browserResetActive))browserAirModeFlag=0; //next motion-1 enter is not from motion 5
 if(!hpActive)groundControlApproach(hpLean244); //0x1211F8 approaches +0x244 every tick
 if(!physicsAttached||crash.active||browserResetActive||cachedCrashWorld.size()<=23){graph.handplantBalance=hpLean244.current;return false;}
 bool entered=false;
 if(!hpActive){if(!hp_try_entry(steering,crouch,brake,skipRail)){graph.handplantBalance=hpLean244.current;return false;}entered=true;}
 hpTick=true;
 auto r=hp_view();bool attached=false;auto a=hp_access(r,attached);
 a.requestMotion=[&](int next){if(next!=1)throw std::runtime_error("Unexpected handplant motion request");hp_launch(r);r=hp_view();};
 bool leftControl=false;
 a.requestControl=[&](int next){
  hp_end(r);leftControl=true;physicsState.controlState=gs.controlState=next;
  if(next==4){passiveMode=true;heldAirMode=false;auto g=gs;g.turn=physicsState.turn;g.crouch=physicsState.crouch;g.brake=physicsState.brake;originalPassiveAirBegin(passive,g,prewind);air={};}
  else if(next!=0)throw std::runtime_error("Unexpected handplant control request");
 };
 if(!entered){
  const uint32_t word0=padCommand.active&&padCommand.control==11?padCommand.words.word0:0u;
  auto result=originalHandplantControlUpdate(hpControl,r,word0,a);
  if(result.stop==OriginalHandplantControlResult::Stop::Exit)hpLastExitPhase=r.phase;
  if(attached){hpTick=false;graph.handplantBalance=hpLean244.current;return true;}
  if(!leftControl)hp_apply(r);
 }
 //0x1200D0 timers, then 0x1211F8 filter approach (120378 first: the wind is the post-controller +0x1E0, e.g. the entry at handplant-ground 569).
 browser_controller_takeoff_wind(physicsState.velocity);
 browser_boost_tick(boostState,boostProfile,physicsState.timeScale,hpMotionMode,physicsState.controlState);physicsState.boost=boostState.amount;physicsState.boostWindow=boostState.window;physicsState.boostTierCounter=boostState.tier;
 for(auto* value:{&physicsState.turn,&physicsState.brake,&physicsState.crouch,&physicsState.presentationLift,&physicsState.animationTurn,&physicsState.extraLean,&physicsState.boardAlignment,&physicsState.presentationRoll,&physicsState.balance280,&physicsState.adjustment28C,&physicsState.adjustment298})groundControlApproach(*value);
 groundControlApproach(hpLean244);graph.handplantBalance=hpLean244.current;
 gs.turn=physicsState.turn;gs.brake=physicsState.brake;gs.crouch=physicsState.crouch;gs.animationTurn=physicsState.animationTurn;
 if(hpMotionMode==5){
  r=hp_view();originalHandplantMotionUpdate(hpMotion,r,a);hp_apply(r);
  airMotionThisTick=false;::grounded=false;commit_rider_physics();
 } else {
  // Motion 1 (0x139A20 + orientation tail 0x139A64) while control 11 or its successor owns the air.
  hpAirTick=true;airtime+=1./60;
  browser_wind_push(true); // 139A20 begins with 125970 (web/core.cpp)
  auto airborne=OriginalAirState{physicsState.position,physicsState.velocity};effectiveSpeedLimit=3333.33349609375f;
  if(!advance_prediction(airborne))airborne.step(effectiveSpeedLimit);
  physicsState.position=airborne.position;physicsState.velocity=airborne.velocity;airMotionThisTick=true;::grounded=false;
  // 139A80 / 139A8C, after 113648, as in every air motion tick (fuzz r13-0191 1963: the port kept the handplant's ground normal)
  browser_air_contact_frame();
  if(browserPredictionAvailable){
   OriginalAirAlignmentContext context;context.normal=browserTrajectory.normal;context.heading=browserTrajectory.heading;
   context.physicalForward=physicsState.physicalForward;context.predictedTime=browserTrajectory.predictedTime;context.elapsedTime=browserTrajectory.elapsed;
   context.timeScale=physicsState.timeScale;context.adjustSpin=0;context.controlState=physicsState.controlState;context.airModeFlag=browserAirModeFlag;
   context.trajectoryStatus=browserTrajectory.status;context.surfaceIndex=browserTrajectory.surface;context.surfaceFlags=browserTrajectory.patchFlags;
   context.surfaceProperty44=browser_surface_property(context.surfaceIndex);
   auto basis=originalAirAlignmentStage(physicsState.quaternion,context);physicsState.quaternion=basis.quaternion;physicsState.physicalForward=basis.forward;physicsState.boardUp=basis.up;
  }
  commit_rider_physics();
 }
 return true;
}
static void setup_handplant(){browserHandplantStep=step_handplant;browserHandplantReset=clear_handplant;}
// setup_handplant(): per rider context, rider_statics_animation (web/animation_bridge.cpp)

// QA: control/motion/phase, balance, counters (source units).
extern "C" EMSCRIPTEN_KEEPALIVE float* handplant_info(){
 RIDER_LOCAL static float v[24];v[0]=hpActive;v[1]=hpPhase;v[2]=hpMotionMode;v[3]=hpControl.side;v[4]=hpControl.fast;v[5]=hpControl.railFlag;
 v[6]=physicsState.presentationRoll.current;v[7]=hpLean244.current;v[8]=hpControl.timer;v[9]=hpControl.smoothed;
 v[10]=hpAttempts;v[11]=hpEntries;v[12]=hpLaunches;v[13]=hpAttaches;v[14]=hpFailedCruise;v[15]=hpLastScoreKind;
 v[16]=hpMotion.duration;v[17]=hpMotion.elapsed;v[18]=hpLastExitPhase;v[19]=hpLastReject;v[20]=hpClip;v[21]=browserAirModeFlag;v[22]=hpStats;v[23]=hpEntryControl;
 return v;
}
