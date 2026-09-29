#pragma once
#include "animation_sequence.hpp"
#include <span>
namespace ssx {
struct OriginalAnimationEventMarker {float time=0;unsigned bit=0;bool end=false;};
struct OriginalAnimationEventFlags {uint64_t latched=0,raised=0;bool completed=false,seekPending=false;};
// Full primary3135B0 clock/event behavior, including reverse and multi-wrap
// intervals. Markers are authored frame times converted with original float1/30.
void originalAnimationPrimaryStep(OriginalAnimationSlot&,float sequenceRate,float dt,
    OriginalAnimationEventFlags&,std::span<const OriginalAnimationEventMarker>);
}
