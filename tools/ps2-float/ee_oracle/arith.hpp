#pragma once
// The two arithmetic profiles the EE oracle runs (docs/ps2-float.md "EE oracle"):
//   exact  the console (engine/ps2_fpu.hpp), what ARMSX2's clamp mode 4 and its interpreters compute;
//   mode1  ARMSX2's clamp mode 1, the profile today's gates were captured in, through the port's own mode-1 helpers
//          (engine/software_float.hpp, engine/original_float.hpp): EE add / sub guard-masked and chopped, EE DIV / SQRT
//          rounded to nearest, every multiply plainly chopped, VU add / sub chopped with no guard mask, VU DIV / SQRT
//          chopped. Results that would overflow clamp to +-FLT_MAX. The flags of mode1 are approximate (Z and S exact).
#include "ps2_fpu.hpp"
#include "original_float.hpp"

#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>

namespace oracle_arith {

using ssx::ps2fpu::Result;

inline bool mode1 = false;

inline float asFloat(uint32_t bits) {
    return std::bit_cast<float>(bits);
}

inline uint32_t asBits(float value) {
    return std::bit_cast<uint32_t>(value);
}

inline Result mode1Result(uint32_t bits, bool operandsNonzero) {
    const bool zero = (bits & 0x7FFFFFFFu) == 0;
    return {bits, (bits & 0x7FFFFFFFu) == 0x7F7FFFFFu, zero && operandsNonzero};
}

inline uint32_t eeAdd(uint32_t a, uint32_t b, bool subtract) {
    if (!mode1) {
        return ssx::ps2fpu::addSubResult(a, b, subtract).bits;
    }
    return asBits(ssx::originalScalarAddSub(asFloat(a), asFloat(b), subtract));
}

inline uint32_t eeMul(uint32_t a, uint32_t b) {
    if (!mode1) {
        return ssx::ps2fpu::mulBits(a, b);
    }
    return asBits(ssx::software_float::mul(asFloat(a), asFloat(b)));
}

inline uint32_t zeroDivisorMode1(uint32_t a, uint32_t b) {
    return ((a ^ b) & 0x80000000u) | 0x7F7FFFFFu;
}

inline uint32_t eeDiv(uint32_t a, uint32_t b) {
    if (!mode1) {
        return ssx::ps2fpu::divBits(a, b);
    }
    if ((b & 0x7F800000u) == 0) {
        return zeroDivisorMode1(a, b);
    }
    return asBits(ssx::originalScalarDivide(asFloat(a), asFloat(b)));
}

inline uint32_t eeSqrt(uint32_t t) {
    if (!mode1) {
        return ssx::ps2fpu::sqrtBits(t);
    }
    return asBits(ssx::originalScalarSqrt(asFloat(t & 0x7FFFFFFFu)));
}

inline uint32_t eeRsqrt(uint32_t a, uint32_t t) {
    if (!mode1) {
        return ssx::ps2fpu::rsqrtBits(a, t);
    }
    if ((t & 0x7F800000u) == 0) {
        return (a & 0x80000000u) | 0x7F7FFFFFu;
    }
    return eeDiv(a, eeSqrt(t));
}

inline uint32_t eeMac(uint32_t acc, uint32_t a, uint32_t b, bool subtract) {
    if (!mode1) {
        return ssx::ps2fpu::eeMulAccumulateBits(acc, a, b, subtract);
    }
    return eeAdd(acc, eeMul(a, b), subtract);
}

inline bool nonzero(uint32_t a) {
    return (a & 0x7F800000u) != 0;
}

inline Result vuAddSub(uint32_t a, uint32_t b, bool subtract) {
    if (!mode1) {
        return ssx::ps2fpu::addSubResult(a, b, subtract);
    }
    const float x = asFloat(a);
    const float y = asFloat(b);
    const uint32_t bits = asBits(subtract ? ssx::software_float::sub(x, y) : ssx::software_float::add(x, y));
    return mode1Result(bits, nonzero(a) && nonzero(b) && a != (b ^ (subtract ? 0u : 0x80000000u)));
}

inline Result vuMul(uint32_t a, uint32_t b) {
    if (!mode1) {
        return ssx::ps2fpu::mulResult(a, b);
    }
    return mode1Result(asBits(ssx::software_float::mul(asFloat(a), asFloat(b))), nonzero(a) && nonzero(b));
}

inline Result vuMac(uint32_t acc, uint32_t a, uint32_t b, bool subtract) {
    const Result product = vuMul(a, b);
    if (product.overflow && !mode1) {
        return {product.bits ^ (subtract ? 0x80000000u : 0u), true, false};
    }
    return vuAddSub(acc, product.bits, subtract);
}

inline uint32_t vuDiv(uint32_t a, uint32_t b) {
    if (!mode1) {
        return ssx::ps2fpu::divBits(a, b);
    }
    if ((b & 0x7F800000u) == 0) {
        return zeroDivisorMode1(a, b);
    }
    return asBits(ssx::software_float::div(asFloat(a), asFloat(b)));
}

inline uint32_t vuSqrt(uint32_t t) {
    if (!mode1) {
        return ssx::ps2fpu::sqrtBits(t);
    }
    return asBits(ssx::software_float::sqrt(asFloat(t & 0x7FFFFFFFu)));
}

inline uint32_t vuRsqrt(uint32_t a, uint32_t t) {
    if (!mode1) {
        return ssx::ps2fpu::rsqrtBits(a, t);
    }
    if ((t & 0x7F800000u) == 0) {
        return (a & 0x80000000u) | 0x7F7FFFFFu;
    }
    return vuDiv(a, vuSqrt(t));
}

}
