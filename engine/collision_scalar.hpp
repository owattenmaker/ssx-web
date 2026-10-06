#pragma once
#include "terrain_contact_math.hpp"
#include <bit>
#include <limits>
namespace ssx::collision_scalar {
// PCSX2 iFPU.cpp recDIV_S_xmm/recSQRT_S_xmm use nearest independently of
// the game's FPUFPCR chop mode. VU arithmetic must not use these helpers.
struct Nearest {int prior=std::fegetround();Nearest(){std::fesetround(FE_TONEAREST);}~Nearest(){std::fesetround(prior);}};
inline float divide(float a,float b) {
#if SSX_PS2_EXACT_FPU
    if(software_float::exactArithmetic)return ps2fpu::div(a,b);
#endif
    Nearest rounding;volatile float x=a,y=b;
    if(y==0)return std::copysign(std::numeric_limits<float>::max(),std::signbit(x)!=std::signbit(y)?-1.f:1.f);
    volatile float result=x/y;return result;
}
inline float squareRoot(float x) {
#if SSX_PS2_EXACT_FPU
    if(software_float::exactArithmetic)return ps2fpu::sqrt(x);
#endif
    if(x==0)return x;Nearest rounding;volatile float argument=std::abs(x);volatile float result=std::sqrt(argument);return result;
}
inline float constant(uint32_t bits){return std::bit_cast<float>(bits);}
inline float atan(float x) {
    using namespace terrain_original;Rounding rounding;
    float square=mul(x,x),t,root;
    if(x< -1||x>1){t=divide(1,originalScalarAdd(square,1));root=squareRoot(t);}
    else {t=divide(square,originalScalarAdd(square,1));root=squareRoot(t);}
    float value=mul(t,constant(0x3d6137ab));value=originalScalarAdd(constant(0x3d55033a),value);value=mul(t,value);
    value=originalScalarAdd(constant(0x3d8a908b),value);value=mul(t,value);value=originalScalarAdd(constant(0x3e2bba25),value);
    value=mul(t,value);value=originalScalarAdd(value,1);value=mul(root,value);
    if(x< -1)return originalScalarAdd(value,constant(0xbfc90fdb));if(x>1)return originalScalarSubtract(constant(0x3fc90fdb),value);return x<0?-value:value;
}
inline std::array<float,2> sincos(float x) {
    using namespace terrain_original;Rounding rounding;
    float scaled=mul(x,constant(0x3f22f983));scaled=x<0?originalScalarSubtract(scaled,.5f):originalScalarAdd(scaled,.5f);
    int quadrant=int(scaled);x=originalScalarSubtract(x,mul(float(quadrant),constant(0x3fc90fdb)));
    float square=mul(x,x),value=mul(square,constant(0x3638ef1f));value=originalScalarAdd(value,constant(0xb9500d03));
    value=mul(value,square);value=originalScalarAdd(value,constant(0x3c088889));value=mul(value,square);value=originalScalarAdd(value,constant(0xbe2aaaab));
    value=mul(value,square);value=originalScalarAdd(value,1);float sine=mul(value,x),cosine=squareRoot(originalScalarSubtract(1,mul(sine,sine)));
    switch(quadrant&3){case 0:return {sine,cosine};case 1:return {cosine,-sine};case 2:return {-sine,-cosine};default:return {-cosine,sine};}
}
}
