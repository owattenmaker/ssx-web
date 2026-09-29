#pragma once
#include <array>
#include <cstdint>
namespace ssx {
// Original PS2 pad-read payload: status/type, active-low button bytes, RX/RY/LX/LY,
// then Right/Left/Up/Down/Triangle/Circle/Cross/Square/L1/R1/L2/R2 pressures.
using OriginalPadPacket=std::array<uint8_t,20>;
using OriginalPadValues=std::array<float,24>;
OriginalPadValues originalDecodePad(const OriginalPadPacket&,bool pressureMode=true,bool analogMode=true);
struct OriginalButtonState {
    float value=0;
    uint32_t pressed=0,released=0,held=0,repeat=0,repeatTimer=0,edgeAge=0;
};
using OriginalPadState=std::array<OriginalButtonState,24>;
// 0x321298. Call once per actual input sample consumed, not per renderer frame.
void originalUpdatePad(OriginalPadState&,const OriginalPadValues&);
// 0x127998 packs axis values into signed six-bit fields; 0x131620 decodes
// using the exact original reciprocal31. This is additional to stick deadzone.
float originalQuantizeAxis(float value);
}
