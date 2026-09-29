#include "native_world.h"
#include "rider_pose_stages.h"
#include "replay_io.h"
#include "race_event_asset.h"
#include "original_command.hpp"
#include <limits>
#include <cerrno>
namespace ssx {
namespace {
double number(id value){
    if(![value isKindOfClass:NSNumber.class]||CFGetTypeID((__bridge CFTypeRef)value)==CFBooleanGetTypeID()||!std::isfinite([value doubleValue]))throw std::runtime_error("World seed requires a finite number");
    return [value doubleValue];
}
int32_t integer(id value){double x=number(value);if(x!=std::trunc(x)||x<INT32_MIN||x>INT32_MAX)throw std::runtime_error("World seed integer out of range");return int32_t(x);}
uint32_t word(id value){double x=number(value);if(x!=std::trunc(x)||x<0||x>UINT32_MAX)throw std::runtime_error("World seed word out of range");return uint32_t(x);}
uint64_t mask(id value){if(![value isKindOfClass:NSString.class]||![value hasPrefix:@"0x"])throw std::runtime_error("World mask must be a hex string");const char* start=[value UTF8String];char* end=nullptr;errno=0;auto result=strtoull(start,&end,16);if(errno||end==start||*end)throw std::runtime_error("Invalid world mask");return result;}
bool flag(id value){if(!value||CFGetTypeID((__bridge CFTypeRef)value)!=CFBooleanGetTypeID())throw std::runtime_error("World seed requires a boolean");return [value boolValue];}
NSArray* array(id value,size_t size=SIZE_MAX){if(![value isKindOfClass:NSArray.class]||(size!=SIZE_MAX&&[value count]!=size))throw std::runtime_error("World seed array shape differs");return value;}
NSDictionary* object(id value){if(![value isKindOfClass:NSDictionary.class])throw std::runtime_error("World seed requires an object");return value;}
template<size_t N>std::array<float,N> vector(id value){auto a=array(value,N);std::array<float,N> result;for(size_t k=0;k<N;++k){double x=number(a[k]);if(std::abs(x)>std::numeric_limits<float>::max())throw std::runtime_error("World vector outside float range");result[k]=float(x);}return result;}
OriginalPairRecord pairRecord(NSDictionary* d){return {flag(d[@"enabled"]),float(number(d[@"planar_distance_cm"])),float(number(d[@"bearing"])),integer(d[@"last_contact_tick"]),integer(d[@"last_checked_tick"]),integer(d[@"last_attack_tick"])};}
}
NativeRaceWorld::NativeRaceWorld(NSDictionary* source,NSString* assetRoot,const CollisionWorld& terrain,const CollisionWorld& scenery,
    const WorldBodyCollision* bodies,Vec3 spawn,Vec3 normal,WorldControls humanInput):humanControls(std::move(humanInput)){
    source=object(source);if(integer(source[@"schema_version"])!=1||!humanControls)throw std::runtime_error("Unsupported native world seed or absent human input");
    auto race=readOriginalRaceEventAsset(object(source[@"original_race_event"]));
    NSString* hash=source[@"provenance"][@"ee_sha256"];
    if(![hash isKindOfClass:NSString.class]||race.sourceEEHash!=hash.UTF8String)throw std::runtime_error("Shared seed provenance differs from race seed");
    auto roster=array(source[@"participants"],race.participants.size());
    if(roster.count==0||roster.count>6)throw std::runtime_error("Invalid shared world roster size");
    auto npc=object(source[@"original_npcs"]);auto pairs=object(source[@"original_pair_collision"]);
    if(integer(pairs[@"tick"])!=race.clock.totalTicks||integer(npc[@"provenance"][@"total_ticks"])!=race.clock.totalTicks)throw std::runtime_error("Shared seed clocks differ");
    eventVariant=int8_t(race.configuration[1]);
    auto classes=array(source[@"animation_classes"],animationClasses.size());for(size_t i=0;i<animationClasses.size();++i)animationClasses[i]=integer(classes[i]);
    simulation=std::make_unique<RiderWorld>(race,terrain,scenery,bodies);participants.resize(roster.count);
    OriginalRandomState random;auto randomWords=array(pairs[@"random_state"],6);for(unsigned i=0;i<6;++i)random.words[i]=word(randomWords[i]);simulation->seedRandom(random);
    for(NSDictionary* p in array(npc[@"ai_paths"])){
        if(integer(p[@"index"])!=int(paths.size()))throw std::runtime_error("AI path bank order differs");
        OriginalNpcPath path;auto& g=path.geometry;g.origin=vector<3>(p[@"origin"]);g.low=vector<3>(p[@"low"]);g.high=vector<3>(p[@"high"]);
        for(id segment in array(p[@"segments"]))g.segments.push_back(vector<4>(segment));
        if(g.segments.empty())throw std::runtime_error("Empty native AI path");
        for(NSDictionary* e in array(p[@"events"]))g.events.push_back({word(e[@"type"]),word(e[@"value"]),float(number(e[@"start"])),float(number(e[@"end"]))});
        path.flags38=word(p[@"flags38"]);path.field3C=word(p[@"field3c"]);paths.push_back(std::move(path));
    }
    auto relations=array(npc[@"relationships"][@"scores"],6);for(unsigned a=0;a<6;++a){auto row=array(relations[a],6);for(unsigned b=0;b<6;++b)relationships[a][b]=integer(row[b]);}
    OriginalRiderPairSystem::Records records;auto pairSeeds=array(pairs[@"participants"],roster.count);
    for(size_t slot=0;slot<participants.size();++slot){
        auto& p=participants[slot];NSDictionary* d=object(roster[slot]);NSDictionary* initial=object(d[@"initial"]);NSDictionary* pair=object(pairSeeds[slot]);
        if(integer(d[@"slot"])!=int(slot)||integer(pair[@"slot"])!=int(slot))throw std::runtime_error("Native selected roster order differs");
        p.human=flag(initial[@"is_human"]);if(p.human!=race.participants[slot].human)throw std::runtime_error("Native controller ownership differs from race seed");
        NSString* package=d[@"package"];if(![package isKindOfClass:NSString.class]||![package hasPrefix:@"RIDER_"]||![package.lastPathComponent isEqual:package])throw std::runtime_error("Invalid native rider package");
        p.rider=std::make_unique<PrototypeRider>();
        initializeNativeReplay(*p.rider,@{@"native":@{@"initial":initial}},terrain,spawn,normal,0);
        if(p.rider->currentControlState()!=integer(d[@"control_state"]))throw std::runtime_error("Native seeded control state differs");
        p.rig=std::make_shared<const AnimationRigAsset>(loadOriginalAnimationRig([assetRoot stringByAppendingPathComponent:package]));
        p.animation=makeOriginalRiderAnimation(p.rig,object(initial[@"original_animation"]));
        if(!p.animation)throw std::runtime_error("Native world requires each original rider animation");
        p.context.currentTimeScale=p.rider->originalGroundState().timeScale;
        p.air.uberEnabled=flag(d[@"uber_enabled"]);
        auto upper=object(d[@"upper_reaction"]);if(word(upper[@"clock_tick"])!=uint32_t(race.clock.totalTicks))throw std::runtime_error("Upper reaction seed clock differs");
        p.upperIdleSeconds=float(number(upper[@"idle_seconds"]));p.upperContext.reactionMask=mask(upper[@"reaction_mask"]);p.upperContext.lookbackMask=mask(upper[@"lookback_mask"]);
        auto upperPeers=array(upper[@"peers"],6);for(unsigned i=0;i<6;++i){NSDictionary* peer=object(upperPeers[i]);p.upperPeers[i]={flag(peer[@"enabled"]),flag(peer[@"attack_eligible"]),float(number(peer[@"distance_cm"])),float(number(peer[@"bearing"])),word(peer[@"last_reaction_tick"])};}
        auto grab=object(d[@"grab_control"]);auto grabState=object(grab[@"state"]);auto grabProfile=object(grab[@"profile"]);
        p.grabState={integer(grabState[@"state"]),integer(grabState[@"index"])};p.grabProfile.playbackRate=originalGrabPlaybackRate(float(number(grabProfile[@"grab_stat"])));
        auto definitions=array(grabProfile[@"grabs"],15);for(unsigned i=0;i<15;++i){NSDictionary* definition=object(definitions[i]);p.grabProfile.grabs[i]={integer(definition[@"semantic"]),integer(definition[@"upper_semantic"])};}
        RiderControllerCallbacks callbacks;callbacks.normalUpperReaction=[this,slot](PrototypeRider&){upperReaction(slot);};callbacks.normalGain=[this,slot](PrototypeRider&){participants.at(slot).upperIdleSeconds=0;};
        callbacks.airGain=[this,slot](PrototypeRider&){participants.at(slot).grabState={};};callbacks.airGrab=[this,slot](PrototypeRider&,const RiderInput& input){return grabControl(slot,input);};p.rider->setControllerCallbacks(std::move(callbacks));
        p.pair.kind880=integer(pair[@"kind880"]);p.pair.disabled=flag(pair[@"disabled"]);p.pair.weightAttribute=integer(pair[@"weight_attribute"]);
        p.pair.resolvedCollisionStat=float(number(pair[@"resolved_collision_stat"]));p.pair.impulse.ragdollSubmode=integer(pair[@"ragdoll_submode"]);
        p.pair.attack.facing340=vector<3>(pair[@"facing340"]);p.pair.attack.strength350=float(number(pair[@"strength350"]));p.pair.attack.resolvedAttackStat=float(number(pair[@"resolved_attack_stat"]));
        auto row=array(pair[@"records"],6);for(unsigned other=0;other<6;++other)records[slot][other]=pairRecord(object(row[other]));
        auto stages=originalPoseStages(p.animation);auto local=stages.prepareLocal;auto posed=stages.finishWorld;
        stages.prepareLocal=[local,slot](const PrototypeRider& rider,double time){if(!rider.grounded&&rider.currentControlState()==5&&!rider.airAngularSupported)throw std::runtime_error("Native airborne control lifecycle unsupported for rider "+std::to_string(slot));if(!local(rider,time))throw std::runtime_error("Native local pose unsupported for rider "+std::to_string(slot)+" control "+std::to_string(rider.currentControlState()));return true;};
        stages.finishWorld=[posed,slot](const PrototypeRider& rider,double time){auto body=posed(rider,time);if(!body)throw std::runtime_error("Native world pose unavailable for rider "+std::to_string(slot));return body;};
        simulation->addRider(slot,*p.rider,[this](RiderWorld&,size_t i){return controls(i);},std::move(stages));
        simulation->actor(slot).trackProgressEnabled=flag(d[@"track_progress_enabled"]);
    }
    // The route bank includes the human: later NPCs can follow that rider's
    // path, even though human paths do not contribute to NPC traffic scoring.
    auto routes=array(npc[@"participant_routes"],roster.count);
    for(size_t slot=0;slot<participants.size();++slot){
        NSDictionary* d=object(routes[slot]);if(integer(d[@"slot"])!=int(slot))throw std::runtime_error("AI route roster order differs");
        if(flag(d[@"human"])!=participants[slot].human)throw std::runtime_error("AI route ownership differs");
        auto& r=participants[slot].context.driving.route;r.pathIndex=integer(d[@"path_index"]);
        if(r.pathIndex<0||size_t(r.pathIndex)>=paths.size())throw std::runtime_error("Initial AI path outside bank");
        auto c=object(d[@"cache"]);r.cache.origin=vector<3>(c[@"origin"]);r.cache.distance=float(number(c[@"distance"]));r.cache.segment=integer(c[@"segment"]);
        r.closestPoint=vector<3>(d[@"closest_point"]);r.lookaheadPoint=vector<3>(d[@"lookahead_point"]);r.previousLookaheadPoint=vector<3>(d[@"previous_lookahead_point"]);r.lateralDistance=float(number(d[@"lateral_distance"]));r.heading=float(number(d[@"heading"]));
        r.previousDistance=float(number(d[@"previous_distance"]));r.currentDistance=float(number(d[@"current_distance"]));
    }
    std::vector<bool> seen(participants.size());
    for(NSDictionary* d in array(npc[@"riders"])){
        int slot=integer(d[@"slot"]);if(slot<0||size_t(slot)>=participants.size()||participants[slot].human||seen[slot])throw std::runtime_error("Invalid or duplicate NPC seed");seen[slot]=true;
        auto& p=participants[slot];auto& s=p.driving;auto raw=object(d[@"driving_state"]);
        s.longFlightTicks484=integer(raw[@"long_flight_ticks484"]);
        s.desiredSpeedDF0=float(number(raw[@"desired_speed_df0"]));s.parameterDF8=float(number(raw[@"parameter_df8"]));s.parameterDFC=float(number(raw[@"parameter_dfc"]));
        s.boardTimerE40=integer(raw[@"board_timer_e40"]);s.targetPeerE70=integer(raw[@"target_peer_e70"]);s.offRouteTicksE74=integer(raw[@"off_route_ticks_e74"]);s.oppositeHeadingTicksE78=integer(raw[@"opposite_heading_ticks_e78"]);
        s.defensiveTimerF30=float(number(raw[@"defensive_timer_f30"]));s.defensiveDecisionF34=integer(raw[@"defensive_decision_f34"]);s.lastAttackTickF4=integer(raw[@"last_attack_tick_f4"]);s.behaviorCounterF38=int16_t(integer(raw[@"behavior_counter_f38"]));
        s.regionStartE50=vector<3>(raw[@"region_start_e50"]);s.regionEndE60=vector<3>(raw[@"region_end_e60"]);
        auto trick=object(raw[@"trick"]);auto& t=s.trick;t.indexE0C=integer(trick[@"index_e0c"]);t.enabledE10=integer(trick[@"enabled_e10"]);t.decisionE14=integer(trick[@"decision_e14"]);t.phaseE18=integer(trick[@"phase_e18"]);t.kindE1C=integer(trick[@"kind_e1c"]);t.fieldE20=integer(trick[@"field_e20"]);t.remainingE34=float(number(trick[@"remaining_e34"]));t.spinE38=float(number(trick[@"spin_e38"]));t.flipE3C=float(number(trick[@"flip_e3c"]));
        t.fieldE24=integer(trick[@"field_e24"]);t.fieldE28=integer(trick[@"field_e28"]);t.releaseE2C=float(number(trick[@"release_e2c"]));t.startE30=float(number(trick[@"start_e30"]));
        auto normalCounts=array(trick[@"normal_counts"],15),uberCounts=array(trick[@"uber_counts"],15),tweakCounts=array(trick[@"tweak_counts"],15);
        for(unsigned i=0;i<15;++i){t.normalCounts[i]=integer(normalCounts[i]);t.uberCounts[i]=integer(uberCounts[i]);t.tweakCounts[i]=integer(tweakCounts[i]);}
        NSString* behavior=d[@"behavior"][@"function"];
        if([behavior isEqual:@"0x00100680"])s.behavior=OriginalNpcBehavior::Cruise100680;
        else if([behavior isEqual:@"0x001009e0"])s.behavior=OriginalNpcBehavior::Jump1009E0;
        else if([behavior isEqual:@"0x00100f88"])s.behavior=OriginalNpcBehavior::Peer100F88;
        else if([behavior isEqual:@"0x00100b90"])s.behavior=OriginalNpcBehavior::Designated100B90;
        else throw std::runtime_error("Unknown native NPC behavior");
        auto score=object(d[@"score_state"]);p.context.scoring.roleE00=int16_t(integer(score[@"role_e00"]));p.context.scoring.allowFlag0E04=flag(score[@"allow_flag0_e04"]);p.context.scoring.randomizeE08=flag(score[@"randomize_e08"]);
        if(score[@"followed_participant_slot"]!=NSNull.null)p.followedSlot=integer(score[@"followed_participant_slot"]);
        auto pace=object(d[@"pacing_state"]);p.context.pacing.negativeThresholdDC=float(number(pace[@"negative_threshold_dc"]));p.context.pacing.positiveThresholdE0=float(number(pace[@"positive_threshold_e0"]));p.context.pacing.modeE4=integer(pace[@"mode_e4"]);p.context.pacing.eventVariant=eventVariant;
        auto catalog=object(d[@"grab_catalog"]);p.grabs.grabStat=float(number(catalog[@"grab_stat"]));p.grabs.eventId=integer(catalog[@"event_id"]);
        auto timing=[](NSDictionary* entry){return OriginalNpcGrabTiming{integer(entry[@"semantic"]),float(number(entry[@"marker1"])),float(number(entry[@"marker2"]))};};
        auto normal=array(catalog[@"normal"],15),tweak=array(catalog[@"tweak"],15),uber=array(catalog[@"uber"],2);
        for(unsigned i=0;i<15;++i){p.grabs.normal[i]=timing(object(normal[i]));p.grabs.tweak[i]=timing(object(tweak[i]));for(unsigned tier=0;tier<2;++tier)p.grabs.uber[tier][i]=timing(object(array(uber[tier],15)[i]));}
        p.air.grabs=&p.grabs;p.context.air=&p.air;
    }
    for(size_t i=0;i<participants.size();++i)if(!participants[i].human&&!seen[i])throw std::runtime_error("NPC provider state missing");
    simulation->installPairs([this](RiderWorld&,unsigned slot){
        participants[slot].animation->consumeRiderEvents(*participants[slot].rider);
        auto view=participants[slot].pair;
        int semantic=participants[slot].animation->currentSemantic(1);
        view.attack.animationClass1=semantic==438?0:animationClasses.at(semantic);
        if(view.attack.animationClass1==3||view.attack.animationClass1==13)throw std::runtime_error("Native attack marker lifecycle not yet connected");
        return view;
    },[this](RiderWorld&,unsigned target,unsigned,const OriginalPairReactionRequest& request,const CollisionRandom& random){
        if(request.kind!=OriginalPairReactionRequest::Kind::Soft)throw std::runtime_error("Native shared crash lifecycle pending for rider "+std::to_string(target));
        auto& participant=participants.at(target);auto& rider=*participant.rider;
        OriginalCollisionContext context;context.physical=rider.currentPhysicalFrame();context.velocityCmps=OriginalAirState::fromNative(rider.position,rider.velocity).velocity;
        context.motionMode=rider.grounded?0:1;context.controlState=rider.currentControlState();context.reverseStance=rider.originalGroundState().reverseStance;context.manualSpin=rider.originalGroundState().manualSpin;
        if(context.controlState==1)throw std::runtime_error("Soft impact requires original control1 cancellation");
        auto result=originalSoftCollisionReaction({},context,request.event,random);
        if(result.kind==OriginalCollisionReactionKind::Ignored)return;
        if(!rider.grounded||(context.controlState!=0&&context.controlState!=2))throw std::runtime_error("Unsupported native soft entry control");
        participant.animation->consumeRiderEvents(rider);
        if(!participant.animation->playSemantic(result.animation))throw std::runtime_error("Original soft collision animation unavailable");
        //11FEC8 invokes the old normal-controller exit after108388 plays the
        // new body animation. Its upper attack/recovery branch clears channel0.
        if(context.controlState==0){int upper=participant.animation->currentClass(1);if(upper==3||upper==13){participant.animation->fadeChannel(0,.1f);participant.animation->setChannelRate(1,1);}}
        rider.enterSoftCollision(result.animation,result.manualSpin,request.attack,result.strongSoftImpact);
    },records,unsigned(integer(pairs[@"excluded_tail_count"])),flag(pairs[@"knockdown_cheat"]));
    simulation->updateNpcProgress=[this](RiderWorld&,size_t slot){progress(slot);};
}
void NativeRaceWorld::refreshContext(size_t slot){
    auto& p=participants.at(slot);auto& c=p.context;auto& d=c.driving;auto& rider=*p.rider;
    auto physical=OriginalAirState::fromNative(rider.position,rider.velocity);auto basis=rider.currentPhysicalFrame();
    d.position=physical.position;d.velocity=physical.velocity;d.boardUp=basis.up;d.physicalForward=basis.forward;
    d.boostMeter=rider.originalBoostState().meter;d.superTime=rider.originalBoostState().superTime;d.tick=simulation->clock.totalTicks;
    d.motionMode=rider.grounded?0:1;d.controlState=rider.currentControlState();d.trajectoryElapsed=rider.originalAirTrajectoryState().elapsed;
    d.physicalRightZ=basis.right[2];d.prewindStyle=rider.originalGroundState().prewindStyle;d.mainAnimationClass=p.animation->currentClass(2);
    d.selfSlot=unsigned(slot);d.participantCount=unsigned(participants.size());d.peers=simulation->pairSystem()->records()[slot];
    d.designatedPeer=p.followedSlot>=0?std::optional<int>(p.followedSlot):std::nullopt;
    occupancy.clear();c.pacing.referenceRemaining.reset();
    for(size_t other=0;other<participants.size();++other){auto& peer=participants[other];d.peerHuman[other]=peer.human;
        d.peerVelocities[other]=OriginalAirState::fromNative(peer.rider->position,peer.rider->velocity).velocity;
        d.peerPositions[other]=OriginalAirState::fromNative(peer.rider->position,peer.rider->velocity).position;
        d.peerPathIndices[other]=peer.context.driving.route.pathIndex;
        if(!peer.human)occupancy.push_back(d.peerPathIndices[other]);
        int semantic=peer.animation->currentSemantic(1);d.peerUpperAnimationClass[other]=semantic==438?0:animationClasses.at(semantic);
        if(!c.pacing.referenceRemaining&&d.peers[other].enabled&&peer.human)c.pacing.referenceRemaining=simulation->actor(other).progress.remaining;
    }
    c.pacing.remaining=simulation->actor(slot).progress.remaining;c.currentTimeScale=rider.originalGroundState().timeScale;
    c.scoring.position=d.position;c.scoring.velocity=d.velocity;c.scoring.currentPathIndex=d.route.pathIndex;
    c.scoring.followedPathIndex=p.followedSlot>=0?participants.at(p.followedSlot).context.driving.route.pathIndex:-1;c.scoring.npcPathIndices=occupancy;
    c.scoring.computerControlled=!p.human;
    auto& air=p.air;const auto& trajectory=rider.originalAirTrajectoryState();air.trajectoryStatus=trajectory.status;air.predictedTime=trajectory.predictedTime;air.elapsed=trajectory.elapsed;
    air.angular=rider.originalAirControlState();air.trickStat=rider.originalAirControlProfile().trickStat;air.boostModifier=rider.originalBoostState().modifier;air.superTime=rider.originalBoostState().superTime;
    air.mainAnimationClass=p.animation->currentClass(2);air.boostTierCounter=rider.originalBoostState().tier;
}
RiderInput NativeRaceWorld::controls(size_t slot){
    auto& p=participants.at(slot);if(p.human)return humanControls(*simulation,slot);
    refreshContext(slot);auto result=originalNpcControl(p.driving,p.context,paths,[this,slot](unsigned peer){return relationships.at(slot).at(peer);},[this](){return simulation->randomWord();});
    p.commands=result.words;p.rider->setControllerTimeScale(result.timeScale);
    return originalDecodeCommand(p.rider->currentControlState(),result.words[0],result.words[1]);
}
void NativeRaceWorld::progress(size_t slot){
    refreshContext(slot);auto& p=participants[slot];originalNpcRouteProgress(paths,p.context.driving.route,p.context.scoring,simulation->actor(slot).progress.bestRemaining,simulation->clock.totalTicks,[this](){return simulation->randomWord();});
    p.rider->setRouteHeading(p.context.driving.route.heading); //1125C0 writes rider4CC; ground drive reads it next tick.
}
void NativeRaceWorld::upperReaction(size_t slot){
    auto& p=participants.at(slot);const auto& records=simulation->pairSystem()->records()[slot];
    for(unsigned peer=0;peer<6;++peer){p.upperPeers[peer].enabled=records[peer].enabled;p.upperPeers[peer].distanceCm=records[peer].planarDistanceCm;p.upperPeers[peer].bearing=records[peer].bearing;}
    p.upperContext.upperClass=p.animation->currentClass(1);if(p.upperContext.upperClass<0)throw std::runtime_error("Original upper animation class unavailable");
    p.upperContext.physicalForward=p.rider->currentPhysicalFrame().forward;p.upperContext.reverseStance=p.rider->originalGroundState().reverseStance;p.upperContext.clockTick=uint32_t(simulation->clock.totalTicks);
    auto request=originalUpperReaction(p.upperIdleSeconds,p.upperPeers,p.upperContext,[this](){return simulation->randomWord();});
    if(request.semantic>=0){
        if(request.semantic>=319&&request.semantic<=321)throw std::runtime_error("Original upper reaction requires weighted animation variant selection");
        if(!p.animation->playSemantic(request.semantic,1,request.mask))throw std::runtime_error("Original upper reaction animation unavailable");
    }
}
bool NativeRaceWorld::grabControl(size_t slot,const RiderInput& input){
    auto& p=participants.at(slot);p.animation->consumeRiderEvents(*p.rider);
    constexpr std::array<uint8_t,15> masks={1,2,4,8,3,5,9,6,10,12,7,11,13,14,15};int requested=-1;
    if(input.grabMask){auto found=std::find(masks.begin(),masks.end(),input.grabMask);if(found==masks.end())throw std::runtime_error("Invalid four-shoulder grab combination");requested=int(found-masks.begin());}
    OriginalGrabAnimationAccess access;
    access.mainClass=[&](){int cls=p.animation->currentClass(2);if(cls<0)throw std::runtime_error("Grab main class unavailable");return cls;};
    access.mainFlags=[&](){auto current=p.animation->mainAnimationState();if(!current)throw std::runtime_error("Grab event flags unavailable");return current->flags;};
    access.play=[&](int semantic,bool force){if(!p.animation->playSemantic(semantic,1,~uint64_t(0),force))throw std::runtime_error("Original grab animation unavailable: "+std::to_string(semantic));};
    access.setRate=[&](unsigned channel,float rate){p.animation->setChannelRate(channel,rate);};access.fade=[&](unsigned channel,float time){p.animation->fadeChannel(channel,time);};
    access.score=[&](int index,bool begin){grabScoreRequests.push_back({uint32_t(simulation->clock.totalTicks),unsigned(slot),index,begin});};
    auto result=originalGrabLifecycle(p.grabState,p.grabProfile,requested,input.boostHeld,access);
    if(!result.supported)throw std::runtime_error("Original tweak/Uber grab lifecycle not yet connected for rider "+std::to_string(slot));
    return result.active;
}
}
