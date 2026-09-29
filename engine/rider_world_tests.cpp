#include "rider_world.hpp"
#include <cassert>
#include <cstdio>
using namespace ssx;
int main(){
    CollisionWorld flat({{{-100,0,-100},{100,0,-100},{100,0,100}},{{-100,0,-100},{100,0,100},{-100,0,100}}});
    OriginalGroundProfile profile;profile.bodyScale=1;profile.speedLimit=5000;profile.depthTarget1=1;profile.depthTarget3=2;
    GroundCurve zero={{{0,0},{10,0},{100,0},{1000,0}}};profile.surface.slipFriction=zero;profile.lateralSpeedCurve=zero;profile.turnMinCurve=profile.turnMaxCurve=zero;
    std::array<PrototypeRider,2> riders;
    OriginalRaceEventAsset asset;asset.clock.phase=asset.clock.previous=RacePhase::Race;asset.clock.totalTicks=338;asset.clock.raceTicks=158;
    OriginalRacePath path;path.origin={0,0,0};path.low={-10000,-10000,-10000};path.high={10000,1000000,10000};path.remainingAtOrigin=1000000;path.segments.push_back({0,1,0,1000000});asset.paths.push_back(path);
    for(unsigned i=0;i<2;++i){
        OriginalGroundState s;s.position={float(i*100),0,2.5f};s.velocity={0,300,0};s.normal=s.previousNormal=s.boardNormal=s.presentationUp=s.boardUp={0,0,1};s.forward=s.physicalForward={0,1,0};s.lateral={-1,0,0};s.depth1=1;s.depth3=2;
        riders[i].reset({}, {0,1,0},0);riders[i].seedOriginalGround(profile,s);
        OriginalRaceParticipant participant;participant.human=i==0;participant.progress.pathIndex=0;participant.progress.remaining=participant.progress.bestRemaining=1000000;asset.participants.push_back(participant);
    }
    RiderWorld world(asset,flat,flat);std::array<bool,2> local{},worldPose{};std::vector<std::pair<int,int>> visits;
    world.refreshProximity=[&](RiderWorld& w){assert(w.clock.totalTicks==338&&w.clock.raceTicks==159);for(auto& r:riders)assert(r.framePhase()==RiderFramePhase::Idle);};
    for(size_t i=0;i<2;++i){
        RiderPoseStages poses;
        poses.prepareLocal=[&,i](const PrototypeRider&,double){for(auto& r:riders)assert(int(r.framePhase())>=int(RiderFramePhase::Motion));local[i]=true;return true;};
        poses.finishWorld=[&,i](const PrototypeRider& r,double)->std::optional<BodyCollisionVolume>{assert(local[0]&&local[1]);worldPose[i]=true;BodyCollisionVolume b;b.broadCenterCm=OriginalAirState::fromNative(r.position,r.velocity).position;return b;};
        world.addRider(i,riders[i],[&](RiderWorld&,size_t){for(auto& r:riders)assert(int(r.framePhase())>=int(RiderFramePhase::Started));return RiderInput{};},poses);
    }
    unsigned interactions=0;
    world.resolveInteractions=[&](RiderWorld& w,size_t i){
        assert(worldPose[0]&&worldPose[1]);++interactions;
        if(i==0){assert(riders[1].framePhase()==RiderFramePhase::WorldPose);auto b=OriginalAirState::fromNative(riders[1].position,riders[1].velocity);w.actor(1).rider->applyWorldContact(b.position,{0,777,0},false);}
        else assert(riders[1].originalGroundState().velocity[1]==777);
    };
    world.observePhase=[&](RiderWorld&,size_t i,RiderFramePhase p){visits.push_back({int(p),int(i)});};
    world.tick();assert(interactions==2);assert(world.clock.totalTicks==339&&world.clock.raceTicks==159);
    assert(riders[0].elapsed==1./60&&riders[1].elapsed==1./60);assert(riders[1].originalGroundState().velocity[1]==777);
    assert(!world.frameActive()&&riders[0].framePhase()==RiderFramePhase::Idle&&riders[1].framePhase()==RiderFramePhase::Idle);
    assert(visits.size()==2*(riderFramePhases.size()+1));
    for(size_t i=0;i<visits.size();i+=2){assert(visits[i].second==0&&visits[i+1].second==1&&visits[i].first==visits[i+1].first);}
    bool rejected=false;try{riders[0].runFramePhase(RiderFramePhase::Motion);}catch(const std::exception&){rejected=true;}assert(rejected);
    RiderWorld incomplete(asset,flat,flat);rejected=false;try{incomplete.tick();}catch(const std::exception&){rejected=true;}assert(rejected&&incomplete.clock.totalTicks==338);
    // The installed original pair dispatcher uses generated current bodies,
    // translates both actors once, and commits reciprocal source timestamps.
    std::array<PrototypeRider,2> contacting;
    RiderWorld pairWorld(asset,flat,flat);
    for(unsigned i=0;i<2;++i){
        auto state=riders[i].originalGroundState();state.position={float(i*10),0,2.5f};state.velocity={0,300,0};
        contacting[i].reset({}, {0,1,0},0);contacting[i].seedOriginalGround(profile,state);
        RiderPoseStages pose{[](const PrototypeRider&,double){return true;},[](const PrototypeRider& r,double)->std::optional<BodyCollisionVolume>{
            BodyCollisionVolume b;b.count=1;b.broadRadiusCm=100;b.broadCenterCm=OriginalAirState::fromNative(r.position,r.velocity).position;
            b.spheres[0].centerCm=b.broadCenterCm;b.spheres[0].radiusCm=25;b.landingCenterCm=terrain_original::Vector{123,0,0};return b;}};
        pairWorld.addRider(i,contacting[i],[](RiderWorld&,size_t){return RiderInput{};},pose);
    }
    pairWorld.installPairs([](RiderWorld&,unsigned slot){OriginalPairActorView view;view.kind880=slot==0?7:0;view.weightAttribute=100;return view;},
        [](RiderWorld&,unsigned,unsigned,const OriginalPairReactionRequest&,const CollisionRandom&){throw std::runtime_error("Unexpected reaction in equal-velocity pair fixture");});
    pairWorld.tick();
    auto pa=OriginalAirState::fromNative(contacting[0].position,contacting[0].velocity),pb=OriginalAirState::fromNative(contacting[1].position,contacting[1].velocity);
    // Original VU reciprocal makes the unit direction one ULP below1.
    assert(pa.position[0]==std::bit_cast<float>(0xc1afffffu)&&pb.position[0]==std::bit_cast<float>(0x41ffffffu));
    assert(contacting[0].lastBodyVolume->spheres[0].centerCm[0]==pa.position[0]&&contacting[1].lastBodyVolume->spheres[0].centerCm[0]==pb.position[0]);
    assert(contacting[0].lastBodyVolume->landingCenterCm->at(0)==123);
    const auto& pairRecords=pairWorld.pairSystem()->records();assert(pairRecords[0][1].lastContactTick==338&&pairRecords[1][0].lastContactTick==338);
    // An actor can remain in the same source-float position while a child
    // sphere crosses an exponent boundary. Reconstructing delta from rounded
    // actor positions would silently drop the AA0 translation.
    PrototypeRider precise;precise.reset({655.360078125,0,0},{0,1,0},0);
    precise.beginFrame({},flat,flat);
    for(auto phase:riderFramePhases){if(phase==RiderFramePhase::MotionContacts)break;precise.runFramePhase(phase);}
    BodyCollisionVolume body;body.count=1;body.broadCenterCm={65535.99609375f,0,0};body.spheres[0].centerCm=body.broadCenterCm;body.landingCenterCm=terrain_original::Vector{7,8,9};
    precise.lastBodyVolume=body;auto before=OriginalAirState::fromNative(precise.position,precise.velocity);
    precise.translateWorldContact({.004f,0,0});auto after=OriginalAirState::fromNative(precise.position,precise.velocity);
    assert(before.position[0]==after.position[0]);assert(precise.lastBodyVolume->spheres[0].centerCm[0]==65536.f);
    assert(precise.lastBodyVolume->landingCenterCm==body.landingCenterCm);
    // A contact enters soft control after posing. Completion is read from the
    // live animation interface, and normal control does not execute on the
    // same tick that soft recovery changes its state ID back to0.
    PrototypeRider soft;soft.reset({}, {0,1,0},0);auto softState=riders[0].originalGroundState();softState.controlState=0;softState.prewindStyle=0;softState.velocity={0,1600,0};
    soft.seedOriginalGround(profile,softState);bool complete=false;unsigned gains=0,normalUpdates=0;
    soft.setControllerCallbacks({[&](PrototypeRider&){++normalUpdates;},[&](PrototypeRider&){++gains;},{}});
    RiderPoseStages softPoses;softPoses.mainAnimation=[&](){return std::optional<BodyAnimationState>{{55,6,0,complete}};};
    soft.beginFrame({},flat,flat,nullptr,nullptr,&softPoses);
    for(auto phase:riderFramePhases){if(phase==RiderFramePhase::MotionContacts)soft.enterSoftCollision(55,0,false,false);soft.runFramePhase(phase);}
    assert(soft.currentControlState()==3);normalUpdates=0;
    RiderInput attemptingJump;attemptingJump.jumpHeld=attemptingJump.jumpPressed=true;
    soft.advance(1./60,attemptingJump,flat,flat,nullptr,nullptr,&softPoses);
    assert(soft.currentControlState()==3&&soft.grounded&&normalUpdates==0&&soft.originalGroundState().animationIndex==55);
    complete=true;soft.advance(1./60,{},flat,flat,nullptr,nullptr,&softPoses);
    assert(soft.currentControlState()==0&&gains==1&&normalUpdates==0&&soft.originalGroundState().animationIndex==55);
    soft.advance(1./60,{},flat,flat,nullptr,nullptr,&softPoses);assert(normalUpdates==1&&soft.originalGroundState().animationIndex!=55);
    std::puts("Shared frame: roster barriers, local/world pose separation, cross-rider mutation order and one global clock verified");
}
