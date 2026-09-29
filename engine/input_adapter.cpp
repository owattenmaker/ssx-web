#include "input.hpp"
#include "original_input.hpp"
#include <bit>
#include <cfenv>
#include <cmath>
#include <stdexcept>
#pragma STDC FENV_ACCESS ON
namespace ssx {
namespace {
struct Round {int old=std::fegetround();explicit Round(int mode){if(std::fesetround(mode))throw std::runtime_error("Input adapter rounding");}~Round(){std::fesetround(old);}};
constexpr std::array<Button,16> physical={Button::Select,Button::Pause,Button::L3,Button::R3,
    Button::Count,Button::Count,Button::Count,Button::Count,Button::North,Button::East,
    Button::South,Button::West,Button::L1,Button::R1,Button::L2,Button::R2};
InputSample fromValues(const OriginalPadValues& values){
    Round round(FE_TOWARDZERO);InputSample result;result.stickDomain=StickDomain::OriginalResponse;
    result.leftX=values[21]-values[20];result.leftY=values[22]-values[23];
    result.rightX=values[17]-values[16];result.rightY=values[18]-values[19];
    result.dpadX=values[4]-values[5];result.dpadY=values[6]-values[7];
    for(unsigned i=0;i<16;i++)if(physical[i]!=Button::Count&&values[i]>0)result.buttons|=bit(physical[i]);
    return result;
}
InputSample hardwareResponse(const InputSample& sample){
    OriginalPadPacket packet{};packet[2]=packet[3]=255;
    packet[4]=originalDeviceAxisByte(sample.rightX);packet[5]=originalDeviceAxisByte(-sample.rightY);
    packet[6]=originalDeviceAxisByte(sample.leftX);packet[7]=originalDeviceAxisByte(-sample.leftY);
    auto pressure=[](float value){
        if(!std::isfinite(value))throw std::runtime_error("Nonfinite controller D-pad value");
        Round round(FE_TONEAREST);return uint8_t(std::floor(double(std::clamp(value,0.f,1.f))*255.+.5));
    };
    packet[8]=pressure(sample.dpadX);packet[9]=pressure(-sample.dpadX);
    packet[10]=pressure(sample.dpadY);packet[11]=pressure(-sample.dpadY);
    auto result=fromValues(originalDecodePad(packet));result.buttons=sample.buttons;
    return result;
}
}
uint8_t originalDeviceAxisByte(float value){
    if(!std::isfinite(value))throw std::runtime_error("Nonfinite controller stick value");
    Round round(FE_TONEAREST);
    double normalized=std::clamp(double(value),-1.,1.);
    return uint8_t(std::floor((normalized+1.)*127.5+.5));
}
InputSample originalReplayInput(const std::array<uint8_t,18>& movieBytes){
    OriginalPadPacket packet{};packet[1]=0x79;
    std::copy(movieBytes.begin(),movieBytes.end(),packet.begin()+2);
    return fromValues(originalDecodePad(packet));
}
RiderInput InputMapper::update(const InputSample& sample){
    auto s=sample.stickDomain==StickDomain::OriginalResponse?sample:hardwareResponse(sample);
    Round round(FE_TOWARDZERO);
    auto held=[&](Button b){return bool(s.buttons&bit(b));};
    auto pressed=[&](Button b){return bool((s.buttons&~previous)&bit(b));};
    auto positive=[](float x){return std::max(x,0.f);};
    RiderInput r;
    // INPUT.MAP expressions run before the provider's six-bit command packing.
    float right=std::max(positive(s.leftX),positive(s.dpadX));
    float left=std::max(positive(-s.leftX),positive(-s.dpadX));
    r.turn=originalQuantizeAxis(right-left);
    r.crouch=originalQuantizeAxis(std::max(positive(s.leftY),positive(s.dpadY)));
    r.brake=originalQuantizeAxis(std::max(positive(-s.leftY),positive(-s.dpadY)));
    r.boardPress=originalQuantizeAxis(s.rightY);r.boardPivot=originalQuantizeAxis(s.rightX);
    r.prewindTurn=originalQuantizeAxis(s.leftX);r.spin=originalQuantizeAxis(s.dpadX);
    r.flip=originalQuantizeAxis(s.dpadY);r.airAdjustLR=r.prewindTurn;r.airAdjustFB=originalQuantizeAxis(s.leftY);
    r.jumpHeld=held(bindings.jump);r.jumpPressed=pressed(bindings.jump);
    r.boostHeld=held(bindings.boost);r.boostPressed=pressed(bindings.boost);
    r.handplant=held(bindings.handplant); // Boost is not the original reset/recovery action.
    r.ollieHeld=held(Button::R3);r.pausePressed=pressed(Button::Pause);
    for(unsigned i=0;i<4;i++)if(held(bindings.grabs[i]))r.grabMask|=1u<<i;
    constexpr std::array<uint8_t,15> grabCodes={1,2,4,8,3,5,9,6,10,12,7,11,13,14,15};
    if(r.grabMask){auto found=std::find(grabCodes.begin(),grabCodes.end(),r.grabMask);r.passiveInputCode=int(found-grabCodes.begin());}
    previous=s.buttons;return r;
}
}
