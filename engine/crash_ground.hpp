#pragma once
#include "crash_motion.hpp"
#include "ground_motion.hpp"
namespace ssx {
struct OriginalCrashSlidingAlignment {
    terrain_original::Vector normal{},heading{},relativeVelocityBeforeForces{};
    float gain=0,maximumRate=0;
};
// Original four-point curve evaluation at137EA4..137F34. This is the same
// authored surface+90 slip curve used by riding, with crash-specific damping.
inline float originalCrashSlipCurve(const GroundCurve& curve,float value) {
    using namespace terrain_original;
    if(value<curve[0].x)return curve[0].y;
    unsigned i=value>curve[1].x?(value>curve[2].x?2:1):0;
    if(value>curve[3].x)return curve[3].y;
    float difference=originalScalarSubtract(curve[i+1].y,curve[i].y);
    float numerator=mul(difference,originalScalarSubtract(value,curve[i].x));
    return originalScalarAdd(curve[i].y,collision_scalar::divide(numerator,originalScalarSubtract(curve[i+1].x,curve[i].x)));
}
// Original137D18 through its121AA0 call. Ground contact correction/airborne
// transition follows this integration and remains a separate ordered stage.
inline OriginalCrashSlidingAlignment originalCrashSlidingForces(const GroundSurface& surface,
        OriginalCrashMotionState& state,OriginalCrashActorState& actor,terrain_original::Vector surfaceVelocity,
        int animationClass2,terrain_original::Vector physicalForward) {
    using namespace terrain_original;Rounding rounding;
    float angularSpeed=terrain_original::sqrt(dot(state.angularVelocity,state.angularVelocity));
    float speed=terrain_original::sqrt(dot(actor.velocity,actor.velocity));float dt=mul(actor.timeScale,.01666666753590107f);
    float target=originalScalarAdd(angularSpeed,mul(mul(originalScalarSubtract(mul(speed,.012500000186264515f),angularSpeed),5),dt));
    if(angularSpeed>.0010000000474974513f){float factor=collision_scalar::divide(target,angularSpeed);for(auto& x:state.angularVelocity)x=mul(x,factor);}
    auto relative=difference(actor.velocity,surfaceVelocity);float normalSpeed=dot(relative,actor.groundNormal);Vector tangent;
    for(unsigned k=0;k<3;++k)tangent[k]=sub(relative[k],mul(actor.groundNormal[k],normalSpeed));
    float tangentSpeed=terrain_original::sqrt(dot(tangent,tangent));
    float damping=originalScalarAdd(mul(originalCrashSlipCurve(surface.slipFriction,mul(tangentSpeed,.035999998450279236f)),3),1.5f);
    if(tangentSpeed<1111.111083984375f&&(actor.detached||animationClass2==29||animationClass2==25)) {
        float weight=originalScalarSubtract(1,mul(tangentSpeed,.0009000000427477062f));weight=mul(weight,weight);weight=mul(weight,weight);
        damping=originalScalarAdd(mul(weight,40),mul(originalScalarSubtract(1,weight),damping));
    }
    damping=mul(damping,std::clamp(originalScalarAdd(actor.groundNormal[2],.30000001192092896f),0.f,1.f));
    damping=std::min(damping,collision_scalar::divide(1,dt));
    Vector acceleration;for(unsigned k=0;k<3;++k)acceleration[k]=add(k==2?-1800.f:0.f,mul(tangent[k],-damping));
    for(unsigned k=0;k<3;++k){actor.position[k]=add(actor.position[k],mul(actor.velocity[k],dt));actor.velocity[k]=add(actor.velocity[k],mul(acceleration[k],dt));}
    speed=terrain_original::sqrt(dot(actor.velocity,actor.velocity));if(speed>3333.33349609375f){float factor=collision_scalar::divide(3333.33349609375f,speed);for(auto& x:actor.velocity)x=mul(x,factor);}
    OriginalCrashSlidingAlignment result;result.normal=actor.groundNormal;result.relativeVelocityBeforeForces=relative;
    for(unsigned k=0;k<3;++k)result.heading[k]=add(physicalForward[k],mul(actor.velocity[k],.0036000001709908247f));
    float squared=dot(result.heading,result.heading);float reciprocal=squared>0?div(1,terrain_original::sqrt(squared)):std::numeric_limits<float>::max();for(auto& x:result.heading)x=mul(x,reciprocal);
    result.gain=originalScalarAdd(actor.timeScale,actor.timeScale);result.maximumRate=mul(actor.timeScale,6.632251739501953f);return result;
}
struct OriginalCrashGroundHit {
    float fraction=-1;
    terrain_original::Vector point{},normal{},surfaceVelocity{};
    int surface=0,surfaceProperty44=0,patchFlags=0;
    bool hasPatch=false,hasLiveEntity=false;
};
struct OriginalCrashGroundContactEffects {
    bool beginPredictor=false,requestReset=false,impact=false,dynamicCallbackRequired=false;
    float predictorSpeedLimit=3333.33349609375f,impactSpeed=0;
    terrain_original::Vector entityForce{};
};
// Original137D18 post-query stage. oldSurfaceDepth is +18 of the surface
// selected BEFORE the force/query stage; the hit can choose another material.
inline OriginalCrashGroundContactEffects originalCrashSlidingContact(OriginalCrashMotionState& state,
        OriginalCrashActorState& actor,const OriginalCrashGroundHit& hit,float bodyScale,float oldSurfaceDepth,
        terrain_original::Vector relativeVelocityBeforeForces) {
    using namespace terrain_original;Rounding rounding;OriginalCrashGroundContactEffects effects;
    if(hit.fraction<0){effects.beginPredictor=true;state.flag4=1;return effects;}
    actor.surfaceVelocity=hit.surfaceVelocity;actor.groundNormal=hit.normal;actor.surface=hit.surface;actor.contactPoint=hit.point;
    actor.contactDistance=dot(difference(actor.position,hit.point),hit.normal);
    float depth=mul(bodyScale,oldSurfaceDepth),distance=dot(difference(actor.position,hit.point),hit.normal);
    auto removeClosing=[&](){float closing=dot(actor.velocity,hit.normal);if(closing<0)for(unsigned k=0;k<3;++k)actor.velocity[k]=sub(actor.velocity[k],mul(hit.normal[k],closing));};
    if(distance< -depth) {
        effects.impact=true;effects.impactSpeed=terrain_original::sqrt(dot(hit.surfaceVelocity,hit.surfaceVelocity));removeClosing();
        float amount=originalScalarAdd(distance,depth);for(unsigned k=0;k<3;++k)actor.position[k]=sub(actor.position[k],mul(hit.normal[k],amount));
    } else if(distance<0) {
        effects.impact=true;effects.impactSpeed=terrain_original::sqrt(dot(actor.velocity,actor.velocity));
        float amount=mul(mul(actor.timeScale,.01666666753590107f),originalScalarSubtract(collision_scalar::divide(mul(distance,-2700),depth),dot(actor.velocity,hit.normal)));
        for(unsigned k=0;k<3;++k)actor.velocity[k]=add(actor.velocity[k],mul(hit.normal[k],amount));
    } else if(distance>10) {removeClosing();effects.beginPredictor=true;state.flag4=1;}
    if(hit.hasLiveEntity){effects.dynamicCallbackRequired=true;float inverse=div(1,mul(actor.timeScale,.01666666753590107f));for(unsigned k=0;k<3;++k)effects.entityForce[k]=mul(add(sub(relativeVelocityBeforeForces[k],actor.velocity[k]),actor.surfaceVelocity[k]),inverse);}
    effects.requestReset=hit.surfaceProperty44!=0||(hit.hasPatch&&(hit.patchFlags&2));
    return effects;
}

}
