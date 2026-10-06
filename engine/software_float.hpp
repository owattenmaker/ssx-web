#pragma once
#include <bit>
#include <cmath>
#include <cstdint>
#if SSX_PS2_EXACT_FPU
#include "ps2_fpu.hpp"
#endif

namespace ssx::software_float {
#if SSX_PS2_EXACT_FPU
// The console's arithmetic (engine/ps2_fpu.hpp, docs/ps2-float.md) in a core built with SSX_PS2_EXACT_FPU. On by default;
// the capture comparers turn it off for their setup before a capture's first tick (PS2_ARITH=exact): the baseline savestates
// carry mode-1 history.
inline bool exactArithmetic = true;
#endif
// The EE FPU (and PCSX2/ARMSX2's EE emulation: DAZ + FTZ) has no denormals: a denormal operand reads as
// zero and an underflowing result is flushed to a signed zero (tech-trip-stop: the decaying +0x2C8 lift).
inline float eeFlush(float value){uint32_t bits=std::bit_cast<uint32_t>(value);return (bits&0x7f800000u)==0&&(bits&0x007fffffu)!=0?std::bit_cast<float>(bits&0x80000000u):value;}
// These helpers run under nearest rounding (including WebAssembly). They
// emulate IEEE binary32 toward-zero arithmetic, not all PS2 exceptional-value
// semantics. Keep EE scalar DIV/SQRT's separate nearest policy at call sites.
inline float reduceMagnitude(float value) {
    return std::bit_cast<float>(std::bit_cast<uint32_t>(value)-1u);
}
inline float correct(float rounded,double residual) {
    if(rounded>0 && residual<0)return reduceMagnitude(rounded);
    if(rounded<0 && residual>0)return reduceMagnitude(rounded);
    return rounded;
}
inline float addExact(float a,float b) {
    const double x=a,y=b,sum=x+y;
    const float rounded=float(sum);
    if(!std::isfinite(a)||!std::isfinite(b))return rounded;
    if(std::isinf(rounded))return reduceMagnitude(rounded);
    // FastTwoSum retains the sign of operands too small for binary64's sum.
    // That sign matters when an exactly representable result needs truncating.
    const double error=std::abs(x)>=std::abs(y)?y-(sum-x):x-(sum-y);
    return correct(rounded,(sum-double(rounded))+error);
}
inline float mulExact(float a,float b) {
    const double exact=double(a)*double(b);
    const float rounded=float(exact);
    if(!std::isfinite(a)||!std::isfinite(b))return rounded;
    if(std::isinf(rounded))return reduceMagnitude(rounded);
    return correct(rounded,exact-double(rounded));
}
// Reference definitions (the general case: zero/tiny/huge/non-finite results, inexact binary64 sums). The fast
// paths below return exactly these results; test-software-float (tools/, docs/sim-performance.md) checks it.
// WebAssembly: [[gnu::const]] (docs/sim-performance.md "Rounding-mode loads"). They read no memory and no floating-point
// environment (wasm has only nearest), so the compiler may keep a caller's originalRoundingMode load across the call: a
// function that opens OriginalRounding and does inline arithmetic tests the mode it just stored once, not once per operation.
// Native builds leave them plain: there the host rounding mode is state they depend on.
#if defined(__EMSCRIPTEN__)
#define SSX_SOFTWARE_FLOAT_CONST [[gnu::const]]
#else
#define SSX_SOFTWARE_FLOAT_CONST
#endif
[[gnu::noinline]] SSX_SOFTWARE_FLOAT_CONST inline float addReference(float a,float b){return eeFlush(addExact(eeFlush(a),eeFlush(b)));}
[[gnu::noinline]] SSX_SOFTWARE_FLOAT_CONST inline float mulReference(float a,float b){return eeFlush(mulExact(eeFlush(a),eeFlush(b)));}
// Fast toward-zero binary32 from an exact binary64 value v whose biased exponent is in [897, 1150] (|v| in
// [2^-126, 2^128)): clearing the low 29 fraction bits truncates v to 24 significant bits, which is the toward-zero
// binary32 value, exactly representable (normal, <= FLT_MAX), so the conversion does not round.
inline constexpr uint64_t truncate24Mask=~uint64_t(0x1fffffffu);
inline bool normalBinary32Range(uint64_t bits){return uint32_t((bits>>52)&0x7ffu)-897u<=253u;}
// Toward-zero sum. A binary32 sum is exact in binary64 whenever the exponents differ by < 29; then either the low
// 29 bits of the sum are not all zero (truncating the exact sum is the result) or the sum is itself a binary32
// value. The FastTwoSum residual (exact for |x| >= |y|) tells an exact sum from a rounded one; a rounded sum that
// lands on a binary32 value, and every zero/tiny/huge/non-finite case, takes the reference path.
// x, y: a and b after eeFlush (a, b themselves go to the reference path).
inline float addFlushed(double x,double y,float a,float b){
    const double sum=x+y;
    const uint64_t bits=std::bit_cast<uint64_t>(sum);
    if(normalBinary32Range(bits)){
        if(bits&~truncate24Mask){
            // sum is RN(exact), strictly between two 24-bit values at least one binary64 ulp away: exact lies within
            // half an ulp of sum, so it truncates to the same 24-bit value.
            return float(std::bit_cast<double>(bits&truncate24Mask));
        }
        const double error=std::abs(x)>=std::abs(y)?y-(sum-x):x-(sum-y);
        if(error==0)return float(sum);
    }else if(sum==0)return float(sum); // exact zero (a binary64 sum of binary32 values never rounds to zero)
    return addReference(a,b);
}
// eeFlush changes only a zero-exponent operand (a denormal), so when both exponent fields are nonzero the operands are
// used as they are and the two eeFlush tests are skipped (docs/sim-performance.md "Operand flush test").
inline bool exponentsNonzero(float a,float b){return ((std::bit_cast<uint32_t>(a)&0x7f800000u)!=0)&((std::bit_cast<uint32_t>(b)&0x7f800000u)!=0);}
inline float add(float a,float b){
#if SSX_PS2_EXACT_FPU
    if(exactArithmetic)return ssx::ps2fpu::add(a,b);
#endif
    if(exponentsNonzero(a,b))return addFlushed(a,b,a,b);
    return addFlushed(eeFlush(a),eeFlush(b),a,b);
}
inline float sub(float a,float b){return add(a,-b);}
// Toward-zero product. binary32 x binary32 is exact in binary64 (48 significant bits).
inline float mulFlushed(double x,double y,float a,float b){
    const double exact=x*y;
    const uint64_t bits=std::bit_cast<uint64_t>(exact);
    if(normalBinary32Range(bits))return float(std::bit_cast<double>(bits&truncate24Mask));
    if(exact==0)return float(exact);
    return mulReference(a,b);
}
inline float mul(float a,float b){
#if SSX_PS2_EXACT_FPU
    if(exactArithmetic)return ssx::ps2fpu::mul(a,b);
#endif
    if(exponentsNonzero(a,b))return mulFlushed(a,b,a,b);
    return mulFlushed(eeFlush(a),eeFlush(b),a,b);
}
inline float divExact(float a,float b) {
    const float rounded=float(double(a)/double(b));
    if(!std::isfinite(a)||!std::isfinite(b)||b==0)return rounded;
    if(std::isinf(rounded))return reduceMagnitude(rounded);
    const double remainder=double(a)-double(rounded)*double(b);
    return correct(rounded,std::signbit(b)?-remainder:remainder);
}
inline float div(float a,float b){
#if SSX_PS2_EXACT_FPU
    if(exactArithmetic)return ssx::ps2fpu::div(a,b);
#endif
    return eeFlush(divExact(eeFlush(a),eeFlush(b)));
}
inline float sqrtExact(float a) {
    const float rounded=float(std::sqrt(double(a)));
    if(std::isfinite(a)&&a>0&&double(rounded)*double(rounded)>double(a))return reduceMagnitude(rounded);
    return rounded;
}
inline float sqrt(float a){
#if SSX_PS2_EXACT_FPU
    if(exactArithmetic)return ssx::ps2fpu::sqrt(a);
#endif
    return eeFlush(sqrtExact(eeFlush(a)));
}
}
