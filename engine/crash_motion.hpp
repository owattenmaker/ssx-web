#pragma once
#include "terrain_contact_math.hpp"
#include "collision_scalar.hpp"
#include "air_trajectory.hpp"
#include "air_alignment.hpp"

namespace ssx {
struct OriginalCrashMotionState {
    int submode=0;                    // motion owner+30: 0 sliding, 1 airborne
    int flag4=0;
    terrain_original::Vector detachedVelocity{},detachedAngularVelocity{},angularVelocity{}; // +10/+20/+30
    terrain_original::Vector low{},high{}; // +50/+60
};
struct OriginalCrashActorState {
    terrain_original::Vector position{},velocity{},groundNormal{0,0,1};
    terrain_original::Vector surfaceVelocity{},contactPoint{};
    float contactDistance=0;int surface=0;
    std::array<float,4> quaternion{0,0,0,1};
    terrain_original::Vector detachedPosition{};
    std::array<float,4> detachedQuaternion{0,0,0,1};
    bool detached=false;             // rider+150
    float timeScale=1,spinRate=0,flipRate=0; // +300/+2DC/+2E0
};
struct OriginalCrashMotionEntry {
    bool beginPredictor=false;
    float predictorSpeedLimit=3333.33349609375f;
};
// 0x136C40. Storage allocation is native-owned; this is the real motion2
// state/velocity initialization, not a switch to ordinary riding physics.
inline OriginalCrashMotionEntry originalCrashMotionBegin(OriginalCrashMotionState& state,
        OriginalCrashActorState& actor,int previousMotionMode) {
    using namespace terrain_original;Rounding rounding;OriginalCrashMotionEntry result;
    state.low=state.high=actor.position;
    if(previousMotionMode==0){float normalSpeed=dot(actor.velocity,actor.groundNormal);for(unsigned k=0;k<3;++k)actor.velocity[k]=sub(actor.velocity[k],mul(actor.groundNormal[k],normalSpeed));state.submode=0;}
    else {state.submode=1;result.beginPredictor=true;}
    state.flag4=0;actor.spinRate=actor.flipRate=0;return result;
}
// 0x136D40: detach the selected presented bone as a separately moving root.
inline void originalCrashDetach(OriginalCrashMotionState& state,OriginalCrashActorState& actor,
        terrain_original::Vector position,std::array<float,4> quaternion,
        terrain_original::Vector velocity,terrain_original::Vector angularVelocity) {
    using namespace terrain_original;Rounding rounding;
    actor.detachedPosition=position;actor.detachedQuaternion=quaternion;actor.detached=true;
    state.detachedVelocity=velocity;state.detachedAngularVelocity=angularVelocity;
    if(state.submode==0){float amount=dot(actor.groundNormal,velocity);for(unsigned k=0;k<3;++k)state.detachedVelocity[k]=sub(velocity[k],mul(actor.groundNormal[k],amount));}
}
// 0x136F30, called before either sliding or airborne crash first-phase motion.
inline void originalCrashDetachedStep(OriginalCrashMotionState& state,OriginalCrashActorState& actor) {
    using namespace terrain_original;Rounding rounding;if(!actor.detached)return;
    float dt=mul(actor.timeScale,.01666666753590107f);
    for(unsigned k=0;k<3;++k)actor.detachedPosition[k]=add(actor.detachedPosition[k],mul(state.detachedVelocity[k],dt));
    Vector acceleration{mul(state.detachedVelocity[0],-.20000000298023224f),mul(state.detachedVelocity[1],-.20000000298023224f),-1800};
    for(unsigned k=0;k<3;++k)state.detachedVelocity[k]=add(state.detachedVelocity[k],mul(acceleration[k],dt));
    float speed=terrain_original::sqrt(dot(state.detachedVelocity,state.detachedVelocity));
    if(speed>3333.33349609375f){float factor=collision_scalar::divide(3333.33349609375f,speed);for(auto& x:state.detachedVelocity)x=mul(x,factor);}
    float damping=originalScalarSubtract(1,mul(dt,.5f));for(auto& x:state.detachedAngularVelocity)x=mul(x,damping);
    auto q=actor.detachedQuaternion;auto w=state.detachedAngularVelocity;Vector qv{q[0],q[1],q[2]};auto crossValue=cross(w,qv);
    std::array<float,4> derivative;
    for(unsigned k=0;k<3;++k)derivative[k]=add(mul(mul(w[k],q[3]),.5f),mul(crossValue[k],.5f));
    derivative[3]=add(add(mul(-.5f,mul(q[0],w[0])),mul(-.5f,mul(q[1],w[1]))),mul(-.5f,mul(q[2],w[2])));
    for(unsigned k=0;k<4;++k)q[k]=originalScalarAdd(q[k],mul(derivative[k],dt));
    float squared=add(add(add(mul(q[0],q[0]),mul(q[1],q[1])),mul(q[2],q[2])),mul(q[3],q[3]));
    float inverse=div(1,terrain_original::sqrt(squared));for(unsigned k=0;k<4;++k)actor.detachedQuaternion[k]=mul(q[k],inverse);
}
struct OriginalCrashAirAlignmentRequest {
    bool align=false;
    float gain=0,maximumRate=0;
};
// Post-predictor caller arguments and angular decay in 0x137750. The predictor
// must already have advanced the actor position/velocity with 0x113648.
inline OriginalCrashAirAlignmentRequest originalCrashAirAlignmentRequest(OriginalCrashMotionState& state,
        float timeScale,int trajectoryStatus,float predictedTime,float elapsedTime) {
    using namespace terrain_original;Rounding rounding;OriginalCrashAirAlignmentRequest result;
    if(trajectoryStatus==1||trajectoryStatus==3) {
        result.align=true;float remaining=originalScalarSubtract(predictedTime,elapsedTime);
        result.gain=remaining>=.01666666753590107f?collision_scalar::divide(timeScale,remaining):mul(timeScale,59.999996185302734f);
        result.maximumRate=mul(timeScale,6.632251739501953f);
    }
    float dt=mul(timeScale,.01666666753590107f),damping=originalScalarSubtract(1,mul(dt,1.5f));
    for(auto& x:state.angularVelocity)x=mul(x,damping);
    return result;
}
// Complete airborne first motion phase, 0x137750. The native predictor and
// physical orientation helpers are shared with ordinary flight, as original.
inline void originalCrashAirFirstPhase(OriginalCrashMotionState& state,OriginalCrashActorState& actor,
        OriginalAirTrajectory& trajectory,const OriginalAirTrajectoryQuery& query) {
    using namespace terrain_original;Rounding rounding;
    if(state.submode!=1)throw std::runtime_error("Air crash phase requested for sliding crash");
    auto current=trajectory.step(mul(actor.timeScale,.01666666753590107f),{actor.position,actor.velocity},query);
    actor.position=current.position;actor.velocity=current.velocity;
    auto request=originalCrashAirAlignmentRequest(state,actor.timeScale,trajectory.status,trajectory.predictedTime,trajectory.elapsed);
    if(request.align)actor.quaternion=originalAirAlignment(actor.quaternion,trajectory.normal,trajectory.heading,request.gain,request.maximumRate).quaternion;
    actor.quaternion=originalRebuildOrientation(actor.quaternion).quaternion;
}
}
