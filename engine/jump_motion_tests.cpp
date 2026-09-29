#include "jump_motion.hpp"
#include "air_motion.hpp"
#include "riding.hpp"
#include <bit>
#include <cassert>
#include <cfenv>
#include <cstdio>
namespace {
float f(uint32_t b){return std::bit_cast<float>(b);}
std::array<float,3> v(uint32_t x,uint32_t y,uint32_t z){return {f(x),f(y),f(z)};}
void same(std::array<float,3> actual,std::array<float,3> expected){for(int i=0;i<3;i++)assert(std::bit_cast<uint32_t>(actual[i])==std::bit_cast<uint32_t>(expected[i]));}
ssx::OriginalJumpState fixture30(){
    ssx::OriginalJumpState s;
    // PCSX2 snow-jam-jump-30 EE SHA256:
    // 9bf9a696de85acd1a352cf5b36346664b845f42672dbcb7d5a21c902e8df7ed4
    s.position=v(0xc8048ed9,0x464e1329,0xc860c2d8);
    s.velocity=v(0xc4e65084,0xc3a5ac3f,0xc43a7817);
    s.normal=v(0xbec09967,0xbc92bee0,0x3f6d2712);
    s.takeoffNormal=v(0xbebeff47,0xbc9a0336,0x3f6d78c8);
    s.forward=v(0xbf697a42,0xbe2d4398,0xbebf4a31);
    s.charge=f(0x3f7626b5);s.speedLimit=f(0x4510cf31);
    s.ticksSinceGroundFocus=368-201;s.flags=0x49;
    return s;
}
const auto position31=v(0xc8049805,0x464dfada,0xc860c3ac);
const auto velocity31=v(0xc5091ea4,0xc3b5af2d,0xc366946e);
}
int main(){
    // Measured next-frame PCSX2 EE SHA256:
    // 0bc4f890ce1c27234f2f557909e2b1cb573da3dd3cd9737b0e86262fabd6c2ff
    for(int rounding:{FE_TONEAREST,FE_UPWARD}){
        std::fesetround(rounding);
        auto state=fixture30();ssx::originalJumpTakeoff(state);
        assert(std::fegetround()==rounding);
        ssx::OriginalAirState air{state.position,state.velocity};air.step();
        same(air.position,position31);same(air.velocity,velocity31);
        assert(std::fegetround()==rounding);
    }
    std::fesetround(FE_TONEAREST);
    // The seeded controller must retain frame-boundary normals and release before
    // grounded integration. Empty collision worlds isolate the verified takeoff.
    ssx::PrototypeRider rider;ssx::CollisionWorld empty({});auto source=fixture30();
    ssx::OriginalAirState initial{source.position,source.velocity};
    rider.reset({0,0,0},{0,1,0},0);rider.position=initial.nativePosition();rider.velocity=initial.nativeVelocity();
    rider.seedOriginalJump(source,true);ssx::RiderInput released;
    rider.advance(1./60,released,empty,empty);
    auto actual=ssx::OriginalAirState::fromNative(rider.position,rider.velocity);
    same(actual.position,position31);same(actual.velocity,velocity31);
    assert(!rider.grounded&&rider.airborneTicks==1);
    // Measured charge30->31 filter transition, after the release impulse.
    assert(std::bit_cast<uint32_t>(float(rider.jumpCharge))==0x3f5d891fu);
    std::puts("Original jump30->31: all6 position/velocity floats and charge match PCSX2, including native controller release ordering");
}
