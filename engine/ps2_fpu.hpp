#pragma once
// The PS2's EE FPU and VU FMAC / FDIV arithmetic, bit for bit, in integers (docs/ps2-float.md).
//
// The hardware model is the one ARMSX2 measured on an SCPH-90000 and ships as its console model
// (pcsx2/FPU.cpp, EeFpuModel.h at ARMSX2 2ecae98a2c, eeClampMode / vuClampMode 4), with the
// multiplier array from GitHubProUser67's PS2Float series. This is an independent integer
// implementation of the same rules, checked against the console rows and against that model
// (tools/ps2-float/, docs/ps2-float.md):
//   - a word with exponent field 0 reads as a signed zero (no denormals);
//   - exponent 255 is an ordinary binade: 0x7FFFFFFF = (2 - 2^-23) * 2^128 is the largest value,
//     and there are no infinities or NaNs;
//   - the adder keeps one guard bit: the operand with the smaller exponent loses its low
//     (|de| - 1) mantissa bits before an exact add, then the sum is chopped (toward zero);
//     an add / sub result below 2^-126 keeps its mantissa bits with exponent field 0;
//   - the multiplier chops, and its Booth array comes back one ULP low on some operand pairs;
//     which pairs depends on the operand order (ft is the recoded operand), so mul is not
//     commutative;
//   - divide and square root are a radix-2 SRT digit recurrence with no rounding step;
//     rsqrt is div(fs, sqrt(ft)) with an ordinary single in between;
//   - multiply-accumulate rounds twice (product, then the accumulate).
// The EE FPU and the VU FMAC are the same unit: VU0 macro and micro arithmetic use these too.
//
// Everything here works on the raw words in integers, or in double arithmetic whose results are exact (the fast
// paths, a square-root seed), so no host rounding mode, FTZ / DAZ or FMA contraction can change a result: native and
// WebAssembly give the same bits.
#include <bit>
#include <cstdint>

namespace ssx::ps2fpu {

// What one rounding step produced: the word, plus the two facts the flag registers need.
struct Result {
    uint32_t bits;
    // The exact result was 2^129 or more, so the word saturated to +-0x7FFFFFFF.
    bool overflow;
    // The exact result was nonzero and below 2^-126.
    bool underflow;
};

inline constexpr uint32_t signBit = 0x80000000u;
inline constexpr uint32_t magnitudeMask = 0x7FFFFFFFu;
inline constexpr uint32_t mantissaMask = 0x007FFFFFu;
inline constexpr uint32_t hiddenBit = 0x00800000u;
inline constexpr uint32_t maxMagnitude = 0x7FFFFFFFu;

inline constexpr uint32_t exponentOf(uint32_t word) {
    return (word >> 23) & 0xFFu;
}

inline constexpr uint32_t significandOf(uint32_t word) {
    return hiddenBit | (word & mantissaMask);
}

inline constexpr int bitLength(uint64_t value) {
    return 64 - std::countl_zero(value);
}

// ---- Adder -------------------------------------------------------------------------------

// The guard mask: the operand with the smaller exponent keeps one bit below the larger
// operand's last mantissa bit. Past 24 exponents apart it keeps only its sign.
inline constexpr void guardMask(uint32_t& a, uint32_t& b) {
    const int difference = int(exponentOf(a)) - int(exponentOf(b));
    if (difference >= 25) {
        b &= signBit;
    } else if (difference >= 2) {
        b &= 0xFFFFFFFFu << (difference - 1);
    } else if (difference <= -25) {
        a &= signBit;
    } else if (difference <= -2) {
        a &= 0xFFFFFFFFu << (-difference - 1);
    }
}

// The word for a chopped result: sign, biased exponent and a 24-bit significand.
// exponent >= 256 saturates; exponent <= 0 is the caller's underflow rule.
inline constexpr uint32_t packChopped(uint32_t sign, int exponent, uint32_t significand) {
    if (exponent >= 256) {
        return sign | maxMagnitude;
    }
    return sign | (uint32_t(exponent) << 23) | (significand & mantissaMask);
}

// a + b (or a - b) on the EE adder.
inline constexpr Result addSubResult(uint32_t a, uint32_t b, bool subtract) {
    guardMask(a, b);
    if (subtract) {
        b ^= signBit;
    }
    const uint32_t ea = exponentOf(a);
    const uint32_t eb = exponentOf(b);
    if (ea == 0 && eb == 0) {
        // Two zeros: -0 only when both are -0 (an exact zero sum is +0).
        return {(a & b & signBit), false, false};
    }
    if (eb == 0) {
        return {a, false, false};
    }
    if (ea == 0) {
        return {b, false, false};
    }
    // Both nonzero and, after the mask, within 24 exponents: the larger shifted up by the
    // difference and the smaller sum exactly in 49 bits at the smaller operand's scale.
    const uint32_t low = ea < eb ? ea : eb;
    const uint64_t ma = uint64_t(significandOf(a)) << (ea - low);
    const uint64_t mb = uint64_t(significandOf(b)) << (eb - low);
    const bool negativeA = (a & signBit) != 0;
    const bool negativeB = (b & signBit) != 0;
    uint64_t magnitude = 0;
    uint32_t sign = 0;
    if (negativeA == negativeB) {
        magnitude = ma + mb;
        sign = a & signBit;
    } else if (ma >= mb) {
        magnitude = ma - mb;
        sign = a & signBit;
    } else {
        magnitude = mb - ma;
        sign = b & signBit;
    }
    if (magnitude == 0) {
        return {0, false, false};
    }
    // The value is magnitude * 2^(low - 150); its leading bit sits at bitLength - 1.
    const int length = bitLength(magnitude);
    const int exponent = length + int(low) - 24;
    const uint32_t significand = length > 24 ? uint32_t(magnitude >> (length - 24)) : uint32_t(magnitude << (24 - length));
    if (exponent >= 256) {
        return {sign | maxMagnitude, true, false};
    }
    if (exponent <= 0) {
        // No denormal path: the normalised mantissa comes out with the exponent field forced to 0.
        return {sign | (significand & mantissaMask), false, true};
    }
    return {packChopped(sign, exponent, significand), false, false};
}

inline constexpr uint32_t addBits(uint32_t a, uint32_t b) {
    return addSubResult(a, b, false).bits;
}

inline constexpr uint32_t subBits(uint32_t a, uint32_t b) {
    return addSubResult(a, b, true).bits;
}

// ---- Multiplier --------------------------------------------------------------------------

// The 3-bit Booth window that selects digit `digit` of b: 0 and 7 select zero, 1 and 2 select +a,
// 3 selects +2a, 4 selects -2a, 5 and 6 select -a.
inline constexpr uint32_t boothWindow(uint32_t b, uint32_t digit) {
    return (digit ? b >> (digit * 2 - 1) : b << 1) & 7u;
}

// Digit `digit`'s partial product of a, 32 bits wide (no column above 31 reaches the decision at
// column 15). A negative digit is left as a one's complement; its +1 is boothCorrection().
inline constexpr uint32_t boothPartial(uint32_t a, uint32_t b, uint32_t digit) {
    const uint32_t window = boothWindow(b, digit);
    uint32_t partial = a << (digit * 2);
    if (window == 3 || window == 4) {
        partial += partial;
    }
    if (window >= 4 && window <= 6) {
        partial ^= 0u - (1u << (digit * 2));
    }
    return (window >= 1 && window <= 6) ? partial : 0u;
}

// The +1 a negative digit owes. Digits 0..4 never get theirs: their columns are not built.
inline constexpr uint32_t boothCorrection(uint32_t b, uint32_t digit) {
    const uint32_t window = boothWindow(b, digit);
    return (window >= 4 && window <= 6) ? (1u << (digit * 2)) : 0u;
}

// One 3:2 carry-save row: the sum bits, and the carry bits into `carry`.
inline constexpr uint32_t carrySave(uint32_t a, uint32_t b, uint32_t c, uint32_t& carry) {
    const uint32_t partial = a ^ b;
    carry = ((partial & c) | (a & b)) << 1;
    return partial ^ c;
}

// The significand product as the console's array forms it: the exact product, less 2^15 where
// the truncated low columns come up short at column 15.
inline constexpr uint64_t multiplyArray(uint32_t a, uint32_t b) {
    const uint64_t exact = uint64_t(a) * uint64_t(b);
    const uint32_t p0 = boothPartial(a, b, 0);
    const uint32_t p1 = boothPartial(a, b, 1);
    const uint32_t p2 = boothPartial(a, b, 2);
    const uint32_t p3 = boothPartial(a, b, 3);
    const uint32_t p4 = boothPartial(a, b, 4);
    const uint32_t p5 = boothPartial(a, b, 5);
    const uint32_t p6 = boothPartial(a, b, 6);
    const uint32_t p7 = boothPartial(a, b, 7);
    uint32_t carry0 = 0;
    uint32_t carry1 = 0;
    uint32_t carry2 = 0;
    uint32_t carry3 = 0;
    uint32_t carry4 = 0;
    uint32_t carry5 = 0;
    const uint32_t sum0 = carrySave(p1, p2, p3, carry0);
    const uint32_t sum1 = carrySave(p4 & ~0x7FFu, p5 & ~0xFFFu, p6, carry1);
    // Digit 5's two surviving low bits and the corrections digits 5 and 6 still receive ride on
    // rows they did not originate in.
    const uint32_t high1 = carry1 | boothCorrection(b, 6) | (p5 & 0x800u);
    const uint32_t row7 = p7 | ((p5 & 0x400u) + boothCorrection(b, 5));
    const uint32_t sum2 = carrySave(p0, sum0, carry0, carry2);
    const uint32_t sum3 = carrySave(row7, sum1, high1, carry3);
    const uint32_t sum4 = carrySave(carry2, sum3, carry3, carry4);
    const uint32_t sum5 = carrySave(sum2, sum4, carry4, carry5);
    const uint32_t low = sum5 & ~0x7FFFu;
    const uint32_t high = (carry5 + boothCorrection(b, 7)) & ~0x7FFFu;
    return exact - ((uint64_t((low + high) ^ uint32_t(exact))) & 0x8000u);
}

// Whether the array's loss reaches the result: one ULP low. Needs the exact product's tail below
// the ULP to be under 2^15, which leaves the array running on ~0.3% of random operand pairs.
inline constexpr bool multiplyOneUlpLow(uint32_t fs, uint32_t ft) {
    if (exponentOf(fs) == 0 || exponentOf(ft) == 0) {
        return false;
    }
    const uint32_t a = significandOf(fs);
    const uint32_t b = significandOf(ft);
    const uint64_t exact = uint64_t(a) * uint64_t(b);
    const int shift = (exact >> 47) ? 24 : 23;
    if ((exact & ((uint64_t(1) << shift) - 1u)) >= 0x8000u) {
        return false;
    }
    return (exact >> shift) != (multiplyArray(a, b) >> shift);
}

// fs * ft on the EE multiplier (operand order matters: ft is the Booth-recoded operand).
inline constexpr Result mulResult(uint32_t fs, uint32_t ft) {
    const uint32_t sign = (fs ^ ft) & signBit;
    if (exponentOf(fs) == 0 || exponentOf(ft) == 0) {
        return {sign, false, false};
    }
    const uint64_t product = uint64_t(significandOf(fs)) * uint64_t(significandOf(ft));
    const bool carry = (product >> 47) != 0;
    const int shift = carry ? 24 : 23;
    const uint32_t significand = uint32_t(product >> shift);
    const bool tail = (product & ((uint64_t(1) << shift) - 1u)) != 0;
    const int exponent = int(exponentOf(fs)) + int(exponentOf(ft)) - 127 + (carry ? 1 : 0);
    if (exponent >= 256) {
        return {sign | maxMagnitude, true, false};
    }
    if (exponent <= 0) {
        return {sign, false, true};
    }
    const uint32_t word = packChopped(sign, exponent, significand);
    // The deficit is measured only inside the range: a product past 0x7FFFFFFF that chops back
    // onto it, and a decrement that would leave the normals, are left alone.
    if (exponent == 255 && significand == 0xFFFFFFu && tail) {
        return {word, false, false};
    }
    if ((word & magnitudeMask) == hiddenBit) {
        return {word, false, false};
    }
    if (multiplyOneUlpLow(fs, ft)) {
        return {word - 1u, false, false};
    }
    return {word, false, false};
}

inline constexpr uint32_t mulBits(uint32_t fs, uint32_t ft) {
    return mulResult(fs, ft).bits;
}

// The exact product was above 0x7FFFFFFF (EE MADD's test: then the accumulate never runs).
inline constexpr bool productAboveMax(uint32_t fs, uint32_t ft) {
    if (exponentOf(fs) == 0 || exponentOf(ft) == 0) {
        return false;
    }
    const uint64_t product = uint64_t(significandOf(fs)) * uint64_t(significandOf(ft));
    const bool carry = (product >> 47) != 0;
    const int shift = carry ? 24 : 23;
    const int exponent = int(exponentOf(fs)) + int(exponentOf(ft)) - 127 + (carry ? 1 : 0);
    if (exponent >= 256) {
        return true;
    }
    const bool tail = (product & ((uint64_t(1) << shift) - 1u)) != 0;
    return exponent == 255 && uint32_t(product >> shift) == 0xFFFFFFu && tail;
}

// EE MADD.S / MSUB.S / MADDA.S / MSUBA.S: acc +- fs * ft in two roundings. A product above the
// range ends the instruction there, saturated (the accumulator is never read).
inline constexpr uint32_t eeMulAccumulateBits(uint32_t acc, uint32_t fs, uint32_t ft, bool subtract) {
    const uint32_t product = mulBits(fs, ft) ^ (subtract ? signBit : 0u);
    if (productAboveMax(fs, ft)) {
        return product;
    }
    return addBits(acc, product);
}

// VU MADD / MSUB (and the A forms, OPMSUB): the same two roundings; an overflowing product
// (2^129 or more) ends the instruction saturated.
inline constexpr uint32_t vuMulAccumulateBits(uint32_t acc, uint32_t fs, uint32_t ft, bool subtract) {
    const Result product = mulResult(fs, ft);
    if (product.overflow) {
        return product.bits ^ (subtract ? signBit : 0u);
    }
    return addSubResult(acc, product.bits, subtract).bits;
}

// ---- Divide / square-root unit -------------------------------------------------------------

// The partial remainder in the redundant carry-save form the recurrence carries it in.
struct SrtRemainder {
    uint32_t sum;
    uint32_t carry;
};

// The digit as two all-ones masks, one per sign; both zero for the digit 0.
struct SrtDigit {
    uint32_t plus;
    uint32_t minus;
};

inline constexpr uint32_t srtDigitValue(SrtDigit digit) {
    return digit.minus - digit.plus;
}

inline constexpr SrtRemainder srtCarrySave(uint32_t a, uint32_t b, uint32_t c) {
    const uint32_t partial = a ^ b;
    const uint32_t carry = (a & b) | (partial & c);
    return {partial ^ c, carry << 1};
}

// The selection function. It assimilates the redundant remainder only partly (the carry word is
// added above bit 23, the low 24 sum bits are OR-ed back), with thresholds +2^23 and -2^24.
inline constexpr SrtDigit srtSelect(SrtRemainder remainder) {
    constexpr uint32_t lowMask = (1u << 24) - 1u;
    const int32_t estimate = int32_t(((remainder.sum & ~lowMask) + remainder.carry) | (remainder.sum & lowMask));
    const uint32_t plus = 0u - uint32_t(estimate >= (1 << 23));
    const uint32_t minus = 0u - uint32_t(estimate < int32_t(~0u << 24));
    return {plus, minus};
}

// On a zero digit the next digit is selected from the pair before recompression, while the state
// advances with the recompressed pair.
inline constexpr SrtRemainder srtSelectionInput(SrtRemainder current, SrtRemainder next, SrtDigit digit) {
    const uint32_t taken = digit.plus | digit.minus;
    return {(current.sum & ~taken) | (next.sum & taken), (current.carry & ~taken) | (next.carry & taken)};
}

// The quotient of two 24-bit significands as 25 digits (25 bits when a >= b, 24 when not).
inline constexpr uint32_t divideSignificandRecurrence(uint32_t a, uint32_t b) {
    const uint32_t divisor = b << 2;
    const uint32_t negatedDivisor = ~divisor;
    SrtRemainder remainder = {a << 2, 0};
    uint32_t quotient = 0;
    SrtDigit digit = {~0u, 0};
    for (int step = 0; step < 24; ++step) {
        quotient = (quotient << 1) + srtDigitValue(digit);
        const uint32_t addend = (negatedDivisor & digit.plus) | (divisor & digit.minus);
        const SrtRemainder current = {remainder.sum, remainder.carry - digit.plus};
        const SrtRemainder next = srtCarrySave(current.sum, current.carry, addend);
        digit = srtSelect(srtSelectionInput(current, next, digit));
        remainder.sum = next.sum << 1;
        remainder.carry = next.carry << 1;
    }
    return (quotient << 1) + srtDigitValue(digit);
}

// The recurrence returns T or T + 1 (T the truncated quotient). Where the exact quotient is far
// enough below T + 1 the answer is T without running a digit; elsewhere the recurrence decides.
// tools/ps2-float checks the shortcut against the recurrence exhaustively.
inline constexpr uint32_t divideSignificand(uint32_t a, uint32_t b) {
    const uint32_t below = a < b ? 1u : 0u;
    const uint64_t wide = (uint64_t(a) << 24) / b;
    const uint32_t truncated = below ? uint32_t(wide) : uint32_t(wide >> 1);
    const uint32_t remainder = uint32_t((uint64_t(a) << (23 + below)) - uint64_t(truncated) * b);
    const uint32_t cap = below ? ((b > (3u << 22)) ? b - (1u << 22) : (1u << 23)) : (1u << 22);
    if (b - remainder > cap) {
        return below ? truncated : (truncated << 1);
    }
    return divideSignificandRecurrence(a, b);
}

// a / b for a nonzero divisor (exponent field != 0). The caller answers a zero divisor.
inline constexpr uint32_t divideNonzero(uint32_t a, uint32_t b) {
    const uint32_t sign = (a ^ b) & signBit;
    if (exponentOf(a) == 0) {
        return sign;
    }
    uint32_t quotient = divideSignificand(significandOf(a), significandOf(b));
    int exponent = int(exponentOf(a)) - int(exponentOf(b)) + 126;
    if (quotient >= (1u << 24)) {
        quotient >>= 1;
        ++exponent;
    }
    if (exponent > 255) {
        return sign | maxMagnitude;
    }
    if (exponent < 1) {
        return sign;
    }
    return sign | (uint32_t(exponent) << 23) | (quotient & mantissaMask);
}

// floor(sqrt(x)) for x below 2^48. x converts to a double exactly and its square root is correctly rounded, so the
// truncated seed is within one of the answer under any rounding mode; the two corrections settle it.
inline uint32_t integerSqrt48(uint64_t x) {
    uint64_t root = uint64_t(__builtin_sqrt(double(x)));
    while (root > 0 && root * root > x) {
        --root;
    }
    while ((root + 1) * (root + 1) <= x) {
        ++root;
    }
    return uint32_t(root);
}

// The square-root recurrence on a placed radicand: the root so far feeds back in place of a
// divisor. Same selector and carry-save state as the divide.
inline constexpr uint32_t sqrtSignificandRecurrence(uint32_t placed) {
    SrtRemainder remainder = {placed, 0};
    uint32_t root = 0;
    SrtDigit digit = {~0u, 0};
    for (int step = 0; step < 24; ++step) {
        const uint32_t weight = 1u << (24 - step);
        const uint32_t basePlus = root + weight;
        const uint32_t baseMinus = root - weight;
        const uint32_t rootPlus = basePlus + weight;
        const uint32_t rootMinus = baseMinus - weight;
        const uint32_t addend = (~basePlus & digit.plus) | (baseMinus & digit.minus);
        const uint32_t taken = digit.plus | digit.minus;
        root = (rootPlus & digit.plus) | (rootMinus & digit.minus) | (root & ~taken);
        const SrtRemainder current = {remainder.sum, remainder.carry - digit.plus};
        const SrtRemainder next = srtCarrySave(current.sum, current.carry, addend);
        digit = srtSelect(srtSelectionInput(current, next, digit));
        remainder.sum = next.sum << 1;
        remainder.carry = next.carry << 1;
    }
    // The last digit has weight 2^1, below the root's last bit: it only reaches the result by
    // borrowing out of it.
    root += srtDigitValue(digit) << 1;
    return (root >> 2) & 0xFFFFFFu;
}

inline constexpr uint32_t sqrtSignificand(uint32_t placed) {
    // The shortcut's frame places the radicand 22 bits higher than the recurrence does.
    const uint64_t x = uint64_t(placed) << 22;
    const uint32_t root = integerSqrt48(x);
    const uint64_t distance = 2ull * root + 1ull - (x - uint64_t(root) * root);
    if (distance > (1u << 23)) {
        return root;
    }
    return sqrtSignificandRecurrence(placed);
}

// sqrt(|t|): the sign is dropped, and zero / denormal inputs give +0.
inline constexpr uint32_t sqrtBits(uint32_t t) {
    const uint32_t exponent = exponentOf(t);
    if (exponent == 0) {
        return 0;
    }
    const uint32_t placed = significandOf(t) << ((exponent & 1u) ? 1 : 2);
    return (((exponent + 127u) >> 1) << 23) | (sqrtSignificand(placed) & mantissaMask);
}

// DIV.S / VDIV: a zero divisor (denormals are zero) saturates with the xor of the signs.
inline constexpr uint32_t divBits(uint32_t a, uint32_t b) {
    if (exponentOf(b) == 0) {
        return ((a ^ b) & signBit) | maxMagnitude;
    }
    return divideNonzero(a, b);
}

// RSQRT.S / VRSQRT: a / sqrt(|t|) through an ordinary single. A zero t saturates with a's sign.
inline constexpr uint32_t rsqrtBits(uint32_t a, uint32_t t) {
    if (exponentOf(t) == 0) {
        return (a & signBit) | maxMagnitude;
    }
    return divideNonzero(a, sqrtBits(t));
}

// ---- Moves, compares, conversions ----------------------------------------------------------

inline constexpr uint32_t absBits(uint32_t a) {
    return a & magnitudeMask;
}

inline constexpr uint32_t negBits(uint32_t a) {
    return a ^ signBit;
}

// MAX.S / MIN.S / VMAX / VMINI: integer order on the raw words (denormals are not flushed).
inline constexpr uint32_t maxBits(uint32_t a, uint32_t b) {
    const int32_t x = int32_t(a);
    const int32_t y = int32_t(b);
    if (x < 0 && y < 0) {
        return uint32_t(x < y ? x : y);
    }
    return uint32_t(x > y ? x : y);
}

inline constexpr uint32_t minBits(uint32_t a, uint32_t b) {
    const int32_t x = int32_t(a);
    const int32_t y = int32_t(b);
    if (x < 0 && y < 0) {
        return uint32_t(x > y ? x : y);
    }
    return uint32_t(x < y ? x : y);
}

// The EE value of a word as an ordered integer: zeros and denormals are 0, exponent 255 is ordinary.
inline constexpr int64_t orderKey(uint32_t word) {
    if (exponentOf(word) == 0) {
        return 0;
    }
    const int64_t magnitude = int64_t(word & magnitudeMask);
    return (word & signBit) ? -magnitude : magnitude;
}

// C.EQ.S / C.LT.S / C.LE.S.
inline constexpr bool equal(uint32_t a, uint32_t b) {
    return orderKey(a) == orderKey(b);
}

inline constexpr bool less(uint32_t a, uint32_t b) {
    return orderKey(a) < orderKey(b);
}

inline constexpr bool lessEqual(uint32_t a, uint32_t b) {
    return orderKey(a) <= orderKey(b);
}

// CVT.S.W / VITOF0: int32 to single, chopped (the EE / VU rounding mode).
inline constexpr uint32_t intToFloatBits(int32_t value) {
    if (value == 0) {
        return 0;
    }
    const uint32_t sign = value < 0 ? signBit : 0u;
    const uint64_t magnitude = value < 0 ? uint64_t(-int64_t(value)) : uint64_t(value);
    const int length = bitLength(magnitude);
    const uint32_t significand = length > 24 ? uint32_t(magnitude >> (length - 24)) : uint32_t(magnitude << (24 - length));
    return sign | (uint32_t(126 + length) << 23) | (significand & mantissaMask);
}

// CVT.W.S / VFTOI0: truncate toward zero; from 2^31 up, saturate by sign.
inline constexpr int32_t floatToIntBits(uint32_t word) {
    const uint32_t exponent = exponentOf(word);
    if (exponent >= 158) {
        return (word & signBit) ? int32_t(0x80000000u) : int32_t(0x7FFFFFFF);
    }
    if (exponent < 127) {
        return 0;
    }
    const uint32_t magnitude = significandOf(word);
    const uint32_t integer = exponent >= 150 ? magnitude << (exponent - 150) : magnitude >> (150 - exponent);
    return (word & signBit) ? -int32_t(integer) : int32_t(integer);
}

// ---- Fast paths ------------------------------------------------------------------------------
// The same words as addBits / mulBits on ordinary operands, through exact double arithmetic: a guard-masked sum of two
// singles within 24 exponents, and any product of two singles, is exact in a double (49 and 48 significant bits), so no
// rounding mode or FMA contraction can touch it, and chopping it is clearing the double's low 29 fraction bits.
// Anything else (zeros, the top binade, results outside the normal range, a product whose tail can meet the multiplier
// deficit) takes the integer path. tests/ps2_fpu_fuzz.cpp holds the two to each other.

inline constexpr uint64_t chopTo24Mask = ~uint64_t(0x1FFFFFFFu);

// Whether a double's biased exponent is a normal single's below the top binade: [2^-126, 2^127 * 2).
inline bool normalSingleRange(uint64_t bits) {
    return uint32_t((bits >> 52) & 0x7FFu) - 897u <= 253u;
}

inline uint32_t addSubFast(uint32_t a, uint32_t b, bool subtract) {
    guardMask(a, b);
    if (subtract) {
        b ^= signBit;
    }
    const uint32_t ea = exponentOf(a);
    const uint32_t eb = exponentOf(b);
    if (ea == 0 || eb == 0 || ea == 255 || eb == 255) {
        return addSubResult(a, b, false).bits;
    }
    const double sum = double(std::bit_cast<float>(a)) + double(std::bit_cast<float>(b));
    const uint64_t bits = std::bit_cast<uint64_t>(sum);
    if (!normalSingleRange(bits)) {
        return addSubResult(a, b, false).bits;
    }
    return std::bit_cast<uint32_t>(float(std::bit_cast<double>(bits & chopTo24Mask)));
}

inline uint32_t mulFast(uint32_t fs, uint32_t ft) {
    const uint32_t ea = exponentOf(fs);
    const uint32_t eb = exponentOf(ft);
    if (ea == 0 || eb == 0 || ea == 255 || eb == 255) {
        return mulResult(fs, ft).bits;
    }
    const double product = double(std::bit_cast<float>(fs)) * double(std::bit_cast<float>(ft));
    const uint64_t bits = std::bit_cast<uint64_t>(product);
    // The product's bits below the single's last bit sit in the top of the double's low 29 fraction bits. When any of
    // fraction bits 21..28 is set the tail is at least 2^15 product units, so the multiplier's deficit cannot reach it.
    if (!normalSingleRange(bits) || ((bits >> 21) & 0xFFu) == 0) {
        return mulResult(fs, ft).bits;
    }
    return std::bit_cast<uint32_t>(float(std::bit_cast<double>(bits & chopTo24Mask)));
}

// ---- Float conveniences --------------------------------------------------------------------

inline float add(float a, float b) {
    return std::bit_cast<float>(addSubFast(std::bit_cast<uint32_t>(a), std::bit_cast<uint32_t>(b), false));
}

inline float sub(float a, float b) {
    return std::bit_cast<float>(addSubFast(std::bit_cast<uint32_t>(a), std::bit_cast<uint32_t>(b), true));
}

// fs * ft in the PS2 instruction's operand order.
inline float mul(float fs, float ft) {
    return std::bit_cast<float>(mulFast(std::bit_cast<uint32_t>(fs), std::bit_cast<uint32_t>(ft)));
}

inline float div(float a, float b) {
    return std::bit_cast<float>(divBits(std::bit_cast<uint32_t>(a), std::bit_cast<uint32_t>(b)));
}

inline float sqrt(float a) {
    return std::bit_cast<float>(sqrtBits(std::bit_cast<uint32_t>(a)));
}

inline float rsqrt(float a, float t) {
    return std::bit_cast<float>(rsqrtBits(std::bit_cast<uint32_t>(a), std::bit_cast<uint32_t>(t)));
}

inline float eeMadd(float acc, float fs, float ft) {
    const uint32_t a = std::bit_cast<uint32_t>(acc);
    return std::bit_cast<float>(eeMulAccumulateBits(a, std::bit_cast<uint32_t>(fs), std::bit_cast<uint32_t>(ft), false));
}

inline float eeMsub(float acc, float fs, float ft) {
    const uint32_t a = std::bit_cast<uint32_t>(acc);
    return std::bit_cast<float>(eeMulAccumulateBits(a, std::bit_cast<uint32_t>(fs), std::bit_cast<uint32_t>(ft), true));
}

inline float vuMadd(float acc, float fs, float ft) {
    const uint32_t a = std::bit_cast<uint32_t>(acc);
    return std::bit_cast<float>(vuMulAccumulateBits(a, std::bit_cast<uint32_t>(fs), std::bit_cast<uint32_t>(ft), false));
}

inline float vuMsub(float acc, float fs, float ft) {
    const uint32_t a = std::bit_cast<uint32_t>(acc);
    return std::bit_cast<float>(vuMulAccumulateBits(a, std::bit_cast<uint32_t>(fs), std::bit_cast<uint32_t>(ft), true));
}

}
