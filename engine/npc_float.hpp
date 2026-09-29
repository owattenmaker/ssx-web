#pragma once
// Scalar operations of the NPC provider leaves (engine/npc_input.cpp, engine/npc_air.cpp). The
// native build runs them under hardware FE_TOWARDZERO (the leaves' Round guard), where these are the
// plain operators; WebAssembly has no rounding modes, and OriginalRounding's emulated policy makes
// terrain_original's helpers use the software toward-zero path instead. int->float conversion also
// follows the mode (EE cvt.s.w under the chop policy).
#include "terrain_contact_math.hpp"
#include <cmath>
#include <cstdint>
namespace ssx {
inline float npcAdd(float a,float b){return terrain_original::add(a,b);}
inline float npcSub(float a,float b){return terrain_original::sub(a,b);}
inline float npcMul(float a,float b){return terrain_original::mul(a,b);}
inline float npcDiv(float a,float b){return terrain_original::div(a,b);}
inline float npcSqrt(float a){return terrain_original::sqrt(a);}
inline float npcIntToFloat(int32_t value){
#if defined(__EMSCRIPTEN__)
 if(originalRoundingMode==FE_TOWARDZERO){const double exact=value;float nearest=float(exact);if(std::abs(double(nearest))>std::abs(exact))nearest=std::nextafter(nearest,0.f);return nearest;}
#endif
 volatile int32_t input=value;return float(input);
}
}
