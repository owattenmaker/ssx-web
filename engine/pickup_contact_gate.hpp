#pragma once
#include <cstdint>
namespace ssx {
//355770: predicate runs before cooldown is checked; accepted contacts replace
// the counter with half the clock rate before forwarding to34FE00.
inline bool originalPickupContactGate(int32_t& cooldown,int32_t clockRate,bool hasComponent,bool componentAccepts){
 if(hasComponent&&!componentAccepts)return false;
 if(cooldown>0)return false;
 cooldown=clockRate/2;
 return true;
}
}
