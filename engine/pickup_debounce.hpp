#pragma once
#include <cstdint>
namespace ssx {
//342D88: zero/negative initial counts do not request completion.
inline bool originalPickupDebounceTick(int32_t& ticks){if(ticks<=0)return false;--ticks;return ticks==0;}
}
