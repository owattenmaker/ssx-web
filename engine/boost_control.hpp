#pragma once
#include <cstdint>
namespace ssx {
struct OriginalBoostProfile {float fullThreshold=0,mediumThreshold=0,drainPerTick=0;
    float tickSeconds=0,modifierThreshold=0,superTimerFloor=0,normalDecay=0,fastDecay=0;};
struct OriginalBoostState {
    float meter=0,amount=0,window=0;
    int32_t tier=0,drainEnabled=0;
    uint8_t feedbackFlags=0; // motion-owner+D27, 2F6AC8 request bit4.
    float modifier=0,superTime=0; // rider2EC/2F0.
};
struct OriginalBoostEffects {bool started=false,denied=false,stopped=false,pressed=false,timerExpired=false;};
// Complete114130 arithmetic and feedback request; manager callbacks are
// returned for native event/audio integration rather than executed as guest code.
OriginalBoostEffects originalBoostTick(OriginalBoostState&,const OriginalBoostProfile&,float timeScale,int motionMode,int controlState);
OriginalBoostEffects originalBoostControl(OriginalBoostState&,const OriginalBoostProfile&,bool held,bool pressed);
}
