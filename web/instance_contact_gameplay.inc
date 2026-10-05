// Original 0x105398 instance-contact phase (104E70 selection/aggregation, 1231A8
// projection, 1057B8 response and 105D98 notification) for the browser rider.
// It runs once per ground (13F178 after 13F488), air (139C88 after 13AA48),
// crash (137860/138640) and rail-leave (13BFA8) tick and owns the filter-1
// query that pickup collection (selected contact copied to rider+9E0) uses.
#include "../engine/instance_contact.hpp"
#include "../engine/rail_body_contact.hpp"
#include "roller_gameplay.inc"
RIDER_LOCAL uint32_t instanceContactTickShared=0xffffffffu; // tick whose post-pose phase already collected pickups
RIDER_LOCAL std::array<uint32_t,64> browserRiderTriggers=[]{std::array<uint32_t,64> a;a.fill(0xffffffffu);return a;}(); // rider+0x5B8: this tick's contacted instances (120F20 clears it, web/stage_world.inc Boost volumes read it)
OriginalCollisionReaction dispatch_body_event(const OriginalCollisionEvent&,const BodyCollisionVolume&,std::array<float,3>,int,int,const CollisionRandom&);
void reseed_prediction(OriginalAirState);void begin_prediction(OriginalAirState);
// 1057B8's air landing on a scenery top, rider side (web/animation_bridge.cpp, after the controller includes).
static float surface_landing_channel_time();static void surface_landing_award(float speed,const OriginalInstanceContactPacket& hit);
static void surface_landing_boundary(bool stanceDiffers);static void surface_landing_control(int control);
RIDER_LOCAL static unsigned instanceContactPasses=0,instanceContactResponses=0,instanceContactNotifications=0,instanceContactSoft=0,instanceContactIncomplete=0,instanceContactUnsupported=0;
RIDER_LOCAL static unsigned railBodyContactNotifications=0;RIDER_LOCAL static int railBodyContactStage=0;RIDER_LOCAL static std::array<float,16> instanceContactLast{};RIDER_LOCAL static std::array<float,3> instanceContactVelocity{}; /*QA: rider velocity after the last 104E70 response*/RIDER_LOCAL static uint32_t instanceContactUnsupportedInstance=0;
void collect_pickup_contact(const WorldBodyQuery&,uint32_t resource);
RIDER_LOCAL extern std::optional<terrain_original::RiderScope> riderScope; // web/core.cpp (rider+0x860, 120E50 -> 332DB8)
void stage_trigger_store(const OriginalInstanceContactPacket&,const OriginalInstanceContactRecord&); // web/stage_script_gameplay.inc (rider+A30 -> 121818)
// Mutable rider view for 1057B8. Callbacks run the browser counterparts of the
// original callees; the translation is the 106538 displacement (+9D0 commit).
struct BrowserInstanceActor {
 int motion=0,controlState=0,ownerWord30=0;
 std::array<float,3>* position=nullptr;std::array<float,3>* velocity=nullptr;std::array<float,4>* quaternion=nullptr;
 std::array<float,3> groundNormal{},railDirection{},presentationUp{0,0,1};
 std::function<void(const std::array<float,3>&)> translate;   // 106538 companion fields (body, 9D0)
 std::function<void(const OriginalInstanceContactPacket&)> steer; // 1065B0 (motion 0)
 std::function<void()> rebuild;                                // 11E098
 std::function<void(const OriginalCollisionEvent&)> notify;    // 105D98
 std::function<void(const OriginalCollisionEvent&)> soft;      // 108388(rider,record,1)
 std::function<void()> beginPredictor;                         // 1135B8 (air surface landing)
 // 1057B8's air landing on a scenery top (the rider only): 312AB0 channel-2 time, 10E910(rider, 0, 0, 0, hit, f12),
 // 119E38(score, stanceDiffers, 0), 11FEC8(13) then 11FEC8(5), 3128E8(anim, 268, 1, -1). Unset: counted as unsupported.
 std::function<float()> channelTime;
 std::function<void(float,const OriginalInstanceContactPacket&)> landingAward;
 std::function<void(bool)> scoreBoundary;
 std::function<void(int)> requestControl;
 std::function<void(int)> play;
 std::array<float,3> offset9D0{};                               // rider+9D0 at the phase (108A48 board query)
};
static ContactQuad contact_quad(const std::array<float,3>& v,float w=0){return {v[0],v[1],v[2],w};}
static std::array<float,3> contact_xyz(const ContactQuad& q){return {q[0],q[1],q[2]};}
static OriginalCollisionEvent instance_contact_event(const OriginalInstanceContactRecord& record,int surface){
 OriginalCollisionEvent event;event.pointCm=contact_xyz(record.point);event.incomingDirection=contact_xyz(record.direction);event.normal=contact_xyz(record.normal);
 event.closingSpeedCmps=record.closingSpeed;event.surface=surface;if(surface>=0&&surface<19)event.surfaceProperty44=browser_surface_property(surface);return event;
}
// Returns the accumulated 106538 translation.
static std::array<float,3> run_instance_contacts(BodyCollisionVolume& volume,std::array<float,3> queryOffset,const std::array<float,3>* callerDirection,BrowserInstanceActor& actor){
 std::array<float,3> moved{};
 if(!browserBodies||browserResetActive)return moved;
 ++instanceContactPasses;
 OriginalInstanceContactRider rider;rider.controlState=actor.controlState;rider.motionMode=actor.motion;rider.ownerWord30=actor.ownerWord30;
 rider.groundNormal=contact_quad(actor.groundNormal);rider.railDirection=contact_quad(actor.railDirection);rider.velocity=contact_quad(*actor.velocity);
 const ContactQuad caller=callerDirection?contact_quad(*callerDirection):ContactQuad{};
 OriginalInstanceContactPhaseHost host;std::optional<WorldBodyQuery> query;
 host.query=[&](OriginalInstanceContactPacket& out)->float{
  BodyCollisionVolume shifted=volume;
  {terrain_original::Rounding rounding;for(unsigned k=0;k<3;k++)if(queryOffset[k]!=0){shifted.broadCenterCm[k]=terrain_original::add(shifted.broadCenterCm[k],queryOffset[k]);for(unsigned i=0;i<shifted.count;i++)shifted.spheres[i].centerCm[k]=terrain_original::add(shifted.spheres[i].centerCm[k],queryOffset[k]);}}
  query=browserBodies->query(shifted,actor.groundNormal,nullptr,false,1,nullptr,riderScope?&*riderScope:nullptr); // 104E70: 334458 over the rider+0x860 scope list
  pickupQueryComplete=query->complete();pickupUnsupported=query->unsupportedInstances.size();pickupContacts=query->instanceContacts.size();
  if(!query->complete()){++instanceContactIncomplete;instanceContactUnsupportedInstance=query->unsupportedInstances.empty()?0xffffffffu:query->unsupportedInstances[0];return -1;} // Never respond/award from an incomplete list.
  std::vector<OriginalInstanceContactPacket> packets;
  for(const auto& hit:query->instanceContacts){
   OriginalInstanceContactPacket p;p.point=contact_quad(hit.pointCm,1);p.normal=contact_quad(hit.normal);p.surfaceVelocity=contact_quad(hit.surfaceVelocityCmps);
   p.depth=hit.penetrationCm;p.surface=hit.surface;p.instance=hit.instance;p.node=hit.node;
   p.descriptor={std::bit_cast<float>(hit.nodeValue),std::bit_cast<float>(hit.nodeAuxiliary),hit.nodeFlags,int16_t(hit.surface)};p.instanceFlags=hit.instanceFlags;p.entity=hit.entity;
   packets.push_back(p);
  }
  auto selected=originalInstanceContactQuery(packets,shifted.broadCenterCm,-1,browser_entity_selected); // 32F650(mode1): 32F8C0 returns -1
  if(!packets.empty()){browserRiderTriggers.fill(0xffffffffu);for(size_t k=0;k<selected.instances.size()&&k<63;k++)browserRiderTriggers[k]=selected.instances[k];} // 104E70 -> rider+0x5B8 (0x108C28; untouched on an empty query)
  if(selected.selected>=0)out=selected.contact;return selected.result;
 };
 // 106F78 first: hips-axis rail contact (engine/rail_body_contact.hpp); its result is ignored.
 host.boundaryContacts=[&]{
  if(cachedCrashWorld.empty())return;
  OriginalRailBodyContactRider rb;rb.motionMode=actor.motion;rb.ownerWord30=actor.ownerWord30;rb.velocity=*actor.velocity;rb.groundNormal=actor.groundNormal;
  rb.hipsPosition=cachedCrashWorld[0].position;rb.hipsQuaternion=cachedCrashWorld[0].rotation; // rider+0x89C = "hips"
  OriginalRailBodyContactHost bh;bh.query=browserRailQuery;
  bh.attachable=[&]{auto r=rail_view();r.motionMode=actor.motion;r.velocity=*actor.velocity;r.offset9D0=actor.offset9D0;OriginalRailQueryResult tmp;return originalRailAttachTest(r,rail_access(),tmp);};
  bh.translate=[&](const RailVector& d){terrain_original::Rounding rounding;for(unsigned k=0;k<3;k++){(*actor.position)[k]=terrain_original::add((*actor.position)[k],d[k]);moved[k]=terrain_original::add(moved[k],d[k]);actor.offset9D0[k]=terrain_original::add(actor.offset9D0[k],d[k]);}if(actor.translate)actor.translate(d);};
  bh.steer=[&](const RailVector& n){if(actor.steer){OriginalInstanceContactPacket p;p.normal=contact_quad(n);actor.steer(p);}};
  bh.rebuild=[&]{if(actor.rebuild)actor.rebuild();};
  bh.notify=[&](const OriginalInstanceContactRecord& rec,int surface){++railBodyContactNotifications;*actor.velocity=rb.velocity;if(actor.notify)actor.notify(instance_contact_event(rec,surface));};
  auto result=originalRailSnapContact(rb,bh,browser_rail_snap_hooks());railBodyContactStage=result.stage;
   };
 host.store=[&](const OriginalInstanceContactPacket& p,const OriginalInstanceContactRecord& record){browser_set_piece_contact(p.instance);stage_trigger_store(p,record);if(!browser_scripted_contact(p,record))collect_pickup_contact(*query,p.instance);}; // rider+A30 -> 121818
 host.respond=[&](const OriginalInstanceContactPacket& hit,const OriginalInstanceContactRecord& record,float depth){
  ++instanceContactResponses;
  OriginalInstanceResponseRider r;r.controlState=actor.controlState;r.motionMode=actor.motion;r.ownerWord30=actor.ownerWord30;
  r.velocity=contact_quad(*actor.velocity);r.presentationUp=contact_quad(actor.presentationUp);r.secondaryCounter=collisionHistory.secondaryCounter;
  // 119E38's stance argument of a surface landing (+0x320 != +0x324): it was never set, so a switch-stance rider landing on a bouncy
  // instance cleared score +0x00 (PS2 hl2/crash-eba3 1664, hl2/press-bra2 1221: +0x00 stays 1)
  r.stanceDiffers=!physicsState.state320Equals324;
  OriginalInstanceResponseHost h;
  h.entityRigid=[&]{return browser_entity_rigid(hit.instance);};
  h.softCollision=[&](const OriginalInstanceContactRecord& rec){++instanceContactSoft;if(actor.soft)actor.soft(instance_contact_event(rec,hit.surface));};
  h.translate=[&](const ContactQuad& delta){const auto d=contact_xyz(delta);terrain_original::Rounding rounding;for(unsigned k=0;k<3;k++){(*actor.position)[k]=terrain_original::add((*actor.position)[k],d[k]);moved[k]=terrain_original::add(moved[k],d[k]);}if(actor.translate)actor.translate(d);};
  h.steer=[&](const OriginalInstanceContactPacket& p){if(actor.steer)actor.steer(p);};
  h.rebuild=[&]{if(actor.rebuild)actor.rebuild();};
  h.channelClass=[]{return graph.currentClass(2);};
  h.notify=[&](const OriginalInstanceContactRecord& rec,int surface){++instanceContactNotifications;*actor.velocity=contact_xyz(r.velocity);if(actor.notify)actor.notify(instance_contact_event(rec,surface));r.velocity=contact_quad(*actor.velocity);}; //104E70 writes +0x1E0 before 105D98 and never after: keep what the reaction did (a 10EB30 crash from the ground projects it, 13B/crash motion enter; tech-oob-neutral 2691)
  // Air landing on a scenery top (class 1/2/9/11, up.n > 0.3): the peak-impact stat, then (unless clip 268 is already
  // past 0.2) 10E910 / 119E38 / control 13 -> 5 / clip 268, and the predictor restart. The rider actor wires them
  // (run_rider_instance_contacts: PS2 peak3/fr-throne-unload 15035, entry probe of 11FEC8 ra 0x105CC0 / 0x105CCC).
  h.surfaceLanding=[&](float closing){collisionHistory.peakImpactCmps=closing<collisionHistory.peakImpactCmps?collisionHistory.peakImpactCmps:closing;audio_event(AE_RUMBLE_IMPACT,closing);}; /*105C30 -> owner +0xDFC rumble*/
  h.requestedSemantic=[]{return graph.requestedSemantics[2];};
  h.channelTime=[&]{if(actor.channelTime)return actor.channelTime();++instanceContactUnsupported;return 0.f;};
  h.landingAward=[&](float speed,const OriginalInstanceContactPacket& p){if(actor.landingAward){actor.landingAward(speed,p);return;}++instanceContactUnsupported;audio_event(AE_RUMBLE_IMPACT,speed*0.5f);}; /*10EAD8 rumble*/
  h.scoreBoundary=[&](bool s){if(actor.scoreBoundary)actor.scoreBoundary(s);};
  h.requestControl=[&](int c){if(actor.requestControl)actor.requestControl(c);};
  h.play=[&](int semantic){if(actor.play)actor.play(semantic);};
  h.beginPredictor=[&]{*actor.velocity=contact_xyz(r.velocity);if(actor.beginPredictor)actor.beginPredictor();};
  auto result=originalInstanceContactResponse(r,hit,record,depth,h);
  *actor.velocity=contact_xyz(r.velocity);collisionHistory.secondaryCounter=r.secondaryCounter;instanceContactVelocity=*actor.velocity;
  instanceContactLast={float(result.path),result.closingSpeed,result.impulse,result.upAlignment,float(hit.instance),float(hit.node),hit.depth,hit.normal[0],hit.normal[1],hit.normal[2],record.closingSpeed,float(motionTick),moved[0],moved[1],moved[2],float(actor.motion)};
 };
 // 105770: 296088(audio, rider, instance, node) unless instance flag 0x2000 (audio observer).
 host.audio=[&](const OriginalInstanceContactPacket& hit){const auto& v=*actor.velocity;audio_event(AE_CONTACT,float(hit.instance),float(hit.node),std::sqrt(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]));};
 originalInstanceContactPhase(rider,callerDirection?&caller:nullptr,host);
 return moved;
}
// 108388 from control 1 (web/boardpress_gameplay.inc, included after this file)
static int board_press_soft_cancel();static void board_press_soft_enter(int,float);
// Browser rider (physicsState) in motion 0/1/4.
static std::array<float,3> run_rider_instance_contacts(BodyCollisionVolume& volume,std::array<float,3> queryOffset,const std::array<float,3>* callerDirection,int motion,int eventControl,const std::array<float,3>& railDirection){
 BrowserInstanceActor actor;actor.motion=motion;actor.controlState=physicsState.controlState;actor.ownerWord30=0;
 actor.position=&physicsState.position;actor.velocity=&physicsState.velocity;actor.quaternion=&physicsState.quaternion;
 actor.groundNormal=physicsState.normal;actor.railDirection=railDirection;actor.presentationUp=physicsState.presentationUp;
 actor.channelTime=surface_landing_channel_time;actor.landingAward=surface_landing_award;actor.scoreBoundary=surface_landing_boundary;
 actor.requestControl=surface_landing_control;actor.play=[](int semantic){if(!graph.enter(semantic,-1,~uint64_t(0),true))throw std::runtime_error("Missing surface landing animation");}; /*3128E8(anim, 268, force 1, -1)*/
 actor.translate=[&](const std::array<float,3>& d){terrain_original::Rounding rounding;for(unsigned k=0;k<3;k++){instancePendingTranslation[k]=terrain_original::add(instancePendingTranslation[k],d[k]); /*106538: +0x9D0 now*/volume.broadCenterCm[k]=terrain_original::add(volume.broadCenterCm[k],d[k]);for(unsigned i=0;i<volume.count;i++)volume.spheres[i].centerCm[k]=terrain_original::add(volume.spheres[i].centerCm[k],d[k]);}};
 actor.steer=[&](const OriginalInstanceContactPacket& p){physicsState.quaternion=originalObstacleOrientation(physicsState.quaternion,physicsState.boardUp,physicsState.physicalForward,contact_xyz(p.normal));};
 actor.rebuild=[&]{auto rebuilt=originalRebuildOrientation(physicsState.quaternion);physicsState.quaternion=rebuilt.quaternion;physicsState.physicalForward=rebuilt.forward;physicsState.boardUp=rebuilt.up;};
 // 108388 gates on the motion mode (11FE98 == 0 or 4), not on contact: 13F178 runs 105398 after a passive
 // departure (114298) while the motion is still 0, so that tick's soft reaction enters control 3 (pipe-uber 1142).
 actor.notify=[&](const OriginalCollisionEvent& event){
  auto reaction=dispatch_body_event(event,volume,physicsState.velocity,motion,eventControl,[](){return rng.nextMotion();});
  if(reaction.controlOneCancelled)board_press_soft_enter(reaction.animation,reaction.manualSpin); /*108388 from control 1 (web/boardpress_gameplay.inc)*/
  else if(reaction.kind==OriginalCollisionReactionKind::Soft&&(motion==0||motion==4)&&!reaction.cancelControlOne&&(physicsState.controlState==0||physicsState.controlState==2||(browserRailActive&&physicsState.controlState==7)))soft_collision_begin(reaction.animation,reaction.manualSpin);
 };
 actor.soft=[&](const OriginalCollisionEvent& event){
  terrain_original::Rounding rounding;auto physical=originalOrientationBasis(physicsState.quaternion);OriginalCollisionContext context;
  if(volume.reactionFrame)context.presentation=*volume.reactionFrame;
  context.physical={physical.right,physical.forward,physical.up,physicsState.position};context.velocityCmps=physicsState.velocity;
  context.motionMode=motion;context.controlState=eventControl;context.reverseStance=physicsState.reverseStance;context.manualSpin=physicsState.manualSpin;
  if(eventControl==1)context.cancelControlOne=board_press_soft_cancel; // 108388's 131348 (web/boardpress_gameplay.inc)
  auto reaction=originalSoftCollisionReaction(collisionProfile,context,event,[](){return rng.nextMotion();});
  if(reaction.controlOneCancelled)board_press_soft_enter(reaction.animation,reaction.manualSpin); /*108388 from control 1 (web/boardpress_gameplay.inc)*/
  else if(reaction.kind==OriginalCollisionReactionKind::Soft&&(motion==0||motion==4)&&!reaction.cancelControlOne&&(physicsState.controlState==0||physicsState.controlState==2||(browserRailActive&&physicsState.controlState==7)))soft_collision_begin(reaction.animation,reaction.manualSpin);
 };
 actor.beginPredictor=[&]{if(!::grounded)begin_prediction({physicsState.position,physicsState.velocity});}; // 1135B8: full restart (113198 + 113618)
 // rider+9D0 here: this tick's landing (query offset) and accepted 13F488/13AA48 body-response translations.
 actor.offset9D0=queryOffset;if(browserBodyResponse&&browserBodyResponse->accepted){terrain_original::Rounding rounding;for(unsigned k=0;k<3;k++)actor.offset9D0[k]=terrain_original::add(actor.offset9D0[k],browserBodyResponse->translationCm[k]);}
 const auto velocityBefore=physicsState.velocity;
 auto moved=run_instance_contacts(volume,queryOffset,callerDirection,actor);instancePendingTranslation={}; // (the caller adds moved to the tick's contacts translation)
 if(moved!=std::array<float,3>{}||physicsState.velocity!=velocityBefore){
  // Unlike the 13F488 browser path, 1057B8 leaves the +3A0/+3B0 ground tangents
  // untouched (verified: carve tick 408 keeps the pre-impact tangents).
  commit_rider_physics();
  // A departure tick (ground motion 0, rider already left): 13F2CC 11FE78(1) -> 1399E0 seeds the flight
  // with 1135B8 from +0x110/+0x1E0 after 105398, so the contact's push/bounce starts the flight (pipe-uber 1143).
  if(motion==0&&!::grounded&&!crash.active)begin_prediction({physicsState.position,physicsState.velocity});
 }
 return moved;
}
extern "C" EMSCRIPTEN_KEEPALIVE float* instance_contact_info(){RIDER_LOCAL static float v[26];for(unsigned k=0;k<3;k++)v[23+k]=instanceContactVelocity[k]; /*[23..25] velocity after the last response*/v[0]=instanceContactPasses;v[1]=instanceContactResponses;v[2]=instanceContactNotifications;v[3]=instanceContactSoft;v[4]=instanceContactIncomplete;v[5]=instanceContactUnsupported;for(unsigned i=0;i<16;i++)v[6+i]=instanceContactLast[i];v[22]=float(instanceContactUnsupportedInstance);return v;}
// Crash motion 2 (137860 airborne / 138640 sliding): owner+0x30 is the submode.
static void crash_instance_contacts(BodyCollisionVolume& volume,const std::function<void(std::array<float,3>)>& translate){
 BrowserInstanceActor actor;actor.motion=2;actor.controlState=8;actor.ownerWord30=crash.motion.submode;
 actor.position=&crash.actor.position;actor.velocity=&crash.actor.velocity;actor.quaternion=&crash.actor.quaternion;
 actor.groundNormal=crash.actor.groundNormal;actor.presentationUp=physicsState.presentationUp;
 actor.translate=[&](const std::array<float,3>& d){translate(d);};
 actor.rebuild=[&]{crash.actor.quaternion=originalRebuildOrientation(crash.actor.quaternion).quaternion;};
 actor.notify=[&](const OriginalCollisionEvent& event){
  OriginalCollisionContext context;context.motionMode=2;context.controlState=8;context.ragdollSubmode=crash.motion.submode;context.velocityCmps=crash.actor.velocity;
  auto reaction=originalCollisionReaction(collisionProfile,collisionHistory,context,event,crash.host.random);if(context.controlState!=9&&reaction.kind!=OriginalCollisionReactionKind::SurfaceReset)audio_event(AE_RUMBLE_IMPACT,event.closingSpeedCmps); /*105E7C: owner +0xDFC rumble*/
  if(reaction.kind==OriginalCollisionReactionKind::SurfaceReset)crash.host.requestReset(1);
  else if(reaction.kind==OriginalCollisionReactionKind::RagdollImpact){crash.control.impactPending54=1;crash.control.impactVelocity60=crash.actor.velocity;if(reaction.resetPredictor)crash.beginPredictor(physicsProfile.speedLimit);} //105D98 -> 1135B8(rider+788,pos,vel,rider+2E4): full restart (metro-mix-glide 719)
 };
 // 108388 ignores motion 2 (its grounded gate), so the soft path has no effect here.
 advance_pickup_timers(motionTick);instanceContactTickShared=motionTick; // same post-pose tick as the ground/air phase
 run_instance_contacts(volume,{},nullptr,actor);
}
// crash.host.instanceContacts=crash_instance_contacts: registered per rider context by rider_statics_animation (web/animation_bridge.cpp)
