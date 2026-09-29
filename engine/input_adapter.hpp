#pragma once
#include <array>
#include <cstdint>
namespace ssx {
struct InputSample;
// The GameController API supplies normalized coordinates. This native adapter
// quantizes -1..1 to the nearest byte on the symmetric0..255 PS2 range.
uint8_t originalDeviceAxisByte(float normalized);
// PS2 movie/controller order: two active-low flag bytes, RX/RY/LX/LY,12pressures.
// Returned sticks have already passed the original driver's axial response.
InputSample originalReplayInput(const std::array<uint8_t,18>& movieBytes);
}
