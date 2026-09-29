#pragma once
#include "collision.hpp"
#include <array>

namespace ssx {
// Recovered original trajectory step0x1139A0, called by airborne motion0x139A20.
// State stays in original Z-up float centimeters for reproducible rounding.
// It contains no console CPU/runtime dependency. Contact/launch rules are separate.
struct OriginalAirState {
    std::array<float,3> position{},velocity{};
    static OriginalAirState fromNative(Vec3 position,Vec3 velocity);
    Vec3 nativePosition() const;
    Vec3 nativeVelocity() const;
    // Exactly one original60Hz step. Returns whether the source speed cap ran.
    bool step(float maximumSpeed=3333.33349609375f,float* resultingSpeed=nullptr);
};
}
