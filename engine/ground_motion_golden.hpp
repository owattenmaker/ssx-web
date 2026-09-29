#pragma once
#include "ground_motion.hpp"
#include <type_traits>
namespace ssx {
struct GroundGoldenCase {
    OriginalGroundProfile profile;
    OriginalGroundState initial;
    std::array<float,3> position,velocity;
    float depth1,depth3,distance;
};
static_assert(std::is_trivially_copyable_v<GroundGoldenCase>);
inline constexpr uint32_t GroundGoldenMagic=0x47524e44,GroundGoldenVersion=1,GroundGoldenCount=32;
}
