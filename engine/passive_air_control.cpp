#include "passive_air_control.hpp"
#include "original_float.hpp"
#include "terrain_contact_math.hpp"
#include <bit>
#include <cfenv>
#include <cmath>
#include <algorithm>
#include <stdexcept>
#pragma STDC FENV_ACCESS ON
namespace ssx {namespace {
constexpr float F(uint32_t bits){return std::bit_cast<float>(bits);}
using Round=OriginalRounding;
float angle(float y,float x){
    if(x==0)return y==0?y:y<0?F(0xbfc90fdb):F(0x3fc90fdb);
    float result=originalAtan(originalScalarDivide(y,x));
    if(x<0)result=y>0?originalScalarAdd(result,F(0x40490fdb)):originalScalarSubtract(result,F(0x40490fdb));
    return result;
}
float axis(uint32_t word,unsigned shift){int32_t value=(word>>shift)&63;if(value>=32)value-=64;return terrain_original::mul(float(value),F(0x3d042108));}
}
OriginalPassiveAirCommand originalDecodePassiveAirCommand(uint32_t a,uint32_t b){
    Round round;if((a&0xc0000fffu)||(b&~63u))throw std::runtime_error("Unknown original passive air command bits");
    return {axis(a,24),axis(b,0),int32_t(int8_t((a>>16)&255)),bool(a&0x1000),bool(a&0x2000),bool(a&0x4000),bool(a&0x8000)};
}
void originalPassiveAirBegin(OriginalPassiveAirState& s,const OriginalGroundState& g,OriginalAirPrewindState& p){
    Round round;s.entryMagnitude=std::max(std::abs(g.turn.current),g.crouch.current);s.entryAngle=angle(g.crouch.current,g.turn.current);
    s.upperLatch=s.identityLatch=1;s.lastIdentity=-1;p.spin.rate=p.flip.rate=F(0x3d088889);p.spin.target=p.flip.target=0;
}
OriginalPassiveAirResult originalPassiveAirStep(OriginalPassiveAirState& s,OriginalGroundState& g,const OriginalPassiveAirCommand& command,const OriginalPassiveAirAccess& a){
    Round round;OriginalPassiveAirResult result;
    if(a.recover){if(a.recover(command.recover)){result.stop=OriginalPassiveAirResult::Stop::Recovery;return result;}}
    else if(command.recover)throw std::runtime_error("Passive air recovery callback missing");
    if(!a.upper)throw std::runtime_error("Passive air upper-action callback missing");
    if(s.upperLatch){s.upperLatch=command.upper14||command.upper15;if(a.upper(command.upper14,command.upper15)){result.stop=OriginalPassiveAirResult::Stop::Upper;return result;}}
    else (void)a.upper(false,false); //12F7BC deliberately ignores this return.
    float turn=command.turn,crouch=command.crouch;
    if(command.handplant){if(!a.handplant)throw std::runtime_error("Passive air handplant callback missing");if(a.handplant(crouch>.5f)){result.stop=OriginalPassiveAirResult::Stop::Handplant;return result;}crouch=1;}
    if(!a.rail)throw std::runtime_error("Passive air automatic rail callback missing");
    if(a.rail()){result.stop=OriginalPassiveAirResult::Stop::Rail;return result;}
    if(!a.stopBoost)throw std::runtime_error("Passive air boost callback missing");a.stopBoost();result.stopBoostRequested=true;
    if(s.identityLatch&&s.lastIdentity==-1)s.lastIdentity=command.identity;
    if(command.identity!=s.lastIdentity||command.identity==-1)s.identityLatch=0;
    if(!s.identityLatch&&s.lastIdentity!=-1)s.entryMagnitude=-1;
    float magnitude=std::max(std::abs(turn),crouch),direction=angle(crouch,turn);bool cancel=s.entryMagnitude<0;
    if(!cancel){float threshold=std::max(originalScalarSubtract(s.entryMagnitude,.5f),0.f);cancel=magnitude<=threshold;
        if(!cancel){float delta=originalScalarSubtract(direction,s.entryAngle);float turns=originalScalarAdd(terrain_original::mul(delta,F(0x3e22f983)),.5f);float integer=float(int32_t(turns));if(turns<integer)integer=originalScalarSubtract(integer,1.f);delta=originalScalarSubtract(delta,terrain_original::mul(integer,F(0x40c90fdb)));cancel=std::abs(delta)>F(0x3fc90fdc);}}
    if(cancel){s.entryMagnitude=-1;turn=crouch=0;}
    constexpr float rate=F(0x3d4cccce);g.turn.rate=rate;g.turn.target=turn;g.animationTurn.rate=rate;g.animationTurn.target=g.turn.current;g.crouch.rate=rate;g.crouch.target=crouch;g.brake.rate=rate;g.brake.target=0;result.targetsWritten=true;
    if(!a.mainAnimation)throw std::runtime_error("Passive air animation metadata missing");auto animation=a.mainAnimation();
    auto request=[&](int semantic){result.animationSemantic=semantic;if(a.requestAnimation)a.requestAnimation(semantic,-1.f,0);};
    if(g.crouch.current==0&&g.brake.current==0&&g.turn.current==0&&!s.identityLatch){
        if(animation.semantic!=287&&animation.animationClass!=9)request(287);
        if(a.requestControl)a.requestControl(5);result.stop=OriginalPassiveAirResult::Stop::Control5;
    }else if(animation.animationClass!=9){
        float limit=terrain_original::mul(originalScalarSubtract(1.f,std::abs(g.turn.current)),.5f);
        int semantic=g.crouch.current<=limit?9:10;if(semantic!=animation.semantic)request(semantic);
    }
    return result;
}
OriginalPassiveAirExit originalPassiveAirLeave(bool active,int animationClass){
    OriginalPassiveAirExit result;result.fade=result.setUpperRate=active&&(animationClass==3||animationClass==13);return result;
}
}
