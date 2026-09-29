#include "attack_control.hpp"
#include "original_float.hpp"
#include "terrain_contact_math.hpp"
#include <bit>
#include <stdexcept>
#pragma STDC FENV_ACCESS ON
namespace ssx {
namespace {
constexpr float F(uint32_t bits){return std::bit_cast<float>(bits);}
// VU0 VMULx.xyzw by -1 (0x1166BC / 0x116854): an exact sign flip of all four lanes
// (+0 becomes -0). Denormal lanes (never present in the unit right vector) keep
// their payload, as in the recompiled oracle.
std::array<float,4> negated(const std::array<float,4>& v){
    std::array<float,4> out;
    for(unsigned k=0;k<4;++k)out[k]=std::bit_cast<float>(std::bit_cast<uint32_t>(v[k])^0x80000000u);
    return out;
}
}
float originalAttackClampTurn(float turn){return -.5f<=turn?std::min(turn,.5f):-.5f;}
bool originalAttackControlExit(int upperClass){return upperClass==3||upperClass==13;}
bool originalAttackStep(OriginalAttackRider& r,bool left,bool right,const OriginalAttackAccess& a){
    if(!a.channelClass||!a.requestedSemantic||!a.latched||!a.raised||!a.setRate||!a.weights||!a.setWeights||!a.fadeTo||!a.play||!a.fade)
        throw std::runtime_error("Original attack animator callback missing");
    OriginalRounding round;
    auto marker2=[&](){return a.latched(1,2)&&a.raised(1,2);}; //1446A0 then 1446B8
    // 0x1163F0..0x11651C: an active block (class 3).
    if(a.channelClass(1)==3){
        if(left&&right){
            if(marker2()){
                a.setRate(1,0);a.play(originalAttackBlockCycle,false);
                const auto upper=a.weights(1);
                a.setWeights(0,upper.weight,upper.weight);
                if(upper.fadeRemaining!=0)a.fadeTo(0,upper.targetWeight,upper.fadeRemaining);
            }
        }else{a.setRate(1,1);a.fade(0,F(0x3dcccccdu));}
    }
    // 0x116520..0x1166F4: an active punch (class 13).
    if(a.channelClass(1)==13){
        const int semantic=a.requestedSemantic(1);
        const bool leftPunch=r.switchStance?semantic==originalAttackPunchToeSide:semantic==originalAttackPunchHeelSide;
        const bool hold=left?(!right&&leftPunch):(right&&!leftPunch);
        if(hold){
            if(marker2()){a.setRate(1,0);a.play(semantic==originalAttackPunchToeSide?originalAttackPunchToeCycle:originalAttackPunchHeelCycle,false);}
            if(a.channelClass(0)==13){
                const float step=terrain_original::mul(r.timeScale,F(0x3c360b62u));
                float next=1;
                if(originalScalarAdd(step,1.f)<r.strength)next=originalScalarSubtract(r.strength,step);
                else if(r.strength<originalScalarSubtract(1.f,step))next=originalScalarAdd(r.strength,step);
                r.strength=next;a.setWeights(0,r.strength,r.strength);
            }
            r.facing=leftPunch?negated(r.right):r.right;
        }else{a.setRate(1,1);a.fade(0,F(0x3dcccccdu));}
    }
    // 0x1166F8..0x1168B8: start a punch or block.
    if(!left&&!right){}
    else if(left&&right){
        if(a.channelClass(1)==13&&a.latched(1,0)&&!a.latched(1,1))return true;
        if(a.channelClass(1)==3&&!a.latched(1,1))return true;
        a.play(originalAttackBlock,true);r.strength=0;
    }else{
        if(a.channelClass(1)==3&&a.latched(1,0)&&!a.latched(1,1))return true;
        if(a.channelClass(1)==13&&!a.latched(1,1))return true;
        int semantic;
        if(left){r.facing=negated(r.right);semantic=r.switchStance?originalAttackPunchToeSide:originalAttackPunchHeelSide;}
        else{r.facing=r.right;semantic=r.switchStance?originalAttackPunchHeelSide:originalAttackPunchToeSide;}
        a.play(semantic,true);r.strength=0;
    }
    const int upper=a.channelClass(1);return upper==3||upper==13;
}
}
