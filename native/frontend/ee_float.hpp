#pragma once
// EE scalar FPU arithmetic (web/ee-scalar-float.js, PCSX2 semantics of the recompiled originals): MUL / ADD / SUB round toward zero
// (chop), ADD / SUB keep one guard bit, DIV.S rounds to nearest; the VU's ADD / SUB (one chop, no guard bit); SQRT.S nearest.
#include <cmath>
#include <cstdint>
#include <cstring>

namespace ssx_native {

namespace ee {
inline uint32_t bits(float x) {
    uint32_t b;
    std::memcpy(&b, &x, 4);
    return b;
}
inline float fromBits(uint32_t b) {
    float f;
    std::memcpy(&f, &b, 4);
    return f;
}
// an exactly representable double rounded toward zero to float32
inline float chop(double exact) {
    float r = float(exact);
    if (r != 0 && std::abs(double(r)) > std::abs(exact)) {
        r = fromBits(bits(r) - 1);
    }
    return r;
}
inline float mul(float a, float b) {
    return chop(double(a) * double(b));
}
inline float addSub(float a, float b, bool subtract) {
    uint32_t x = bits(a);
    uint32_t y = bits(b);
    const int d = int((x >> 23) & 255) - int((y >> 23) & 255);
    if (d >= 25) {
        y &= 0x80000000u;
    } else if (d <= -25) {
        x &= 0x80000000u;
    } else if (d > 0) {
        y &= 0xffffffffu << (d - 1);
    } else if (d < 0) {
        x &= 0xffffffffu << (-d - 1);
    }
    const double l = fromBits(x);
    const double r = fromBits(y);
    return chop(subtract ? l - r : l + r);
}
inline float add(float a, float b) {
    return addSub(a, b, false);
}
inline float sub(float a, float b) {
    return addSub(a, b, true);
}
// the correctly rounded float32 quotient (DIV.S nearest)
inline float div(float a, float b) {
    const double q = double(a) / double(b);
    const float r = float(q);
    if (!std::isfinite(q) || double(r) == q) {
        return r;
    }
    const float other = fromBits(bits(r) + (std::abs(double(r)) > std::abs(q) ? uint32_t(-1) : 1u));
    if ((double(r) + double(other)) / 2 != q) {
        return r;
    }
    const double rest = (double(a) - q * double(b)) / double(b);
    if (rest > 0) {
        return r > other ? r : other;
    }
    if (rest < 0) {
        return r < other ? r : other;
    }
    return (bits(r) & 1) ? other : r;
}
// VU (and terrain_original) ADD / SUB: one chop rounding of the exact sum, no guard bit (TwoSum gives the exact error)
inline float vuAddSub(float a, float b, bool subtract) {
    const double x = a;
    const double y = subtract ? -double(b) : double(b);
    const double s = x + y;
    const double bb = s - x;
    const double err = (x - (s - bb)) + (y - bb);
    float r = float(s);
    if (r == 0) {
        return r;
    }
    const double over = (double(r) - s) - err;
    if (over != 0 && ((over > 0) == (r > 0))) {
        r = fromBits(bits(r) - 1);
    }
    return r;
}
inline float vuAdd(float a, float b) {
    return vuAddSub(a, b, false);
}
inline float vuSub(float a, float b) {
    return vuAddSub(a, b, true);
}
// SQRT.S rounds to nearest
inline float sqrtNearest(float x) {
    return float(std::sqrt(double(x)));
}
}  // namespace ee

}  // namespace ssx_native
