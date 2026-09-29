#include "air_control.hpp"
#include "air_motion.hpp"
#include "riding.hpp"
#include <bit>
#include <cassert>
#include <cfenv>
#include <cstdio>
namespace {
float f(unsigned b){return std::bit_cast<float>(b);}
std::array<float,3> v(unsigned x,unsigned y,unsigned z){return {f(x),f(y),f(z)};}
void same(float actual,unsigned expected){assert(std::bit_cast<unsigned>(actual)==expected);}
void same(std::array<float,3> actual,std::array<float,3> expected){for(int i=0;i<3;i++)assert(actual[i]==expected[i]);}
}
int main(){
    // Actual PCSX2 baseline snow-jam-air-isolated EE SHA256:
    // dc494bd8de22d74c1147a87470af22fceedd9dae72f10e6c75d66d33802e3fb6
    ssx::OriginalAirState initial{v(0xc8049805,0x464dfada,0xc85be1ac),v(0xc5091ea4,0xc3b5af2d,0x447a0000)};
    ssx::OriginalAirPresentation physical{initial.position,{f(0x3dc1cf15),f(0x3db5d241),f(0xbf424dfd),f(0xbf2355b7)}};
    ssx::OriginalAirControlProfile profile;profile.trickStat=f(0x3dba2e8c); // nearest scalar1/11
    auto state=ssx::originalAirControlBegin(0,0);
    ssx::PrototypeRider rider;rider.reset({0,0,0},{0,1,0},0);rider.grounded=false;
    rider.position=initial.nativePosition();rider.velocity=initial.nativeVelocity();
    rider.seedOriginalAirControl(profile,state,physical,{-11.37656498f,-.380158812f,56.00214386f});
    ssx::CollisionWorld empty({});ssx::RiderInput input;
    // Original accepted-command SHA256:
    // ea1cbd91b55297a543e4cf738846fbed96c047fe09940527a1d076fb4f2879cb
    // One neutral command followed by29 fullpositiveSpin commands.
    for(int frame=0;frame<30;frame++){
        input.spin=frame?f(0x3f7fffff):0;
        auto ticks=rider.airControlTicks;rider.advance(1./120,input,empty,empty);assert(rider.airControlTicks==ticks);
        rider.advance(1./120,input,empty,empty);
    }
    // Actual PCSX2 snow-jam-air-spin-30 EE SHA256:
    // 28a485bacc12f43f76501476a416ec267c05cb05de68c8a899a5bf664a74b448
    const auto& actual=rider.originalAirControlState();
    assert(actual.mode==0&&actual.phase==1&&actual.extended==0);
    same(actual.targetSpin,0x40490fdc);same(actual.progressSpin,0x401184ff);same(actual.totalSpin,0x401184ff);
    same(actual.scoredSpin,0x401184ff);same(actual.maxSpin,0x408ffd3a);same(actual.maxFlip,0x403ffc4e);
    same(actual.holdSpin,0x3eddddd8);same(actual.spinRate,0x4096899a);
    for(float zero:{actual.targetFlip,actual.progressFlip,actual.totalFlip,actual.adjustSpin,actual.adjustFlip,
                    actual.scoredFlip,actual.axisBlend,actual.holdFlip,actual.inputAngle,actual.idleTime,actual.flipRate})assert(zero==0);
    auto motion=ssx::OriginalAirState::fromNative(rider.position,rider.velocity);
    same(motion.position,v(0xc8059d5a,0x464b461f,0xc85b7e4c));same(motion.velocity,v(0xc4f819ab,0xc3a45df8,0x440fbff6));
    assert(rider.airControlTicks==30&&rider.airborneTicks==30&&rider.hasOriginalAirPresentation());
    assert(rider.airPhysicalOrientationPending); // This checkpoint test has not supplied a native trajectory world query.
    auto cached=rider.originalAirPresentationPose();
    for(int i=0;i<100;i++){const auto& repeated=rider.originalAirPresentationPose();assert(repeated.quaternion==cached.quaternion&&repeated.position==cached.position);}
    // Rendering/cache reads cannot advance the source axis-blend state.
    state.axisBlend=.2f;state.totalSpin=state.totalFlip=1;state.spinRate=state.flipRate=5;
    rider.seedOriginalAirControl(profile,state,physical,{0,0,50});
    for(int i=0;i<100;i++)(void)rider.originalAirPresentationPose();assert(rider.originalAirControlState().axisBlend==.2f);
    input.grabMask=1;rider.advance(1./60,input,empty,empty);
    assert(!rider.airAngularSupported&&!rider.hasOriginalAirPresentation()&&rider.airControlTicks==0);
    // Integrating the recovered predictor must retain the independently captured
    // flight translation and 60Hz cadence. A clear synthetic world isolates its
    // state/cache plumbing; actual terrain query parity has a separate oracle.
    for(bool complete:{true,false}){
        ssx::PrototypeRider flight;flight.reset({0,0,0},{0,1,0},0);flight.grounded=false;
        flight.position=initial.nativePosition();flight.velocity=initial.nativeVelocity();
        flight.seedOriginalAirControl(profile,ssx::originalAirControlBegin(0,0),physical,{0,0,0});
        ssx::OriginalAirTrajectory trajectory;trajectory.begin(initial);ssx::OriginalAirAlignmentContext alignment;
        flight.seedOriginalAirTrajectory(trajectory,alignment);unsigned queries=0;
        flight.bindOriginalAirTrajectoryQuery([&](auto,auto,int){++queries;ssx::OriginalAirTrajectoryHit hit;hit.complete=complete;return hit;},[](int){return 0;});
        for(int tick=0;tick<30;tick++){
            flight.advance(1./120,{},empty,empty);assert(flight.airborneTicks==uint64_t(tick));
            flight.advance(1./120,{},empty,empty);
        }
        auto actual=ssx::OriginalAirState::fromNative(flight.position,flight.velocity);
        same(actual.position,v(0xc8059d5a,0x464b461f,0xc85b7e4c));same(actual.velocity,v(0xc4f819ab,0xc3a45df8,0x440fbff6));
        assert(queries>0);assert(flight.airPhysicalOrientationPending==!complete);
        if(complete){assert(flight.originalAirTrajectoryState().elapsed>0);assert(flight.originalAirPhysicalQuaternion()==physical.quaternion);}
        else{assert(queries==1);assert(flight.originalAirTrajectoryState().elapsed==0);}
    }
    // Original111538 enters control5 during release, without redispatching the
    // newly entered handler that tick. Common1211F8 still advances all triplets.
    {
        ssx::PrototypeRider jump;jump.reset({0,0,0},{0,1,0},0);
        ssx::OriginalGroundProfile groundProfile;groundProfile.speedLimit=3333.33349609375f;
        ssx::OriginalGroundState groundState;groundState.position={0,0,100};groundState.velocity={1000,0,0};
        groundState.normal=groundState.previousNormal=groundState.boardUp={0,0,1};groundState.forward=groundState.physicalForward={1,0,0};groundState.crouch.current=.8f;groundState.controlState=2;
        jump.seedOriginalGround(groundProfile,groundState);
        ssx::OriginalJumpState launch;launch.position=groundState.position;launch.velocity=groundState.velocity;launch.normal=launch.takeoffNormal={0,0,1};launch.forward={1,0,0};launch.speedLimit=groundProfile.speedLimit;launch.charge=.8f;
        jump.seedOriginalJump(launch,true);ssx::OriginalAirPrewindState prewind;prewind.spin={.7f,0,.7f};
        jump.seedOriginalAirEntry(profile,prewind,{0,0,50});jump.lastBodyVolume=ssx::BodyCollisionVolume{};
        jump.advance(1./60,{},empty,empty);
        assert(jump.hasOriginalAirControl()&&jump.airAngularSupported&&jump.airControlTicks==0);
        assert(jump.originalAirControlState().mode==1&&jump.originalAirControlState().progressSpin==0);
        assert(!jump.lastBodyVolume&&jump.bodyCollisionMissingPose);
        float retainedRate=jump.originalGroundState().crouch.rate;assert(retainedRate>0);
        for(int i=0;i<12;i++){jump.advance(1./60,{},empty,empty);assert(jump.originalGroundState().crouch.rate==retainedRate);}
        assert(jump.airControlTicks==12&&jump.originalGroundState().crouch.current==0&&jump.jumpCharge==0);
    }
    std::puts("Originalairspin30: allangularstate and translation floats match livePCSX2;60Hz/cachedpose/unsupportedgrabguard verified");
}
