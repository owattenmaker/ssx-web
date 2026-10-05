#pragma once
#include "rider_local.hpp"
#include "../engine/crash_entry.hpp"
#include "../engine/crash_recovery.hpp"
#include "../engine/crash_animation.hpp"
#include "../engine/crash_world.hpp"
#include "../engine/crash_collision.hpp"
#include "../engine/crash_detached.hpp"
// The streamed peak worlds (web/stage_script_gameplay.inc): their dynamic entities have no scripted seed, so the crash's world
// queries leave them out instead of failing (engine/crash_world.hpp partialEntities).
bool browser_crash_partial_world();
RIDER_LOCAL extern bool browserHumanRider; // web/world_bridge.cpp: actor +0x868 detail (computer riders query coarse)
namespace ssx {
// Host operations are the same boundaries used by PrototypeRider's recovered
// crash controller. Motion and recovery below use the original engine routines.
struct BrowserCrashHost {
 std::function<OriginalCrashClipState()> clip;
 std::function<OriginalCrashRecoveryInputs()> recoveryInputs;
 std::function<void(float)> cameraShake;
 std::function<void(int)> play;
 std::function<void(float)> rate,seek;
 std::function<void(int)> enterControl,leaveMotion,requestReset,observer;
 std::function<bool()> stanceDiffers;
 std::function<void(float)> refund,impact,collisionImpact,presentation;
 std::function<AnimationTransform(int)> preview;
 std::function<AnimationTransform()> localRoot;
 std::function<void(const AnimationTransform&)> offsetRoots;
 std::function<float()> speedLimit; // rider+0x2E4 (105D98 passes it to 1135B8)
 // 138960 (137D18's sliding probe): a terrain hit with a patch writes +0x430 (patch +0x150), +0xAAC / +0xAB0 (u, v) and +0x2D4 (the patch
 // flags, which 13C948 reads as the heading-boost bit 0x10 on the get-up tick); without a patch +0x430 = -1
 std::function<void(const OriginalWorldSegmentHit&)> groundPatch;
 CollisionRandom random;
 // 105398 after the body response (137860/138640); translate is 106538's companion update.
 std::function<void(BodyCollisionVolume&,const std::function<void(std::array<float,3>)>&)> instanceContacts;
 std::function<void()> pairs; // 107888 after 105398 (137CD4 / 1388CC): rider pairs through the race host
};
// The crash motion probes and body queries share the rider contact caches rider+0x864 (138960, as 13A7B0 and ground contact)
// and rider+0x868 (137860/138640 body queries, as 13F488/13AA48); entering control 8 does not clear them (metro-mix-glide 1096).
terrain_original::ContactCache& browser_rider_terrain_cache();terrain_original::ContactCache& browser_rider_body_cache(); // web/world_bridge.cpp
const terrain_original::RiderScope* browser_rider_scope(); // web/world_bridge.cpp: rider+0x860, null before the first refresh
struct BrowserCrashRuntime {
 OriginalCrashActorState actor;OriginalCrashMotionState motion;OriginalCrashControlState control;
 OriginalAirTrajectory trajectory;
 std::array<float,24> lastAirContact{}; // QA: the last airborne body contact (hit, penetration, normal, terrain fraction/normal/point/surface, landed, instance, surface)
 BrowserCrashHost host;bool active=false;float penalty=0;uint32_t ticks=0;
 float resetPermission=-1;int riderCategory=1;
 void beginPredictor(float limit=3333.33349609375f){trajectory.begin({actor.position,actor.velocity},limit);}
 void beginControl(OriginalCrashActorState value,terrain_original::Vector offset9D0={}){
  // owner+0x30 (the crash submode) survives from the previous crash until 136C40: 12CA30's 136D40 board detach reads it (an air
  // get-up leaves 1; runs/riders/viggo-uber-a 2131 keeps the board's normal speed, owner+0x30 = 1 from 1821 to 2131).
  const int retainedSubmode=motion.submode;
  actor=value;motion={};control={};ticks=0;
  motion.submode=retainedSubmode;
  auto clip=host.clip();originalCrashControlBegin(control,motion,actor,clip.primary,clip.secondary,offset9D0,clip.animationClass);
 }
 void beginMotion(int previousMotion){
  auto entry=originalCrashMotionBegin(motion,actor,previousMotion);if(entry.beginPredictor)beginPredictor(entry.predictorSpeedLimit);active=true;
 }
 float playback(){auto clip=host.clip();auto rate=originalCrashPlaybackRate(motion.angularVelocity,clip.duration10,clip.animationClass,control.recovery70,resetPermission);host.rate(rate.applied);return rate.base;}
 // 10F280(rider,quick): 119BB0's quick get-up award (+0.1, popup 0x21) through 10E098; not a refund of the bail penalty.
 void refund(bool quick){host.refund(quick?1.f:0.f);}
 void select(OriginalCrashAnimationSelection kind){
  auto request=originalCrashSelectAnimation(kind,host.clip().semantic,actor.detached);
  if(request.rebakeRoot){auto bake=originalCrashRootBake({actor.position,actor.quaternion},host.localRoot(),host.preview(request.semantic));actor.position=bake.physical.position;actor.quaternion=bake.physical.rotation;host.offsetRoots(bake.animationRootOffset);}
  if(request.play)host.play(request.semantic);if(request.updatePlaybackRate)playback();
 }
 OriginalCrashContinuationCallbacks continuation(){
  OriginalCrashContinuationCallbacks cb;cb.currentClip=host.clip;cb.updatePlaybackRate=[&]{return playback();};
  cb.playGroundContinuation=[&]{select(OriginalCrashAnimationSelection::GroundContinuation);};cb.playAirContinuation=[&]{select(OriginalCrashAnimationSelection::AirContinuation);};
  cb.playGroundGetUp=[&]{select(OriginalCrashAnimationSelection::GroundGetUp);};cb.playAirGetUp=[&]{select(OriginalCrashAnimationSelection::AirGetUp);};
  cb.playSpecialLanding=[&]{select(OriginalCrashAnimationSelection::SpecialLanding);};cb.rebakeResetClip=[&]{select(OriginalCrashAnimationSelection::ResetClip);};
  cb.seekClipSeconds=host.seek;cb.reportImpact=host.impact;cb.setRecoveryPresentation=host.presentation;
  cb.cameraShake=host.cameraShake;cb.notify=[&](OriginalCrashObserver value){host.observer(int(value));};
  cb.refundCrashBoost=[&](bool quick){refund(quick);};cb.requestReset=host.requestReset;return cb;
 }
 void stepControl(uint32_t command){
  originalCrashRecoveryMeter(control,command,riderCategory);
  auto inputs=host.recoveryInputs();inputs.resetPermission470=resetPermission;
  if(control.phase==0){auto effects=originalCrashInitialControlStep(control,motion,actor,host.clip());if(effects.beginPredictor)beginPredictor();if(effects.playContinuation)host.play(effects.continuationSemantic);if(effects.updatePlaybackRate)playback();if(effects.groundContinuationObserverRequired)host.observer(int(OriginalCrashObserver::GroundMoving));}
  else if(control.phase==1)originalCrashAirRecoveryStep(control,motion,actor,inputs,continuation());
  else if(control.phase==2)originalCrashGroundRecoveryStep(control,motion,actor,inputs,continuation());
  else if(control.phase==3||control.phase==4){
   OriginalCrashRecoveryCallbacks cb;cb.reportImpact=host.impact;cb.setRecoveryPresentation=host.presentation;
   cb.stopCrashEffect=[&]{host.observer(int(OriginalCrashObserver::StopCrash));};cb.playAnimation=host.play;
   cb.enterControl=host.enterControl;
   cb.enterMotion=[&](int next){active=false;host.leaveMotion(next);};
   cb.setAirScoringStance=[&](bool value){host.observer(2000+int(value));};
   cb.refundCrashBoost=[&](bool quick){refund(quick);};cb.requestReset=host.requestReset;
   auto clip=host.clip();if(control.phase==3)originalCrashGetUpStep(control,motion.submode,clip.progress,clip.complete,host.stanceDiffers(),cb);
   else originalCrashResetClipStep(control,resetPermission,clip.complete,cb);
  }else throw std::runtime_error("Unknown recovered crash phase");
 }
 void stepMotion(const CollisionWorld& terrain,const WorldBodyCollision& world,const std::array<OriginalGroundProfile,19>& materials,const OriginalLandingProfile& landing,const std::function<int(int)>& property){
  terrain_original::Rounding rounding;originalCrashDetachedStep(motion,actor);
  if(motion.submode==0){
   const int surface=std::clamp(actor.surface,0,18);const auto& material=materials[surface];
   auto alignment=originalCrashSlidingForces(material.surface,motion,actor,actor.surfaceVelocity,host.clip().animationClass,originalOrientationBasis(actor.quaternion).forward);
   actor.quaternion=originalRebuildOrientation(originalAirAlignment(actor.quaternion,alignment.normal,alignment.heading,alignment.gain,alignment.maximumRate).quaternion).quaternion;
   OriginalCrashWorldQueries queries(terrain,world,property,browser_crash_partial_world(),browser_rider_scope());const auto probe=queries.terrainContact({actor.position,actor.groundNormal},&browser_rider_terrain_cache());
   if(host.groundPatch&&probe.fraction>=0)host.groundPatch(probe);
   auto hit=queries.motionHit(probe);
   auto effects=originalCrashSlidingContact(motion,actor,hit,landing.bodyScale,landing.materials[surface].depth3,alignment.relativeVelocityBeforeForces);
   if(effects.impact)host.collisionImpact(effects.impactSpeed);if(effects.requestReset)host.requestReset(1);if(effects.beginPredictor)beginPredictor(effects.predictorSpeedLimit);
  }else originalCrashAirFirstPhase(motion,actor,trajectory,[&](auto end,auto start,int mode){
   // A dynamic entity the browser has no callbacks for (flag 0x40000000 without a scripted seed: every one in the streamed
   // peak worlds, e.g. Gravitude's mdl_ERA5_CRbillboard_1000) makes the world query incomplete (cause 2). The flight
   // prediction then just stops (web/prediction_bridge.cpp); here the throw used to stop the core mid-tick (a frozen game in
   // the whole-mountain runs, docs/peak3.md section 6), so the crash flight ignores that instance and keeps the other hits.
   auto hit=queryOriginalAirTrajectoryWorld(terrain,&world,end,start,mode);if(!hit.complete&&hit.unavailableCause==2&&browser_crash_partial_world())hit.complete=true;return hit;});
  ++ticks;
 }
 std::array<float,3> contactTranslation{};
  // 105D98 dispatch of a body contact while the rider is the ragdoll (motion 2): also used by a
  // 121750 whose touchdown entered the crash before its 13AA48 query (web/animation_bridge.cpp).
  void impactReaction(const WorldBodyHit& hit,const ObstacleResponse& response,terrain_original::Vector before,const std::function<int(int)>& property,OriginalCollisionProfile& profile,OriginalCollisionHistory& history){
   terrain_original::Rounding rounding;
   OriginalCollisionEvent event;event.pointCm=hit.pointCm;event.normal=response.normal;event.closingSpeedCmps=response.closingSpeedCmps;event.surface=hit.surface;event.surfaceProperty44=property(hit.surface);
   OriginalCollisionContext context;context.motionMode=2;context.controlState=8;context.ragdollSubmode=motion.submode;context.velocityCmps=actor.velocity;
   auto reaction=originalCollisionReaction(profile,history,context,event,host.random);
   if(reaction.kind==OriginalCollisionReactionKind::SurfaceReset)host.requestReset(1);
   else if(reaction.kind==OriginalCollisionReactionKind::RagdollImpact){control.impactPending54=1;control.impactVelocity60=before;if(reaction.resetPredictor){if(!host.speedLimit)throw std::runtime_error("Crash predictor restart needs rider+0x2E4");beginPredictor(host.speedLimit());}} //105D98 (motion 2, owner+0x30 == 1): 1135B8 = 113198 + 113618, a full restart, not 113618 alone
  }
 void contacts(BodyCollisionVolume& volume,const CollisionWorld& terrain,const WorldBodyCollision& world,const OriginalLandingProfile& landing,const std::function<int(int)>& property,OriginalCollisionProfile& profile,OriginalCollisionHistory& history){
  terrain_original::Rounding rounding;OriginalCrashWorldQueries queries(terrain,world,property,browser_crash_partial_world(),browser_rider_scope());const bool wasSliding=motion.submode==0;contactTranslation={};
  //106538 accumulates the same displacement into rider+9D0, which 121750/310530 commits to the cached pose.
  auto translate=[&](auto delta){for(unsigned k=0;k<3;k++){contactTranslation[k]=terrain_original::add(contactTranslation[k],delta[k]);volume.broadCenterCm[k]=terrain_original::add(volume.broadCenterCm[k],delta[k]);for(unsigned i=0;i<volume.count;i++)volume.spheres[i].centerCm[k]=terrain_original::add(volume.spheres[i].centerCm[k],delta[k]);}};
  auto impact=[&](const WorldBodyHit& hit,const ObstacleResponse& response,terrain_original::Vector before){impactReaction(hit,response,before,property,profile,history);};
  if(motion.submode==0){
   auto query=queries.slidingBody(volume,actor.groundNormal,&browser_rider_body_cache());
   if(query.best.hit){const auto& hit=query.best;auto before=actor.velocity;auto response=originalCrashSlidingBodyResponse(hit.penetrationCm,actor.groundNormal,hit.normal,actor.velocity,hit.surfaceVelocityCmps);
    // 138640 pushes (106538) and bounces the velocity inline; it calls only the hit entity's contact-velocity callback (vt+0x154),
    // never 105D98, so a sliding bounce leaves the collision history +0x3E0/+0x3F0 and the impact flag +0x54 alone (PS2 Gravitude
    // Mac 977: the velocity bounces, +0x3E0 only decays).
    if(response.accepted){for(unsigned k=0;k<3;k++)actor.position[k]=terrain_original::add(actor.position[k],response.translationCm[k]);translate(response.translationCm);actor.velocity=response.velocityCmps;(void)before;}}

  }else{
   auto query=queries.airborneBody(volume,browserHumanRider,&browser_rider_body_cache());
   if(query.best.hit){const auto& hit=query.best;auto before=actor.velocity;auto ground=queries.motionHit(queries.terrainContact({actor.position,hit.normal},&browser_rider_terrain_cache()));auto effects=originalCrashAirBodyResponse(motion,actor,hit.penetrationCm,hit.normal,ground,60);
    lastAirContact={1,hit.penetrationCm,hit.normal[0],hit.normal[1],hit.normal[2],ground.fraction,ground.normal[0],ground.normal[1],ground.normal[2],ground.point[0],ground.point[1],ground.point[2],float(ground.surface),float(effects.landed),float(hit.instance),float(hit.surface),before[0],before[1],before[2],actor.position[0],actor.position[1],actor.position[2],float(hit.terrain),0};
    if(effects.moved)translate(effects.translation);
    if(effects.landed){control.impactPending54=1;control.impactVelocity60=effects.impactVelocity;host.collisionImpact(-effects.impactSpeed);}
    // Not landed: 137B98..137CB4 only push along the body normal, drop the closing velocity and restart the predictor (1135B8); no 105D98 reaction.
    if(effects.beginPredictor)beginPredictor(effects.predictorSpeedLimit);
   }
  }
  if(host.instanceContacts)host.instanceContacts(volume,[&](std::array<float,3> delta){translate(delta);});
  if(host.pairs)host.pairs();
  if(actor.detached){auto board=originalCrashDetachedBody({actor.detachedPosition,actor.detachedQuaternion},landing.bodyScale);auto query=queries.detachedBody(board);if(query.best.hit)originalCrashDetachedBodyResponse(motion,actor,query.best.penetrationCm,query.best.normal,false,60,host.random);}
  if(wasSliding)originalCrashSlidingFinish(motion,actor);
 }
};
}
