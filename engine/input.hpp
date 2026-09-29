#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include "input_adapter.hpp"

namespace ssx {
// Physical positions are stable across Xbox and PlayStation labels.
enum class Button : unsigned { South, West, East, North, L1, L2, R1, R2, L3, R3, Pause, Select, Count };
constexpr uint32_t bit(Button b) { return 1u << unsigned(b); }
enum class StickDomain { DeviceNormalized, OriginalResponse };
struct InputSample {
    float leftX=0, leftY=0, rightX=0, rightY=0, dpadX=0, dpadY=0;
    uint32_t buttons=0;
    StickDomain stickDomain=StickDomain::DeviceNormalized;
};
struct RiderInput {
    float turn=0, crouch=0, brake=0, boardPress=0, boardPivot=0;
    float prewindTurn=0, spin=0, flip=0, airAdjustLR=0, airAdjustFB=0;
    bool jumpHeld=false, jumpPressed=false, boostHeld=false, boostPressed=false;
    bool handplant=false, ollieHeld=false, recoverPressed=false, pausePressed=false;
    uint8_t grabMask=0; // L1, L2, R1, R2 stay independent; all 16 combinations survive.
    int passiveInputCode=-1;
    bool passiveUpper14=false,passiveUpper15=false;
    // Additional 0x127998 provider fields (original_input_provider.hpp). The
    // mapper above never sets them; decoded original words do.
    bool attackLeft=false,attackRight=false,tweak=false,lateSpin=false,wipeoutRecover=false;
    float railSpin=0,railBalance=0,handplantBalance=0,gateAnticipate=0;
    int railUberIdentity=-1;
};
struct Bindings {
    Button jump=Button::South, boost=Button::West, handplant=Button::East;
    std::array<Button,4> grabs={Button::L1,Button::L2,Button::R1,Button::R2};
};
class InputMapper {
    uint32_t previous=0;
public:
    Bindings bindings;
    void reset() { previous=0; }
    RiderInput update(const InputSample& sample);
};
}
