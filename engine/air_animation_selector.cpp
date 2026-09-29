#include "air_animation_selector.hpp"
#include "original_float.hpp"
#include "terrain_contact_math.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <cfenv>
#include <stdexcept>
#pragma STDC FENV_ACCESS ON
namespace ssx {namespace {
constexpr float F(uint32_t bits){return std::bit_cast<float>(bits);}
using Round=OriginalRounding;
float add(float a,float b){return originalScalarAdd(a,b);}float sub(float a,float b){return originalScalarSubtract(a,b);}float div(float a,float b){return originalScalarDivide(a,b);}
float direction(float y,float x){if(x==0)return y==0?y:y<0?F(0xbfc90fdb):F(0x3fc90fdb);float value=originalAtan(div(y,x));if(x<0)value=y>0?add(value,F(0x40490fdb)):sub(value,F(0x40490fdb));return value;}
unsigned sector(float angle){
    if(std::abs(angle)>F(0x402fede0))return 0;
    if(angle>F(0x3ffb53d3))return 1;
    if(angle>F(0x3f96cbe5))return 2;
    if(angle>F(0x3ec90fdc))return 3;
    if(angle<F(0xbffb53d3))return 4;
    if(angle<F(0xbf96cbe5))return 5;
    if(angle<F(0xbec90fdc))return 6;return 7;
}
int phaseTwoSemantic(int semantic){
    constexpr std::array<int,28> table={306,307,308,309,310,311,312,313,287,306,307,309,309,287,306,307,308,309,287,287,306,307,308,309,310,311,312,313};
    return semantic>=269&&semantic<=296?table[semantic-269]:287;
}
}
OriginalAirAnimationResult originalSelectAirAnimation(const OriginalAirControlState& air,const OriginalAirControlFrame& frame,OriginalAirAnimationState& state,const OriginalAirAnimationContext& c,const OriginalAirAnimationAccess& access){
    Round round;OriginalAirAnimationResult result{c.mainSemantic,false,state.nextRate};int desired=c.mainSemantic;
    auto setRate=[&](float rate){state.nextRate=rate;if(access.setNextRate)access.setNextRate(rate);};
    auto duration=[&](int semantic){if(!access.duration)throw std::runtime_error("Original air animation duration callback missing");return access.duration(semantic);};
    auto finish=[&](){result.semantic=desired;result.played=desired!=c.mainSemantic;result.rate=state.nextRate;if(result.played){if(access.play)access.play(desired,state.nextRate);setRate(1);}return result;};
    state.adjustment298.rate=F(0x3e2aaaab);state.adjustment298.target=terrain_original::mul(air.adjustSpin,div(1.f,F(0x3fdf66f4)));
    state.adjustment28C.rate=F(0x3e2aaaab);state.adjustment28C.target=terrain_original::mul(air.adjustFlip,div(1.f,F(0x3fdf66f4)));
    if(c.grabActive||c.mainSemantic==288)return finish();
    if(air.adjustSpin!=0||air.adjustFlip!=0){
        if(c.mainClass==9)return finish();
        float x=c.reverseStance?-state.adjustment298.current:state.adjustment298.current;
        constexpr int choices[]={297,298,299,300,304,303,302,301};desired=choices[sector(direction(state.adjustment28C.current,x))];return finish();
    }
    if(frame.phaseBefore==3&&uint32_t(air.phase)<2){
        constexpr int choices[]={270,274,272,273,276,271,275,269};float x=c.reverseStance?-frame.effectiveSpin:frame.effectiveSpin;desired=choices[sector(direction(frame.effectiveFlip,x))];
        float rate=add(terrain_original::mul(c.trickStat,F(0x3f0010fc)),1.f);rate=terrain_original::mul(rate,std::max(air.flipRate,air.spinRate));rate=terrain_original::mul(rate,F(0x3e124716));setRate(rate);return finish();
    }
    if(air.phase==0&&air.targetSpin==0&&air.targetFlip==0){desired=287;return finish();}
    if(air.phase==1){
        if(!air.extended||c.mainClass!=2||
            (air.progressSpin==air.targetSpin&&air.progressFlip==air.targetFlip)||(!c.mainCompleted&&c.mainSemantic!=287))return finish();
        float flip=air.progressFlip<air.targetFlip?1.f:air.progressFlip>air.targetFlip?-1.f:0;
        float spin=air.progressSpin<air.targetSpin?1.f:air.progressSpin>air.targetSpin?-1.f:0;if(c.reverseStance)spin=-spin;
        float angle=direction(flip,spin);if(std::abs(angle)>F(0x402fede0)||angle>F(0x3ffb53d3))desired=294;
        else if(angle>F(0x3f96cbe5)||angle>F(0x3ec90fdc))desired=293;
        else if(angle<F(0xbffb53d3)||angle<F(0xbf96cbe5))desired=296;
        else desired=295;return finish();
    }
    if(frame.phaseBefore!=2&&air.phase==2){
        desired=phaseTwoSemantic(desired);if(desired==c.mainSemantic)return finish();float clipDuration=duration(desired);
        float spinDelta=std::abs(sub(air.progressSpin,air.targetSpin)),flipDelta=std::abs(sub(air.progressFlip,air.targetFlip));
        float spinBlend=terrain_original::mul(spinDelta,F(0x3e22f982)),flipBlend=terrain_original::mul(flipDelta,F(0x3e22f982));
        float spinComplement=sub(1.f,spinBlend),flipComplement=sub(1.f,flipBlend);
        float flipValue=terrain_original::mul(flipBlend,F(0x409ffff0));spinComplement=terrain_original::mul(spinComplement,F(0x40f00234));flipComplement=terrain_original::mul(flipComplement,F(0x40f00234));float spinValue=terrain_original::mul(spinBlend,F(0x409ffff0));
        flipValue=add(flipValue,flipComplement);spinValue=add(spinValue,spinComplement);
        float flipTime=div(flipDelta,std::max(flipValue,air.maxFlip)),spinTime=div(spinDelta,std::max(spinValue,air.maxSpin));float remaining=std::max(spinTime,flipTime);
        if(c.boostModifier>0)remaining=terrain_original::mul(remaining,F(0x3f000418));
        float factor=div(1.f,add(terrain_original::mul(c.trickStat,F(0x3f0010fc)),1.f));remaining=terrain_original::mul(remaining,factor);
        float rate=remaining>F(0x3a83126f)?div(clipDuration,remaining):2.f;setRate(std::min(rate,2.f));return finish();
    }
    if(c.trajectoryStatus==1||c.trajectoryStatus==3){
        float remaining=sub(c.predictedTime,c.elapsed),clipDuration=duration(305);
        if(desired==287&&remaining<=clipDuration){float rate=remaining>F(0x3a83126f)?div(clipDuration,remaining):1.f;desired=305;setRate(std::min(rate,2.f));}
        else if(desired==305&&clipDuration<remaining)desired=287;
    }
    return finish();
}
}
