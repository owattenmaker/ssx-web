#pragma once
#include "ground_motion.hpp"
#include <cstdint>
namespace ssx {
struct OriginalSoftControlInput {
    float turn=0,balance=0;
    bool boostPressed=false,boostHeld=false,recovery=false;
};
OriginalSoftControlInput originalSoftControlInput(uint32_t command0); //12E778 fields
struct OriginalSoftControlContext {
    int motionMode=0;
    bool mainSequenceComplete=false; //312AE8 -> current main sequence+C0
    bool obstructionHandled=false; //106848 already handled an external world transition
};
struct OriginalSoftControlResult {
    enum class Stop {None,ModeGate,Recovery,Obstruction};
    Stop stop=Stop::None;
    int nextControl=-1,nextMotion=-1;
    bool requestBoost=false,boostHeld=false,boostPressed=false;
    bool requestBalance=false,restoreStance=false,resetRail=false,clearBoostWindows=false;
    bool normalGainFocus=false;
    float balance=0;
};
//12E778 updates targets only;1211F8 approaches them later. External recovery,
// rail restoration1326C8 and stance restoration115640 are explicit requests.
OriginalSoftControlResult originalSoftControlStep(const OriginalGroundProfile&,
    OriginalGroundState&,const OriginalSoftControlContext&,const OriginalSoftControlInput&);
}
