#pragma once
#include "input.hpp"
#include <array>
#include <functional>
namespace ssx {
struct OriginalAirControlState {
    // Motion owner+0x230. Field names preserve the verified angular roles.
    int mode=0;                          // +0; prewind continuation mode1
    int phase=3;                         // +0x0C
    float targetFlip=0,targetSpin=0;      // +0x10/+0x14
    float progressFlip=0,progressSpin=0;  // +0x18/+0x1C
    float totalSpin=0,totalFlip=0;        // +0x20/+0x24
    float adjustSpin=0,adjustFlip=0;      // +0x28/+0x2C
    float scoredSpin=0,scoredFlip=0;      // +0x30/+0x34 (event accumulation only)
    float maxSpin=0,maxFlip=0;            // +0x38/+0x3C
    float axisBlend=0;                   // +0x40
    int extended=0;                      // +0x44
    float holdSpin=0,holdFlip=0,inputAngle=0,idleTime=0; // +48/+4C/+50/+54
    float spinRate=0,flipRate=0;          // Rider+0x2DC/+0x2E0
};
struct OriginalAirControlProfile {
    float trickStat=0; // Original1495A8 stat(4,12), supplied by roster/snapshot.
    bool boostModifier=false; // Rider+0x2EC>0.
    bool landingAnimation=false; // Channel-2 request 0x120 (288, the in-flight stance switch clip), read through 312AA0.
    bool grabLifecycleResolved=false,grabActive=false; //1352A8 result, supplied by recovered grab lifecycle.
    // 0x135BE0 in-flight stance switch (engine/air_switch.hpp): called in phase 3 before the D-pad
    // input (a switch skips it) and in an unfinished phase 2, both only without a grab or a 288
    // request. It may rewrite the angular fields. Empty = never switches.
    std::function<bool(OriginalAirControlState&)> airSwitch;
};
// Original133128 airborne control entry; consumes filtered rider2A4/2B0.
OriginalAirControlState originalAirControlBegin(float prewindSpin,float prewindFlip);
// Original1158B8 direction snap. Returned pair is flip,spin.
std::array<float,2> originalAirDirection(float flip,float spin,float angularStep,float deadzone);
struct OriginalAirControlFrame {int phaseBefore=3;float effectiveSpin=0,effectiveFlip=0;bool trickStarted=false; /*13371C: a phase-3 D-pad trick start; 133734 then calls 119C98*/};
// Verified no-contact/no-grab spin/flip/air-adjust stages13366C..134334.
// Grab animation/state interruption and landing response are separate systems.
void originalAirControlStep(OriginalAirControlState&,const RiderInput&,const OriginalAirControlProfile&,OriginalAirControlFrame* output=nullptr);
struct OriginalAirPresentation {
    std::array<float,3> position;
    std::array<float,4> quaternion;
};
// Original134DD0/135180 physical->presentation transform. Pivot is the original
// scaled skeleton pivot in centimeters; it is not a guessed board origin.
// Reconstruct a cached/current pose without advancing mutable blend timing.
OriginalAirPresentation originalAirPresentationCurrent(const OriginalAirControlState&,
    OriginalAirPresentation physical,std::array<float,3> pivot);
OriginalAirPresentation originalAirPresentation(OriginalAirControlState&,
    OriginalAirPresentation physical,std::array<float,3> pivot);
}
