#pragma once
#include <array>
#include <cstdint>
#include <stdexcept>
namespace ssx {
//13BFF0..13C05C: temporary rider+AA0 active sphere mask for105398.
//13C078 then sets3 for13C140, and13C094 restoresFFFFFFFF for107888.
inline uint32_t originalRailExitBodyMask(int control,int identity){
 if(control!=12)return 0x16;
 if(identity<0||identity>=4)throw std::runtime_error("Invalid original rail Uber exit identity");
 constexpr std::array<uint32_t,4> masks={0x141,0x16,2,2};return masks[identity];
}
}

namespace ssx {
//13BFF0..13C09C. Callbacks operate on the same live body/actor so effects of
//the first query are visible to the next. The final mask is FFFFFFFF, not the
//incoming mask. Actual contact response/rebuild implementations are external.
template<class TriggerQuery,class PhysicalQuery,class Rebuild>
void originalRailExitContactPhases(uint32_t& activeMask,int control,int identity,
        TriggerQuery&& trigger,PhysicalQuery&& physical,Rebuild&& rebuild){
 activeMask=originalRailExitBodyMask(control,identity);trigger();
 activeMask=3;physical();
 activeMask=0xffffffffu;rebuild();
}
}
