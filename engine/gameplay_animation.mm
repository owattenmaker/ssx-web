#include "gameplay_animation.h"
#include <stdexcept>
#include "ground_motion.hpp"
namespace ssx {namespace {
float value(NSDictionary* d,NSString* key){id v=d[key];if(![v isKindOfClass:NSNumber.class]||!std::isfinite([v doubleValue]))throw std::runtime_error("Missing gameplay animation field");return [v floatValue];}
OriginalGrabDefinition definition(NSDictionary* d){return {int(value(d,@"semantic")),int(value(d,@"upper_semantic")),int(value(d,@"score_id")),int(value(d,@"begin_points")),int(value(d,@"hold_points"))};}
}
GameplayAnimation::GameplayAnimation(std::shared_ptr<OriginalRiderAnimation> animation,NSDictionary* initial):player(std::move(animation)){
    if(!player)throw std::runtime_error("Original gameplay animation player is missing");
    player->setControllerDriven(true);airAnimationState.nextRate=value(initial[@"original_animation"],@"next_rate");
    NSArray* words=initial[@"original_animation"][@"random_state"];if(words.count!=6)throw std::runtime_error("Gameplay animation RNG seed is missing");
    random=std::make_shared<OriginalRandomState>();for(unsigned i=0;i<6;++i)random->words[i]=[words[i] unsignedIntValue];
    player->bindVariantRandom([state=random](){return state->next();});
    NSDictionary* config=initial[@"original_grab_control"];if(!config)throw std::runtime_error("Original grab definitions are missing");
    grabState={int(value(config[@"state"],@"state")),int(value(config[@"state"],@"index"))};
    auto profile=config[@"profile"];grabProfile.playbackRate=originalGrabPlaybackRate(value(profile,@"grab_stat"));grabProfile.extendedDefinitions=[profile[@"extended_definitions"] boolValue];
    NSArray* normal=profile[@"grabs"],*tweak=profile[@"tweak"],*uber=profile[@"uber"];
    if(normal.count!=15||tweak.count!=15||uber.count!=2||[uber[0] count]!=15||[uber[1] count]!=15)throw std::runtime_error("Incomplete original grab catalog");
    for(unsigned i=0;i<15;++i){grabProfile.grabs[i]=definition(normal[i]);grabProfile.tweak[i]=definition(tweak[i]);for(unsigned tier=0;tier<2;++tier)grabProfile.uber[tier][i]=definition(uber[tier][i]);}
    uberEnabled=[config[@"context"][@"uber_enabled"] boolValue];legWeight=value(config[@"context"],@"leg_weight");
}
void GameplayAnimation::bind(PrototypeRider& rider){
    auto self=shared_from_this();RiderControllerCallbacks callbacks;
    callbacks.nextRandom=[state=random](){return state->next();};
    callbacks.airGain=[self](PrototypeRider&){self->grabState={};};
    callbacks.airGrab=[self](PrototypeRider& r,const RiderInput& input){return self->grab(r,input);};
    callbacks.requestAnimation=[self,&rider](int semantic,float rate){self->player->consumeRiderEvents(rider);if(self->player->currentSemantic(2)!=semantic&&!self->player->playSemantic(semantic,rate))throw std::runtime_error("Original animation driver unavailable: "+std::to_string(semantic));auto result=self->player->mainAnimationState();if(!result)throw std::runtime_error("Missing main animation state");return *result;};
    callbacks.upperAnimationClass=[self](){return self->player->currentClass(1);};
    callbacks.animationRate=[self](unsigned channel,float rate){self->player->setChannelRate(channel,rate);};
    callbacks.animationFade=[self](unsigned channel,float time){self->player->fadeChannel(channel,time);};
    // Attacks and handplants are not recovered yet. Their inputs are ignored
    // with a one-time diagnostic instead of pausing play; nothing is faked.
    callbacks.upperAction=[](bool left,bool right){static bool reported=false;if((left||right)&&!reported){reported=true;NSLog(@"Attack gameplay is not connected yet; the input is ignored");}return false;};
    callbacks.handplantAction=[](bool)->bool{static bool reported=false;if(!reported){reported=true;NSLog(@"Handplant gameplay is not connected yet; the input is ignored");}return false;};
    // The single-player scene currently registers terrain and rigid obstacles,
    // but no grindable rail entities. Rail registration/attachment remains work.
    callbacks.railAction=[](){return false;};
    callbacks.timerUpdate=[self](const PrototypeRider& r){self->player->consumeRiderEvents(r);self->legWeight=originalGrabLegWeight(self->legWeight,self->player->currentClass(2));self->player->setLegWeight(self->legWeight);};
    callbacks.airAnimation=[self](PrototypeRider& r,const OriginalAirControlFrame& frame){self->selectAir(r,frame);};
    callbacks.frameComplete=[self](const PrototypeRider& r){if(self->afterFrame)self->afterFrame(r);};
    // Hard crash (10EB30) root bake and control8 recovery clip access.
    callbacks.presentedRoot=[self](const AnimationTransform& physical,const RiderRootPresentation& values){return self->player->presentRoot(physical,values);};
    callbacks.previewClipRoot=[self](int semantic){return self->player->previewRoot(semantic);};
    callbacks.currentScaledLocalRoot=[self](){return self->player->scaledLocalRoot();};
    callbacks.offsetAnimationRoots=[self](const AnimationTransform& offset){self->player->offsetSequenceRoots(offset);};
    callbacks.rotateAnimationRoot=[self](float angle){auto sc=originalSinCos(angle*.5f);self->player->rotateSequenceRoots({0,0,sc[0],sc[1]});};
    callbacks.resetAnimationRootBasis=[self](){self->player->resetDefaultRoot({0,0,0},{0,0,0,1});};
    callbacks.playCrashAnimation=[self,&rider](int semantic){
        self->player->consumeRiderEvents(rider);self->grabState={};
        if(!self->player->playSemantic(semantic,1,~uint64_t(0),true))throw std::runtime_error("Original crash animation unavailable: "+std::to_string(semantic));
        auto state=self->player->mainAnimationState();if(!state)throw std::runtime_error("Missing crash animation state");return *state;};
    callbacks.crashClip=[self](unsigned primary,unsigned secondary)->std::optional<OriginalCrashClipState>{
        auto state=self->player->mainAnimationState();auto p=self->player->posedBone(primary);auto q=self->player->posedBone(secondary);
        if(!state||!p||!q)return std::nullopt;
        OriginalCrashClipState clip;clip.semantic=state->semantic;clip.animationClass=state->animationClass;clip.complete=state->completed;
        clip.progress=self->player->channelProgress(2);clip.duration10=self->player->channelDuration(2);clip.speed90=self->player->channelRate(2);
        clip.primary=*p;clip.secondary=*q;return clip;};
    callbacks.seekMainAnimation=[self](float seconds){if(!self->player->seekChannel(2,seconds))throw std::runtime_error("Crash continuation seek found no main sequence");};
    callbacks.detachedBoard=[self](std::optional<AnimationTransform> board){self->player->setDetachedBoard(board);};
    callbacks.crashObserver=[self](int observer){self->events.push_back({-observer-1,false,false});};
    callbacks.posedBoneTransform=[self](unsigned bone){return self->player->posedBone(bone);};
    callbacks.setAnimationMirror=[self](bool mirror){self->player->setDefaultMirror(mirror);};
    callbacks.setAnimationRootHalfAngle=[self](float half){auto sc=originalSinCos(half);self->player->resetDefaultRoot({0,0,0},{0,0,sc[0],sc[1]});};
    callbacks.railScoreEvent=[self](int kind,float value){self->events.push_back({1000+kind,value>0,false});};
    callbacks.railBalance=[self](float balance){self->player->setRailBalance(balance);};
    callbacks.rotateAnimationRootQuaternion=[self](std::array<float,4> q){self->player->rotateSequenceRoots(q);};
    callbacks.resetAnimationRoot=[self](std::array<float,3> p,std::array<float,4> q){self->player->resetDefaultRoot(p,q);};
    rider.setControllerCallbacks(std::move(callbacks));
}
bool GameplayAnimation::grab(PrototypeRider& rider,const RiderInput& input){
    player->consumeRiderEvents(rider);
    constexpr std::array<uint8_t,15> masks={1,2,4,8,3,5,9,6,10,12,7,11,13,14,15};int index=-1;
    if(input.grabMask){auto found=std::find(masks.begin(),masks.end(),input.grabMask);if(found==masks.end())throw std::runtime_error("Invalid grab chord");index=int(found-masks.begin());}
    OriginalGrabAnimationAccess access;
    access.mainClass=[&](){return player->currentClass(2);};
    access.mainFlags=[&](){auto state=player->mainAnimationState();if(!state)throw std::runtime_error("Missing original grab markers");return state->flags;};
    access.play=[&](int semantic,bool force){if(!player->playSemantic(semantic,1,~uint64_t(0),force))throw std::runtime_error("Original grab animation unavailable: "+std::to_string(semantic));};
    access.setRate=[&](unsigned channel,float rate){player->setChannelRate(channel,rate);};access.fade=[&](unsigned channel,float time){player->fadeChannel(channel,time);};
    access.mappedScore=[&](int score,bool begin){events.push_back({score,begin,false});};access.advancedStarted=[&](){events.push_back({0,false,true});};
    auto boost=rider.originalBoostState();auto result=originalGrabLifecycle(grabState,grabProfile,index,input.boostHeld,access,{boost.superTime,boost.tier,uberEnabled});
    if(!result.supported)throw std::runtime_error("Unsupported original grab state");return result.active;
}
void GameplayAnimation::selectAir(PrototypeRider& rider,const OriginalAirControlFrame& frame){
    auto main=player->mainAnimationState();if(!main)throw std::runtime_error("Air animation state missing");
    const auto& ground=rider.originalGroundState();const auto& trajectory=rider.originalAirAnimationTrajectoryState();const auto& profile=rider.originalAirControlProfile();
    airAnimationState.adjustment28C=ground.adjustment28C;airAnimationState.adjustment298=ground.adjustment298;
    OriginalAirAnimationContext context{main->semantic,main->animationClass,main->completed,ground.reverseStance,profile.grabActive,profile.trickStat,float(profile.boostModifier),trajectory.status,trajectory.predictedTime,trajectory.elapsed};
    OriginalAirAnimationAccess access;access.duration=[&](int semantic){return player->semanticDuration(semantic);};access.setNextRate=[](float){};
    access.play=[&](int semantic,float rate){if(!player->playSemantic(semantic,rate))throw std::runtime_error("Original air animation missing: "+std::to_string(semantic));};
    originalSelectAirAnimation(rider.originalAirControlState(),frame,airAnimationState,context,access);
    rider.setAirAnimationAdjustments(airAnimationState.adjustment28C,airAnimationState.adjustment298);
    if(auto current=player->mainAnimationState())rider.synchronizeAnimationState(*current);
}
}
