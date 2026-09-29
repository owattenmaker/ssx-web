#include "soft_collision_control.hpp"
#include "terrain_contact_math.hpp"
#include <bit>
namespace ssx {
OriginalSoftControlInput originalSoftControlInput(uint32_t command){
    terrain_original::Rounding rounding;
    auto axis=[&](unsigned shift){unsigned raw=(command>>shift)&63;int value=raw>=32?int(raw)-64:int(raw);return float(value)*std::bit_cast<float>(0x3d042108u);};
    return {axis(15),axis(21),bool(command&(1u<<13)),bool(command&(1u<<14)),bool(command&(1u<<12))};
}
OriginalSoftControlResult originalSoftControlStep(const OriginalGroundProfile& profile,
    OriginalGroundState& state,const OriginalSoftControlContext& context,const OriginalSoftControlInput& input){
    terrain_original::Rounding rounding;OriginalSoftControlResult result;
    if(state.modeTiming>=0){result.stop=OriginalSoftControlResult::Stop::ModeGate;result.nextControl=10;return result;}
    if(input.recovery){result.stop=OriginalSoftControlResult::Stop::Recovery;result.nextControl=9;result.nextMotion=3;result.clearBoostWindows=true;return result;}
    if(context.obstructionHandled){result.stop=OriginalSoftControlResult::Stop::Obstruction;return result;}
    float forwardSpeed=terrain_original::dot(state.velocity,state.forward);
    result.requestBoost=true;
    if(context.motionMode!=1){result.boostHeld=input.boostHeld;result.boostPressed=input.boostPressed;}
    if(context.motionMode==4){result.requestBalance=true;result.balance=input.balance;}
    else {
        float turn=context.motionMode==1?0.f:input.turn;
        if(context.motionMode!=1&&forwardSpeed<0)turn=-turn;
        groundTurnTarget(state.turn,turn,state.velocity,profile.surface.id);
    }
    groundCrouchBrakeTargets(state.crouch,state.brake,0,0,forwardSpeed,state.turn.current);
    state.animationTurn.target=0;state.animationTurn.rate=std::bit_cast<float>(0x3d088889u);
    if(context.mainSequenceComplete){
        if(context.motionMode==4){result.resetRail=true;result.nextControl=7;}
        else {result.restoreStance=true;result.nextControl=context.motionMode==1?4:0;result.normalGainFocus=result.nextControl==0;}
    }
    return result;
}
}
