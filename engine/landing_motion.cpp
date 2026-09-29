#include "landing_motion.hpp"
#include "original_float.hpp"
#include "terrain_contact_math.hpp"
#include <algorithm>
#include <bit>
#include <cfenv>
#include <cmath>
#include <stdexcept>
#pragma STDC FENV_ACCESS ON
namespace ssx {namespace {
using V=std::array<float,3>;
constexpr float F(uint32_t b){return std::bit_cast<float>(b);}
float A(float a,float b){return originalScalarAdd(a,b);}float S(float a,float b){return originalScalarSubtract(a,b);}
using Round=OriginalRounding;
float dot(V a,V b){float x=terrain_original::mul(a[0],b[0]),y=terrain_original::mul(a[1],b[1]),z=terrain_original::mul(a[2],b[2]);float sum=terrain_original::add(x,y);sum=terrain_original::add(sum,z);return terrain_original::add(sum,0.f);}
V difference(V a,V b){for(unsigned i=0;i<3;i++)a[i]=terrain_original::sub(a[i],b[i]);return a;}
V cross(V a,V b){return {terrain_original::sub(terrain_original::mul(a[1],b[2]),terrain_original::mul(a[2],b[1])),terrain_original::sub(terrain_original::mul(a[2],b[0]),terrain_original::mul(a[0],b[2])),terrain_original::sub(terrain_original::mul(a[0],b[1]),terrain_original::mul(a[1],b[0]))};}
}
OriginalLandingImpact originalLandingImpact(const OriginalLandingState& s,const OriginalWorldSegmentHit& hit){
 Round round;OriginalLandingImpact out;if(!(hit.fraction>=.5f))return out;
 out.relativeNormalSpeed=dot(difference(s.rider.velocity,hit.surfaceVelocityCmps),hit.normal);
 if(!(out.relativeNormalSpeed<0))return out;out.contact=true;out.speedBefore=terrain_original::sqrt(dot(s.rider.velocity,s.rider.velocity));return out;
}
void originalLandingVelocityResponse(OriginalGroundState& r,const OriginalLandingMaterial& m,float impact){
 Round round;
 for(unsigned i=0;i<3;i++){float impulse=terrain_original::mul(r.normal[i],(-impact));impulse=terrain_original::mul(impulse,m.normalImpulseFactor);r.velocity[i]=terrain_original::add(r.velocity[i],impulse);}
 float speed=-dot(r.velocity,r.normal);
 if(m.maximumNormalSpeed<speed){float extra=S(speed,m.maximumNormalSpeed);for(unsigned i=0;i<3;i++)r.velocity[i]=terrain_original::add(r.velocity[i],terrain_original::mul(r.normal[i],extra));}
}
bool originalLandingResolveContact(OriginalLandingState& s,const OriginalWorldSegmentHit& hit,const OriginalLandingMaterial& m,float impact){
 Round round;s.trajectoryPredictionTime=0;s.translationCm={};
 if(m.recovery||(hit.hasPatch&&(hit.patchFlags&2)))return false;
 auto& r=s.rider;float penetration=S(dot(difference(hit.position,r.position),hit.normal),m.depth3);
 if(penetration>0)for(unsigned i=0;i<3;i++){s.translationCm[i]=terrain_original::mul(hit.normal[i],penetration);r.position[i]=terrain_original::add(r.position[i],s.translationCm[i]);}
 r.normal=r.previousNormal=hit.normal;r.surfaceVelocity=hit.surfaceVelocityCmps;s.impactNormalSpeed=-impact;
 if(hit.hasPatch){s.patchId=uint32_t(hit.patchId);s.patchU=hit.patchU;s.patchV=hit.patchV;}else s.patchId=0xffffffffu;
 originalLandingVelocityResponse(r,m,impact);
 float along=dot(r.physicalForward,r.normal);V forward;for(unsigned i=0;i<3;i++)forward[i]=terrain_original::sub(r.physicalForward[i],terrain_original::mul(r.normal[i],along));
 float norm=dot(forward,forward);if(!(norm>0))throw std::runtime_error("Degenerate original landing tangent requires VU boundary handling");
 float inverse=terrain_original::div(1.f,terrain_original::sqrt(norm));for(unsigned i=0;i<3;i++)r.forward[i]=terrain_original::mul(forward[i],inverse);r.lateral=cross(r.normal,r.forward);
 s.surface=hit.surface;s.groundPoint=hit.position;r.distance=dot(difference(r.position,hit.position),r.normal);return true;
}
OriginalLandingChoice originalLandingClassify(const OriginalLandingState& s,const OriginalLandingClassification& c){
 Round round;OriginalLandingChoice out;auto flag=[&](unsigned n){return bool(c.animationFlags&(uint64_t(1)<<n));};
 if(c.animationClass==20||c.animationClass==21){if(!flag(c.animationClass==20?2:3))out.crashAnimation=0x169;return out;}
 const auto& r=s.rider;float up=dot(r.boardUp,r.normal);
 bool crash=(c.animationClass==19&&!flag(2))||(c.animationClass==18&&!flag(2)&&flag(0))||up<0;
 if(crash){
  float right=dot(s.physicalRight,r.normal),forward=dot(r.physicalForward,r.normal);
  if(std::abs(right)<std::abs(forward)&&std::abs(up)<std::abs(forward))out.crashAnimation=forward<0?0x162:0x163;
  else if(std::abs(right)<std::abs(up)&&std::abs(forward)<std::abs(up)){
   if(up<0)out.crashAnimation=0x15e;
   else{out.consumedRandom=true;float speed=terrain_original::sqrt(dot(r.velocity,r.velocity));unsigned choice=c.randomWord&(speed>F(0x450ae38e)?3u:1u);out.crashAnimation=choice==0?0x160:choice==1?0x161:0x15f;}
  }else out.crashAnimation=right<0?0x165:0x164;
 }else if(c.manualState330==0){
  float angle=dot(r.boardUp,r.forward);float factor=A(terrain_original::mul(c.landingStat,F(0x3e99986c)),1.f);
  if(terrain_original::mul(factor,F(0x3f3330bb))<angle)out.crashAnimation=0x15c;else if(angle<terrain_original::mul(factor,F(0xbf3330bb)))out.crashAnimation=0x15d;
 }
 return out;
}
void originalLandingGroundLeave(OriginalGroundState& r,uint32_t tick,uint32_t& last){
 r.boardAlignment.rate=F(0x3d4cccce);r.boardAlignment.target=0;
 r.extraLean.rate=F(0x3d4cccce);r.extraLean.target=0;
 r.presentationLift.rate=F(0x3fd55556);r.presentationLift.target=0;
 last=last==0xffffffffu?tick-500:tick;
}
void originalLandingGroundEnter(OriginalGroundState& r,const OriginalLandingMaterial& material,float bodyScale,uint32_t tick,uint32_t leave){
 Round round;r.boardBouncePhase=0;r.depth1=terrain_original::mul(bodyScale,material.depth1);r.depth3=terrain_original::mul(bodyScale,material.depth3);r.boardNormal=r.normal;
 int32_t duration=std::bit_cast<int32_t>(tick-leave);float factor=F(0x3f333333);
 if(duration>40){float time=float(duration-40);factor=A(terrain_original::mul(time,F(0x3c23d70b)),factor);}factor=std::min(factor,1.f);
 for(float& v:r.velocity)v=terrain_original::mul(v,factor);
}
int originalLandingAnimation(float impact,float& spin){
 if(impact<F(0xc50ae38e)){spin=0;return 0x3f;}
 if(spin<-F(0x40490fdc))return 0x42;if(spin>F(0x40490fdc))return 0x43;
 return impact<F(0xc49c4000)?0x3e:0x3d;
}
}

namespace ssx {int originalReverseLandingAnimation(float impact,float& spin){if(impact<F(0xc50ae38e)){spin=0;return 0x41;}return 0x40;} }
