#include "snow_context.hpp"
#include "rider_pose_motion.hpp"
#include "terrain_contact_math.hpp"
#include "original_float.hpp"
#include <cmath>
namespace ssx {
OriginalSnowBoardFrame originalSnowUnitBoardFrame(const AnimationTransform& board){
 auto frame=originalRiderCollisionFrame(board);return {frame.right,frame.forward,frame.up,frame.origin};
}
SnowColour originalSnowEnvironmentColour(SnowColour c){terrain_original::Rounding rounding;return {terrain_original::mul(c[1],2),terrain_original::mul(c[2],2),terrain_original::mul(c[3],2),1};}
bool originalSnowImpactTrigger(OriginalSnowContextState&s,SnowVector position,SnowVector normal,float strength,int surface,bool kind,const OriginalSnowRiderInput&i){
 using namespace terrain_original;Rounding rounding;auto delta=difference(s.impact.positionCm,position);float distance=std::abs(terrain_original::sqrt(dot(delta,delta)));strength=std::abs(strength);
 bool special=i.trackingInhibited&&!i.trackingAD0&&i.trackingAFC&&i.trackingB00;
 if(!(s.impact.strength<strength)||(!((s.impact.wideScatter?100.f:180.f)<distance)&&!special))return false;
 s.impact.strength=kind?strength:originalScalarAdd(strength,strength);s.impact.positionCm=position;s.impact.normal=normal;s.impact.kind=kind;s.impactSurface=surface;return true;
}
void originalSnowVisibility(OriginalSnowContextState&s,int mode,bool tracking){
 s.visibilityMode=mode;bool wide=s.impact.wideScatter;auto&e=s.emitterEnabled;
 if(mode==0){e={true,wide,wide,wide,wide||tracking,true,true,wide,wide,wide};}
 else if(mode==1){e={true,wide,wide,wide,false,true,true,wide,wide,false};}
 else{e={false,false,false,false,false,true,wide,false,false,false};}
}
OriginalSnowContext originalSnowContextStep(OriginalSnowContextState&s,const OriginalSnowContextInput&i,const OriginalSnowSurface&surface){
 using namespace terrain_original;Rounding rounding;OriginalSnowContext result;result.rider=originalSnowRiderCache(i.rider);auto&cache=result.rider;
 float deceleration=mul(originalScalarSubtract(s.previousSpeedCmps,cache.speedCmps),59.999996185302734f);
 bool request=555.5555419921875f<deceleration&&555.5555419921875f<cache.speedCmps&&i.rider.controlState!=9;
 float amount=mul(deceleration,.5f);
 if(i.rider.trackingInhibited&&!i.rider.trackingAD0&&i.rider.trackingAFC){request=27.77777862548828f<=deceleration;amount=deceleration;}
 if(cache.groundEmission&&request){SnowVector position;for(unsigned k=0;k<3;++k)position[k]=add(i.board.originCm[k],mul(i.groundNormal[k],10));result.impactTriggered=originalSnowImpactTrigger(s,position,i.groundNormal,amount,surface.id,true,i.rider);}
 s.previousSpeedCmps=cache.speedCmps;if(s.visibilityMode!=i.visibilityMode)originalSnowVisibility(s,i.visibilityMode,i.rider.trackingInhibited);
 auto colour=originalSnowEnvironmentColour(i.environmentARGB);auto&t=result.trail;t.board=i.board;t.velocityCmps=i.rider.velocityCmps;t.groundNormal=i.groundNormal;t.colour=colour;t.absoluteSpeedCmps=std::abs(cache.speedCmps);t.edgeBias=cache.edgeBias;t.groundEmission=cache.groundEmission;t.surfaceActive=surface.trailActive;t.suppressed=i.suppressed;
 auto&c=result.cloud;c.trail=t;c.trail.surfaceActive=surface.cloudActive;c.lateral=i.lateral;c.direction=cache.direction;c.turn=i.rider.turn;c.turnAmount=cache.turnAmount;c.maxHeightCm=surface.cloudMaxHeightCm;c.reverse=i.rider.reverse;
 auto&chunk=result.chunks;chunk.cloud=c;chunk.brake=i.rider.brake;chunk.secondaryBrake274=i.secondaryBrake274;chunk.chanceScales={surface.smallChunkChance,surface.largeChunkChance};chunk.impact=s.impact;chunk.motionMode=i.rider.motionMode;chunk.largeImpactActive=surface.largeImpactActive;
 if(surface.chunksUseWakeVelocity)chunk.wakeVelocity=i.wakeVelocity;chunk.minWakeScale=surface.minWakeVelocityScale;chunk.maxWakeScale=surface.maxWakeVelocityScale;
 auto&impact=result.impact;impact.boardOriginCm=i.board.originCm;impact.velocityCmps=i.rider.velocityCmps;impact.groundNormal=i.groundNormal;impact.colour=colour;impact.smallActive=surface.impactActive;impact.largeActive=surface.largeImpactActive;impact.trackingInhibited=i.rider.trackingInhibited;impact.decay=surface.impactMultiplier;impact.motionMode=i.rider.motionMode;
 return result;
}
}
