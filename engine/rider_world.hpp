#pragma once
#include "riding.hpp"
#include "original_random.hpp"
#include "rider_pair_system.hpp"
#include <memory>
namespace ssx {
class RiderWorld;
using WorldControls=std::function<RiderInput(RiderWorld&,size_t)>;
using WorldActorCallback=std::function<void(RiderWorld&,size_t)>;
struct WorldRider {
    size_t sourceIndex=0;
    PrototypeRider* rider=nullptr;
    WorldControls controls;
    RiderPoseStages poses;
    OriginalRaceProgress progress;
    OriginalRiderRaceFinish finish;
    int humanIndex=-1;
    bool trackProgressEnabled=true;
    RaceCourseEffects courseEffects;
    std::vector<OriginalRacePathEvent> unhandledEvents;
};
// Owns shared timing and dispatch, not an invented AI. Each NPC must supply its
// recovered controller and pose stages. Pair/scenery callbacks run inside the
// actor's second motion phase while every participant's pose is already ready.
class RiderWorld {
    const CollisionWorld& terrain;const CollisionWorld& scenery;
    const WorldBodyCollision* bodyWorld;
    OriginalRaceEventAsset asset;
    std::vector<WorldRider> actors;
    std::vector<RiderInteraction> interactions;
    std::optional<OriginalRandomState> random;
    std::unique_ptr<OriginalRiderPairSystem> pairs;
    bool pairKnockdownCheat=false;
    bool running=false,activated=false;
    double accumulator=0;
public:
    OriginalRaceClock clock;
    OriginalRaceCheckpoints checkpoints;
    RaceClockEffects clockEffects;
    std::function<void(RiderWorld&)> refreshProximity;
    WorldActorCallback resolveInteractions;
    WorldActorCallback updateNpcProgress;
    std::function<void(RiderWorld&,size_t,RiderFramePhase)> observePhase;
    RiderWorld(const OriginalRaceEventAsset& source,const CollisionWorld& ground,const CollisionWorld& obstacles,const WorldBodyCollision* bodies=nullptr)
        :terrain(ground),scenery(obstacles),bodyWorld(bodies),asset(source),clock(source.clock),checkpoints(source.checkpoints){
        if(asset.configuration[0]!=0)throw std::runtime_error("Shared race game type not recovered");
    }
    RiderWorld(const RiderWorld&)=delete;RiderWorld& operator=(const RiderWorld&)=delete;
    RiderWorld(RiderWorld&&)=delete;RiderWorld& operator=(RiderWorld&&)=delete;
    void addRider(size_t sourceIndex,PrototypeRider& rider,WorldControls controls,RiderPoseStages poses){
        if(running)throw std::runtime_error("Cannot change roster during a shared frame");
        if(sourceIndex>=asset.participants.size()||sourceIndex!=actors.size())throw std::runtime_error("Riders must follow the original selected-roster order");
        for(auto& actor:actors)if(actor.rider==&rider)throw std::runtime_error("Duplicate native rider in world");
        if(!controls)throw std::runtime_error("A native controller is required for every participant");
        const auto& source=asset.participants[sourceIndex];
        int human=-1;if(source.human){human=0;for(auto& actor:actors)human+=actor.humanIndex>=0;}
        actors.push_back({sourceIndex,&rider,std::move(controls),std::move(poses),source.progress,source.finish,human});
    }
    size_t size()const{return actors.size();}
    WorldRider& actor(size_t index){return actors.at(index);}
    const WorldRider& actor(size_t index)const{return actors.at(index);}
    void seedRandom(OriginalRandomState state){if(running)throw std::runtime_error("Cannot reseed shared RNG mid-frame");random=state;}
    uint32_t randomWord(){if(!random)throw std::runtime_error("Original shared RNG state is missing");return random->next();}
    const std::optional<OriginalRandomState>& randomState()const{return random;}
    void tick(){
        if(running)throw std::runtime_error("Shared frame is already active");
        if(actors.size()!=asset.participants.size()||actors.empty())throw std::runtime_error("Complete original roster required before world stepping");
        for(auto& actor:actors)if(actor.rider->framePhase()!=RiderFramePhase::Idle)throw std::runtime_error("Participant has an unfinished frame");
        if(!activated){for(auto& actor:actors){actor.rider->useSharedRaceClock();actor.rider->setDetailedBodyQueries(asset.participants[actor.sourceIndex].human);}activated=true;}
        running=true;
        std::vector<float> finished;for(auto& actor:actors)if(actor.humanIndex>=0)finished.push_back(actor.finish.elapsed);
        clockEffects=originalRaceClockBeginTick(clock,finished);
        if(pairs)pairs->refreshProximity(clock.totalTicks);
        if(refreshProximity)refreshProximity(*this); //10F560, before120F20.
        interactions.clear();interactions.reserve(actors.size());
        for(size_t i=0;i<actors.size();++i)interactions.push_back([this,i](PrototypeRider&){if(resolveInteractions)resolveInteractions(*this,i);if(pairs)pairs->resolveActor(unsigned(i),clock.totalTicks,pairKnockdownCheat);});
        for(size_t i=0;i<actors.size();++i){auto& a=actors[i];originalRaceFinishElapsedStep(a.finish);a.courseEffects={};a.unhandledEvents.clear();
            a.rider->beginFrame({},terrain,scenery,bodyWorld,nullptr,&a.poses,RiderFrameTiming{uint32_t(clock.totalTicks),a.finish.elapsed},&interactions[i]);
            if(observePhase)observePhase(*this,i,RiderFramePhase::Started);
        }
        for(auto phase:riderFramePhases)for(size_t i=0;i<actors.size();++i){auto& a=actors[i];
            if(phase==RiderFramePhase::Controls)a.rider->setFrameInput(a.controls(*this,i));
            if(phase==RiderFramePhase::Events){
                auto physical=OriginalAirState::fromNative(a.rider->position,a.rider->velocity);
                if(a.trackProgressEnabled){
                    auto events=originalRaceProgressStep(asset.paths,a.progress,physical.position,physical.velocity,clock.totalTicks);
                    a.courseEffects=originalRaceApplyCourseEvents(events,a.finish,checkpoints,a.humanIndex,clock.raceTicks,true);
                    for(auto e:events)if(e.type!=1&&e.type!=11)a.unhandledEvents.push_back(e);
                }
                if(updateNpcProgress)updateNpcProgress(*this,i); //1125C0 after112338.
            }
            if(observePhase)observePhase(*this,i,phase);
            a.rider->runFramePhase(phase);
        }
        originalRaceClockEndTick(clock);for(auto& actor:actors)actor.rider->completeSharedTick(uint32_t(clock.totalTicks));running=false;
    }
    void advance(double seconds){
        if(!std::isfinite(seconds)||seconds<=0)return;accumulator+=std::min(seconds,.25);
        while(accumulator+1e-12>=1.0/60){tick();accumulator-=1.0/60;}
    }
    void installPairs(std::function<OriginalPairActorView(RiderWorld&,unsigned)> attributes,
                      std::function<void(RiderWorld&,unsigned,unsigned,const OriginalPairReactionRequest&,const CollisionRandom&)> reaction,
                      std::optional<OriginalRiderPairSystem::Records> records={},unsigned excludedTail=0,bool knockdownCheat=false){
        if(running||actors.size()!=asset.participants.size()||!attributes||!reaction)throw std::runtime_error("Pair system requires the complete initialized native roster and lifecycle callbacks");
        OriginalPairCallbacks callbacks;
        callbacks.liveView=[this,attributes](unsigned slot){
            auto view=attributes(*this,slot);auto& rider=*actors.at(slot).rider;
            auto live=OriginalAirState::fromNative(rider.position,rider.velocity);auto physical=rider.currentPhysicalFrame();
            view.slot=slot;view.body=rider.lastBodyVolume?&*rider.lastBodyVolume:nullptr;
            view.impulse.velocityCmps=live.velocity;view.impulse.motionMode=rider.grounded?0:1;view.impulse.controlState=rider.currentControlState();
            view.impulse.groundNormal=rider.hasOriginalPoseControls()?rider.originalGroundState().normal:terrain_original::Vector{float(rider.normal.x),float(-rider.normal.z),float(rider.normal.y)};
            view.impulse.physicalUp=physical.up;view.reaction.physical=physical;
            if(view.body&&view.body->reactionFrame)view.reaction.presentation=*view.body->reactionFrame;
            if(auto animation=rider.currentMainAnimation())view.reaction.animationClass=animation->animationClass;
            if(rider.hasOriginalPoseControls()){view.reaction.reverseStance=rider.originalGroundState().reverseStance;view.reaction.manualSpin=rider.originalGroundState().manualSpin;}
            if(!rider.grounded&&rider.hasOriginalAirControl())view.reaction.manualSpin=rider.originalAirControlState().spinRate;
            view.attack.positionCm=live.position;
            if(rider.hasOriginalBoost())view.boost=rider.originalBoostState().amount;
            return view;
        };
        callbacks.translate=[this](unsigned slot,auto delta){actors.at(slot).rider->translateWorldContact(delta);};
        callbacks.setVelocity=[this](unsigned slot,auto velocity,bool reseed){actors.at(slot).rider->setWorldContactVelocity(velocity,reseed);};
        callbacks.react=[this,reaction](unsigned target,unsigned other,const auto& request,const CollisionRandom& random){reaction(*this,target,other,request,random);};
        callbacks.randomWord=[this](){return randomWord();};
        pairs=std::make_unique<OriginalRiderPairSystem>(unsigned(actors.size()),std::move(callbacks),excludedTail);
        if(records)pairs->seedRecords(*records);pairKnockdownCheat=knockdownCheat;
    }
    const OriginalRiderPairSystem* pairSystem()const{return pairs.get();}
    bool frameActive()const{return running;}
};
}
