#pragma once
#include "original_float.hpp"
namespace ssx {
//10E770/10E7D0 update rider+2E8/+2EC before requesting feedback.
inline float originalPickupCounterAward(float counter,float amount){OriginalRounding round;return originalScalarAdd(counter,amount);}
}
