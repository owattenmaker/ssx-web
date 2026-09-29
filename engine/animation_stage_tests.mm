#import "rider_animation_player.h"
#include <cassert>
#include "soft_collision_control.hpp"
#include "ground_animation_control.hpp"
#include "grab_lifecycle.hpp"
#include <bit>
#include <cstdio>
static ssx::AnimationPacket constantPacket(float x){
    std::vector<uint8_t> data{6,0x11,0x11,0x11};
    for(float value:{x,0.f,0.f,0.f,0.f,0.f}){auto bits=std::bit_cast<uint32_t>(value);data.push_back(bits>>8);data.push_back(bits>>16);data.push_back(bits>>24);}
    return ssx::AnimationPacket(data,2);
}
static std::shared_ptr<ssx::AnimationRigAsset> rig(){
    auto result=std::make_shared<ssx::AnimationRigAsset>();result->character="zoe";result->bones.resize(24);
    for(unsigned i=0;i<24;++i){auto&bone=result->bones[i];bone.parent=i==0||i==22?-1:0;bone.part=i<22?0:1;bone.translationChannel=0;bone.rotationChannel=3;}
    for(unsigned i=0;i<2;++i){ssx::AnimationClip clip;clip.id=100+i;clip.duration=1;clip.segments.push_back({0,constantPacket(i?100.f:1.f)});clip.segments.push_back({1,constantPacket(0)});result->clips.push_back(std::move(clip));}
    ssx::AnimationStateDefinition next;next.semantic=287;next.animationClass=2;next.initialClip=101;result->stateDefinitions[287]=next;
    return result;
}
static std::shared_ptr<ssx::OriginalRiderAnimation> player(std::shared_ptr<const ssx::AnimationRigAsset>r,int semantic=5,float duration=1){
    ssx::OriginalAnimationSequence sequence;sequence.semantic=semantic;sequence.channel=2;sequence.priority=1;sequence.slots.push_back({100,0,duration,1,1,false,true});
    return std::make_shared<ssx::OriginalRiderAnimation>(r,std::vector{sequence},ssx::AnimationVector{1,1,1},ssx::RiderPoseContact{},true);
}
int main(){@autoreleasepool{
    ssx::PrototypeRider rider;rider.grounded=true;rider.position={0,0,0};ssx::OriginalGroundState state;state.controlState=0;state.animationIndex=5;state.animationSelectionSupported=true;rider.seedOriginalPoseControls(state);
    auto asset=rig();auto staged=player(asset);constexpr double first=1.0/60,second=2.0/60;
    // Requested identity survives a fading sequence, and can be empty while
    // playback still contributes bones. Original311AE8 reads this slot.
    ssx::OriginalAnimationSequence fading;fading.semantic=316;fading.channel=1;fading.slots.push_back({100,0,1,1,1,false,true});
    ssx::AnimationStateDefinition head;head.semantic=317;head.animationClass=3;asset->stateDefinitions[317]=head;
    ssx::OriginalRiderAnimation requested(asset,{fading},{1,1,1},{},false,{},0,{},false,~uint64_t(0),{438,317,438,438,438,438});
    assert(requested.currentSemantic(1)==317&&requested.currentClass(1)==3);
    requested.fadeChannel(1,.1f);assert(requested.currentSemantic(1)==438&&requested.currentClass(1)==0&&requested.sequenceState().size()==1);
    assert(!staged->finishWorldPose(rider,first));assert(staged->prepareLocalPose(rider,first));
    float tick=std::bit_cast<float>(0x3c888889u);assert(staged->sequenceState()[0].slots[0].time==tick);
    assert(staged->prepareLocalPose(rider,first));assert(staged->sequenceState()[0].slots[0].time==tick);
    rider.position={1,2,3};auto world=staged->finishWorldPose(rider,first);assert(world&&world->at(0).position[0]==101);
    auto before=world->at(0).position;rider.position={4,5,6};assert(staged->finishWorldPose(rider,first)->at(0).position==before);
    assert(!staged->finishWorldPose(rider,second));assert(staged->prepareLocalPose(rider,second));assert(staged->finishWorldPose(rider,second)->at(0).position[0]==401);
    auto wrapped=player(asset);rider.position={1,2,3};assert(wrapped->poseAt(rider,first)->at(0).position==before);
    assert(wrapped->finishBodyPose(rider,first));
    // Completion metadata changes AFTER local sampling. The new clip starts at
    // time0, while world FK still consumes the completed clip's sampled pose.
    ssx::AnimationStateDefinition ended;ended.semantic=5;ended.completionKind=1;ended.initialClip=100;asset->stateDefinitions[5]=ended;
    auto completing=player(asset,5,tick);rider.position={0,0,0};assert(completing->prepareLocalPose(rider,first));
    assert(completing->sequenceState()[0].semantic==287&&completing->sequenceState()[0].slots[0].time==0);
    assert(completing->finishWorldPose(rider,first)->at(0).position[0]==1);
    state.animationIndex=287;rider.seedOriginalPoseControls(state);assert(completing->prepareLocalPose(rider,second));assert(completing->finishWorldPose(rider,second)->at(0).position[0]==100);
    // Actual Psymon/Moby glide -> glide+1 filtered controls and clip clocks.
    // Only initial scalar state is seeded; both targets/approach and player driver
    // run natively. Expected values are not sampled pose/bone inputs.
    constexpr uint32_t firstFrame[2][5]={{0xbf781df1,0x3b377601,0xbf78c61d,0x3b282c26,0x3ea5d968},{0xbf702ffd,0x3bb80061,0xbf718153,0x3ba8ab06,0x3ea100e2}};
    for(const auto&gold:firstFrame){
        ssx::OriginalAirPrewindState controls;controls.spin={std::bit_cast<float>(gold[0]),std::bit_cast<float>(gold[1]),-1};
        ssx::OriginalAirPrewindContext context;context.style=0;context.animationClass=12;context.animationIndex=246;
        assert(ssx::originalAirPrewindTargets(controls,-1,0,context));ssx::originalAirPrewindApproach(controls);
        assert(std::bit_cast<uint32_t>(controls.spin.current)==gold[2]&&std::bit_cast<uint32_t>(controls.spin.rate)==gold[3]);
        auto prewindRig=rig();ssx::AnimationStateDefinition definition;definition.semantic=246;definition.kind=7;definition.animationClass=12;definition.initialClip=100;prewindRig->stateDefinitions[246]=definition;
        auto actorPlayer=player(prewindRig,246,std::bit_cast<float>(0x3eaaaaabu));
        state.controlState=2;state.animationIndex=246;state.animationClass=12;state.reverseStance=true;rider.seedOriginalPoseControls(state);rider.seedOriginalAirEntry({},controls,{});
        assert(actorPlayer->prepareLocalPose(rider,first));assert(std::bit_cast<uint32_t>(actorPlayer->sequenceState()[0].slots[0].time)==gold[4]);
    }
    // A soft bump finishes through+C0, without an end-event63 callback. The
    // controller reads last tick's flag, so normal control resumes one phase later.
    auto bumpRig=rig();ssx::AnimationStateDefinition bump;bump.semantic=55;bump.animationClass=6;bump.kind=1;bump.initialClip=100;bumpRig->stateDefinitions[55]=bump;
    ssx::AnimationStateDefinition cruise;cruise.semantic=5;cruise.animationClass=7;cruise.kind=2;cruise.initialClip=101;bumpRig->stateDefinitions[5]=cruise;
    auto bumpPlayer=player(bumpRig,55,tick*2);ssx::OriginalGroundState bumpState;bumpState.modeTiming=-1;bumpState.controlState=3;bumpState.animationIndex=55;bumpState.animationClass=6;bumpState.animationSelectionSupported=true;bumpState.velocity={0,1600,0};bumpState.forward={0,1,0};bumpState.manualSpin=3.1415927410125732f;
    ssx::OriginalGroundProfile bumpProfile;ssx::PrototypeRider bumped;bumped.grounded=true;
    for(unsigned frame=1;frame<=2;++frame){bool complete=bumpPlayer->mainAnimationState()->completed;auto response=ssx::originalSoftControlStep(bumpProfile,bumpState,{0,complete,false},{});assert(response.nextControl==-1);bumped.seedOriginalPoseControls(bumpState);assert(bumpPlayer->prepareLocalPose(bumped,frame/60.));assert(bumpPlayer->finishBodyPose(bumped,frame/60.));}
    assert(bumpPlayer->mainAnimationState()->completed);assert(!(bumpPlayer->mainAnimationState()->flags&(uint64_t(1)<<63)));
    auto recovered=ssx::originalSoftControlStep(bumpProfile,bumpState,{0,true,false},{});assert(recovered.nextControl==0&&recovered.restoreStance&&recovered.normalGainFocus);
    bumpState.controlState=0;bumped.seedOriginalPoseControls(bumpState);assert(bumpPlayer->prepareLocalPose(bumped,3./60));assert(bumpPlayer->mainAnimationState()->semantic==55);
    assert(ssx::originalSelectGroundAnimation(bumpProfile,bumpState,{}));assert(bumpState.animationIndex==5);bumped.seedOriginalPoseControls(bumpState);assert(bumpPlayer->prepareLocalPose(bumped,4./60));assert(bumpPlayer->mainAnimationState()->semantic==5);
    // Original104A60 callback2 replaces IntoSpin with its authored cycle after
    // local sampling, preserving fade but using the new cycle's normal rate.
    auto airRig=rig();ssx::AnimationStateDefinition into;into.semantic=269;into.animationClass=9;into.kind=0;into.completionKind=2;into.initialClip=100;airRig->stateDefinitions[269]=into;
    ssx::AnimationStateDefinition spinning;spinning.semantic=289;spinning.animationClass=2;spinning.kind=2;spinning.initialClip=101;airRig->stateDefinitions[289]=spinning;
    auto spinner=player(airRig,269,tick);ssx::PrototypeRider airRider;airRider.grounded=false;ssx::OriginalGroundState airState;airState.controlState=5;airState.animationIndex=269;airRider.seedOriginalPoseControls(airState);
    ssx::OriginalAirControlState angular;angular.mode=1;angular.phase=1;angular.targetSpin=-3.1415927410125732f;airRider.seedOriginalAirControl({},angular,{{0,0,0},{0,0,0,1}},{});
    assert(spinner->prepareLocalPose(airRider,first));assert(spinner->currentSemantic(2)==289);assert(spinner->sequenceState()[0].slots[0].time==0);assert(spinner->finishWorldPose(airRider,first));
    assert(spinner->prepareLocalPose(airRider,second));assert(spinner->sequenceState()[0].slots[0].time==tick);assert(spinner->finishBodyPose(airRider,second));
    angular.mode=3;airRider.seedOriginalAirControl({},angular,{{0,0,0},{0,0,0,1}},{});assert(!spinner->prepareLocalPose(airRider,3./60));
    // Marker1 freezes the main grab clock on the following control phase;
    // repeated pose reads cannot advance it or retrigger a scoring request.
    auto grabRig=rig();grabRig->clips[0].eventTimes={0,tick*2,.2f,.3f};
    ssx::AnimationStateDefinition grab;grab.semantic=77;grab.animationClass=18;grab.kind=0;grab.completionKind=5;grab.initialClip=100;grabRig->stateDefinitions[77]=grab;
    ssx::AnimationStateDefinition grabCycle;grabCycle.semantic=90;grabCycle.animationClass=18;grabCycle.kind=2;grabCycle.channel=1;grabCycle.initialClip=101;grabRig->stateDefinitions[90]=grabCycle;
    auto grabPlayer=player(grabRig,77);ssx::OriginalGrabState grabState{0,4};ssx::OriginalGrabProfile grabProfile;grabProfile.grabs[4]={77,90};unsigned starts=0,stops=0;
    ssx::OriginalGrabAnimationAccess access;access.mainClass=[&](){return grabPlayer->currentClass(2);};access.mainFlags=[&](){return grabPlayer->mainAnimationState()->flags;};access.play=[&](int semantic,bool force){assert(grabPlayer->playSemantic(semantic,1,~uint64_t(0),force));};access.setRate=[&](unsigned channel,float rate){grabPlayer->setChannelRate(channel,rate);};access.fade=[&](unsigned channel,float duration){grabPlayer->fadeChannel(channel,duration);};access.score=[&](int index,bool begin){assert(index==4);++(begin?starts:stops);};
    for(unsigned frame=1;frame<=4;++frame){auto result=ssx::originalGrabLifecycle(grabState,grabProfile,4,false,access);assert(result.supported&&result.active);assert(grabPlayer->prepareLocalPose(airRider,frame/60.));assert(grabPlayer->finishBodyPose(airRider,frame/60.));}
    assert(grabState.state==2&&starts==1&&stops==0);auto body=std::find_if(grabPlayer->sequenceState().begin(),grabPlayer->sequenceState().end(),[](const auto&s){return s.channel==2;});assert(body->rate==0&&body->slots[0].time==tick*2);
    assert(ssx::originalGrabLifecycle(grabState,grabProfile,-1,false,access).supported);assert(grabState.state==5&&stops==1&&grabPlayer->currentSemantic(1)==438);
    // Both players draw from one callback. Repeated semantic requests still
    // consume lookup randomness before deciding whether a clip can inherit time.
    auto variantRig=rig();ssx::AnimationStateDefinition randomHead;randomHead.semantic=319;randomHead.channel=1;randomHead.kind=0;randomHead.initialClip=0xffffffffu;randomHead.blendSeconds=.23f;variantRig->stateDefinitions[319]=randomHead;
    variantRig->animationVariants[319]={{1,100,1},{2,100,2}};variantRig->variantClips={{1,100},{2,101}};
    randomHead.semantic=315;variantRig->stateDefinitions[315]=randomHead;variantRig->animationVariants[315]={{519,0,0}};
    randomHead.semantic=317;randomHead.initialClip=100;variantRig->stateDefinitions[317]=randomHead;
    auto firstPlayer=player(variantRig),secondPlayer=player(variantRig);unsigned cursor=0;std::array<uint32_t,3>draws{199,101,0};auto sharedDraw=[&](){assert(cursor<draws.size());return draws[cursor++];};
    firstPlayer->bindVariantRandom(sharedDraw);secondPlayer->bindVariantRandom(sharedDraw);firstPlayer->setVariantFlags(1);secondPlayer->setVariantFlags(0);
    ssx::PrototypeRider cachedRider;cachedRider.grounded=true;ssx::OriginalGroundState cachedState;cachedState.controlState=0;cachedState.animationIndex=5;cachedState.animationSelectionSupported=true;cachedRider.seedOriginalPoseControls(cachedState);
    assert(firstPlayer->prepareLocalPose(cachedRider,first));auto cachedPose=firstPlayer->finishWorldPose(cachedRider,first);assert(cachedPose);
    firstPlayer->resetDefaultRoot({3,4,5},{0,0,0,1});assert(firstPlayer->playSemantic(319));assert(secondPlayer->playSemantic(319));
    assert(cursor==2&&firstPlayer->sequenceState()[0].slots[0].clip==100&&secondPlayer->sequenceState()[0].slots[0].clip==101);
    assert(secondPlayer->playSemantic(319));assert(cursor==3&&secondPlayer->sequenceState()[0].slots[0].clip==100); // A different weighted leaf must not inherit101.
    size_t oldCount=firstPlayer->sequenceState().size();assert(firstPlayer->playSemantic(315));assert(cursor==3&&firstPlayer->currentSemantic(1)==319&&firstPlayer->sequenceState().size()==oldCount);
    auto oldMirror=firstPlayer->sequenceState()[0].mirror;firstPlayer->rotateSequenceRoots({0,0,1,0});auto rotated=firstPlayer->sequenceState()[0].root;assert(rotated.position[0]==-3&&rotated.position[1]==-4&&rotated.position[2]==5&&firstPlayer->sequenceState()[0].mirror==oldMirror);
    assert(firstPlayer->finishWorldPose(cachedRider,first)->at(0).position==cachedPose->at(0).position);
    firstPlayer->resetDefaultRoot({7,8,9},{0,0,0,1});assert(firstPlayer->sequenceState()[0].root.position==rotated.position);assert(firstPlayer->playSemantic(317));assert((firstPlayer->sequenceState()[0].root.position==ssx::AnimationVector{7,8,9}));assert(firstPlayer->sequenceState()[0].mirror==oldMirror);
    puts("Native animation phase separation, requested slots, cached poses and completion boundaries passed");
}}
