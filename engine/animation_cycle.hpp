#pragma once
#include <array>
#include <cstdint>
namespace ssx {
struct OriginalAnimationCyclePair {
    std::array<uint32_t,2> clips{};
    std::array<float,2> times{},durations{},weights{1,0},rates{1,1};
    float sequenceRate=1;bool completed=false;
};
// Original103E28. Choices are [HS2,HS1,forward,TS1,TS2], ordered by104178.
void originalFiveWayAnimationStep(OriginalAnimationCyclePair&,const std::array<uint32_t,5>& choices,const std::array<float,5>& durations,float amount,float timeScale=1);
}
namespace ssx {
// Original103CC8; choices [negative,center,positive].
void originalThreeWayAnimationStep(OriginalAnimationCyclePair&,const std::array<uint32_t,3>& choices,const std::array<float,3>& durations,float amount,float timeScale=1);
}
