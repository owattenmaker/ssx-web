#pragma once
#include "crash_motion.hpp"
#include "animation_motion.hpp"
#include "body_collision.hpp"
#include "collision_event.hpp"
#include <bit>
namespace ssx {
// Original137550: four unscaled20cm spheres along the posed board's X axis.
// The scalar matrix expansion order differs from11E098 and is intentional.
inline BodyCollisionVolume originalCrashDetachedBody(AnimationTransform posedSecondary,float bodyScale,uint32_t mask=0xffffffff) {
    using namespace terrain_original;Rounding rounding;auto q=posedSecondary.rotation;
    float yy=mul(q[1],q[1]),zz=mul(q[2],q[2]),xz=mul(q[0],q[2]),wy=mul(q[3],q[1]);
    float yz=originalScalarAdd(yy,zz),xy=mul(q[0],q[1]),wz=mul(q[3],q[2]);
    float z=originalScalarSubtract(xz,wy),y=originalScalarAdd(xy,wz);
    Vector axis{originalScalarSubtract(1,originalScalarAdd(yz,yz)),originalScalarAdd(y,y),originalScalarAdd(z,z)};
    for(auto& x:axis)x=mul(x,bodyScale);
    BodyCollisionVolume result;result.broadCenterCm=posedSecondary.position;result.broadRadiusCm=mul(bodyScale,90);result.count=4;result.activeMask=mask;
    constexpr float offsets[4]={70,30,-30,-70};
    for(unsigned n=0;n<4;++n){result.spheres[n].radiusCm=20;for(unsigned k=0;k<3;++k){float offset=mul(axis[k],std::abs(offsets[n]));result.spheres[n].centerCm[k]=offsets[n]<0?sub(posedSecondary.position[k],offset):add(posedSecondary.position[k],offset);}}
    return result;
}
inline float originalCrashRandomRange(float low,float high,uint32_t word) {
    using namespace terrain_original;Rounding rounding;float unit=originalScalarSubtract(std::bit_cast<float>((word&0x7fffffu)|0x3f800000u),1);
    return originalScalarAdd(low,mul(originalScalarSubtract(high,low),unit));
}
struct OriginalCrashDetachedContactEffects {
    bool moved=false,bounced=false,stopped=false,dynamicCallbackRequired=false;
    terrain_original::Vector translation{},entityForce{};
    unsigned randomDraws=0;
};
// Original137138 after336850 returned the detached-body contact. It changes
// secondary130, the secondary collision volume and bounds; actor110 is untouched.
inline OriginalCrashDetachedContactEffects originalCrashDetachedBodyResponse(OriginalCrashMotionState& state,
        OriginalCrashActorState& actor,float penetration,terrain_original::Vector normal,bool hasLiveEntity,
        int sourceTicksPerSecond,const CollisionRandom& random) {
    using namespace terrain_original;Rounding rounding;OriginalCrashDetachedContactEffects result;if(!actor.detached||penetration<0)return result;
    result.moved=true;for(unsigned k=0;k<3;++k){result.translation[k]=mul(mul(normal[k],1.100000023841858f),penetration);actor.detachedPosition[k]=add(actor.detachedPosition[k],result.translation[k]);state.low[k]=add(state.low[k],result.translation[k]);state.high[k]=add(state.high[k],result.translation[k]);}
    Vector negative;for(unsigned k=0;k<3;++k)negative[k]=mul(normal[k],-1);float closing=dot(negative,state.detachedVelocity);
    if(closing>=555.5555419921875f||normal[2]<.800000011920929f) {
        result.bounced=true;float amount=originalScalarAdd(closing,std::max(mul(closing,.5f),27.77777862548828f));Vector delta;
        for(unsigned k=0;k<3;++k){delta[k]=mul(normal[k],amount);state.detachedVelocity[k]=add(state.detachedVelocity[k],delta[k]);}
        if(hasLiveEntity){result.dynamicCallbackRequired=true;float factor=mul(float(sourceTicksPerSecond),-.05000000074505806f);for(unsigned k=0;k<3;++k)result.entityForce[k]=mul(delta[k],factor);}
        for(auto& x:state.detachedAngularVelocity)x=mul(x,.8500000238418579f);
        if(terrain_original::sqrt(dot(state.detachedAngularVelocity,state.detachedAngularVelocity))<12.56637191772461f) {
            auto draw=[&](float low,float high){if(!random)throw std::runtime_error("Shared crash RNG unavailable");++result.randomDraws;return originalCrashRandomRange(low,high,random());};
            float pitch=draw(-1.5707963705062866f,1.5707963705062866f),yaw=draw(-3.1415927410125732f,3.1415927410125732f);
            auto p=collision_scalar::sincos(pitch),y=collision_scalar::sincos(yaw);float magnitude=mul(draw(45,180),.01745329424738884f);
            Vector axis{mul(p[1],y[0]),mul(p[1],y[1]),p[0]};for(unsigned k=0;k<3;++k)state.detachedAngularVelocity[k]=add(state.detachedAngularVelocity[k],mul(axis[k],magnitude));
        }
    } else {
        result.stopped=true;if(hasLiveEntity){result.dynamicCallbackRequired=true;float factor=mul(float(sourceTicksPerSecond),.05000000074505806f);for(unsigned k=0;k<3;++k)result.entityForce[k]=mul(state.detachedVelocity[k],factor);}
        state.detachedVelocity={};state.detachedAngularVelocity={};
    }
    return result;
}
}
