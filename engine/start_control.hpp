#pragma once
#include "terrain_contact_math.hpp"
#include <vector>
namespace ssx {
struct OriginalStartControlState {int phase=0;float steadyTime=0,low=0,high=0,pose=0;};
struct OriginalStartEntry {int semantic=0;bool mirror=false;float rootHalfAngle=0;};
//12BE20: caller copies reference stance, clears animation, sets zero root
//translation and this root angle, then plays the selected semantic at default rate.
inline OriginalStartEntry originalStartControlEnter(OriginalStartControlState&s,bool referenceStance){
 const bool manual=s.phase==2;s={};s.phase=manual?2:0;
 return {manual?1:0,referenceStance,referenceStance?-1.5707963705062866f:-0.f};
}
struct OriginalStartControlInput {uint32_t command=0;int gamePhase=4,raceTicks=0,delaySeconds=0,motion=3,semantic=0;float finishElapsed=-1,mainTime=0,tilt=0;terrain_original::Vector forward{0,1,0};};
struct OriginalStartAction {enum Kind{StopBoost,Tilt,Crouch,Velocity,Motion,Play,Control} kind;int value=0;float amount=0;terrain_original::Vector vector{};};
//12C028/12C078. The source compares truncated race seconds to rider+B30.
inline bool originalStartReleaseAllowed(const OriginalStartControlInput&i){OriginalRounding round;return i.gamePhase==5&&int32_t(terrain_original::mul(float(i.raceTicks),.01666666753590107f))>=i.delaySeconds;}
inline bool originalStartContinueAllowed(const OriginalStartControlInput&i){return i.gamePhase==5||i.finishElapsed>=0;}
//12BF68 with12C230/12C408/12C0C0/12C130. Caller applies action requests in
//order; Motion0 is the normal successful start-motion handoff.
inline std::vector<OriginalStartAction> originalStartControlStep(OriginalStartControlState&s,const OriginalStartControlInput&i){
 OriginalRounding round;using namespace terrain_original;std::vector<OriginalStartAction> out{{OriginalStartAction::StopBoost}};
 auto emit=[&](OriginalStartAction::Kind kind,int value=0,float amount=0){out.push_back({kind,value,amount,{}});};
 auto launch=[&](float speed){auto v=i.forward;for(auto&x:v)x=mul(x,speed);out.push_back({OriginalStartAction::Velocity,0,0,v});emit(OriginalStartAction::Motion,0);};
 const unsigned raw=(i.command>>12)&63;const float axis=mul(float(raw>=32?int(raw)-64:int(raw)),.032258063554763794f);
 if(s.phase==0){
  const float target=axis>0?.6200000047683716f:axis<0?0.f:.3100000023841858f,previous=s.pose;
  emit(OriginalStartAction::Tilt,0,i.tilt);const float difference=std::abs(originalScalarSubtract(target,s.pose));
  float rate=difference>=.20000000298023224f?mul(difference,4.5f):.9000000357627869f;rate=mul(rate,.01666666753590107f);
  if(originalScalarAdd(target,rate)<s.pose)s.pose=originalScalarSubtract(s.pose,rate);
  else if(s.pose<originalScalarSubtract(target,rate))s.pose=originalScalarAdd(s.pose,rate);else s.pose=target;
  s.steadyTime=originalScalarAdd(s.steadyTime,.01666666753590107f);
  if(previous!=s.pose){s.steadyTime=0;if(previous<s.pose)s.high=s.pose;else s.low=s.pose;}
  if(!originalStartReleaseAllowed(i)||!originalStartContinueAllowed(i))return out;
  float speed=0;
  if(target>.5f&&s.steadyTime<.4000000059604645f){speed=originalScalarDivide(mul(mul(originalScalarSubtract(s.high,s.low),2027.77783203125f),originalScalarSubtract(1.f,s.pose)),originalScalarAdd(s.steadyTime,1.f));}
  if(speed<555.5555419921875f)speed=555.5555419921875f;s.high=speed;s.phase=1;
 }else if(s.phase==1){
  emit(OriginalStartAction::Tilt,0,i.tilt);
  if(s.pose>1.0149999856948853f)s.pose=originalScalarSubtract(s.pose,.015000001527369022f);
  else if(s.pose<.9850000143051147f)s.pose=originalScalarAdd(s.pose,.015000001527369022f);else s.pose=1;
  emit(OriginalStartAction::Crouch,0,1);int motion=i.motion;
  if(motion==3&&s.pose>=.6200000047683716f){launch(s.high);motion=0;}
  if(s.pose==1){emit(OriginalStartAction::Play,6);emit(OriginalStartAction::Control,motion==1?4:0);}
 }else if(s.phase==2){if(axis>0){emit(OriginalStartAction::Play,2);s.phase=3;}}
 else if(s.phase==3){
  emit(OriginalStartAction::Crouch,0,1);
  if(i.semantic==2){if(i.motion==3&&i.mainTime>.5f)launch(555.5555419921875f);}
  else {emit(OriginalStartAction::Control,i.motion==1?4:0);s.phase=0;}
 }
 return out;
}
}
