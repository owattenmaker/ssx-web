#pragma once
#include <bit>
#include <cstdint>
#include <cfenv>
#include <cmath>
#include <stdexcept>
#include "original_rounding.hpp"
#pragma STDC FENV_ACCESS ON
namespace ssx {
// PCSX2 iFPU.cpp FPU_ADD_SUB: EE keeps one alignment guard bit.
// VU ADD/SUB retain ordinary IEEE arithmetic and must not call these helpers.
// Nearest policy: the volatile operands keep the operand order of the add (the NaN an add of two NaNs propagates).
[[gnu::noinline]] inline float originalScalarAddSubNearest(uint32_t x,uint32_t y,bool subtract){
    volatile float left=std::bit_cast<float>(x),right=std::bit_cast<float>(y);
    return software_float::eeFlush(subtract?left-right:left+right);
}
inline float originalScalarAddSub(float a,float b,bool subtract){
    uint32_t x=std::bit_cast<uint32_t>(a),y=std::bit_cast<uint32_t>(b);
    int difference=int((x>>23)&255)-int((y>>23)&255);
    if(difference>=25)y&=0x80000000u;
    else if(difference<=-25)x&=0x80000000u;
    else if(difference>0)y&=0xffffffffu<<(difference-1);
    else if(difference<0)x&=0xffffffffu<<(-difference-1);
#if defined(__EMSCRIPTEN__)
    // Toward zero (the rider contexts' policy): no volatile operands, which cost a stack frame per call.
    if(originalRoundingMode==FE_TOWARDZERO)return subtract?software_float::sub(std::bit_cast<float>(x),std::bit_cast<float>(y)):software_float::add(std::bit_cast<float>(x),std::bit_cast<float>(y));
    return originalScalarAddSubNearest(x,y,subtract);
#else
    volatile float left=std::bit_cast<float>(x),right=std::bit_cast<float>(y);
    return software_float::eeFlush(subtract?left-right:left+right);
#endif
}
inline float originalScalarAdd(float a,float b){return originalScalarAddSub(a,b,false);}
inline float originalScalarSubtract(float a,float b){return originalScalarAddSub(a,b,true);}
// PCSX2's EE scalar DIV.S/SQRT.S use nearest rounding independently of the
// surrounding EE/VU chop mode. This is not a policy for VU DIV/RSQRT.
#if defined(__EMSCRIPTEN__)
// WebAssembly only has nearest rounding (the rider-context rounding policy is the software originalRoundingMode), so
// the native fesetround round trip is a no-op there.
inline float originalScalarDivide(float a,float b){return software_float::eeFlush(software_float::eeFlush(a)/software_float::eeFlush(b));}
inline float originalScalarSqrt(float value){return software_float::eeFlush(std::sqrt(software_float::eeFlush(value)));}
#else
inline float originalScalarDivide(float a,float b){
    int old=std::fegetround();if(std::fesetround(FE_TONEAREST))throw std::runtime_error("Scalar DIV rounding");
    volatile float left=software_float::eeFlush(a),right=software_float::eeFlush(b);float result=left/right;std::fesetround(old);return software_float::eeFlush(result);
}
inline float originalScalarSqrt(float value){
    int old=std::fegetround();if(std::fesetround(FE_TONEAREST))throw std::runtime_error("Scalar SQRT rounding");
    volatile float input=software_float::eeFlush(value);float result=std::sqrt(input);std::fesetround(old);return software_float::eeFlush(result);
}
#endif
}
