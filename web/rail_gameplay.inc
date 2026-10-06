#include "../engine/rail_uber_entry.hpp"
RIDER_LOCAL extern uint16_t browserPatchFlags; // rider+0x2D4 (web/core.cpp)
#include "../engine/rail_contact_response.hpp"
#include "../engine/rail_exit_mask.hpp"
#include "../engine/rider_pair_system.hpp"
#include "../engine/soft_collision_control.hpp"
extern void retain_jump_camera(const OriginalJumpState&);
// Ordinary authored-rail controller/motion adapter. Included after graph/crash/reset setup.
RIDER_LOCAL extern void (*browserRailTickBegin)();RIDER_LOCAL extern bool browserRailJumpRelease;RIDER_LOCAL extern bool browserRailControllerRan;RIDER_LOCAL extern bool browserRailActive;RIDER_LOCAL extern bool browserRailBoostTicked;RIDER_LOCAL extern bool (*browserRailStep)(float,int,int,int);RIDER_LOCAL extern void (*browserRailReset)();
RIDER_LOCAL extern OriginalBoostProfile boostProfile;RIDER_LOCAL extern bool boostHeld;bool soft_animation_finished();
void begin_prediction(OriginalAirState); // web/core.cpp
void publish_rail_rider(const OriginalRailRider&,bool);void leave_rail_motion();void publish_rail_charge(float);void retain_rail_crouch(float);RIDER_LOCAL extern uint32_t groundFocusTick;
void advance_pickup_timers(uint32_t);void dispatch_pickup_contacts(const BodyCollisionVolume&);
OriginalCollisionReaction dispatch_body_event(const OriginalCollisionEvent&,const BodyCollisionVolume&,std::array<float,3>,int,int,const CollisionRandom&);
extern "C" void soft_collision_begin(int,float);
// 108388 from control 1 (web/boardpress_gameplay.inc, included later)
static void board_press_soft_enter(int,float);
static float rail_held_balance(float steering); // control 2 RailBalance (web/input_bridge.inc)
RIDER_LOCAL static unsigned railExitPasses=0,railExitHits=0,railExitIncomplete=0,railExitReactions=0;
RIDER_LOCAL static OriginalRailUberState railUber;RIDER_LOCAL static int railShoulderMask=0;RIDER_LOCAL static unsigned railUberEntries=0,railUberCompletions=0,railUberUnavailable=0;
RIDER_LOCAL static OriginalRailMotionState railMotion;RIDER_LOCAL static OriginalRailControlState railControl;
RIDER_LOCAL static GroundControlValue railSteer,railBalance,railTolerance,initialRailSteer,initialRailBalance,initialRailTolerance;
RIDER_LOCAL static unsigned railQueryBone=22;RIDER_LOCAL static RailVector railQueryOffset{};
static void setup_rail_context(const json&config){const auto&c=config.at("original_rail_context");railQueryBone=c.at("board_root_bone");if(railQueryBone!=22)throw std::runtime_error("Unverified rail query bone");railQueryOffset=c.at("offset9d0").get<RailVector>();auto triplet=[](const json&v){return GroundControlValue{v.at(0),v.at(1),v.at(2)};};initialRailSteer=triplet(c.at("steer"));initialRailBalance=triplet(c.at("balance"));initialRailTolerance=triplet(c.at("tolerance"));railSteer=initialRailSteer;railBalance=initialRailBalance;railTolerance=initialRailTolerance;}
static void board_press_rail_begin(float);static void board_press_rail_attach();static void board_press_rail_update();static int32_t board_press_style(); //web/boardpress_gameplay.inc
RIDER_LOCAL static bool railOwned=false,railPostPending=false,railUpperReactions=false;RIDER_LOCAL static unsigned railAttachments=0,railExits=0,railTicks=0,railObservers=0;
RIDER_LOCAL static int railSurface=-1;RIDER_LOCAL static bool railHeldJump=false,railPreviousJump=false,railReleaseFrame=false;RIDER_LOCAL static float railFlipInput=0,railRotationInput=0,railBalanceInput=0;RIDER_LOCAL static unsigned railJumps=0,railRotations=0;
static OriginalRailRider rail_view(){
 OriginalRailRider r;r.position=physicsState.position;r.velocity=physicsState.velocity;r.quaternion=physicsState.quaternion;
 const auto basis=originalOrientationBasis(r.quaternion);r.right=basis.right;r.forward=basis.forward;r.up=basis.up;
 r.contactNormal=physicsState.normal;r.surfaceForward=physicsState.forward;r.lateral=physicsState.lateral;r.surfaceVelocity=physicsState.surfaceVelocity;r.contactPoint=browserTrailContact;
 //108A48/1086B8 use rider+8A0 (captured22), not the board mesh bone23.
 if(cachedCrashWorld.size()>railQueryBone){r.bonePosition=cachedCrashWorld[railQueryBone].position;r.boneQuaternion=cachedCrashWorld[railQueryBone].rotation;}r.offset9D0=railQueryOffset;
 r.speedLimit=physicsProfile.speedLimit;r.timeScale=physicsState.timeScale;r.boost=physicsState.boost;
 r.turn=physicsState.turn;r.animationTurn=physicsState.animationTurn;r.extraLean=physicsState.extraLean;r.brake=physicsState.brake;r.crouch=physicsState.crouch;r.presentationRoll=physicsState.presentationRoll;r.balance280=physicsState.balance280;
 r.steer22C=railSteer;r.balance=railBalance;r.tolerance25C=railTolerance;r.manualSpin2DC=physicsState.manualSpin;r.style=physicsState.prewindStyle;
 r.reverseStance=physicsState.reverseStance;r.state324=physicsState.state320Equals324?r.reverseStance:!r.reverseStance;
 r.flag330=board_press_style();r.surfaceId=browserRailActive?railSurface:physicsProfile.surface.id;r.controlState=railOwned?physicsState.controlState:gs.controlState;r.motionMode=browserRailActive?4: ::grounded?0:1;return r;
}
static void rail_apply(const OriginalRailRider&r){
 physicsState.position=r.position;physicsState.velocity=r.velocity;physicsState.quaternion=r.quaternion;physicsState.normal=r.contactNormal;physicsState.surfaceVelocity=r.surfaceVelocity;
 physicsState.turn=r.turn;physicsState.animationTurn=r.animationTurn;physicsState.extraLean=r.extraLean;physicsState.brake=r.brake;physicsState.crouch=r.crouch;physicsState.presentationRoll=r.presentationRoll;physicsState.balance280=r.balance280;
 physicsState.forward=r.surfaceForward;physicsState.lateral=r.lateral; //0x115168 negates +0x3A0/+0x3B0 on a stance flip
 railSteer=r.steer22C;railBalance=r.balance;railTolerance=r.tolerance25C;physicsState.manualSpin=r.manualSpin2DC;physicsState.prewindStyle=r.style;physicsState.reverseStance=r.reverseStance;physicsState.state320Equals324=r.reverseStance==r.state324;
 railSurface=r.surfaceId;graph.railBalance=railBalance.current;publish_rail_rider(r,browserRailActive);
}
static std::array<float,3> run_rider_instance_contacts(BodyCollisionVolume&,std::array<float,3>,const std::array<float,3>*,int,int,const std::array<float,3>&);
RIDER_LOCAL static std::array<float,8> railExitDebug{}; // QA: last rail post 105398 displacement, 106F78 stage/notifications, motion, control, mask
static void rail_exit_contacts(OriginalRailRider& r){
 if(!browserBodyVolume||!browserBodies)throw std::runtime_error("Rail exit requires cached collision body");
 ++railExitPasses;rail_apply(r);
 // The normal post-pose pass observes motionTick after the motion step increments it.
 advance_pickup_timers(motionTick+1);
 originalRailExitContactPhases(browserBodyVolume->activeMask,r.controlState,railUber.identity,[&]{
  //105398 (a1=0): filter-1 selection, pickup copy and 1057B8 response.
  const auto velocity=physicsState.velocity;const auto quaternion=physicsState.quaternion;const bool softBefore=browserSoftActive;const bool crashBefore=crash.active;
  const auto moved=run_rider_instance_contacts(*browserBodyVolume,{},nullptr,r.motionMode,r.controlState,railMotion.direction);
  railExitDebug={moved[0],moved[1],moved[2],0.f,0.f,float(r.motionMode),float(r.controlState),float(browserBodyVolume->activeMask)};
  if(moved!=std::array<float,3>{}||physicsState.velocity!=velocity||physicsState.quaternion!=quaternion){
   terrain_original::Rounding rounding;for(unsigned k=0;k<3;k++)r.offset9D0[k]=terrain_original::add(r.offset9D0[k],moved[k]);
   r.position=physicsState.position;r.velocity=physicsState.velocity;r.quaternion=physicsState.quaternion;railQueryOffset=r.offset9D0;
   const auto frame=originalRebuildOrientation(r.quaternion);r.right=frame.right;r.forward=frame.forward;r.up=frame.up;
  }
  if(browserSoftActive&&!softBefore)r=rail_view(); //a soft reaction (108388) replaced the controller's triplets: as the 13C140 path above
  // a crash (1057B8 -> 10EB30 -> 11FEC8(13)) ran control 7's exit 132048 (enter_crash rail_control_exit): the pair phase's rail_apply
  // must not restore the old +0x238 target (fuzz r2-0204-m0 2663, six riders)
  if(crash.active&&!crashBefore)r=rail_view();
 },[&]{
  if(physicsState.controlState==9)return; //13C16C
  auto query=inspect_body_contacts(*browserBodyVolume,{0,0,0}); //13C1A0: 32F650 without the 13F488 normal filter (the response only rejects an opposed normal)
  if(!query||!query->complete()){++railExitIncomplete;return;}
  const auto& hit=query->best;if(!hit.hit)return;
  OriginalRailContactState contact{r.position,r.velocity,r.contactNormal,r.forward,railMotion.entryPosition,std::bit_cast<float>(railMotion.word40)};
  auto response=originalRailContactResponse(contact,hit.penetrationCm,hit.normal,[&](auto& actor,const auto& delta){
   translateOriginalPairBody(*browserBodyVolume,delta);
   for(unsigned k=0;k<3;k++)r.offset9D0[k]=terrain_original::add(r.offset9D0[k],delta[k]);
   const auto frame=originalRebuildOrientation(r.quaternion);r.quaternion=frame.quaternion;r.right=frame.right;r.forward=frame.forward;r.up=frame.up;actor.forward=frame.forward;
  });
  if(!response.accepted)return;++railExitHits;
  railMotion.entryPosition=contact.previousPosition;railMotion.word40=std::bit_cast<int32_t>(contact.stationarySeconds);
  r.position=contact.position;r.velocity=contact.velocity;railQueryOffset=r.offset9D0;rail_apply(r);
  if(response.notify){
   OriginalCollisionEvent event;event.pointCm=hit.pointCm;event.normal=response.normal;event.incomingDirection=response.incomingDirection;event.closingSpeedCmps=response.impulse;event.surface=hit.surface;event.surfaceProperty44=browser_surface_property(hit.surface);
   auto reaction=dispatch_body_event(event,*browserBodyVolume,r.velocity,4,r.controlState,[](){return rng.nextMotion();});++railExitReactions;
   // 108388's 11FEC8(3) runs the control-7 exit 132048 (+0x238 target 0, rate 1/15): take the rider back from the globals so the
   // pair phase's rail_apply below does not restore the pre-reaction triplets (PS2 Kick Doubt Moby 2944: +0x238 0.4636 -> 0.3969 -> 0.3303).
   if(reaction.controlOneCancelled){board_press_soft_enter(reaction.animation,reaction.manualSpin);r=rail_view();} /*a press on the rail (control 1, motion 4)*/
   else if(reaction.kind==OriginalCollisionReactionKind::Soft&&!reaction.cancelControlOne){soft_collision_begin(reaction.animation,reaction.manualSpin);r=rail_view();}
   // a crash (10EB30 -> 11FEC8(13)) ran the same exit (enter_crash rail_control_exit; fuzz r2-0204-m0 2663 with the pairs hosted)
   else if(crash.active)r=rail_view();
  }
 },[&]{
  //107888 rider-pair dispatch (13C098) through the race host (web/ai-racers.js); single-rider: nothing.
  if(rider_pairs_hosted()){rail_apply(r);rider_pair_point();r=rail_view();}
 });
 // Reactions may replace motion/control and physically rebake the crash root.
 r=rail_view();r.motionMode=browserResetActive?3:crash.active?2:r.motionMode;
}
// 0x13B07C..0x13B098: an entity-owned rail answers vtable+0x154, the contact velocity at the rail point (out+0x20), which the rail
// motion stores as +0x3D0 (PS2 riders/fareastmyth-uber-b 2478: a Snow Jam log teeter tips under the rider; the port kept 0).
static bool rail_entity_velocity(const OriginalRailQueryResult& hit,RailVector& velocity){
 const auto hooks=browser_rail_snap_hooks();
 if(!hooks.hasEntity(hit))return false;
 ssx::RollerQuad linear{},angular{};
 hooks.contactVelocity(hit,linear,angular);
 velocity={linear[0],linear[1],linear[2]};
 return true;
}
static OriginalRailAccess rail_access(){
 OriginalRailAccess a;a.exitContacts=rail_exit_contacts;a.query=browserRailQuery;a.attachForce=browser_teeter_attach_force; /*log teeters (rail_bridge.cpp)*/a.entityVelocity=rail_entity_velocity; /*13B094: a moving entity rail's +0x3D0*/a.channel2Class=[](){return graph.currentClass(2);};a.channel2SequenceFlag=[](unsigned bit){return bool(graph.flags(2)&(uint64_t(1)<<bit));};a.currentSemantic=[](){return graph.requestedSemantics[2];};
 a.playAnimation=[](int semantic,float,int){if(!graph.enter(semantic))throw std::runtime_error("Missing authored rail animation");};
 // 0x311B48 rotates the sequence roots by -angle: sincos(-angle*0.5) scaling 0x4FF160 = (0,0,1,0), then 0x311BF0.
 a.rotateAnimation=[](float angle){terrain_original::Rounding rounding;auto sc=originalSinCos(terrain_original::mul(-angle,.5f));graph.offsetSequenceRoots({{}, {terrain_original::mul(sc[0],0.f),terrain_original::mul(sc[0],0.f),sc[0],sc[1]}});};
 a.setAnimationRoot=[](float half){terrain_original::Rounding rounding;auto sc=originalSinCos(half);graph.defaultRoot={{},{terrain_original::mul(sc[0],0.f),terrain_original::mul(sc[0],0.f),sc[0],sc[1]}};};a.setAnimationSwitch=[](int value){graph.defaultMirror=value!=0;};
 a.requestControl=[](int mode){physicsState.controlState=gs.controlState=mode;};a.requestMotion=[](int mode){if(mode!=4&&mode!=1)throw std::runtime_error("Unsupported rail motion transition");if(mode==1&&browserRailActive)audio_event(AE_TAKEOFF,audioSpeed75C); /*13C0CC: 294170 on the rail leave*/browserRailActive=mode==4;};
 // Original score boundaries and meter awards are wired independently.
 // Grind-specific audiovisual observers remain unported.
 a.railEntryScore=[](bool stance,int style,int flag){++railObservers;return rail_score_entry(stance,style,flag).meterDelta;};a.awardScore=[](float value){++railObservers;award_rail_meter(value);};a.airborneRailEvent=[](int style,int flag,float speed,const OriginalRailQueryResult& hit){++railObservers;
  {auto normal=hit.tangent;if(terrain_original::dot(physicsState.velocity,hit.tangent)<0)for(auto& x:normal)x=-x;record_snow_crash(hit.point,normal,speed,hit.surface);} //106CB8: 10E910 -> 111AA0(rail point, tangent toward v, rail speed f23, rail surface)
  auto award=rail_score_entry(!physicsState.state320Equals324,style,flag);audio_event(AE_LANDING,award.meterDelta,audioImpact770,1);audio_event(AE_RUMBLE_IMPACT,speed*0.5f); /*106CB8 -> 10E910 -> 2948D0; 10EAD8 rumble*/originalLandingUberProgression(boostState,0,award.committedUbers);award_rail_meter(award.meterDelta);if(award.meterDelta>0)boostState.modifier=0;};a.recordRailSurface=[](int value){railSurface=value;};
 a.balanceStat=[](){return landingProfile.landingStat;};a.railSpinScore=[](int style,float spin){++railObservers;return score_rail_rotation(style,spin);};
 a.recovery=[](bool requested){if(requested)browserCrashResetReason=1;return requested;};a.upperAction=[](bool held,bool pressed){
  if(!originalCrouchRequest(prewind.jumpGate,held,pressed))return false;
  railHeldJump=true;physicsState.controlState=gs.controlState=2;physicsState.animationTurn.target=0;physicsState.animationTurn.rate=.03333333507180214f;return true;
 };
 a.boost=[](bool held,bool pressed){auto effect=originalBoostControl(boostState,boostProfile,held,pressed);physicsState.boost=boostState.amount;return effect.started;};
 a.uberEntry=[](OriginalRailRider& r,int identity){
  // Owned start rider B2C=3; bit1 permits rail Uber tricks.
  auto gate=originalRailUberEntry(railControl.identity,railUber.identity,identity,boostState.superTime,3);
  if(gate.notifyUnavailable){++railUberUnavailable;audio_event(AE_UBER_DENIED);} /*1326A0 -> 299B70*/
  if(!gate.enter)return false;
  originalRailControlLeave(r);r.controlState=physicsState.controlState=gs.controlState=12;
  const auto entry=originalRailUberEnter(railUber.identity,r.style);
  if(entry.setRoot){auto sc=originalSinCos(entry.rootHalfAngle);graph.defaultRoot={{},{0,0,sc[0],sc[1]}};}
  if(entry.clearSwitch)graph.defaultMirror=false;
  if(!graph.enter(entry.semantic))throw std::runtime_error("Missing original Uber grind entry");
  begin_rail_uber_score(entry.tier,entry.scoreStyle);r.style=entry.newStyle;railUber.phase=0;
  graph.fade(1,entry.channelFade);graph.fade(0,entry.channelFade);++railUberEntries;return true;
 };
 a.upperReactions=[](){railUpperReactions=true;}; /*0x115B58/0x115D48 then run in animation_tick*/a.restoreStance=[](){restore_stance(physicsState.prewindStyle);};a.leaveEffects=[](){++railObservers;};return a;
}
static void clear_rails(){browserRailActive=railOwned=railPostPending=false;railMotion={};railControl={};railUber={};railShoulderMask=0;railUberEntries=railUberCompletions=railUberUnavailable=0;railSteer=initialRailSteer;railBalance=initialRailBalance;railTolerance=initialRailTolerance;railAttachments=railExits=railTicks=railObservers=0;railExitPasses=railExitHits=railExitIncomplete=railExitReactions=0;railSurface=-1;railHeldJump=railPreviousJump=railReleaseFrame=false;railFlipInput=railRotationInput=railBalanceInput=0;railJumps=railRotations=0;graph.railBalance=0;}
// Crash motion replaces rail ownership without clearing accumulated rail stats.
//13C5A0: every motion4 exit restarts the rail attachment tolerance ramp.
static void rail_motion_exit(){railTolerance={std::bit_cast<float>(0xbe800000u),std::bit_cast<float>(0x3d088889u),1.f};}
static void rail_reset_leave(){if(browserRailActive&&railOwned&&physicsState.controlState==7){auto view=rail_view();originalRailControlLeave(view);rail_apply(view);}}
// 0x132048, control 7's exit (111578 from 11FEC8): +0x240 = 0, +0x23C = 1/15. Nothing else.
RIDER_LOCAL static uint32_t railControlExits=0; // QA: rail_control_exit calls (rail_debug_info)
static void rail_control_exit(){
 auto view=rail_view();
 originalRailControlLeave(view);
 railBalance=view.balance;
 ++railControlExits;
}
static void detach_rail_for_crash(){
 if(browserRailActive)rail_motion_exit();
 browserRailActive=railOwned=railHeldJump=railPreviousJump=false;
 railReleaseFrame=false;railFlipInput=railRotationInput=railBalanceInput=0;
}
static bool step_rails_tick(float steering,int jump,int brake,int boost);
RIDER_LOCAL static bool railControlAirborne=false; //control 7 Stop::Airborne this tick (the rail motion had already left the rail)
RIDER_LOCAL static bool railHeldRelease=false; //12E9B8's jump release left the rail this tick (it returns before the rail step's own approaches)
// 1211F8 (after the controller, before motion) approaches +0x22C/+0x238/+0x25C every tick; the
// rail controller path below does so itself while the rail owns the rider. Off the rail the
// +0x25C tolerance climbs back from the -0.25 exit value, which widens 1086B8 (108A48) again.
// Reset (control 9) and crash (control 8) ticks return before the rail step, but their 1211F8 pass approaches the rail
// triplets too (tech-select-rail 925: the 18..20 cycle blend keeps reading +0x238 through the reset).
void browser_rail_idle_approach(){if(railOwned)return;for(auto* control:{&railSteer,&railBalance,&railTolerance})groundControlApproach(*control);graph.railBalance=railBalance.current;}
static bool step_rails(float steering,int jump,int brake,int boost){
 const bool owned=railOwned;railControlAirborne=railHeldRelease=false;
 // 12E9B8 in the air after the rail was lost with Cross held: +0x328 (the rail style) is still set, so it steers +0x22C through
 // 113F38 with RailBalance (12EC1C / 12EDF0), not the turn 113E80; 1211F8 then approaches it (PS2 hl2/rail-fence 707..: turn 0)
 if(!owned&&heldAirMode&&jump&&physicsState.prewindStyle!=0&&!::grounded){auto view=rail_view();originalRailSteerTarget(view,rail_held_balance(steering));railSteer=view.steer22C;}
 const bool consumed=step_rails_tick(steering,jump,brake,boost);
 // 1211F8 approaches +0x22C / +0x238 / +0x25C after 12E9B8's release too (PS2 R&B Moby 5033: steer -0.5877 -> -0.6504, the attach
 // tolerance -0.25 -> -0.2167; the next tick's re-attach slid 0.21 cm short without it)
 if((!owned||railControlAirborne||railHeldRelease)&&!railOwned){for(auto* control:{&railSteer,&railBalance,&railTolerance})groundControlApproach(*control);graph.railBalance=railBalance.current;} //a fading rail cycle 18..20 (three-way kind) keeps reading +0x238 off the rail; control 7 leaving for the air (motion already airborne) still reaches 1211F8 this tick
 return consumed;
}
static bool step_rails_tick(float steering,int jump,int brake,int boost){
 railReleaseFrame=false;railQueryOffset={};railUpperReactions=false;browserRailBoostTicked=false; //121020 clears companion translation each motion tick.
 if(!physicsAttached||crash.active||browserResetActive||cachedCrashWorld.size()<=23)return false;
 auto view=rail_view();auto access=rail_access();bool attached=false;
 if(!railOwned||(physicsState.controlState==12&&!browserRailActive)){
  if((browserSoftActive&&view.controlState!=3)||!(view.controlState==0||view.controlState==1||view.controlState==2||view.controlState==3||view.controlState==4||view.controlState==5||view.controlState==11||view.controlState==12))return false; //11: handplant exit event (0x132A30 -> 0x106848); 3: soft control 12E778 tests 106848 after 116378/116120 and returns when it attaches (metro-event-race 2049)
  // 0x106848's requests switch immediately (0x1112B8/0x111538 run exit then enter), so the
  // motion-4 enter and the old controller's exit run inside the attach, in its order:
  // style -> requestMotion(4) [0x13AD20: 0x11FA10 bake, 0x11E098, zeroes] -> requestControl(13)
  // [old control exit, e.g. 0x134CB0] -> 0x115358 -> 0x1326C8 -> requestControl(7) [0x131D08] -> 0x13ADC0.
  auto attachAccess=access;const int attachControl=view.controlState;
  attachAccess.requestMotion=[&](int mode){
   const int previousMotion=view.motionMode;access.requestMotion(mode);if(mode!=4)return;
   if(previousMotion==0){physicsState.turn=view.turn;physicsState.extraLean=view.extraLean;originalLandingGroundLeave(physicsState,motionTick,lastGroundLeave);view.turn=physicsState.turn;view.extraLean=physicsState.extraLean;} //0x13F410; 0x139A18 (motion 1) is empty
   RiderRootPresentation p;p.turn=view.turn.current;p.brake=view.brake.current;p.extraLean=view.extraLean.current;p.roll=view.presentationRoll.current;p.liftCm=physicsState.presentationLift.current;p.lateral=view.lateral;p.controlState=13; //11FA10 alone: no 11EB98 lift
   auto presented=originalRiderRootPresentation({view.position,view.quaternion},graph.sampledLocal->at(22).position,graph.scale,p);view.position=presented.position;view.quaternion=presented.rotation;
   originalRailMotionBegin(railMotion,view); //13AD20: 11E098 rebuild, triplet/state zeroes, entry position
  };
  attachAccess.requestControl=[&](int next){
   if(next==13&&attachControl==5){ //0x134CB0 control-5 exit: fade 1, prewind reset, 134DD0 bake, 11E098, 2DC quantize
    physicsState.position=view.position;physicsState.quaternion=view.quaternion;air.spinRate=view.manualSpin2DC;
    if(landing_air_exit())landingAirExitBaked=false;
    view.position=physicsState.position;view.quaternion=physicsState.quaternion;view.manualSpin2DC=physicsState.manualSpin;
    const auto basis=originalOrientationBasis(view.quaternion);view.right=basis.right;view.forward=basis.forward;view.up=basis.up;
   } //0x131C30 / 0x12FB68 (controls 0/4) run in attack_control_changes; 0x132F98 (11) in the handplant adapter.
   access.requestControl(next);if(next==1)board_press_rail_attach(); //0x106D9C: 26/34 with +0x330, control 1 (0x12FC60)
  };
  auto result=originalRailAttach(view,railMotion,attachAccess);if(!result.attached&&view.controlState!=12)return false;
  if(result.attached){
  if(physicsState.controlState!=1&&physicsState.controlState!=2&&physicsState.controlState!=3)originalRailControlBegin(railControl,view); //control 1 keeps its own state (0x131D08 is control 7's enter); control 3 stays soft on the rail (12E778 requests 7 when its clip ends); a held jump (control 2) stays 2 and never enters 7 (PS2 Gravitude Moby 5136: +0x200 keeps 1/30)
  railOwned=attached=true;++railAttachments;/*+0x2C8 lift keeps its triplet (0x13F410 retargets it only on a ground attach)*/prewind.jumpGate=0;air={};passiveMode=heldAirMode=false;view.controlState=physicsState.controlState;railHeldJump=view.controlState==2;
  }
 }
 if(railOwned&&physicsState.controlState==1&&!browserRailActive){railOwned=false;railReleaseFrame=true;rail_apply(view);return false;} //control 1 in motion 1: 0x12FFF8 next
 const bool uberControlFrame=view.controlState==12;
 const bool softRail=browserSoftActive&&physicsState.controlState==3;
 // 12E9B8 returns right after its 106848 attaches, so a Cross released on the attach tick is released on the rail next tick (PS2
 // riders/griff-uber-c 1730: a charged release onto a rail stays control 2 in motion 4, then leaves the rail at 1731).
 if(railHeldJump&&!jump&&!softRail&&!attached){
  const bool onRail=browserRailActive;
  if(onRail){
   OriginalJumpState takeoff;takeoff.position=view.position;takeoff.velocity=view.velocity;takeoff.normal=view.contactNormal;takeoff.takeoffNormal=physicsState.previousNormal;takeoff.forward=view.surfaceForward;takeoff.boardUp=view.up;takeoff.speedLimit=view.speedLimit;takeoff.charge=view.crouch.current;takeoff.ticksSinceGroundFocus=motionTick-groundFocusTick;takeoff.flags=browserPatchFlags;takeoff.motionMode=4;
   originalJumpTakeoff(takeoff);retain_jump_camera(takeoff);browser_controller_takeoff_wind(takeoff.velocity);rail_score_takeoff(takeoff.rampTakeoff/*114298 passes s1 = its ramp branch to 119E38 (a2), docs/peak2.md*/);view.position=takeoff.position;view.velocity=takeoff.velocity;originalRailMotionLeave(railMotion,view,access);++railJumps;++railExits;
   if(crash.active||browserResetActive){physicsState.velocity=view.velocity;if(crash.active)crash.actor.velocity=view.velocity;commit_rider_physics();detach_rail_for_crash();return true;}
  }
  originalRailControlLeave(view);browserRailActive=railOwned=railHeldJump=railPreviousJump=false;railReleaseFrame=true;railHeldRelease=true;browserRailJumpRelease=true;rail_apply(view);if(onRail){rail_motion_exit();leave_rail_motion();}
  gs=physicsState;gp=physicsProfile;if(browserSoftActive)return false;release_air_control();physicsState.controlState=5;physicsState.turn=gs.turn;physicsState.crouch=gs.crouch;physicsState.brake=gs.brake;physicsState.prewindStyle=0;return false;
 }
 if(view.controlState==12){
  if(!attached){
   originalBoostControl(boostState,boostProfile,boost,boost&&!boostHeld);
   originalRailSteerTarget(view,steering); //0x1365DC: RailBalance (LStickR-LStickL, word0 bits 23..28) unnegated
   groundCrouchBrakeTargets(view.crouch,view.brake,0,0,terrain_original::dot(view.velocity,view.surfaceForward),view.turn.current);
  }
  OriginalRailUberInputs input;input.attached=attached;input.commandIdentity=originalRailUberIdentity(railShoulderMask);input.motionMode=browserRailActive?4: ::grounded?0:1;
  input.currentSemantic=graph.requestedSemantics[2];input.primaryClass=graph.currentClass(2);input.primaryRaisedBit0=bool(graph.flags(2)&1);
  for(const auto& seq:graph.sequences)if(seq.channel==2&&seq.semantic==input.currentSemantic){input.primaryCompleted=seq.completed;break;}
  input.balance238=view.balance.current;input.railBalance=railMotion.balance;
  std::vector<OriginalRailUberRequest> requests;originalRailUberUpdate(railUber,input,view.style,requests);
  for(const auto& request:requests){using R=OriginalRailUberRequest;
   switch(request.kind){
    case R::Play:case R::ReturnToRail:if(!graph.enter(request.value))throw std::runtime_error("Missing Uber grind continuation");break;
    case R::PrimaryRate:graph.setRate(2,request.amount);break;
    case R::BalanceTarget:view.balance.target=request.amount;view.balance.rate=request.rate;break;
    case R::RestoreStance:rail_apply(view);restore_stance(physicsState.prewindStyle);view=rail_view();break;
    case R::RequestControl:view.controlState=physicsState.controlState=gs.controlState=request.value;if(request.value==7)originalRailControlBegin(railControl,view);else {railOwned=false;railReleaseFrame=true;if(request.value==5){air=originalAirControlBegin(0,0);heldAirMode=passiveMode=false;}}break;
    case R::Score:end_rail_uber_score(view.style);break;
    case R::UberCounter:if(boostState.tier<10){++boostState.tier;if(boostState.tier==10)boostState.superTime=60;}physicsState.boostTierCounter=boostState.tier;++railUberCompletions;break;
   }
  }
 }else if(softRail&&browserRailActive&&!attached){ //12E778 returns right after a 106848 attach
  auto result=originalSoftControlStep(physicsProfile,physicsState,{4,soft_animation_finished(),false},{0,railBalanceInput,bool(boost&&!boostHeld),bool(boost),false}); //12E850: RailBalance word0 bits 21..26, not CruiseTurn
  view.turn=physicsState.turn;view.animationTurn=physicsState.animationTurn;view.crouch=physicsState.crouch;view.brake=physicsState.brake;
  if(result.requestBoost)originalBoostControl(boostState,boostProfile,result.boostHeld,result.boostPressed);
  if(result.requestBalance)originalRailSteerTarget(view,result.balance);
  if(result.resetRail){const int semantic=originalRailEntrySemantic(view.style,false);if(!graph.enter(semantic))throw std::runtime_error("Missing post-collision rail animation");}
  if(result.nextControl==7){view.controlState=physicsState.controlState=gs.controlState=7;originalRailControlBegin(railControl,view);browserSoftActive=browserSoftFrame=false;soft_animation_complete(false);}
  else if(result.nextControl>=0)throw std::runtime_error("Unsupported rail soft-control transition");
 }else if(railHeldJump){
  // 12E9B8 returns as soon as its 106848 attaches: the attach tick sets no boost, crouch/brake, steer or prewind target and keeps
  // the ground clip; only 1211F8's approaches run (PS2 Gravitude Moby 5136: +0x2A4/+0x2A8 rates, then the 5136 pose's bone 22).
  if(!attached){
  originalBoostControl(boostState,boostProfile,boost,false); //114130
  groundCrouchBrakeTargets(view.crouch,view.brake,1,0,terrain_original::dot(view.velocity,view.surfaceForward),view.turn.current);
  originalRailSteerTarget(view,rail_held_balance(steering)); //12EC1C / 12EDF0: clamp(RailBalance word1 bits 6..11, +-0.5) unnegated
  originalAirPrewindTargets(prewind,rideLatched?rideSpin:0.f,railFlipInput,{view.style,graph.currentClass(2),graph.requestedSemantics[2],view.manualSpin2DC,false});
  // 12E9B8 selects from the retained prewind currents; 1211F8 approaches them afterwards (as on the ground): a D-pad held with
  // Cross on a rail plays 255 on the tick after its first prewind tick (PS2 air-release/rail-exit-spin 681: 245, 682: 255).
  const int semantic=originalPrewindAnimation(prewind.spin.current,prewind.flip.current,view.reverseStance,view.style);if(semantic<0||(semantic!=graph.requestedSemantics[2]&&!graph.enter(semantic)))throw std::runtime_error("Missing rail crouch animation");
  }
  originalAirPrewindApproach(prewind);
 }else if(view.controlState==1){ //0x12FC80 on the rail (motion 4); 0x106848 does not re-attach while motion 4 owns the rider
  if(!attached){rail_apply(view);board_press_rail_update();view=rail_view();if(!railOwned)return false;} //R3 ollie left the rail: motion 1 runs in the core this tick
 }else if(!attached){
  OriginalRailCommand command;command.turn=steering; /*0x131E00: RailBalance (LStickR-LStickL, word0 bits 25..30) goes to 0x113F38 unnegated; stick left slides toward -right (rail-balance-lr)*/command.identity=originalRailUberIdentity(railShoulderMask);command.rotate=railRotationInput;command.upper14=jump;command.upper13=jump&&!railPreviousJump;command.boostHeld16=boost;command.boostPressed15=boost&&!boostHeld;command.transfer=rideLatched?rideBoard:0;
  access.boardPressEntry=[&view](float press){board_press_rail_begin(press);view.controlState=physicsState.controlState;originalRailControlLeave(view);}; //0x1161D0 -> 0x11FEC8(1): 0x132048 then 0x12FC60
  auto result=originalRailControlStep(railControl,view,railMotion,command,access);if(result.stop==OriginalRailControlResult::Stop::Rotated)++railRotations;
  if(result.stop==OriginalRailControlResult::Stop::Upper){originalRailControlLeave(view); /*1162C8 requestControl(2) runs control 7 exit 0x132048: +0x238 target 0, rate 1/15 (metro-air-tricks 936)*/view.animationTurn=physicsState.animationTurn;const int semantic=originalPrewindAnimation(prewind.spin.current,prewind.flip.current,view.reverseStance,view.style);if(semantic<0||!graph.enter(semantic))throw std::runtime_error("Missing rail crouch entry");}
  if(result.stop==OriginalRailControlResult::Stop::Airborne){railControlAirborne=true;browserRailControllerRan=true;originalRailControlLeave(view);railBalance=view.balance;railOwned=false;view.style=physicsState.prewindStyle;view.quaternion=physicsState.quaternion;view.reverseStance=physicsState.reverseStance;view.state324=physicsState.state320Equals324?view.reverseStance:!view.reverseStance;rail_apply(view);railReleaseFrame=true;heldAirMode=false;gs=physicsState;gp=physicsProfile;passiveMode=physicsState.controlState==4;if(passiveMode)originalPassiveAirBegin(passive,physicsState,prewind);else if(physicsState.controlState==5)air=originalAirControlBegin(0,0);else throw std::runtime_error("Unexpected rail airborne control");
   // 1211F8 still approaches every triplet after 0x132770's exit in this tick (PS2 Gravitude Allegra 3749: +0x220 0.4833 -> 0.4296).
   for(auto* control:{&physicsState.turn,&physicsState.brake,&physicsState.crouch})groundControlApproach(*control);
   return false;}
 }
 if(!browserRailActive&&uberControlFrame){for(auto* control:{&view.steer22C,&view.balance,&view.tolerance25C})groundControlApproach(*control);rail_apply(view);return false;}
 browser_controller_takeoff_wind(view.velocity); //1211F8 -> 120378 reads +0x1E0 after the controller (a 106848 attach bake this tick)
 // 120378 reads +0x320 after the controller too (0x12051C / 0x1207DC / 0x120A9C): a 106848 attach switches the stance first
 // (runs/riders/canhuck-race 2119: Canhuck's tail channel +0x9A0 picks 428 with the attach stance, 429 with the old one).
 browser_controller_stance(view.reverseStance);
 railPreviousJump=jump;browser_boost_tick(boostState,boostProfile,physicsState.timeScale,browserRailActive?4:1,physicsState.controlState);view.boost=physicsState.boost=boostState.amount;physicsState.boostWindow=boostState.window;physicsState.boostTierCounter=boostState.tier;boostHeld=boost;
 for(auto* control:{&view.turn,&view.animationTurn,&view.extraLean,&view.brake,&view.crouch,&view.steer22C,&view.balance,&view.presentationRoll,&view.tolerance25C,&view.balance280})groundControlApproach(*control);
 for(auto* control:{&physicsState.adjustment28C,&physicsState.adjustment298,&physicsState.boardAlignment,&physicsState.presentationLift})groundControlApproach(*control); //1211F8 approaches every triplet (+0x28C/+0x298/+0x2BC/+0x2C8 too)
 // control 7 after the rail motion lost the rail (motion 1): this tick's controller and 1200D0 ran here; step_rider must not run them again
 // (PS2 hl-rail-10 702 / 703: one meter decay a tick, the port drained twice)
 if(!browserRailActive){rail_apply(view);browserRailBoostTicked=true;return false;}
 view.controlState=physicsState.controlState;view.motionMode=4;originalRailMotionStep(railMotion,view,access);rail_apply(view);publish_rail_charge(railHeldJump?view.crouch.current:0);++railTicks;railPostPending=!railMotion.lostRail;
 if(railMotion.lostRail){OriginalJumpState passive;passive.position=view.position;passive.velocity=view.velocity;passive.normal=view.contactNormal;passive.takeoffNormal=physicsState.previousNormal;passive.forward=view.surfaceForward;passive.boardUp=view.up;passive.speedLimit=view.speedLimit;passive.charge=-1;passive.ticksSinceGroundFocus=motionTick-groundFocusTick;passive.flags=browserPatchFlags;passive.motionMode=4;originalJumpTakeoff(passive);retain_jump_camera(passive);if(passive.groundLeaveSentinel)lastGroundLeave=0xffffffffu;view.position=passive.position;view.velocity=passive.velocity;rail_score_takeoff(passive.rampTakeoff/*114298 s1 (ramp branch) -> 119E38 a2*/);const auto leaveEffects=originalRailMotionLeave(railMotion,view,access);++railExits;if(crash.active||browserResetActive){physicsState.velocity=view.velocity;if(crash.active)crash.actor.velocity=view.velocity;commit_rider_physics();detach_rail_for_crash();return true;}rail_apply(view);rail_motion_exit();leave_rail_motion();
  // 13BFA8: 11FE78(1)'s 1399E0 seeded the predictor before the speed clamp (fuzz r7-0053 834: the clamp scaled +0x1E0 by 2 ULP, the flight
  // integrates the unclamped seed)
  if(leaveEffects.requestAirMotion)begin_prediction({physicsState.position,{leaveEffects.predictionVelocity[0],leaveEffects.predictionVelocity[1],leaveEffects.predictionVelocity[2]}});
  if(browserSoftActive){railOwned=false;railReleaseFrame=true;}if(railHeldJump){railOwned=railHeldJump=false;railReleaseFrame=true;heldAirMode=true;passiveMode=false;gs.controlState=physicsState.controlState=2;retain_rail_crouch(view.crouch.current);}}
 return true;
}
// 0x114298(rider,charge) from a controller while riding (board-press R3 ollie 0x1307B8 on the
// rail): the rail takeoff of the crouch release, then motion 1 (0x13C5A0) without control 7.
static void rail_controller_takeoff(float charge){
 auto view=rail_view();auto access=rail_access();
 OriginalJumpState takeoff;takeoff.position=view.position;takeoff.velocity=view.velocity;takeoff.normal=view.contactNormal;takeoff.takeoffNormal=physicsState.previousNormal;takeoff.forward=view.surfaceForward;takeoff.boardUp=view.up;takeoff.speedLimit=view.speedLimit;takeoff.charge=charge;takeoff.ticksSinceGroundFocus=motionTick-groundFocusTick;takeoff.flags=browserPatchFlags;takeoff.motionMode=4;
 originalJumpTakeoff(takeoff);retain_jump_camera(takeoff);browser_controller_takeoff_wind(takeoff.velocity);rail_score_takeoff(takeoff.rampTakeoff/*114298 passes s1 = its ramp branch to 119E38 (a2), docs/peak2.md*/);view.position=takeoff.position;view.velocity=takeoff.velocity;originalRailMotionLeave(railMotion,view,access);++railJumps;++railExits;
 browserRailActive=railOwned=railHeldJump=railPreviousJump=false;railReleaseFrame=true;rail_apply(view);rail_motion_exit();leave_rail_motion();
}
// 0x456B70[4] = 0x13BFA8 is motion 4's post stage: it runs every rail tick after the pose
// (0x11E150, then 105398 / 13C140 / 107888 against the freshly posed body and the speed
// clamp), not only when the rail is lost. Returns the contact translation for the cached pose.
static std::array<float,3> rail_motion_post(){
 if(!railPostPending)return {};railPostPending=false;
 if(!browserRailActive||!railOwned||crash.active||browserResetActive||!browserBodyVolume)return {};
 const auto before=railQueryOffset;auto view=rail_view();auto access=rail_access();
 originalRailMotionLeave(railMotion,view,access);if(!crash.active&&!browserResetActive){rail_apply(view);publish_rail_charge(railHeldJump?view.crouch.current:0);} //rail_apply republishes the rider; keep the step's crouch charge
 std::array<float,3> moved;for(unsigned k=0;k<3;k++)moved[k]=railQueryOffset[k]-before[k];return moved;
}
static void rail_tick_begin(){railReleaseFrame=false;browserRailJumpRelease=false;browserRailControllerRan=false;}
static void setup_rails(){browserRailStep=step_rails;browserRailReset=clear_rails;browserRailTickBegin=rail_tick_begin;}

extern "C" EMSCRIPTEN_KEEPALIVE float* rail_exit_debug(){return railExitDebug.data();}
// QA: [rail_control_exit calls, +0x238 current, +0x240 target, browserRailActive, railOwned]
extern "C" EMSCRIPTEN_KEEPALIVE float* rail_debug_info(){
 RIDER_LOCAL static float out[5];
 out[0]=float(railControlExits);out[1]=railBalance.current;out[2]=railBalance.target;out[3]=browserRailActive;out[4]=railOwned;
 return out;
}
extern "C" EMSCRIPTEN_KEEPALIVE float* rail_exit_info(){RIDER_LOCAL static float out[4];out[0]=railExitPasses;out[1]=railExitHits;out[2]=railExitIncomplete;out[3]=railExitReactions;return out;}

extern "C" EMSCRIPTEN_KEEPALIVE void rail_shoulder_input(int mask){railShoulderMask=mask&15;}
extern "C" EMSCRIPTEN_KEEPALIVE float* rail_uber_info(){RIDER_LOCAL static float out[6];out[0]=railUber.identity;out[1]=railUber.phase;out[2]=railUberEntries;out[3]=railUberCompletions;out[4]=railUberUnavailable;out[5]=physicsState.controlState;return out;}

bool browser_rail_uber_control(){return railOwned&&physicsState.controlState==12;}
// QA: rail attach query inputs (108A48 board root + rider+9D0) and the last query result.
extern "C" EMSCRIPTEN_KEEPALIVE float* rail_attach_probe(){RIDER_LOCAL static float v[12];auto r=rail_view();for(unsigned i=0;i<3;i++){v[i]=r.bonePosition[i];v[3+i]=r.offset9D0[i];}
 OriginalRailQueryResult hit;auto access=rail_access();v[6]=originalRailAttachTest(r,access,hit);v[7]=hit.found;v[8]=hit.distance;for(unsigned i=0;i<3;i++)v[9+i]=hit.point[i];return v;}
// QA: run this rider's body query (current cached AA0 body) with a sphere mask, filter normal and detail;
// returns [hit, penetration, normal xyz, point xyz, instance, terrain, contacts, candidates].
extern "C" EMSCRIPTEN_KEEPALIVE float* body_query_probe(int mask,float nx,float ny,float nz,int detailed){
 RIDER_LOCAL static float v[12];std::fill(std::begin(v),std::end(v),0.f);if(!browserBodyVolume||!browserBodies)return v;
 auto volume=*browserBodyVolume;volume.activeMask=uint32_t(mask);terrain_original::ContactCache cache;
 auto q=browserBodies->query(volume,{nx,ny,nz},nullptr,detailed!=0,2,&cache);
 v[0]=q.best.hit;v[1]=q.best.penetrationCm;for(unsigned k=0;k<3;k++){v[2+k]=q.best.normal[k];v[5+k]=q.best.pointCm[k];}v[8]=float(q.best.instance);v[9]=q.best.terrain;v[10]=q.contacts;v[11]=q.candidates;return v;
}
