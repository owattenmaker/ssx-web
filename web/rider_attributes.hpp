#pragma once
#include "rider_local.hpp"
// Rider attribute stats (career Buy Attributes -> gameplay), ported from the original getters.
// Every getter reads the rider's progress byte k from the runtime attribute bank 0x535538 (+bank*70+char*7),
// integer-divides it by 5 and divides (div.s) by the character maximum byte 0x5308D8+char*15+8+k (11 for every
// rider on the disc), unless the rider is flagged by 0x1477E8 (then 0.5) or has a level override (+0xB34).
//   k0 top speed  0x1494C0 -> 0x11B3F8 speed limit, 0x13CCF0
//   k1 accel      0x1493D8 -> 0x13C948, 0x13CCF0
//   k2 tricks     0x149690 -> 0x120038 grab playback rate (0x10E098 calls it without using the value)
//   k3 edging     0x148D80 / 0x148E68 -> 0x13CCF0, 0x13D028, 0x13D818
//   k4 spin       0x1495A8 -> 0x133128 / 0x133308 air control, 0x135CB0, 0x135DB0
//   k5 toughness  0x148F50 / 0x149038 -> 0x11FF98 collision weight, 0x107888 pair dispatch
//   k6 stability  0x149120 / 0x149208 -> 0x139C88 landing, 0x13AF28 rails
// The browser seeds (captured level-1 riders) hold 1/11 in every field; until set_rider_attributes() is called
// nothing is patched, so default play stays bit-identical to the captures.
#include "../engine/original_float.hpp"
#include <array>
#include <cstdint>
RIDER_LOCAL inline std::array<int32_t,7> browserAttributeRaw{5,5,5,5,5,5,5};
RIDER_LOCAL inline int32_t browserAttributeMaximum=11,browserAttributeOverride=0;
RIDER_LOCAL inline bool browserAttributesSet=false;
inline float browserAttributeStat(int k){
    const int32_t numerator=browserAttributeOverride>0?browserAttributeOverride:browserAttributeRaw[k]/5;
    return ssx::originalScalarDivide(float(numerator),float(browserAttributeMaximum));
}
void browser_apply_ground_attributes();     // web/attribute_bridge.cpp (core profiles)
void browser_apply_animation_attributes();  // web/animation_bridge.cpp (air control / grab statics)
