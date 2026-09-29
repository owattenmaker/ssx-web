#pragma once
#include "crash_ground.hpp"
#include "obstacle_collision.hpp"
namespace ssx {
// Original138640 after its coarse (3x3) body query and optional entity-normal
// callback. Different restitution from cruise: half closing speed, capped555.
inline ObstacleResponse originalCrashSlidingBodyResponse(float penetration,terrain_original::Vector groundNormal,
        terrain_original::Vector hitNormal,terrain_original::Vector velocity,terrain_original::Vector surfaceVelocity) {
    using namespace terrain_original;Rounding rounding;ObstacleResponse result;result.velocityCmps=velocity;
    if(penetration<0)return result;float alignment=dot(groundNormal,hitNormal);
    if(!(-.9998999834060669f<alignment))return result;
    if(alignment<0){for(unsigned k=0;k<3;++k)hitNormal[k]=sub(hitNormal[k],mul(groundNormal[k],alignment));float squared=dot(hitNormal,hitNormal),inverse=squared>0?div(1,terrain_original::sqrt(squared)):std::numeric_limits<float>::max();for(auto& x:hitNormal)x=mul(x,inverse);}
    result.accepted=result.moved=true;result.normal=hitNormal;Vector negative;
    for(unsigned k=0;k<3;++k){result.translationCm[k]=mul(mul(hitNormal[k],1.100000023841858f),penetration);negative[k]=-hitNormal[k];}
    float closing=dot(negative,difference(velocity,surfaceVelocity));result.closingSpeedCmps=closing;if(closing<0)return result;
    float bounce=originalScalarAdd(closing,std::clamp(mul(closing,.5f),27.77777862548828f,555.5555419921875f));
    for(unsigned k=0;k<3;++k)result.velocityCmps[k]=add(velocity[k],mul(hitNormal[k],bounce));result.bounced=true;return result;
}
// 1388D4..138934 follows scenery and rider-pair callbacks, so it sees their
// final velocity and pending airborne flag. Do not run this before interactions.
inline void originalCrashSlidingFinish(OriginalCrashMotionState& state,OriginalCrashActorState& actor) {
    using namespace terrain_original;Rounding rounding;if(state.flag4){state.flag4=0;state.submode=1;}
    if(terrain_original::sqrt(dot(actor.velocity,actor.velocity))<50)actor.velocity={};
}
struct OriginalCrashAirContactEffects {
    bool moved=false,landed=false,beginPredictor=false,impact=false,dynamicCallbackRequired=false;
    float impactSpeed=0,predictorSpeedLimit=3333.33349609375f;
    terrain_original::Vector translation{},impactVelocity{},entityForce{};
};
// 0x137860 main-body response. Call138960 only after the body query hits,
// using the body-hit normal as the terrain probe direction. Translation also
// moves cached AA0 and bounds via106538; callers apply those companion fields.
inline OriginalCrashAirContactEffects originalCrashAirBodyResponse(OriginalCrashMotionState& state,
        OriginalCrashActorState& actor,float bodyPenetration,terrain_original::Vector bodyNormal,
        const OriginalCrashGroundHit& terrainHit,int sourceTicksPerSecond) {
    using namespace terrain_original;Rounding rounding;OriginalCrashAirContactEffects effects;if(bodyPenetration<0)return effects;
    float normalSpeed=0,distance=0;
    if(terrainHit.fraction>=.4000000059604645f){normalSpeed=dot(actor.velocity,terrainHit.normal);distance=dot(difference(actor.position,terrainHit.point),terrainHit.normal);}
    if(terrainHit.fraction>=.4000000059604645f&&normalSpeed<=0&&distance<=10) {
        effects.landed=effects.impact=true;effects.impactSpeed=normalSpeed;effects.impactVelocity=actor.velocity;
        actor.surfaceVelocity=terrainHit.surfaceVelocity;actor.groundNormal=terrainHit.normal;actor.surface=terrainHit.surface;actor.contactPoint=terrainHit.point;
        actor.contactDistance=dot(difference(actor.position,terrainHit.point),terrainHit.normal);
        Vector removed;for(unsigned k=0;k<3;++k){removed[k]=mul(terrainHit.normal[k],-normalSpeed);actor.velocity[k]=add(actor.velocity[k],removed[k]);}
        if(terrainHit.hasLiveEntity){effects.dynamicCallbackRequired=true;for(unsigned k=0;k<3;++k)effects.entityForce[k]=mul(mul(removed[k],-1),float(sourceTicksPerSecond));}
        distance=dot(difference(actor.position,terrainHit.point),terrainHit.normal);
        if(distance<0){effects.moved=true;for(unsigned k=0;k<3;++k)effects.translation[k]=mul(terrainHit.normal[k],-distance);}
        state.submode=0;
    } else {
        effects.moved=effects.beginPredictor=true;for(unsigned k=0;k<3;++k)effects.translation[k]=mul(mul(bodyNormal[k],1.100000023841858f),bodyPenetration);
        if(terrainHit.fraction<0||dot(terrainHit.normal,bodyNormal)<.8999999761581421f){float closing=dot(bodyNormal,actor.velocity);if(closing<0)for(unsigned k=0;k<3;++k)actor.velocity[k]=sub(actor.velocity[k],mul(bodyNormal[k],closing));}
    }
    if(effects.moved)for(unsigned k=0;k<3;++k)actor.position[k]=add(actor.position[k],effects.translation[k]);
    return effects;
}
}
