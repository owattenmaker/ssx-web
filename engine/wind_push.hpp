#pragma once
// Original SSX3 (PS2 USA, SLUS_207.72) rider wind push 0x125970, called first by the ground motion 0x13D818 and the air
// motion 0x139A20 (docs/weather.md). The wind is the rider's own Weather painter block (world painter type 12,
// tWPIGD_Weather, class 0x484058): 0x2EF0A0 = property 4 (+0x28, wind speed km/h), 0x2EE570 = property 3 (+0x20, wind
// direction in degrees), through the environment block 0x4FA370 + rider+0x86C * 0xF0 (+0x20 wrapper). The painter is
// stepped by 0x2ED490 in the rider pass (0x1218D0) after the motion, so a tick's push reads the previous tick's values.
//   gate rider+0x874 (the human flag, set by the human rider constructor 0x125C70; computer riders never push)
//   speed = min(kmh * 27.777779, 1388.8889) (50 km/h cap); nothing below 277.77777 (10 km/h)
//   i = trunc(deg * 0.017453294 * 81.48733) & 0x1FF; dir = (sin[i + 0x80], sin[i], 0, 0) (sine table 0x504FB8)
//   f = speed - dot(v, dir) (VU dot, EE sub.s); nothing unless 0 < f
//   dv = dir * f * 0.015 -> rider vtable +0x20 = 0x1250A8: v += dv (VU add; the host restarts the air predictor 0x1135B8
//   when 11FE98 == 1).
// Float policy as engine/one_way_volume.hpp: EE mul.s terrain_original::mul, EE sub.s originalScalarSubtract, VU lanes
// terrain_original add/mul under OriginalRounding.
#include "flag_cloth.hpp"
#include "one_way_volume.hpp"
#include <array>
#include <bit>
#include <cstdint>
#include <optional>

namespace ssx {
namespace wind_push {
inline const float kCap=std::bit_cast<float>(0x44ad9c72u);      // gp-0x7878 1388.8889 cm/s
inline const float kKmh=std::bit_cast<float>(0x41de38e4u);      // gp-0x7874 27.777779
inline const float kGate=std::bit_cast<float>(0x438ae38eu);     // gp-0x7870 277.77777 cm/s
inline const float kRadians=std::bit_cast<float>(0x3c8efa36u);  // gp-0x786C 0.017453294
inline const float kTable=std::bit_cast<float>(0x42a2f983u);    // gp-0x7868 81.48733 (512 / 2 pi)
inline const float kGain=std::bit_cast<float>(0x3c75c290u);     // gp-0x7864 0.015
// EE cvt.w.s: truncation toward zero, saturating.
inline int32_t cvtWS(float x){if(x!=x)return 0x7FFFFFFF;if(x>=2147483648.f)return 0x7FFFFFFF;if(x<=-2147483648.f)return int32_t(0x80000000u);return int32_t(x);}
}
struct OriginalWindDirection {BoostQuad dir{};int32_t index=0;};
inline OriginalWindDirection originalWindDirection(float degrees){
    using namespace terrain_original;OriginalRounding rounding;
    const float a=mul(mul(degrees,wind_push::kRadians),wind_push::kTable);
    OriginalWindDirection d;d.index=wind_push::cvtWS(a)&0x1FF;
    d.dir={originalFlagSine(unsigned(d.index+0x80)),originalFlagSine(unsigned(d.index)),0.f,0.f};
    return d;
}
// The velocity change 0x125970 hands to 0x1250A8, or none (gate, speed below 10 km/h, already moving with the wind).
inline std::optional<BoostQuad> originalWindPush(float speedKmh,float directionDeg,const BoostQuad& velocity){
    using namespace terrain_original;OriginalRounding rounding;
    float speed=mul(speedKmh,wind_push::kKmh);
    if(wind_push::kCap<speed)speed=wind_push::kCap;
    if(!(wind_push::kGate<=speed))return std::nullopt;
    const auto d=originalWindDirection(directionDeg);
    const float f=originalScalarSubtract(speed,one_way_volume::vdot(velocity,d.dir));
    if(!(0.f<f))return std::nullopt;
    return one_way_volume::vmul(one_way_volume::vmul(d.dir,f),wind_push::kGain);
}
}
