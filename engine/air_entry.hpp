#pragma once
#include "air_control.hpp"
#include "ground_motion.hpp"
namespace ssx {
struct OriginalAirPrewindState {GroundControlValue spin,flip;float jumpGate=0;};
struct OriginalAirPrewindContext {
    int style=0,animationClass=0,animationIndex=0;
    float manualSpin=0;
    bool reverseTurnTriggered=false; // Original114CC0; its physical effect is external.
};
// Original held-control2 prewind target/rate branches12EBE0..12EDCC.
// Returns false when the original requests the separate reverse-turn animation.
bool originalAirPrewindTargets(OriginalAirPrewindState&,float spin,float flip,const OriginalAirPrewindContext&);
// The common1211F8 pass approaches these triplets in every rider motion mode.
void originalAirPrewindApproach(OriginalAirPrewindState&);
// Original12EA30 before jump/control5 entry: snap current filtered prewind.
OriginalAirControlState originalAirControlRelease(OriginalAirPrewindState&);
}
namespace ssx {
bool originalAirReverseTurnRequired(float brake,std::array<float,3> velocity,std::array<float,3> forward);
// Original12EB20..12EB44 requests once, then1211F8 keeps approaching in air.
void originalAirReleaseGroundTargets(const OriginalGroundProfile&,OriginalGroundState&,std::array<float,3> launchVelocity);
}

namespace ssx { bool originalCrouchRequest(float& jumpGate,bool held,bool pressed); }

namespace ssx {
struct OriginalAirExitState {
    GroundControlValue adjustment28C,adjustment298;
    OriginalAirPresentation physical;
    std::array<float,3> right,forward,up;
};
// Original134CB0 tail: quantize landing spin and clear flip rate.
void originalAirExitRates(OriginalAirControlState&);
// Original134CB0 exit: fades channel1 externally, resets prewind, explicitly
// advances134DD0 once, bakes its pose and normalizes11E098, then quantizes2DC.
void originalAirControlExit(OriginalAirControlState&,OriginalAirPrewindState&,
    OriginalAirExitState&,std::array<float,3> pivot);
}

namespace ssx {
struct OriginalReverseTurnResult {bool reversed=false;std::array<float,4> animationRootQuaternion{0,0,0,1};};
//114CC0 and115168 physical/control state; caller applies311B48's animation
// alignment event and the returned root quaternion, with root translation0.
OriginalReverseTurnResult originalReverseTurn(OriginalGroundState&,GroundControlValue& balance280);
}
