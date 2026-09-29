#pragma once
#include "air_entry.hpp"
#include <functional>
#include <optional>
namespace ssx {
struct OriginalPassiveAirState {
    float entryAngle=0,entryMagnitude=0;
    int32_t upperLatch=1,identityLatch=1,lastIdentity=-1;
};
struct OriginalPassiveAirCommand {
    float turn=0,crouch=0;
    int32_t identity=-1;
    bool recover=false,handplant=false,upper14=false,upper15=false;
};
// Command0 run-length bits must already be removed. Identity is signed8-bit;
// the original latch compares it directly rather than interpreting a grab mask.
OriginalPassiveAirCommand originalDecodePassiveAirCommand(uint32_t word0,uint32_t word1);
struct OriginalPassiveAirAnimation {int semantic=0,animationClass=0;};
struct OriginalPassiveAirAccess {
    std::function<bool(bool requested)> recover;
    std::function<bool(bool bit14,bool bit15)> upper;
    std::function<bool(bool backwards)> handplant;
    std::function<bool()> rail;
    std::function<void()> stopBoost;
    std::function<OriginalPassiveAirAnimation()> mainAnimation;
    std::function<void(int semantic,float blend,uint32_t flags)> requestAnimation;
    std::function<void(int control)> requestControl;
};
struct OriginalPassiveAirResult {
    enum class Stop {None,Recovery,Upper,Handplant,Rail,Control5};
    Stop stop=Stop::None;
    bool targetsWritten=false,stopBoostRequested=false;
    std::optional<int> animationSemantic;
};
//12F620 writes only this state and prewind rates/targets; current values survive.
void originalPassiveAirBegin(OriginalPassiveAirState&,const OriginalGroundState&,OriginalAirPrewindState&);
//Complete12F730 controller logic. External gameplay callbacks are explicit and
//ordered. Missing required callbacks fail rather than fabricate world misses.
OriginalPassiveAirResult originalPassiveAirStep(OriginalPassiveAirState&,OriginalGroundState&,
    const OriginalPassiveAirCommand&,const OriginalPassiveAirAccess&);
struct OriginalPassiveAirExit {
    bool fade=false,setUpperRate=false;
    int fadeChannel=0,rateChannel=1;
    float fadeSeconds=.1f,upperRate=1;
};
//12FB68. The fade intentionally targets0 while the rate targets1.
OriginalPassiveAirExit originalPassiveAirLeave(bool activeExit,int upperAnimationClass);
}
