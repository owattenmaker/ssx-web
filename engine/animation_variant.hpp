#pragma once
#include <cstdint>
#include <functional>
#include <span>
namespace ssx {
struct OriginalAnimationVariant {uint32_t leaf=0,weight=0,allowedFlags=0;};
// Original311710. One authored choice bypasses eligibility and consumes no RNG.
// Multiple choices consume exactly one shared draw, even if only one is eligible.
uint32_t originalAnimationVariant(std::span<const OriginalAnimationVariant>,
    uint32_t requiredFlags,const std::function<uint32_t()>& random);
}
