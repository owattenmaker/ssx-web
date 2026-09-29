#pragma once
#include <array>
#include <cstdint>

namespace ssx {
// Original 0x113F88 + 0x12130C. Call once per original 60Hz input tick,
// after processing release; launch consumes the previous tick's charge.
float originalJumpChargeStep(float current, bool held);
struct OriginalJumpState {
    // Original Z-up centimeters and centimeters/second, not engine coordinates.
    std::array<float,3> position{}, velocity{}, normal{0,0,1}, forward{0,1,0};
    std::array<float,3> takeoffNormal{0,0,1}; // rider +0x380
    std::array<float,3> boardUp{0,0,1};       // rider +0x1C0 (mode 4 only)
    float speedLimit=0;                     // rider +0x2E4
    float charge=0;                        // negative = passive takeoff
    uint32_t ticksSinceGroundFocus=0;       // tick - motion owner +0x10
    uint32_t riderState=0;                 // rider +0x434
    uint16_t flags=0;                      // rider +0x2D4
    int motionMode=0;
    std::array<float,3> cameraWallNormal{}; // rider+3C0, horizontal takeoff normal (written on every takeoff).
    float cameraLaunchValue=0; // rider+5A4, camera takeoff amplitude input.
    // 114310..114320: a passive takeoff (charge<0) writes -1 to motion owner+0x14, so the
    // following ground exit 13F410 stores tick-500 and the landing keeps full speed.
    bool groundLeaveSentinel=false;
    // 114660: the positive-ramp blend branch sets s1, passed to 119E38 as the score object's +0x04
    // (alternate trick-identity table 43D4C8).
    bool rampTakeoff=false;
};
// Recovered full velocity/position arithmetic of 0x114298. Animation, sound,
// event notifications and mode transition belong to the caller.
float originalJumpCameraLaunch(std::array<float,3> velocity,std::array<float,3> normal);
void originalJumpTakeoff(OriginalJumpState& state);
}
