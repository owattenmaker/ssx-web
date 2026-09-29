#include "air_trajectory.hpp"
#include "original_float.hpp"
#include "terrain_contact_math.hpp"
#include <algorithm>
#include <bit>
#include <cfenv>
#include <cmath>
#include <stdexcept>
#pragma STDC FENV_ACCESS ON
namespace ssx { namespace {
using V=std::array<float,3>;
constexpr float F(unsigned bits){return std::bit_cast<float>(bits);}
float A(float a,float b){return originalScalarAdd(a,b);}
float S(float a,float b){return originalScalarSubtract(a,b);}
float dot(V a,V b){return terrain_original::sum3(terrain_original::mul(a[0],b[0]),terrain_original::mul(a[1],b[1]),terrain_original::mul(a[2],b[2]));}
using Round=OriginalRounding;
V blend(V a,V b,float x,float y){V out;for(int i=0;i<3;i++){float first=terrain_original::mul(a[i],x),second=terrain_original::mul(b[i],y);out[i]=terrain_original::add(first,second);}return out;}
}
void OriginalAirTrajectory::begin(OriginalAirState current,float maximumSpeed){
 status=0;elapsed=predictedTime=apexTime=integratedTime=0;normal={0,0,1};heading={0,0,0};surface=patchId=-1;patchU=patchV=0;speedLimit=maximumSpeed;reseed(current);
}
void OriginalAirTrajectory::reseed(OriginalAirState current){status=0;prediction=integrated=current;predictedTime=elapsed;}
void OriginalAirTrajectory::extend(const OriginalAirTrajectoryQuery& query){
 Round round;const OriginalAirState before=prediction;
 V direction=before.velocity;float inverse=terrain_original::div(1.f,terrain_original::sqrt(dot(direction,direction)));for(float& x:direction)x=terrain_original::mul(x,inverse);
 float count=0,limit=originalScalarDivide(270000.f,speedLimit);
 do{
  float oldHeight=prediction.position[2];predictedTime=A(predictedTime,F(0x3c888889));count=A(count,1.f);
  float speed=0;prediction.step(speedLimit,&speed);
  if(oldHeight<=prediction.position[2]){apexTime=predictedTime;apexPosition=prediction.position;}
  if(limit<count||dot(direction,prediction.velocity)<terrain_original::mul(speed,F(0x3f7d70a4)))break;
 }while(true);
 V expansion,start,end;for(int i=0;i<3;i++){float distance=terrain_original::sub(prediction.position[i],before.position[i]);expansion[i]=terrain_original::mul(distance,F(0x3ca3d70a));start[i]=terrain_original::sub(before.position[i],expansion[i]);end[i]=terrain_original::add(prediction.position[i],expansion[i]);}
 auto hit=query(end,start,status);
 if(!hit.complete)throw OriginalAirTrajectoryUnavailable("Native air trajectory world query is incomplete: cause="+std::to_string(hit.unavailableCause)+" resource="+std::to_string(hit.unavailableResource)+" mode="+std::to_string(status));
 if(!(hit.fraction>=0)||!(dot(expansion,hit.normal)<0))return;
 float time=terrain_original::mul(hit.fraction,F(0x3c888889));time=terrain_original::mul(count,time);predictedTime=S(predictedTime,time);hitPosition=hit.position;
 heading=blend(before.velocity,prediction.velocity,hit.fraction,S(1.f,hit.fraction));
 if(status==2){status=3;surface=13;normal={0,0,1};}
 else{status=1;surface=hit.surface;normal=hit.normal;}
 if(hit.hasPatch&&status==1){patchFlags=hit.patchFlags;patchId=hit.patchId;patchU=hit.patchU;patchV=hit.patchV;}
 else{patchFlags=-1;patchId=-1;}
}
OriginalAirState OriginalAirTrajectory::step(float seconds,OriginalAirState current,const OriginalAirTrajectoryQuery& query){
 Round round;
 if(predictedTime>60.f){
  if(status==2){status=3;hitPosition={0,0,0};heading={1,0,0};normal={0,0,1};surface=13;}
  else{reseed(current);status=2;}
 }else{
  if(status==1&&S(predictedTime,elapsed)<F(0xbe4ccccd))reseed(current);
  int oldStatus=status;
  if(oldStatus==0||oldStatus==2){
   extend(query);
   while(status==oldStatus&&predictedTime<A(elapsed,seconds))extend(query);
  }
 }
 float remaining=A(S(elapsed,integratedTime),seconds);constexpr float tick=F(0x3c888889);
 while(remaining>=tick){remaining=S(remaining,tick);integrated.step(speedLimit);integratedTime=A(integratedTime,tick);}
 OriginalAirState out=integrated;
 if(remaining>F(0x3c23d70a)){
  auto next=integrated;next.step(speedLimit);float alpha=terrain_original::mul(remaining,F(0x426fffff)),beta=S(1.f,alpha);
  out.position=blend(next.position,integrated.position,alpha,beta);out.velocity=blend(next.velocity,integrated.velocity,alpha,beta);
 }
 elapsed=A(elapsed,seconds);return out;
}
}

namespace ssx { OriginalAirState OriginalAirTrajectory::stepLogic(float scale,OriginalAirState current,const OriginalAirTrajectoryQuery& query){Round round;return step(terrain_original::mul(scale,F(0x3c888889)),current,query);} }
