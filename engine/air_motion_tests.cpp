#include "air_motion.hpp"
#include "riding.hpp"
#include <cassert>
#include <bit>
#include <cfenv>
#include <cstdio>
using Bits=std::array<uint32_t,6>;
static ssx::OriginalAirState decode(const Bits& bits) {
    ssx::OriginalAirState state;
    for (unsigned i=0;i<3;++i) {state.position[i]=std::bit_cast<float>(bits[i]);state.velocity[i]=std::bit_cast<float>(bits[i+3]);}
    return state;
}
static void same(const ssx::OriginalAirState& value,const Bits& bits) {
    for (unsigned i=0;i<3;++i) {
        assert(std::bit_cast<uint32_t>(value.position[i])==bits[i]);
        assert(std::bit_cast<uint32_t>(value.velocity[i])==bits[i+3]);
    }
}
// Numeric golden state values measured from original PCSX2 execution, not a
// second copy of the native algorithm. Source EE hashes identify the checkpoints.
// snow-jam-jump-31: 0bc4f890ce1c27234f2f557909e2b1cb573da3dd3cd9737b0e86262fabd6c2ff
static constexpr Bits golden0={0xc8049805u,0x464dfadau,0xc860c3acu,0xc5091ea4u,0xc3b5af2du,0xc366946eu};
// snow-jam-jump-60: eb4142e9717093176b22a9ff1b5bc77a5678e40c95200eb335e7ca25230bd9d6
static constexpr Bits golden1={0xc805950eu,0x464b5c1du,0xc861150bu,0xc4f8ee17u,0xc3a4eab3u,0xc48f9d31u};
// snow-jam-jump-90: cfa6e49a29257bada93424f8e3dfc58ee6bc14df74fbc9d27521018923a79ca5
static constexpr Bits golden2={0xc8068246u,0x4648e74bu,0xc861ddffu,0xc4e13412u,0xc395329du,0xc5032e93u};
// snow-jam-air-isolated: dc494bd8de22d74c1147a87470af22fceedd9dae72f10e6c75d66d33802e3fb6
static constexpr Bits golden3={0xc8049805u,0x464dfadau,0xc85be1acu,0xc5091ea4u,0xc3b5af2du,0x447a0000u};
// snow-jam-air-isolated-60: 5b01352a9c57e43eb6a0cc25c97b2fa4e843d77c4a7d4de9ced9268912149a27
static constexpr Bits golden4={0xc80689c7u,0x4648d366u,0xc85b500bu,0xc4e073e5u,0xc394b34cu,0x4315ffb8u};
// snow-jam-air-isolated-120: bc922f7c8fe55a6b682a5b87b8985c390fcb6b247d0659539784eac4ec8ac3ba
static constexpr Bits golden5={0xc8082122u,0x46449b77u,0xc85be8abu,0xc4b7b45cu,0xc37368c1u,0xc4c2affdu};
// snow-jam-air-capped: 7d4b53a0260ebfcc2f58d32721a3318655e94b435e09ed960d221b30c59177e6
static constexpr Bits golden6={0xc8049805u,0x464dfadau,0xc85be1acu,0xc5091ea4u,0xc3b5af2du,0x453b8000u};
// snow-jam-air-capped-1: 3ca989d9a9375406215cb9050391edf3069d58332fc595681421670e7545a58b
static constexpr Bits golden7={0xc804a129u,0x464de2a0u,0xc85bd52cu,0xc4f502c7u,0xc3a25200u,0x45274862u};
int main() {
    for (int initialMode:{FE_TONEAREST,FE_UPWARD}) {
        assert(!std::fesetround(initialMode));
        auto falling=decode(golden0);
        for (int i=1;i<=59;++i) {assert(!falling.step());assert(std::fegetround()==initialMode);if (i==29)same(falling,golden1);}
        same(falling,golden2);
        auto rising=decode(golden3);
        for (int i=1;i<=120;++i) {assert(!rising.step());assert(std::fegetround()==initialMode);if (i==60)same(rising,golden4);}
        same(rising,golden5);
    }
    assert(!std::fesetround(FE_TONEAREST));
    // Exact60Hz source ticks survive the controller's120Hz stepping and meters API.
    ssx::CollisionWorld empty({});ssx::RiderInput input;
    auto seed=decode(golden3);
    auto native=ssx::OriginalAirState::fromNative(seed.nativePosition(),seed.nativeVelocity());same(native,golden3);
    ssx::PrototypeRider rider;rider.reset({0,0,0},{0,1,0},0);
    rider.position=seed.nativePosition();rider.velocity=seed.nativeVelocity();rider.grounded=false;
    for (int i=0;i<120;++i)rider.advance(1.0/60,input,empty,empty);
    same(ssx::OriginalAirState::fromNative(rider.position,rider.velocity),golden5);
    assert(rider.airborneTicks==120&&!rider.grounded);
    // A separate original-game state deliberately exceeds the33.33m/s cap.
    auto goldenCapped=decode(golden6);assert(goldenCapped.step());same(goldenCapped,golden7);
    // A second axis-aligned case checks bounding, without claiming golden coverage.
    auto capped=seed;capped.velocity={4000,0,0};assert(capped.step());
    assert(std::sqrt(ssx::dot(capped.nativeVelocity(),capped.nativeVelocity()))<33.334);
    std::puts("Original airborne: all6 floats bit-identical at29/59 falling and60/120 rising/apex steps; one live speed-cap case; rounding restored;60Hz controller integration passed");
}
