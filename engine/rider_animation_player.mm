#import "rider_animation_player.h"
#include "original_float.hpp"
#include "rail_animation.hpp"
#include "animation_completion.hpp"
#include "board_press_animation.hpp"
#include "animation_events.hpp"
#include <bit>
#include <cfenv>
#include <map>
#pragma STDC FENV_ACCESS ON
namespace ssx {namespace {
struct Round {int previous=std::fegetround();Round(){std::fesetround(FE_TOWARDZERO);}~Round(){std::fesetround(previous);}};
AnimationVector vec(NSArray*a){if(a.count!=3)throw std::runtime_error("Invalid animation vector");return {[a[0]floatValue],[a[1]floatValue],[a[2]floatValue]};}
float value(NSDictionary*d,NSString*key,float fallback){return d[key]?[d[key]floatValue]:fallback;}
}
OriginalRiderAnimation::OriginalRiderAnimation(std::shared_ptr<const AnimationRigAsset>r,std::vector<OriginalAnimationSequence>s,AnimationVector size,RiderPoseContact p,bool collision,RiderRootPresentation root,unsigned pivot,AnimationTransform baseRoot,bool baseMirror,uint64_t boneMask,std::array<int,6> requested):rig(std::move(r)),sequences(std::move(s)),scale(size),defaultRoot(baseRoot),defaultMirror(baseMirror),contact(p),presentation(root),pivotBone(pivot),activeBoneMask(boneMask),collisionEnabled(collision){if(!rig||rig->bones.size()<22)throw std::runtime_error("Invalid original rider animation configuration");
    for(unsigned channel=0;channel<6;++channel){if(requested[channel]>=0)requestedSemantics[channel]=requested[channel];else{auto first=std::find_if(sequences.begin(),sequences.end(),[&](const auto&s){return s.channel==int(channel);});if(first!=sequences.end())requestedSemantics[channel]=first->semantic;}}
}
float OriginalRiderAnimation::duration(uint32_t id)const {
    auto clip=std::find_if(rig->clips.begin(),rig->clips.end(),[&](const auto&c){return c.id==id;});
    if(clip==rig->clips.end())throw std::runtime_error("Missing original cycle asset");return clip->duration;
}
float OriginalRiderAnimation::semanticDuration(int semantic)const{
    auto definition=rig->stateDefinitions.find(semantic);if(definition==rig->stateDefinitions.end())throw std::runtime_error("Missing animation duration semantic");
    uint32_t clip=definition->second.initialClip;
    if(definition->second.kind==10)clip=originalRailLeafClipId(originalRailUberBalanceLeaves(semantic).nonNegative);
    if(definition->second.kind==9)clip=originalRailLeafClipId(originalHalfpipeBalanceLeaves(semantic).nonNegative);
    if(auto choices=rig->animationVariants.find(semantic);choices!=rig->animationVariants.end()){
        auto leaf=originalAnimationVariant(choices->second,variantFlags,variantRandom);if(leaf==519)return 0;
        auto mapped=rig->variantClips.find(leaf);if(mapped==rig->variantClips.end())throw std::runtime_error("Missing duration lookup leaf");clip=mapped->second;
    }
    return duration(clip);
}
bool OriginalRiderAnimation::enter(int semantic,float rate,uint64_t mask,bool force){
    auto found=rig->stateDefinitions.find(semantic);if(found==rig->stateDefinitions.end())return false;
    const auto&def=found->second;uint32_t clip=def.initialClip;
    if(def.kind==10)clip=originalRailLeafClipId(originalRailUberBalanceLeaves(semantic).nonNegative);
    if(def.kind==9)clip=originalRailLeafClipId(originalHalfpipeBalanceLeaves(semantic).nonNegative); //104660 handplant balance seek
    if(auto variants=rig->animationVariants.find(semantic);variants!=rig->animationVariants.end()){
        uint32_t leaf=originalAnimationVariant(variants->second,variantFlags,variantRandom);
        if(leaf==519)return true; //312934 returns438 without writing a requested slot.
        auto mapped=rig->variantClips.find(leaf);if(mapped==rig->variantClips.end())return false;clip=mapped->second;
    }
    if(clip==0xffffffffu)return false;OriginalAnimationSequence next;next.semantic=semantic;next.channel=def.channel;next.priority=originalChannelPriority(def.channel);next.endFadeOut=def.endFadeOut;next.rate=rate<0?nextRate:rate;next.root=defaultRoot;next.mirror=defaultMirror;next.mask=mask;
    next.slots.push_back({clip,0,duration(clip),1,1,def.kind==2,true});
    if(rig->cycleMaps.contains(semantic)){auto ids=rig->cycleMaps.at(semantic);next.slots={{ids[2],0,duration(ids[2]),1,1,true,true},{ids[1],0,duration(ids[1]),1,0,false,true}};}
    else if(rig->threeWayMaps.contains(semantic)){auto ids=rig->threeWayMaps.at(semantic);next.slots={{ids[1],0,duration(ids[1]),1,1,true,true},{ids[0],0,duration(ids[0]),1,0,false,true}};}
    bool existing=false;int inherit=-1;
    for(size_t i=0;i<sequences.size();++i)if(sequences[i].channel==def.channel){existing=true;sequences[i].completionEnabled=false;originalAnimationFadeOut(sequences[i],def.blendSeconds);
        const auto&old=sequences[i];bool compatibleClip=!old.slots.empty()&&(old.slots[0].clip==next.slots[0].clip||(def.kind!=0&&def.kind!=1&&def.kind!=2));
        if(compatibleClip&&old.semantic==semantic&&old.mask==next.mask&&old.mirror==next.mirror&&old.root.position==next.root.position&&old.root.rotation==next.root.rotation)inherit=int(i);
    }
    if(def.blendSeconds==0){std::erase_if(sequences,[&](const auto&s){return s.channel==def.channel;});}
    else if(inherit>=0&&!force){next.flags=sequences[inherit].flags;next.slots=sequences[inherit].slots;next.weight=sequences[inherit].weight;next.targetWeight=1;next.fadeRemaining=next.weight<1?def.blendSeconds*originalScalarSubtract(1.f,next.weight):0;sequences.erase(sequences.begin()+inherit);}
    else if(existing){next.weight=0;next.targetWeight=1;next.fadeRemaining=def.blendSeconds;}
    else if(def.firstFadeIn!=0){next.weight=0;next.targetWeight=1;next.fadeRemaining=def.firstFadeIn;}
    auto place=std::find_if(sequences.begin(),sequences.end(),[&](const auto&s){return s.channel>=def.channel;});sequences.insert(place,std::move(next));requestedSemantics.at(def.channel)=semantic;return true;
}
void OriginalRiderAnimation::advance(const OriginalGroundState&state,float prewindSpin,float prewindFlip){
    float dt=state.timeScale*std::bit_cast<float>(0x3c888889u);
    for(auto&sequence:sequences){
        auto definition=rig->stateDefinitions.find(sequence.semantic);
        if(definition!=rig->stateDefinitions.end()&&definition->second.kind==8){originalAnimationStartStep(sequence,startPose,state.timeScale);continue;}
        if(definition!=rig->stateDefinitions.end()&&definition->second.kind==7){originalAnimationPrewindStep(sequence,prewindSpin,prewindFlip,state.timeScale);continue;}
        if(definition!=rig->stateDefinitions.end()&&definition->second.kind==11){originalAnimationAirAdjustStep(sequence,definition->second.followupClip,duration(definition->second.followupClip),state.adjustment28C.current,state.adjustment298.current,state.timeScale);continue;}
        if(definition!=rig->stateDefinitions.end()&&definition->second.kind==9){auto leaves=originalHalfpipeBalanceLeaves(sequence.semantic);originalTwoWayBalanceStep(sequence,originalRailLeafClipId(leaves.negative),duration(originalRailLeafClipId(leaves.negative)),originalRailLeafClipId(leaves.nonNegative),duration(originalRailLeafClipId(leaves.nonNegative)),handplantBalance,state.timeScale);continue;}
        //1047F0/104728 board-press seeks: slot-0 clock only, then the sequence fade.
        if(definition!=rig->stateDefinitions.end()&&(definition->second.kind==13||definition->second.kind==14)){originalBoardPressSeekStep(sequence,originalBoardPressSeekAmount(definition->second.kind,sequence.semantic,boardPress268,boardPress280),state.timeScale);continue;}
        if(definition!=rig->stateDefinitions.end()&&definition->second.kind==10){auto leaves=originalRailUberBalanceLeaves(sequence.semantic);originalRailUberBalanceStep(sequence,sequence.semantic,duration(originalRailLeafClipId(leaves.negative)),duration(originalRailLeafClipId(leaves.nonNegative)),railBalance,state.timeScale);continue;}
        bool cycle=sequence.slots.size()>=2&&sequence.slots[0].enabled&&sequence.slots[1].enabled;
        cycle=cycle&&(rig->cycleMaps.contains(sequence.semantic)||rig->threeWayMaps.contains(sequence.semantic));
        if(cycle){OriginalAnimationCyclePair pair;for(unsigned i=0;i<2;++i){auto&slot=sequence.slots[i];pair.clips[i]=slot.clip;pair.times[i]=slot.time;pair.durations[i]=slot.duration;pair.rates[i]=slot.rate;pair.weights[i]=slot.weight;}pair.sequenceRate=sequence.rate;
            if(rig->cycleMaps.contains(sequence.semantic)){const auto&choices=rig->cycleMaps.at(sequence.semantic);std::array<float,5> lengths;for(unsigned i=0;i<5;++i)lengths[i]=duration(choices[i]);originalFiveWayAnimationStep(pair,choices,lengths,state.reverseStance?-state.animationTurn.current:state.animationTurn.current,state.timeScale);}
            else{const auto&choices=rig->threeWayMaps.at(sequence.semantic);std::array<float,3> lengths;for(unsigned i=0;i<3;++i)lengths[i]=duration(choices[i]);originalThreeWayAnimationStep(pair,choices,lengths,(sequence.semantic==28||sequence.semantic==36)?originalBoardPressHoldAmount(boardPress274):sequence.semantic==11?(state.reverseStance?state.brake.current:-state.brake.current):(sequence.semantic>=18&&sequence.semantic<=20)?(state.reverseStance?-railBalance:railBalance):(state.reverseStance?-state.animationTurn.current:state.animationTurn.current),state.timeScale);}
            sequence.completed=pair.completed;
            for(unsigned i=0;i<2;++i){auto&slot=sequence.slots[i];slot.clip=pair.clips[i];slot.time=pair.times[i];slot.duration=pair.durations[i];slot.weight=pair.weights[i];}
        }else for(auto&slot:sequence.slots){
            auto definition=rig->stateDefinitions.find(sequence.semantic);
            if(&slot==sequence.slots.data()){
                auto clip=std::find_if(rig->clips.begin(),rig->clips.end(),[&](const auto&c){return c.id==slot.clip;});if(clip==rig->clips.end())throw std::runtime_error("Missing event clip");
                std::vector<OriginalAnimationEventMarker> events;for(unsigned i=0;i<clip->eventTimes.size();++i)events.push_back({clip->eventTimes[i],i,false});
                if(sequence.completionEnabled&&definition!=rig->stateDefinitions.end()&&(definition->second.completionKind||definition->second.kind==0))events.push_back({0,63,true});
                OriginalAnimationEventFlags flags{sequence.flags,sequence.raisedFlags,sequence.completed,sequence.seekPending};originalAnimationPrimaryStep(slot,sequence.rate,dt,flags,events);
                sequence.flags=flags.latched;sequence.raisedFlags=flags.raised;sequence.completed=flags.completed;sequence.seekPending=flags.seekPending;
            }else originalAnimationSlotStep(slot,sequence.rate,dt);
            if(definition!=rig->stateDefinitions.end()&&slot.enabled&&!slot.loop&&slot.time>=slot.duration){
                if(definition->second.kind==12&&slot.clip==definition->second.initialClip){slot.clip=definition->second.followupClip;slot.duration=duration(slot.clip);slot.time=0;slot.loop=true;}
            }
        }
        originalAnimationFadeStep(sequence,dt);
    }
    std::erase_if(sequences,[](const auto&s){return s.stopWhenFaded&&s.fadeRemaining==0&&s.weight==0;});
}
void OriginalRiderAnimation::completeSequences(){
    // 11EB60 calls312490 AFTER3123C0 has sampled this tick's local pose.
    // Dispatch103918 uses descriptor+8, not the animation's class.
    originalAnimationCompletionBatch(sequences,[&](size_t i){
        if(!supported)return;
        auto definition=rig->stateDefinitions.find(sequences[i].semantic);
        if(definition==rig->stateDefinitions.end())return;
        // 103918 -> jump table 0x456990 (engine/animation_completion.hpp, oracle tools/test_animation_completion_native.py).
        const auto completion=originalAnimationCompletion(definition->second.completionKind,sequences[i].semantic,grabEndSemantic==436,boardPress330);
        const unsigned channel=sequences[i].channel;using Action=OriginalAnimationCompletion::Action;
        switch(completion.action){
        case Action::Remove:sequences.erase(sequences.begin()+i);requestedSemantics.at(channel)=438;break; //104CA0 -> 3145F8
        case Action::Keep:requestedSemantics.at(channel)=438;break; //104A60 outside its table: the sequence stays latched
        case Action::Play:{ //312BD0: 144670 moves bit 63 to the raised word; 3128E8 fades the finished sequence out
            auto&s=sequences[i];const uint64_t bit=uint64_t(1)<<63;s.raisedFlags|=s.flags&bit;s.flags&=~bit;
            if(!enter(completion.semantic)){supported=false;return;}break;}
        case Action::Replace:{ //312B18: 3145F8, 3128E8, the head takes the old weight (and a running fade via 313A10)
            auto old=sequences[i];sequences.erase(sequences.begin()+i);if(!enter(completion.semantic)){supported=false;return;}
            auto current=std::find_if(sequences.begin(),sequences.end(),[&](const auto&s){return s.channel==int(channel);});
            if(current==sequences.end()){supported=false;return;}
            current->weight=old.weight;current->targetWeight=old.weight;current->fadeRemaining=0;
            if(old.fadeRemaining!=0){current->targetWeight=old.targetWeight;current->fadeRemaining=old.fadeRemaining;}
            break;}
        }
    });
}
void OriginalRiderAnimation::consumeEvent(const PrototypeRider&rider){
    const auto&event=rider.originalAnimationEvent();if(event.serial==eventSerial)return;eventSerial=event.serial;
    if(event.reverseTurn){
        //115168 calls311B48(-pi/2 quaternion) on existing sequences, then
        //publishes its separately generated root for future sequences.
        auto sineCosine=originalSinCos(-1.5707963705062866211f);
        AnimationTransform turn;turn.rotation={sineCosine[0]*0.f,sineCosine[0]*0.f,sineCosine[0]*1.f,sineCosine[1]};
        for(auto&sequence:sequences)sequence.root=originalAnimationCompose(turn,sequence.root);
        defaultRoot={};defaultRoot.rotation=event.reverseRootQuaternion;defaultMirror=rider.originalGroundState().reverseStance;
    }
    if(event.airExit)fadeChannel(1,0.33000001311302185f);
    if(event.semantic>=0&&!enter(event.semantic,event.rate))supported=false;
}
bool OriginalRiderAnimation::prepareLocalPose(const PrototypeRider&rider,double seconds){
    Round round;consumeEvent(rider);
    uint64_t target=uint64_t(std::max(0.,std::floor(seconds*60+1e-5)));if(target<ticks)throw std::runtime_error("Animation clock moved backwards");
    if(target==ticks&&sampledLocal)return true;
    if(!supported||!rider.hasOriginalPoseControls())return false;
    if(target==ticks){sampledLayers=originalAnimationLayers(sequences);sampledLocal=originalAnimationLocalPose(rig->bones,rig->clips,sampledLayers,activeBoneMask);return true;}
    const auto&state=rider.originalGroundState();bool airborne=!rider.grounded,prewind=state.animationIndex>=245&&state.animationIndex<=267;bool soft=(state.controlState==0||state.controlState==3)&&state.animationIndex>=55&&state.animationIndex<=60;
    bool releasedPrewind=false,grabPose=false,passive=airborne&&state.controlState==4;
    if(controllerDriven){if(airborne&&state.controlState==5&&(!rider.hasOriginalAirControl()||!rider.airAngularSupported))return false;}
    else if(airborne&&!soft&&!passive){if(!rider.hasOriginalAirControl()||!rider.airAngularSupported)return false;
        const auto&air=rider.originalAirControlState();
        int semantic=currentSemantic(2);
        bool into=semantic>=269&&semantic<=286;
        bool cycle=semantic>=289&&semantic<=296&&air.phase==1&&air.extended==0&&air.adjustSpin==0&&air.adjustFlip==0;
        releasedPrewind=air.mode==1&&(air.phase==0||air.phase==1)&&(into||cycle);
        grabPose=currentClass(2)==18; //1352A8 active result skips selection in every supported angular mode.
        // Verified IntoAir and its unextended cycle hold their requested clip.
        // Grab, direction extension and phase2 exit selection remain explicit.
        if(!releasedPrewind&&!grabPose&&(air.mode!=0||air.phase!=3||air.targetFlip!=0||air.targetSpin!=0||air.adjustFlip!=0||air.adjustSpin!=0))return false;
    }else if(!soft&&!passive&&(!state.animationSelectionSupported||state.controlState!=(prewind?2:0))){supported=false;return false;}
    while(ticks<target){
        auto body=std::find_if(sequences.begin(),sequences.end(),[](const auto&s){return s.channel==2;});
        if(controllerDriven){if(body==sequences.end()||body->semantic!=state.animationIndex)if(!enter(state.animationIndex)){supported=false;return false;}}
        else if(airborne&&!soft&&!passive&&!releasedPrewind&&!grabPose){
            if(body==sequences.end())return false;
            int desired=body->semantic;float rate=1;
            if(desired!=268&&desired!=287&&desired!=305)desired=268;
            else if(desired==268){} // Completed by the post-sampling103918 callback.
            else if(rider.airControlTicks!=0){
                const auto&trajectory=rider.originalAirAnimationTrajectoryState();
                auto choice=originalAirLandingAnimation(desired,trajectory.status,trajectory.predictedTime,trajectory.elapsed,duration(rig->stateDefinitions.at(305).initialClip));desired=choice.semantic;rate=choice.rate;
            }
            if(desired!=body->semantic&&!enter(desired,rate)){supported=false;return false;}
        }else if(!releasedPrewind&&!grabPose&&(body==sequences.end()||body->semantic!=state.animationIndex))if(!enter(state.animationIndex)){supported=false;return false;}
        advance(state,rider.originalAirPrewindState().spin.current,rider.originalAirPrewindState().flip.current);sampledLayers=originalAnimationLayers(sequences);sampledLocal=originalAnimationLocalPose(rig->bones,rig->clips,sampledLayers,activeBoneMask);completeSequences();++ticks;
    }
    return supported&&sampledLocal.has_value();
}
std::optional<std::vector<AnimationTransform>> OriginalRiderAnimation::finishWorldPose(const PrototypeRider&rider,double seconds){
    Round round;uint64_t target=uint64_t(std::max(0.,std::floor(seconds*60+1e-5)));
    if(target!=ticks||!sampledLocal)return std::nullopt;
    // Later physical collision/landing mutations do not rebuild same-tick geometry.
    if(worldTicks==target&&cachedWorld)return cachedWorld;
    if(!supported||!rider.hasOriginalPoseControls())return std::nullopt;
    const auto&state=rider.originalGroundState();bool airborne=!rider.grounded;
    auto local=sampledLocal?*sampledLocal:originalAnimationLocalPose(rig->bones,rig->clips,originalAnimationLayers(sequences),activeBoneMask);
    auto physical=OriginalAirState::fromNative(rider.position,rider.velocity);AnimationTransform physicalRoot{physical.position,state.quaternion};auto rootState=presentation;
    rootState.turn=state.turn.current;rootState.brake=state.brake.current;rootState.lateral=state.lateral;rootState.controlState=state.controlState;
    if(rider.groundTicks||airborne){rootState.liftCm=state.presentationLift.current;rootState.extraLean=state.extraLean.current;rootState.roll=state.presentationRoll.current;}
    airPivot.reset();
    if(airborne){auto source=rider.originalAirPhysicalPose();physicalRoot={source.position,source.quaternion};}
    posedGroundState=state;posedAirControl=rider.originalAirControlState();posedPhysical=physicalRoot;posedPresentation=rootState;
    posedAirborne=airborne;posedMotion=rider.groundTicks||airborne;posedBoardDirection=airborne?state.boardNormal:rider.originalGroundDiagnostics().boardNormalForPose;
    if(airborne&&state.controlState==5){AnimationVector pivot;for(unsigned k=0;k<3;++k)pivot[k]=local.at(pivotBone).position[k]*scale[k];airPivot=pivot;
        auto presented=originalAirPresentationCurrent(rider.originalAirControlState(),rider.originalAirPhysicalPose(),pivot);physicalRoot={presented.position,presented.quaternion};rootState.controlState=5;}
    AnimationTransform boardRoot;auto bodyRoot=originalRiderRootPresentation(physicalRoot,local.at(contact.boardRoot).position,scale,rootState,&boardRoot);
    reactionFrame=originalRiderCollisionFrame(boardRoot);
    auto world=originalAnimationWorldPose(rig->bones,local,bodyRoot,scale,{bodyRoot,boardRoot});auto current=contact;current.normal=state.normal;
    if(rider.groundTicks||airborne){current.boardDirection=airborne?state.boardNormal:rider.originalGroundDiagnostics().boardNormalForPose;current.boardLiftCm=state.boardLift;current.boardAlignment=state.boardAlignment.current;}
    originalRiderPoseContact(world,local,scale,current);
    if(detachedBoard&&contact.boardChild<world.size())world[contact.boardChild]=*detachedBoard;
    cachedWorld=world;worldTicks=target;return world;
}
std::optional<BodyCollisionVolume> OriginalRiderAnimation::finishBodyPose(const PrototypeRider&rider,double seconds){
    auto pose=finishWorldPose(rider,seconds);if(!pose||!collisionEnabled)return std::nullopt;
    std::array<terrain_original::Vector,22>centers;for(unsigned i=0;i<22;++i)centers[i]=(*pose)[i].position;
    auto volume=bodyVolumeFromBones(centers,scale[0]);volume.reactionFrame=reactionFrame;volume.landingCenterCm=pose->at(contact.boardRoot).position;volume.airPivotCm=airPivot;
    volume.mainAnimation=mainAnimationState();
    return volume;
}
std::optional<std::vector<AnimationTransform>> OriginalRiderAnimation::poseForRig(const PrototypeRider&,const AnimationRigAsset& target,AnimationVector targetScale)const{
    if(!supported||!cachedWorld||!sampledLocal||sampledLayers.empty()||target.bones.size()<24)return std::nullopt;
    Round round;
    auto local=originalAnimationLocalPose(target.bones,rig->clips,sampledLayers,activeBoneMask);
    const auto& state=posedGroundState;bool airborne=posedAirborne;
    AnimationTransform physicalRoot=posedPhysical;auto rootState=posedPresentation;
    if(airborne&&state.controlState==5){AnimationVector pivot;for(unsigned k=0;k<3;++k)pivot[k]=local.at(pivotBone).position[k]*targetScale[k];
        auto presented=originalAirPresentationCurrent(posedAirControl,{posedPhysical.position,posedPhysical.rotation},pivot);physicalRoot={presented.position,presented.quaternion};}
    AnimationTransform boardRoot;auto bodyRoot=originalRiderRootPresentation(physicalRoot,local.at(contact.boardRoot).position,targetScale,rootState,&boardRoot);
    auto world=originalAnimationWorldPose(target.bones,local,bodyRoot,targetScale,{bodyRoot,boardRoot});auto targetContact=contact;targetContact.normal=state.normal;
    if(posedMotion){targetContact.boardDirection=posedBoardDirection;targetContact.boardLiftCm=state.boardLift;targetContact.boardAlignment=state.boardAlignment.current;}
    originalRiderPoseContact(world,local,targetScale,targetContact);
    if(detachedBoard&&targetContact.boardChild<world.size())world[targetContact.boardChild]=*detachedBoard;
    return world;
}
int OriginalRiderAnimation::currentClass(unsigned channel)const {
    int semantic=currentSemantic(channel);if(semantic==438)return 0;
    auto found=rig->stateDefinitions.find(semantic);return found==rig->stateDefinitions.end()?-1:found->second.animationClass;
}
bool OriginalRiderAnimation::playSemantic(int semantic,float rate,uint64_t mask,bool force){Round round;return enter(semantic,rate,mask,force);}
void OriginalRiderAnimation::consumeRiderEvents(const PrototypeRider&rider){Round round;consumeEvent(rider);}
void OriginalRiderAnimation::rotateSequenceRoots(AnimationQuaternion q){Round round;AnimationTransform rotation;rotation.rotation=q;for(auto&sequence:sequences)sequence.root=originalAnimationCompose(rotation,sequence.root);}
void OriginalRiderAnimation::resetDefaultRoot(AnimationVector position,AnimationQuaternion quaternion){defaultRoot={position,quaternion};}
void OriginalRiderAnimation::setChannelRate(unsigned channel,float rate){for(auto&sequence:sequences)if(sequence.channel==int(channel)){sequence.rate=rate;break;}}
void OriginalRiderAnimation::fadeChannel(unsigned channel,float seconds){
    Round round;requestedSemantics.at(channel)=438;
    for(auto&sequence:sequences)if(sequence.channel==int(channel)){sequence.completionEnabled=false;sequence.flags&=~(uint64_t(1)<<63);originalAnimationFadeOut(sequence,seconds);}
}
std::optional<BodyAnimationState> OriginalRiderAnimation::mainAnimationState()const {
    // Identity/class are the requested channel slots(312AA0/311AE8), whereas
    // flags/C0 belong to the first playback sequence(311B20/312AE8), even fading.
    auto body=std::find_if(sequences.begin(),sequences.end(),[](const auto&s){return s.channel==2;});
    int cls=currentClass(2);if(cls<0)return std::nullopt;
    return BodyAnimationState{currentSemantic(2),cls,body==sequences.end()?0:body->flags,body!=sequences.end()&&body->completed};
}

std::optional<std::vector<AnimationTransform>> OriginalRiderAnimation::poseAt(const PrototypeRider&rider,double seconds){if(!prepareLocalPose(rider,seconds))return std::nullopt;return finishWorldPose(rider,seconds);}
BodyPoseProvider OriginalRiderAnimation::bodyProvider(){auto self=shared_from_this();return [self](const PrototypeRider&rider,double seconds)->std::optional<BodyCollisionVolume>{if(!self->prepareLocalPose(rider,seconds))return std::nullopt;return self->finishBodyPose(rider,seconds);};}

std::shared_ptr<OriginalRiderAnimation> makeOriginalRiderAnimation(std::shared_ptr<const AnimationRigAsset>rig,NSDictionary*source){
    if(!source||!rig)return {};
    static const std::array<std::string,6> known{"zoe","psymon","allegra","moby","griff","luther"};
    if(std::find(known.begin(),known.end(),rig->character)==known.end())return {};
    if(source[@"character"]&&rig->character!=[source[@"character"]UTF8String])return {};
std::vector<OriginalAnimationSequence>sequences;std::map<uint64_t,size_t>groups;
    for(NSDictionary*l in source[@"layers"]){uint64_t id=[l[@"sequence"]unsignedLongLongValue];auto found=groups.find(id);size_t index;
        if(found==groups.end()){index=sequences.size();groups[id]=index;OriginalAnimationSequence s;s.semantic=[l[@"semantic"]intValue];s.channel=[l[@"channel"]intValue];s.priority=[l[@"priority"]intValue];s.mask=[l[@"mask"]unsignedLongLongValue];s.rate=value(l,@"sequence_speed",1);s.weight=value(l,@"sequence_weight",1);s.targetWeight=value(l,@"fade_target",1);s.fadeRemaining=value(l,@"fade_remaining",0);s.stopWhenFaded=[l[@"stop_on_fade"]boolValue];s.completionEnabled=!s.stopWhenFaded;s.completed=[l[@"completed"]boolValue];s.flags=[l[@"sequence_flags"]unsignedLongLongValue];s.raisedFlags=[l[@"raised_flags"]unsignedLongLongValue];s.seekPending=[l[@"seek_pending"]boolValue];s.mirror=[l[@"mirror"]boolValue];if(l[@"root_position"])s.root.position=vec(l[@"root_position"]);if(NSArray*q=l[@"root_rotation"]){if(q.count!=4)throw std::runtime_error("Invalid original sequence root");for(unsigned k=0;k<4;++k)s.root.rotation[k]=[q[k]floatValue];}s.startFadeIn=value(l,@"fade_in",0);s.endFadeOut=value(l,@"fade_out",0);sequences.push_back(s);}else index=found->second;
        auto&s=sequences[index];unsigned slot=[l[@"slot"]unsignedIntValue];if(slot>=3)throw std::runtime_error("Invalid original sequence slot");if(s.slots.size()<=slot)s.slots.resize(slot+1);s.slots[slot]={[l[@"clip"]unsignedIntValue],[l[@"time"]floatValue],[l[@"duration"]floatValue],value(l,@"speed",1),value(l,@"slot_weight",value(l,@"base_weight",value(l,@"weight",1))),[l[@"loop"]boolValue],true};
    }
    NSDictionary*c=source[@"contact"];RiderPoseContact contact;contact.boardDirection=vec(c[@"board_direction"]);contact.normal=vec(c[@"normal"]);contact.boardAlignment=[c[@"board_alignment"]floatValue];contact.boardLiftCm=[c[@"board_lift_cm"]floatValue];contact.legWeight=[c[@"leg_weight"]floatValue];
    if([c[@"legs"]count]!=2)throw std::runtime_error("Original rider needs two leg bindings");for(unsigned i=0;i<2;++i){NSDictionary*l=c[@"legs"][i];auto&leg=contact.legs[i];if([l[@"bones"]count]!=3||[l[@"rotation"]count]!=4)throw std::runtime_error("Invalid original leg binding");leg.thigh=[l[@"bones"][0]intValue];leg.shin=[l[@"bones"][1]intValue];leg.foot=[l[@"bones"][2]intValue];NSArray*q=l[@"rotation"];leg.boardLocalFoot={vec(l[@"position"]),{[q[0]floatValue],[q[1]floatValue],[q[2]floatValue],[q[3]floatValue]}};}
    RiderRootPresentation presentation;if(NSDictionary*p=source[@"presentation"]){presentation.turn=[p[@"turn"]floatValue];presentation.extraLean=[p[@"extra_lean"]floatValue];presentation.brake=[p[@"brake"]floatValue];presentation.roll=[p[@"roll"]floatValue];presentation.liftCm=[p[@"lift_cm"]floatValue];presentation.lateral=vec(p[@"lateral"]);presentation.controlState=[p[@"control_state"]intValue];}
    AnimationTransform baseRoot;if(source[@"default_root_position"])baseRoot.position=vec(source[@"default_root_position"]);if(NSArray*q=source[@"default_root_rotation"]){if(q.count!=4)throw std::runtime_error("Invalid default original animation root");for(unsigned k=0;k<4;++k)baseRoot.rotation[k]=[q[k]floatValue];}
    std::array<int,6> requested{-1,-1,-1,-1,-1,-1};if(NSArray*ids=source[@"current_semantics"]){if(ids.count!=6)throw std::runtime_error("Expected six requested animation channels");for(unsigned channel=0;channel<6;++channel)requested[channel]=[ids[channel]intValue];}
    auto result=std::make_shared<OriginalRiderAnimation>(std::move(rig),std::move(sequences),vec(source[@"scale"]),contact,[source[@"collision_enabled"]boolValue],presentation,[source[@"pivot_bone"]unsignedIntValue],baseRoot,[source[@"default_mirror"]boolValue],source[@"bone_mask"]?[source[@"bone_mask"]unsignedLongLongValue]:~uint64_t(0),requested);
    if(source[@"grab_end_semantic"])result->setGrabEndSemantic([source[@"grab_end_semantic"]intValue]);if(source[@"variant_flags"])result->setVariantFlags([source[@"variant_flags"]unsignedIntValue]);return result;
}
}

namespace ssx {
AnimationTransform OriginalRiderAnimation::presentRoot(const AnimationTransform& physical,const RiderRootPresentation& values)const{
    Round round;
    auto local=sampledLocal?*sampledLocal:originalAnimationLocalPose(rig->bones,rig->clips,originalAnimationLayers(sequences),activeBoneMask);
    AnimationTransform boardRoot;return originalRiderRootPresentation(physical,local.at(contact.boardRoot).position,scale,values,&boardRoot);
}
std::optional<AnimationTransform> OriginalRiderAnimation::previewRoot(int semantic)const{
    Round round;auto found=rig->stateDefinitions.find(semantic);if(found==rig->stateDefinitions.end())return std::nullopt;
    uint32_t clip=found->second.initialClip;
    if(auto variants=rig->animationVariants.find(semantic);variants!=rig->animationVariants.end()){
        // The crash preview samples the semantic's clip without consuming the
        // shared RNG; every crash semantic currently has a single weighted leaf.
        if(variants->second.size()!=1)return std::nullopt;
        auto mapped=rig->variantClips.find(variants->second.front().leaf);if(mapped==rig->variantClips.end())return std::nullopt;clip=mapped->second;
    }
    if(clip==0xffffffffu)return std::nullopt;
    AnimationLayer layer;layer.clip=clip;layer.time=0;layer.weight=1;layer.priority=1;layer.root=defaultRoot;layer.mirror=defaultMirror;
    auto local=originalAnimationLocalPose(rig->bones,rig->clips,{layer},activeBoneMask);if(local.empty())return std::nullopt;return local.front();
}
std::optional<AnimationTransform> OriginalRiderAnimation::scaledLocalRoot()const{
    if(!sampledLocal||sampledLocal->empty())return std::nullopt;Round round;auto root=sampledLocal->front();
    for(unsigned k=0;k<3;++k)root.position[k]=root.position[k]*scale[k];return root;
}
void OriginalRiderAnimation::offsetSequenceRoots(const AnimationTransform& offset){Round round;for(auto&sequence:sequences)sequence.root=originalAnimationCompose(offset,sequence.root);}
bool OriginalRiderAnimation::seekChannel(unsigned channel,float seconds){
    auto found=std::find_if(sequences.begin(),sequences.end(),[&](const auto&s){return s.channel==int(channel);});
    if(found==sequences.end()||found->slots.empty())return false;auto&slot=found->slots.front();
    slot.time=std::clamp(seconds,0.f,slot.duration);found->completed=slot.time>=slot.duration;found->seekPending=true;return true;
}
float OriginalRiderAnimation::channelProgress(unsigned channel)const{
    auto found=std::find_if(sequences.begin(),sequences.end(),[&](const auto&s){return s.channel==int(channel);});
    if(found==sequences.end()||found->slots.empty()||found->slots.front().duration<=0)return 0;Round round;
    return originalScalarDivide(found->slots.front().time,found->slots.front().duration);
}
float OriginalRiderAnimation::channelRate(unsigned channel)const{
    auto found=std::find_if(sequences.begin(),sequences.end(),[&](const auto&s){return s.channel==int(channel);});return found==sequences.end()?1.f:found->rate;
}
float OriginalRiderAnimation::channelDuration(unsigned channel)const{
    auto found=std::find_if(sequences.begin(),sequences.end(),[&](const auto&s){return s.channel==int(channel);});return found==sequences.end()||found->slots.empty()?0.f:found->slots.front().duration;
}
}
