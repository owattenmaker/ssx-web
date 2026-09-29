#include "original_input.hpp"
#include "software_float.hpp"
#include <algorithm>
#include <bit>
#include <cfenv>
#include <stdexcept>
#pragma STDC FENV_ACCESS ON
namespace ssx {
namespace {
struct Round {int old=std::fegetround();Round(){if(std::fesetround(FE_TOWARDZERO))throw std::runtime_error("Input rounding");}~Round(){std::fesetround(old);}};
constexpr float reciprocal255=std::bit_cast<float>(0x3b808081u);
// WebAssembly ignores fesetround, so the same MUL.S uses an explicit toward-zero product.
float towardZeroMul(float a,float b){
#if defined(__EMSCRIPTEN__)
    return software_float::mul(a,b);
#else
    return a*b;
#endif
}
}
OriginalPadValues originalDecodePad(const OriginalPadPacket& packet,bool pressureMode,bool analogMode){
    Round round;OriginalPadValues values{};
    constexpr std::array<unsigned,16> byte={2,2,2,2,2,2,2,2,3,3,3,3,3,3,3,3};
    constexpr std::array<unsigned,16> mask={1,8,2,4,32,128,16,64,16,32,64,128,4,8,1,2};
    for(unsigned i=0;i<16;i++)values[i]=(packet[byte[i]]&mask[i])?0.f:1.f;
    if(pressureMode)for(unsigned i=0;i<12;i++)values[i+4]=towardZeroMul(float(packet[i+8]),reciprocal255);
    if(analogMode)for(unsigned axis=0;axis<4;axis++){
        int raw=packet[4+axis];
        int negative=std::max(((79-raw)*255)/79,0),positive=std::max(((raw-176)*255)/79,0);
        values[16+2*axis]=towardZeroMul(float(negative),reciprocal255);
        values[17+2*axis]=towardZeroMul(float(positive),reciprocal255);
    }
    return values;
}
void originalUpdatePad(OriginalPadState& state,const OriginalPadValues& values){
    for(unsigned i=0;i<24;i++){
        auto& s=state[i];s.value=values[i];
        if(s.edgeAge<3){++s.edgeAge;s.pressed=s.released=0;}
        else {
            unsigned held=s.value>0?1:0;
            if(held==s.held)s.pressed=s.released=0;
            else {s.held=held;s.pressed=held;s.released=!held;s.edgeAge=0;}
        }
        if(s.held){
            if(s.repeatTimer==0){s.repeat=1;s.repeatTimer=24;}
            else {--s.repeatTimer;if(int32_t(s.repeatTimer)>0)s.repeat=0;else {s.repeat=1;s.repeatTimer=12;}}
        }else s.repeat=s.repeatTimer=0;
    }
}
float originalQuantizeAxis(float value){
    Round round;
    float scaled=towardZeroMul(value,31.f);int raw=int(scaled)&63;if(raw>=32)raw-=64;
    return towardZeroMul(float(raw),std::bit_cast<float>(0x3d042108u));
}
}
