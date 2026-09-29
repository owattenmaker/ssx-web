#pragma once
#include "input.hpp"
namespace ssx {
// Decode original recorder words using the controller that is actually running.
// Duration bits must already be removed. Unsupported action/state branches
// throw instead of silently treating another controller's payload as neutral.
RiderInput originalDecodeCommand(int controlState,uint32_t word0,uint32_t word1);
}
