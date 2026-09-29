#pragma once
#include "body_collision.hpp"
#include "collision_event.hpp"
#include <limits>

namespace ssx {
struct OriginalBodyPairContact {
    bool hit=false;
    terrain_original::Vector penetrationCm{},pointCm{};
    int sphereA=-1,sphereB=-1;
};
// 0x329F98 uses the first overlapping child pair, including disabled mask bits.
// The contact point convention is deliberately the original A - n*(rA-depth).
inline OriginalBodyPairContact originalBodyPairContact(const BodyCollisionVolume& a,const BodyCollisionVolume& b) {
    using namespace terrain_original;Rounding rounding;
    if(a.count>20||b.count>20)throw std::runtime_error("Body sphere count exceeds original storage");
    auto overlaps=[](Vector ac,float ar,Vector bc,float br) {
        auto d=difference(bc,ac);float r=originalScalarAdd(br,ar);return !(mul(r,r)<dot(d,d));
    };
    if(!overlaps(a.broadCenterCm,a.broadRadiusCm,b.broadCenterCm,b.broadRadiusCm))return {};
    for(unsigned i=0;i<a.count;++i) {
        const auto& sa=a.spheres[i];if(!overlaps(sa.centerCm,sa.radiusCm,b.broadCenterCm,b.broadRadiusCm))continue;
        for(unsigned j=0;j<b.count;++j) {
            const auto& sb=b.spheres[j];auto d=difference(sb.centerCm,sa.centerCm);float squared=dot(d,d);
            float radius=originalScalarAdd(sb.radiusCm,sa.radiusCm);if(mul(radius,radius)<squared)continue;
            float reciprocal=squared>0?div(1,terrain_original::sqrt(squared)):std::numeric_limits<float>::max();
            Vector normal;for(unsigned k=0;k<3;++k)normal[k]=mul(d[k],reciprocal);
            float depth=originalScalarSubtract(radius,collision_scalar::squareRoot(squared));
            float offset=originalScalarSubtract(sa.radiusCm,depth);OriginalBodyPairContact result;result.hit=true;result.sphereA=i;result.sphereB=j;
            for(unsigned k=0;k<3;++k){result.penetrationCm[k]=mul(normal[k],depth);result.pointCm[k]=sub(sa.centerCm[k],mul(normal[k],offset));}
            return result;
        }
    }
    return {};
}
// 0x11FF98. Inputs are resolved character attributes, not guessed mass units.
inline float originalRiderCollisionWeight(int weightAttribute,float resolvedStat,float boost) {
    using namespace terrain_original;Rounding rounding;
    float factor=originalScalarAdd(mul(resolvedStat,1.5003352165222168f),1);
    float boostFactor=originalScalarAdd(mul(boost,10),1);
    return mul(mul(float(weightAttribute),factor),boostFactor);
}
inline bool originalPairGrounded(int motionMode,int ragdollSubmode) {return motionMode==0||(motionMode==2&&ragdollSubmode==0);}
struct OriginalPairImpulseState {
    terrain_original::Vector velocityCmps{},groundNormal{0,0,1},physicalUp{0,0,1};
    int motionMode=0,controlState=0,ragdollSubmode=0;
};
struct OriginalPairImpulseResult {
    terrain_original::Vector velocityCmps{};
    float clampedImpulseCmps=0,reactionMagnitudeCmps=0;
    bool ignored=false,resetPredictor=false,reactionEligible=false;
};
// Physical prefix and reaction gates of 0x107E70. Reactions remain separate so
// callers cannot accidentally run ordinary grounded physics in crash modes.
inline OriginalPairImpulseResult originalPairImpulse(const OriginalPairImpulseState& state,
        terrain_original::Vector direction,float impulse) {
    using namespace terrain_original;Rounding rounding;OriginalPairImpulseResult result;result.velocityCmps=state.velocityCmps;
    if(state.controlState==10){result.ignored=true;return result;}
    result.clampedImpulseCmps=std::clamp(impulse,-555.5555419921875f,555.5555419921875f);
    result.reactionMagnitudeCmps=std::abs(impulse);Vector delta;
    for(unsigned k=0;k<3;++k)delta[k]=mul(direction[k],result.clampedImpulseCmps);
    if(originalPairGrounded(state.motionMode,state.ragdollSubmode)) {
        float amount=dot(delta,state.groundNormal);for(unsigned k=0;k<3;++k)delta[k]=sub(delta[k],mul(state.groundNormal[k],amount));
    } else result.resetPredictor=state.motionMode==1||(state.motionMode==2&&state.ragdollSubmode==1);
    for(unsigned k=0;k<3;++k)result.velocityCmps[k]=add(state.velocityCmps[k],delta[k]);
    if(result.resetPredictor&&mul(dot(direction,state.physicalUp),result.clampedImpulseCmps)>0)result.reactionMagnitudeCmps=0;
    result.reactionEligible=state.motionMode!=2&&state.controlState!=9&&result.reactionMagnitudeCmps>=39.99532699584961f;
    return result;
}
struct OriginalPairReactionRequest {
    enum class Kind {None,Soft,Crash};
    Kind kind=Kind::None;
    OriginalCollisionEvent event;
    int animation=-1;
    bool attack=false;
    unsigned randomDraws=0;
};
// Remaining 0x107E70 selects the event and dispatches 0x108388 (soft) or
// 0x10EB30 (crash). Their controller/animation lifecycle is a caller operation.
inline OriginalPairReactionRequest originalPairReactionRequest(const OriginalPairImpulseResult& impulse,
        const OriginalCollisionContext& context,terrain_original::Vector direction,
        terrain_original::Vector positionCm,float cachedPlanarDistanceCm,bool attack,bool knockdownCheat,
        const CollisionRandom& random) {
    using namespace terrain_original;Rounding rounding;OriginalPairReactionRequest result;result.attack=attack;
    if(!impulse.reactionEligible)return result;
    auto& event=result.event;event.normal=direction;event.closingSpeedCmps=impulse.clampedImpulseCmps;
    for(unsigned k=0;k<3;++k)event.pointCm[k]=add(positionCm[k],mul(direction[k],cachedPlanarDistanceCm));
    float squared=dot(impulse.velocityCmps,impulse.velocityCmps);
    float reciprocal=squared>0?div(1,terrain_original::sqrt(squared)):std::numeric_limits<float>::max();
    for(unsigned k=0;k<3;++k)event.incomingDirection[k]=mul(impulse.velocityCmps[k],reciprocal);
    float magnitude=knockdownCheat?599.9739990234375f:impulse.reactionMagnitudeCmps;
    if(!(599.9739990234375f<magnitude)){result.kind=OriginalPairReactionRequest::Kind::Soft;return result;}
    result.kind=OriginalPairReactionRequest::Kind::Crash;
    float right=dot(context.presentation.right,direction),forward=dot(context.presentation.forward,direction),up=dot(context.presentation.up,direction);
    auto next=[&](){if(!random)throw std::runtime_error("Original pair random source unavailable");++result.randomDraws;return random();};
    if(std::abs(right)<std::abs(forward)&&std::abs(up)<std::abs(forward)) {
        if(forward<0) {
            unsigned choice=next()%(2222.22216796875f<terrain_original::sqrt(squared)?6:3);
            constexpr int choices[]={334,335,337,333,336,336};result.animation=choices[choice];
        } else result.animation=(next()&1)?331:330;
    } else if(std::abs(right)<std::abs(up)&&std::abs(forward)<std::abs(up))result.animation=331;
    else result.animation=right<0?329:328;
    return result;
}

struct OriginalPairSeparation {
    terrain_original::Vector translationA{},translationB{},direction{};
    float impulseA=0,impulseB=0;
};
// 0x1079D4..107BC0. Supply the owner's weight cached before its slot loop,
// and the other weight resolved at this contact. Both impulses are computed
// before either response call; callbacks execute A then B.
inline OriginalPairSeparation originalPairSeparation(const OriginalBodyPairContact& contact,
        const OriginalPairImpulseState& a,const OriginalPairImpulseState& b,float weightA,float weightB) {
    using namespace terrain_original;Rounding rounding;OriginalPairSeparation result;
    for(unsigned k=0;k<3;++k){result.translationA[k]=mul(contact.penetrationCm[k],-.550000011920929f);result.translationB[k]=mul(contact.penetrationCm[k],.550000011920929f);}
    auto project=[](Vector v,const OriginalPairImpulseState& state){if(originalPairGrounded(state.motionMode,state.ragdollSubmode)){float amount=dot(v,state.groundNormal);for(unsigned k=0;k<3;++k)v[k]=sub(v[k],mul(state.groundNormal[k],amount));}return v;};
    result.translationA=project(result.translationA,a);result.translationB=project(result.translationB,b);
    float squared=dot(contact.penetrationCm,contact.penetrationCm);
    float reciprocal=squared>0?div(1,terrain_original::sqrt(squared)):std::numeric_limits<float>::max();
    for(unsigned k=0;k<3;++k)result.direction[k]=mul(contact.penetrationCm[k],reciprocal);
    float relative=originalScalarSubtract(dot(b.velocityCmps,result.direction),dot(a.velocityCmps,result.direction));
    float amount=collision_scalar::divide(relative,originalScalarAdd(weightA,weightB));
    result.impulseA=mul(weightB,amount);result.impulseB=mul(-weightA,amount);return result;
}
struct OriginalPairRecord {
    bool enabled=false;
    float planarDistanceCm=10000000000.f,bearing=0;
    int32_t lastContactTick=0,lastCheckedTick=0,lastAttackTick=0;
};
// These predicates encode signed tick comparisons; translation is not subject
// to the impulse cooldown. Checked ticks are committed before the body query,
// and contact ticks only after both ordered response callbacks have completed.
inline bool originalPairShouldCheck(const OriginalPairRecord& owner,const OriginalPairRecord& reciprocal,bool otherDisabled,int32_t tick) {
    return owner.enabled&&!otherDisabled&&reciprocal.lastCheckedTick<tick;
}
inline bool originalPairImpulseDue(const OriginalPairRecord& owner,int32_t tick) {
    return owner.lastContactTick<int32_t(uint32_t(tick)-3u);
}
struct OriginalPairProximity {float distanceCm=0,bearingAToB=0,bearingBToA=0;};
// 0x10F6B0..10F7CC: shared proximity records refresh only every sixth tick.
inline OriginalPairProximity originalPairProximity(terrain_original::Vector a,terrain_original::Vector b) {
    using namespace terrain_original;Rounding rounding;auto d=difference(a,b);
    OriginalPairProximity result;result.distanceCm=collision_scalar::squareRoot(originalScalarAdd(mul(d[0],d[0]),mul(d[1],d[1])));
    float angle=0,pi=3.1415927410125732f;
    if(d[0]==0){if(d[1]!=0)angle=d[1]>=0?1.5707963705062866f:-1.5707963705062866f;}
    else {angle=collision_scalar::atan(collision_scalar::divide(d[1],d[0]));if(d[0]<0)angle=d[1]>0?originalScalarAdd(angle,pi):originalScalarSubtract(angle,pi);}
    result.bearingBToA=angle;float opposite=originalScalarAdd(angle,pi);
    float turns=originalScalarAdd(mul(opposite,.15915493667125702f),.5f);float whole=float(int(turns));if(turns<whole)whole=originalScalarSubtract(whole,1);
    result.bearingAToB=originalScalarSubtract(opposite,mul(whole,6.2831854820251465f));return result;
}

struct OriginalPairAttackState {
    terrain_original::Vector positionCm{},velocityCmps{},facing340{};
    int animationClass1=0;
    bool marker0=false,marker1=false;
    float resolvedAttackStat=0,strength350=0;
};
struct OriginalPairAttack {bool hit=false;terrain_original::Vector direction{};float impulseCmps=0;};
// Attack branch 0x107BD8..107E18 also runs when the ordinary body pair misses
// or was checked by the reciprocal owner earlier this tick.
inline OriginalPairAttack originalPairAttack(const OriginalPairAttackState& owner,
        const OriginalPairAttackState& other,const OriginalPairRecord& reciprocal,int32_t tick) {
    using namespace terrain_original;Rounding rounding;OriginalPairAttack result;
    if(owner.animationClass1!=13||!owner.marker0||owner.marker1)return result;
    if(!(reciprocal.lastAttackTick<int32_t(uint32_t(tick)-3u)))return result;
    if(other.animationClass1==3&&other.marker0&&!other.marker1)return result;
    auto offset=difference(other.positionCm,owner.positionCm);float length=terrain_original::sqrt(dot(offset,offset));
    if(length>150||length<.0010000000474974513f)return result;
    float inverse=div(1,length);for(unsigned k=0;k<3;++k)result.direction[k]=mul(offset[k],inverse);
    if(!(dot(result.direction,owner.facing340)>0)){result.direction={};return result;}
    float strength=originalScalarAdd(mul(owner.resolvedAttackStat,3.0001144409179688f),1);
    strength=mul(strength,originalScalarAdd(owner.strength350,.5f));result.impulseCmps=mul(strength,599.9739990234375f);
    float speed=terrain_original::sqrt(dot(owner.velocityCmps,owner.velocityCmps));
    if(speed>.0010000000474974513f){float factor=collision_scalar::divide(.5f,speed);for(unsigned k=0;k<3;++k)result.direction[k]=add(mul(result.direction[k],.5f),mul(owner.velocityCmps[k],factor));}
    result.hit=true;return result;
}

}
