#include "input.hpp"
#include "original_input.hpp"
#include <cassert>
#include <bit>
#include <cfenv>
#include <cmath>
#include <cstdio>

std::array<uint8_t,18> neutral(){std::array<uint8_t,18> b{};b[0]=b[1]=255;for(int i=2;i<6;i++)b[i]=127;return b;}
int main(){
    // Helper equations have independent original-code conformance; these checks
    // exercise the complete adapters and mapper, detecting missing/double stages.
    for(int axis=0;axis<4;axis++)for(int raw=0;raw<256;raw++){
        auto bytes=neutral();bytes[axis+2]=uint8_t(raw);
        ssx::OriginalPadPacket packet{};std::copy(bytes.begin(),bytes.end(),packet.begin()+2);
        auto original=ssx::originalDecodePad(packet);
        float x=original[17+axis*2]-original[16+axis*2];
        if(axis&1)x=-x;
        float expected=ssx::originalQuantizeAxis(x);
        auto sample=ssx::originalReplayInput(bytes);assert(sample.stickDomain==ssx::StickDomain::OriginalResponse);
        ssx::InputMapper replayMapper;auto replay=replayMapper.update(sample);
        float actual=axis==0?replay.boardPivot:axis==1?replay.boardPress:axis==2?replay.turn:replay.airAdjustFB;
        assert(actual==expected);
        ssx::InputSample hardware;
        float value=float(double(raw)/127.5-1.);assert(ssx::originalDeviceAxisByte(value)==raw);
        if(axis&1)value=-value;
        if(axis==0)hardware.rightX=value;if(axis==1)hardware.rightY=value;
        if(axis==2)hardware.leftX=value;if(axis==3)hardware.leftY=value;
        ssx::InputMapper deviceMapper;auto device=deviceMapper.update(hardware);
        float native=axis==0?device.boardPivot:axis==1?device.boardPress:axis==2?device.turn:device.airAdjustFB;
        assert(native==expected);
    }
    for(int sign:{-1,1}){
        // Keyboardfullleft/right and PS2axisextrema/digitalDpad agree afterpacking.
        ssx::InputMapper a,b,c;ssx::InputSample key;key.leftX=float(sign);
        auto keyboard=a.update(key);auto bytes=neutral();bytes[4]=sign<0?0:255;
        auto stick=b.update(ssx::originalReplayInput(bytes));
        bytes=neutral();unsigned button=sign<0?7:5;bytes[0]&=uint8_t(~(1u<<button));bytes[sign<0?7:6]=255;
        auto digital=c.update(ssx::originalReplayInput(bytes));
        assert(keyboard.turn==stick.turn&&stick.turn==digital.turn);
        assert(std::abs(keyboard.turn)==std::bit_cast<float>(0x3f7fffffu));
    }
    // All independentshoulderchords survive rawpad and hardware adapters.
    for(unsigned mask=0;mask<16;mask++){
        auto bytes=neutral();ssx::InputSample hardware;
        constexpr unsigned bits[]={10,8,11,9},pressure[]={14,16,15,17};
        constexpr ssx::Button buttons[]={ssx::Button::L1,ssx::Button::L2,ssx::Button::R1,ssx::Button::R2};
        for(unsigned i=0;i<4;i++)if(mask&(1u<<i)){bytes[1]&=uint8_t(~(1u<<(bits[i]-8)));bytes[pressure[i]]=255;hardware.buttons|=ssx::bit(buttons[i]);}
        ssx::InputMapper replay,device;assert(replay.update(ssx::originalReplayInput(bytes)).grabMask==mask);assert(device.update(hardware).grabMask==mask);
    }
    // Pauseedges remain per UI update; they are deliberately independent of the
    // original pad sampling/debounce state so a paused game can resume immediately.
    ssx::InputMapper pause;ssx::InputSample sample;sample.buttons=ssx::bit(ssx::Button::Pause);
    assert(pause.update(sample).pausePressed);assert(!pause.update(sample).pausePressed);
    pause.update({});assert(pause.update(sample).pausePressed);
    // Original pressuremode observes pressure, not only the active-low flag.
    auto bytes=neutral();bytes[1]&=~uint8_t(1u<<6);
    ssx::InputMapper pressure;assert(!pressure.update(ssx::originalReplayInput(bytes)).jumpHeld);
    bytes[12]=1;assert(pressure.update(ssx::originalReplayInput(bytes)).jumpHeld);
    for(int mode:{FE_UPWARD,FE_DOWNWARD}){std::fesetround(mode);ssx::InputMapper mapper;mapper.update({});assert(std::fegetround()==mode);ssx::originalReplayInput(bytes);assert(std::fegetround()==mode);}
    std::fesetround(FE_TONEAREST);
    std::puts("Native input adapters: all256bytes/fouraxes, hardware conversion, keyboard/digital equivalence,16shoulderchords, pressuremode and pauseedges passed");
}
