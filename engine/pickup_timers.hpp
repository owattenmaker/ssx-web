#pragma once
#include "pickup_debounce.hpp"
namespace ssx {
//3561A8..3561B4 precedes the component update (342D88 for Debounce).
inline void originalPickupContactCooldownTick(int32_t& ticks){if(ticks>0)--ticks;}
//Normal Debounce component path: its owner predicate did not request removal.
//True requests completion(reason1); the caller owns teardown/replacement.
inline bool originalPickupTimersTick(int32_t& contactCooldown,int32_t& debounce){
 originalPickupContactCooldownTick(contactCooldown);
 return originalPickupDebounceTick(debounce);
}
}
