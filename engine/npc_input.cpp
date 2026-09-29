#include "npc_input.hpp"
#include "npc_air.hpp"
#include <bit>
#include <cfenv>
#include <cmath>
#include <stdexcept>
#pragma STDC FENV_ACCESS ON
#include "npc_float.hpp"
namespace ssx {namespace {
struct Round{int previous=std::fegetround();Round(){if(std::fesetround(FE_TOWARDZERO))throw std::runtime_error("NPC input rounding");}~Round(){std::fesetround(previous);}};
float crouch(std::array<float,3> v){
 float a=npcMul(v[0],v[0]),b=npcMul(v[1],v[1]),c=npcMul(v[2],v[2]);float square=npcAdd(a,b);square=npcAdd(square,c);square=npcAdd(square,0.f);
 return npcSqrt(square)<std::bit_cast<float>(0x44505557u)?1.f:std::bit_cast<float>(0x3f7851ecu);
}
uint32_t pack(uint32_t word,float value){return (word&0x03ffffffu)|(uint32_t(int32_t(npcMul(value,31.f)))<<26);}
}
uint32_t originalNpcCrouchWord(uint32_t word,std::array<float,3> velocity,float fieldDF8){Round round;float value=crouch(velocity);if(fieldDF8<20)value=0;return pack(word,value);}
uint32_t originalNpcBoostWord(uint32_t word,std::array<float,3> velocity){Round round;return pack(word,crouch(velocity))|0x30000u;}
}
#include "original_float.hpp"
#include "ground_motion.hpp"
#include <algorithm>
#include <limits>
namespace ssx {namespace {
using NpcV=std::array<float,3>;
float steeringDot(NpcV a,NpcV b){float x=npcMul(a[0],b[0]),y=npcMul(a[1],b[1]),z=npcMul(a[2],b[2]);float sum=npcAdd(x,y);sum=npcAdd(sum,z);return npcAdd(sum,0.f);}
NpcV steeringUnit(NpcV v){float squared=steeringDot(v,v);float inverse=squared>0?npcDiv(1.f,npcSqrt(squared)):std::numeric_limits<float>::max();for(auto&x:v)x=npcMul(x,inverse);return v;}
constexpr float steeringF(uint32_t bits){return std::bit_cast<float>(bits);}
}
NpcV originalNpcSteeringTarget(NpcV position,NpcV closest,NpcV previous){Round round;for(unsigned i=0;i<3;++i){float correction=npcSub(closest[i],position[i]);correction=npcMul(correction,steeringF(0x3dcccccd));previous[i]=npcAdd(previous[i],correction);}return previous;}
float originalNpcSteering(NpcV position,NpcV velocity,NpcV up,NpcV target){
 Round round;if(up[2]<=0)up={0,0,1};NpcV lateral={npcSub(npcMul(velocity[1],up[2]),npcMul(velocity[2],up[1])),npcSub(npcMul(velocity[2],up[0]),npcMul(velocity[0],up[2])),npcSub(npcMul(velocity[0],up[1]),npcMul(velocity[1],up[0]))};
 NpcV direction;for(unsigned i=0;i<3;++i)direction[i]=npcSub(target[i],position[i]);direction[2]=0;direction=steeringUnit(direction);
 float projection=steeringDot(direction,up);for(unsigned i=0;i<3;++i){float normal=npcMul(up[i],projection);direction[i]=npcSub(direction[i],normal);}direction=steeringUnit(direction);
 float x=steeringDot(direction,velocity),y=steeringDot(direction,lateral),angle;
 if(x==0)angle=y==0?y:y<0?-steeringF(0x3fc90fdb):steeringF(0x3fc90fdb);
 else{angle=originalAtan(originalScalarDivide(y,x));if(x<0)angle=y>0?originalScalarAdd(angle,steeringF(0x40490fdb)):originalScalarSubtract(angle,steeringF(0x40490fdb));}
 float value=npcMul(std::abs(angle),steeringF(0x40c9453e));if(value<steeringF(0x3ca3d70a))return 0;
 value=std::min(value,steeringF(0x3f7ae148));return angle<0?-value:value;
}
}
namespace ssx {
std::array<uint32_t,2> originalNpcPrewindCommand(std::array<uint32_t,2> word,const OriginalNpcPath& path,const OriginalNpcPrewindInput& c){
 Round round;auto events=originalNpcPathEvents(path,c.previousDistance,originalScalarAdd(c.currentDistance,50.f));
 float speed=npcSqrt(steeringDot(c.velocity,c.velocity));if(speed<originalScalarSubtract(c.desiredSpeedDF0,steeringF(0x430ae38f)))word[0]|=0x4000;
 if(c.regionModeE1C){NpcV region,offset;for(unsigned k=0;k<3;++k){region[k]=npcSub(c.regionEndE60[k],c.regionStartE50[k]);offset[k]=npcSub(c.position[k],c.regionStartE50[k]);}
  if(npcSqrt(steeringDot(offset,offset))<originalScalarSubtract(npcSqrt(steeringDot(region,region)),50.f))word[0]|=0x2000;return word;
 }
 const OriginalRacePathEvent* jump=nullptr;for(const auto&event:events)if(event.type==16){jump=&event;word[0]|=0x2000;break;}
 auto bits=[](float x){return uint32_t(int32_t(npcMul(x,31.f)))&63u;};
 if(jump){auto target=originalRacePathSample(path.geometry,jump->end);for(unsigned k=0;k<3;++k){float correction=npcSub(c.closestRoutePoint[k],c.position[k]);correction=npcMul(correction,.5f);target[k]=npcSub(target[k],correction);}
  float steering=originalNpcSteering(c.position,c.velocity,c.boardUp,target);word[1]=(word[1]&~63u)|bits(steering);
 }
 word[0]=(word[0]&0xffe07fffu)|(bits(c.plannedSpinE38)<<15);
 word[0]=(word[0]&0xf81fffffu)|(bits(c.plannedFlipE3C)<<21);return word;
}
}
namespace ssx {
void originalNpcPrepareTrick(OriginalNpcTrickPlan& s,bool spin,bool flip,int32_t field20,int32_t kind,float parameter,const std::function<uint32_t()>& next){
 Round round;if(!next)throw std::runtime_error("Original NPC trick plan requires shared RNG");
 s.indexE0C=-1;s.enabledE10=spin||flip;
 uint32_t word=next();float threshold=npcMul(parameter,100.f);s.decisionE14=threshold<=float(word%100)?1:0;
 s.fieldE20=field20;s.kindE1C=kind;s.phaseE18=0;
 if(s.enabledE10){s.spinE38=spin?1.f:0.f;s.flipE3C=flip?1.f:0.f;if(next()&1)s.spinE38=-s.spinE38;if(next()&1)s.flipE3C=-s.flipE3C;}
}
}
namespace ssx {
float originalNpcTimeScaleTarget(const OriginalNpcPacingContext& c){
 Round round;if(!c.referenceRemaining||c.modeE4==0||c.eventVariant==2||c.remaining<50000.f)return 1;
 float delta=originalScalarSubtract(*c.referenceRemaining,c.remaining);
 if(delta<c.negativeThresholdDC&&(c.modeE4==2||c.modeE4==3)){
  float x=originalScalarSubtract(delta,c.negativeThresholdDC);x=originalScalarDivide(x,c.negativeThresholdDC);x=originalScalarAdd(x,1.f);return npcMul(x,steeringF(0x3f8ccccd));
 }
 if(c.positiveThresholdE0<delta&&(c.modeE4==1||c.modeE4==3)){
  float x=originalScalarSubtract(delta,c.positiveThresholdE0);x=originalScalarDivide(x,c.positiveThresholdE0);x=originalScalarAdd(x,1.f);x=originalScalarDivide(1.f,x);return npcMul(x,steeringF(0x3f68ba2e));
 }
 return 1;
}
float originalNpcTimeScaleApproach(float current,float target){
 Round round;constexpr float step=steeringF(0x3a5a740f);
 if(originalScalarAdd(target,step)<current)return originalScalarSubtract(current,step);
 if(current<originalScalarSubtract(target,step))return originalScalarAdd(current,step);
 return target;
}
}
#include "ground_pose_motion.hpp"
namespace ssx {
bool originalNpcSelectPeer(int32_t& target,int32_t& designatedResult,float lateral,std::array<float,3> forward,
    const std::array<OriginalPairRecord,6>& records,std::optional<int> designated,int32_t tick){
 Round round;if(target>=6)throw std::runtime_error("Original NPC peer index outside six participants");
 if(target>=0&&(records[target].planarDistanceCm>1050.f||lateral>600.f))target=-1;
 if(target>=0)return true;
 if(!(lateral<540.f)||tick%12!=0)return false;
 float best=10000000000.f;
 for(unsigned index=0;index<6;++index){const auto& record=records[index];if(!record.enabled||record.planarDistanceCm>700.f)continue;
  bool marked=designated&&*designated==int(index);float x=forward[0],y=forward[1],heading;
  if(x==0)heading=y==0?y:y<0?-steeringF(0x3fc90fdb):steeringF(0x3fc90fdb);
  else{heading=originalAtan(originalScalarDivide(y,x));if(x<0)heading=y>0?originalScalarAdd(heading,steeringF(0x40490fdb)):originalScalarSubtract(heading,steeringF(0x40490fdb));}
  float angle=originalScalarSubtract(record.bearing,heading);float scaled=npcMul(angle,steeringF(0x3e22f983));scaled=originalScalarAdd(scaled,.5f);float whole=float(int32_t(scaled));if(scaled<whole)whole=originalScalarSubtract(whole,1.f);
  float reduction=npcMul(whole,steeringF(0x40c90fdb));float cosine=originalCosine(originalScalarSubtract(angle,reduction));
  float factor=marked?originalScalarAdd(std::abs(cosine),1.f):originalScalarSubtract(1.f,cosine);float score=npcMul(record.planarDistanceCm,factor);
  if(score<=best){target=int(index);best=score;designatedResult=marked;}
 }
 return target>=0;
}
}
namespace ssx {
std::array<uint32_t,2> originalNpcCruiseBehavior(std::array<uint32_t,2> word,OriginalNpcDrivingState& state,const OriginalNpcDrivingContext& c,const OriginalNpcPath& path){
 Round round;auto bits=[](float x){return uint32_t(int32_t(npcMul(x,31.f)))&63u;};
 auto target=originalNpcSteeringTarget(c.position,c.route.closestPoint,c.route.previousLookaheadPoint);
 word[0]=(word[0]&0xfc0fffffu)|(bits(originalNpcSteering(c.position,c.velocity,c.boardUp,target))<<20);
 auto speedZone=originalNpcSpeedZone(path,c.route.previousDistance,c.route.currentDistance);
 if(speedZone){
  state.desiredSpeedDF0=speedZone->speedCmps;float speed=npcSqrt(steeringDot(c.velocity,c.velocity));constexpr float margin=steeringF(0x430ae38f);
  if(originalScalarAdd(state.desiredSpeedDF0,margin)<speed){if(speedZone->started)word[1]=(word[1]&~63u)|31u;}
  else if(speed<originalScalarSubtract(state.desiredSpeedDF0,margin))word[0]=speedZone->started?originalNpcBoostWord(word[0],c.velocity):originalNpcCrouchWord(word[0],c.velocity,state.parameterDF8);
 }else if(auto toggle=originalNpcToggleZone(path,c.route.previousDistance,c.route.currentDistance)){
  float press=*toggle?-1.f:1.f;
  if(state.boardTimerE40==0){press=-press;state.boardTimerE40=60;}
  else if(state.boardTimerE40>0){if(--state.boardTimerE40<=0)state.boardTimerE40=-1;press=-press;}
  word[1]=(word[1]&0xfffff03fu)|(bits(press)<<6);
 }else{
  state.boardTimerE40=0;
  if(c.boostMeter>=steeringF(0x3f666666)&&c.superTime>0&&state.parameterDF8>50.f)word[0]|=0x20000u;
  word[0]=originalNpcCrouchWord(word[0],c.velocity,state.parameterDF8);
 }
 if(auto jump=originalNpcJumpZone(path,c.route.previousDistance,c.route.currentDistance)){
  state.desiredSpeedDF0=jump->speedCmps;state.regionStartE50=jump->startPoint;state.regionEndE60=jump->endPoint;
  state.targetPeerE70=-1;state.behavior=OriginalNpcBehavior::Jump1009E0;state.behaviorCounterF38=0;
 }else{
  int32_t designated=0;
  if(originalNpcSelectPeer(state.targetPeerE70,designated,c.route.lateralDistance,c.physicalForward,c.peers,c.designatedPeer,c.tick)){
   if(designated){state.behavior=OriginalNpcBehavior::Designated100B90;state.behaviorCounterF38=3;}
   else if(!c.peerHuman[state.targetPeerE70]){state.behavior=OriginalNpcBehavior::Peer100F88;state.behaviorCounterF38=2;}
  }
 }
 return word;
}
}
namespace ssx {
std::array<uint32_t,2> originalNpcJumpBehavior(std::array<uint32_t,2> word,OriginalNpcDrivingState& state,const OriginalNpcDrivingContext& c,const OriginalNpcPath& path,const std::function<uint32_t()>& next){
 Round round;auto zone=originalNpcJumpZone(path,c.route.previousDistance,c.route.currentDistance);
 if(!zone){state.behavior=OriginalNpcBehavior::Cruise100680;state.behaviorCounterF38=1;return originalNpcCruiseBehavior(word,state,c,path);}
 state.desiredSpeedDF0=zone->speedCmps;state.regionStartE50=zone->startPoint;state.regionEndE60=zone->endPoint;
 auto target=originalNpcSteeringTarget(c.position,c.route.closestPoint,c.route.previousLookaheadPoint);float steer=originalNpcSteering(c.position,c.velocity,c.boardUp,target);
 word[0]=(word[0]&0xfc0fffffu)|((uint32_t(int32_t(npcMul(steer,31.f)))&63u)<<20);
 float speed=npcSqrt(steeringDot(c.velocity,c.velocity));
 if(zone->started&&c.route.lateralDistance<10000.f&&std::abs(steer)<1.f){
  originalNpcPrepareTrick(state.trick,zone->flags[2]!=0,zone->flags[3]!=0,zone->flags[1],zone->flags[0],state.parameterDFC,next);word[0]|=0xc000;
 }else{
  constexpr float margin=steeringF(0x430ae38f);
  if(speed<originalScalarSubtract(state.desiredSpeedDF0,margin))word[0]=originalNpcBoostWord(word[0],c.velocity);
  else if(originalScalarAdd(state.desiredSpeedDF0,margin)<speed)word[1]=(word[1]&~63u)|31u;
 }
 return word;
}
}

namespace ssx {
std::array<uint32_t,2> originalNpcPeerBehavior(std::array<uint32_t,2> word,OriginalNpcDrivingState& state,OriginalNpcDrivingContext& c,std::span<const OriginalNpcPath> paths,OriginalNpcPathScoreContext score,const std::function<uint32_t()>& random){
 Round round;int peer=state.targetPeerE70;if(peer<0||peer>=6)throw std::runtime_error("Original NPC peer behavior requires a valid target");
 if(c.route.pathIndex<0||size_t(c.route.pathIndex)>=paths.size())throw std::runtime_error("Original NPC peer behavior requires an AI route");
 float angle=originalScalarSubtract(c.peers[peer].bearing,c.route.heading);float scaled=npcMul(angle,steeringF(0x3e22f983));scaled=originalScalarAdd(scaled,.5f);float whole=float(int32_t(scaled));if(scaled<whole)whole=originalScalarSubtract(whole,1.f);
 float reduction=npcMul(whole,steeringF(0x40c90fdb));angle=originalScalarSubtract(angle,reduction);
 float ownSpeed=npcSqrt(steeringDot(c.velocity,c.velocity)),peerSpeed=npcSqrt(steeringDot(c.peerVelocities[peer],c.peerVelocities[peer]));float steer=0;
 auto pathSteer=[&](){return originalNpcSteering(c.position,c.velocity,c.boardUp,originalNpcSteeringTarget(c.position,c.route.closestPoint,c.route.previousLookaheadPoint));};
 if(std::abs(angle)>steeringF(0x3fc90fdc)){
  steer=pathSteer();word[0]=ownSpeed<peerSpeed?originalNpcBoostWord(word[0],c.velocity):originalNpcCrouchWord(word[0],c.velocity,state.parameterDF8);
 }else{
  float distance=c.peers[peer].planarDistanceCm,cone;
  if(distance==0)cone=steeringF(0x3fc90fdb);else{cone=originalAtan(originalScalarDivide(150.f,distance));if(distance<0)cone=originalScalarAdd(cone,steeringF(0x40490fdb));}
  if(std::abs(angle)>cone||ownSpeed<=peerSpeed){steer=pathSteer();word[0]=originalNpcCrouchWord(word[0],c.velocity,state.parameterDF8);}
  else if(!c.peerHuman[peer]){
   if(c.peerPathIndices[peer]==c.route.pathIndex){score.position=c.position;score.velocity=c.velocity;originalNpcSelectPath(paths,c.route,score,false,random);}
   steer=pathSteer();word[0]=originalNpcCrouchWord(word[0],c.velocity,state.parameterDF8);
  }
 }
 word[0]=(word[0]&0xfc0fffffu)|((uint32_t(int32_t(npcMul(steer,31.f)))&63u)<<20);
 if(auto jump=originalNpcJumpZone(paths[c.route.pathIndex],c.route.previousDistance,c.route.currentDistance)){
  state.desiredSpeedDF0=jump->speedCmps;state.regionStartE50=jump->startPoint;state.regionEndE60=jump->endPoint;state.targetPeerE70=-1;state.behavior=OriginalNpcBehavior::Jump1009E0;state.behaviorCounterF38=0;
 }else{
  int32_t marked=0;if(!originalNpcSelectPeer(state.targetPeerE70,marked,c.route.lateralDistance,c.physicalForward,c.peers,c.designatedPeer,c.tick)){state.behavior=OriginalNpcBehavior::Cruise100680;state.behaviorCounterF38=1;}
  else if(marked){state.behavior=OriginalNpcBehavior::Designated100B90;state.behaviorCounterF38=3;}
 }
 return word;
}
}

namespace ssx {
bool originalNpcRecovery(OriginalNpcDrivingState& s,const OriginalNpcDrivingContext& c){
 Round round;float x=c.physicalForward[0],y=c.physicalForward[1],heading;
 if(x==0)heading=y==0?y:y<0?-steeringF(0x3fc90fdb):steeringF(0x3fc90fdb);
 else{heading=originalAtan(originalScalarDivide(y,x));if(x<0)heading=y>0?originalScalarAdd(heading,steeringF(0x40490fdb)):originalScalarSubtract(heading,steeringF(0x40490fdb));}
 float angle=originalScalarSubtract(c.route.heading,heading);float scaled=npcMul(angle,steeringF(0x3e22f983));scaled=originalScalarAdd(scaled,.5f);float whole=float(int32_t(scaled));if(scaled<whole)whole=originalScalarSubtract(whole,1.f);angle=originalScalarSubtract(angle,npcMul(whole,steeringF(0x40c90fdb)));
 auto increment=[](int32_t v){return std::bit_cast<int32_t>(uint32_t(v)+1u);};
 if(s.targetPeerE70<0&&std::abs(angle)>steeringF(0x3f860a93))s.oppositeHeadingTicksE78=increment(s.oppositeHeadingTicksE78);else s.oppositeHeadingTicksE78=0;
 bool wrong=s.oppositeHeadingTicksE78>=61;
 if(c.motionMode==2){wrong=false;s.offRouteTicksE74=increment(s.offRouteTicksE74);}
 else{
  if(c.motionMode==4&&s.oppositeHeadingTicksE78>0)return true;
  if(c.motionMode==1||c.route.lateralDistance<1000.f){
   s.offRouteTicksE74=0;wrong=false;if(c.motionMode==1&&c.trajectoryElapsed>30.f)return true;
  }else s.offRouteTicksE74=increment(s.offRouteTicksE74);
 }
 bool request=wrong||s.offRouteTicksE74>=223;return c.controlState==8?request:request&&s.targetPeerE70<0;
}
}

namespace ssx {
std::optional<bool> originalNpcAttackRequest(OriginalNpcDrivingState& s,const OriginalNpcDrivingContext& c,const std::function<int(unsigned)>& relationship){
 Round round;if(c.participantCount>6)throw std::runtime_error("Original NPC participant limit");if(c.tick%6!=0)return {};
 for(unsigned peer=0;peer<c.participantCount;++peer){if(peer==c.selfSlot||!(c.peers[peer].planarDistanceCm<200.f))continue;
  float x=c.physicalForward[0],y=c.physicalForward[1],heading;
  if(x==0)heading=y==0?y:y<0?-steeringF(0x3fc90fdb):steeringF(0x3fc90fdb);
  else{heading=originalAtan(originalScalarDivide(y,x));if(x<0)heading=y>0?originalScalarAdd(heading,steeringF(0x40490fdb)):originalScalarSubtract(heading,steeringF(0x40490fdb));}
  float angle=originalScalarSubtract(heading,c.peers[peer].bearing);float scaled=npcMul(angle,steeringF(0x3e22f983));scaled=originalScalarAdd(scaled,.5f);float whole=float(int32_t(scaled));if(scaled<whole)whole=originalScalarSubtract(whole,1.f);angle=originalScalarSubtract(angle,npcMul(whole,steeringF(0x40c90fdb)));
  if(!(std::abs(angle)>steeringF(0x3f060a93)&&std::abs(angle)<steeringF(0x40278d37)))continue;
  if(!relationship)throw std::runtime_error("Original NPC attack requires relationship data");if(relationship(peer)<3)continue;
  int32_t next=std::bit_cast<int32_t>(uint32_t(s.lastAttackTickF4)+120u);if(!(next<c.tick))continue;
  s.lastAttackTickF4=c.tick;return angle>0;
 }return {};
}
int32_t originalNpcDefensiveRequest(OriginalNpcDrivingState& s,const OriginalNpcDrivingContext& c,const std::function<uint32_t()>& next){
 Round round;if(s.defensiveTimerF30<0){s.defensiveTimerF30=0;return 0;}
 if(s.defensiveTimerF30>0){s.defensiveTimerF30=originalScalarSubtract(s.defensiveTimerF30,steeringF(0x3c888889));return s.defensiveDecisionF34;}
 if(c.tick%6!=0)return 0;if(c.participantCount>6)throw std::runtime_error("Original NPC participant limit");
 for(unsigned peer=0;peer<c.participantCount;++peer){if(peer==c.selfSlot||!c.peerHuman[peer]||!(c.peers[peer].planarDistanceCm<200.f)||c.peerUpperAnimationClass[peer]!=13)continue;
  if(!next)throw std::runtime_error("Original NPC defense requires shared RNG");s.defensiveDecisionF34=next()%100<25?1:0;s.defensiveTimerF30=2.f;return s.defensiveDecisionF34;
 }return 0;
}
}

namespace ssx {
OriginalNpcProviderResult originalNpcControl(OriginalNpcDrivingState& state,OriginalNpcProviderContext& context,std::span<const OriginalNpcPath> paths,const std::function<int(unsigned)>& relationship,const std::function<uint32_t()>& random){
 OriginalNpcProviderResult result;auto& c=context.driving;
 result.timeScale=originalNpcTimeScaleApproach(context.currentTimeScale,originalNpcTimeScaleTarget(context.pacing));context.currentTimeScale=result.timeScale;
 auto path=[&]()->const OriginalNpcPath&{if(c.route.pathIndex<0||size_t(c.route.pathIndex)>=paths.size())throw std::runtime_error("Original NPC provider has no AI path");return paths[c.route.pathIndex];};
 switch(c.controlState){
  case 0:{
   if(originalNpcRecovery(state,c))result.words[0]|=0x1000;
   state.trick.fieldE20=0;
   if(auto attack=originalNpcAttackRequest(state,c,relationship))result.words[0]|=*attack?0x80000u:0x40000u;
   else if(originalNpcDefensiveRequest(state,c,random))result.words[0]|=0xc0000u;
   switch(state.behavior){
    case OriginalNpcBehavior::Cruise100680:result.words=originalNpcCruiseBehavior(result.words,state,c,path());break;
    case OriginalNpcBehavior::Jump1009E0:result.words=originalNpcJumpBehavior(result.words,state,c,path(),random);break;
    case OriginalNpcBehavior::Peer100F88:result.words=originalNpcPeerBehavior(result.words,state,c,paths,context.scoring,random);break;
    case OriginalNpcBehavior::Designated100B90:result.words=originalNpcDesignatedBehavior(result.words,state,c,path());break;
   }
   break;
  }
  case 1:result.words=originalNpcManualCommand(result.words,c,path());break;
  case 2:{
   OriginalNpcPrewindInput input;input.position=c.position;input.velocity=c.velocity;input.boardUp=c.boardUp;input.closestRoutePoint=c.route.closestPoint;
   input.previousDistance=c.route.previousDistance;input.currentDistance=c.route.currentDistance;input.desiredSpeedDF0=state.desiredSpeedDF0;input.regionModeE1C=state.trick.kindE1C!=0;input.regionStartE50=state.regionStartE50;input.regionEndE60=state.regionEndE60;input.plannedSpinE38=state.trick.spinE38;input.plannedFlipE3C=state.trick.flipE3C;
   result.words=originalNpcPrewindCommand(result.words,path(),input);break;
  }
  case 4:if(!context.air)throw std::runtime_error("Original NPC passive air context missing");result.words=originalNpcPassiveAirCommand(result.words,state,c,*context.air,random);break;
  case 6:result.words=originalNpcStartCommand(result.words,state,c.tick,c.selfSlot,random);break;
  case 5:if(!context.air)throw std::runtime_error("Original NPC air context missing");result.words=originalNpcAirCommand(result.words,state,*context.air,random);break;
  case 7:result.words=originalNpcRailCommand(result.words,state,c,path(),random);break;
  case 8:if(originalNpcRecovery(state,c))result.words[0]|=0x1000;break;
  case 3:case 9:case 10:case 11:case 12:case 13:break; // Original zero-command dispatch entries.
  default:throw std::runtime_error("Original NPC control producer pending for state "+std::to_string(c.controlState));
 }
 if(c.controlState!=5)state.trick.remainingE34=0;
 return result;
}
}

namespace ssx {
std::array<uint32_t,2> originalNpcStartCommand(std::array<uint32_t,2> words,OriginalNpcDrivingState& s,int32_t tick,unsigned slot,const std::function<uint32_t()>& random){
 Round round;float remaining=originalScalarSubtract(180.f,float(tick));float threshold=originalScalarSubtract(33.f,float(int32_t(uint32_t(slot-1)*10u)));
 uint32_t value;
 if(remaining<threshold&&slot<4)value=31;
 else if(remaining<72.f&&slot<3)value=33;
 else{
  if(tick%20==0){if(!random)throw std::runtime_error("Original NPC start shared RNG missing");uint32_t word=random();float amount;
   if(int32_t(word)>=0)amount=npcIntToFloat(int32_t(word));else{amount=npcIntToFloat(int32_t((word>>1)|(word&1)));amount=originalScalarAdd(amount,amount);}
   s.desiredSpeedDF0=npcMul(amount,std::bit_cast<float>(0x3c23d70au));if(!(random()&1))s.desiredSpeedDF0=-s.desiredSpeedDF0;
  }
  value=uint32_t(int32_t(npcMul(s.desiredSpeedDF0,31.f)))&63;
 }
 words[0]=(words[0]&0xfffc0fffu)|(value<<12);return words;
}
}

namespace ssx {
std::array<uint32_t,2> originalNpcDesignatedBehavior(std::array<uint32_t,2> word,OriginalNpcDrivingState& state,const OriginalNpcDrivingContext& c,const OriginalNpcPath& path){
 Round round;int peer=state.targetPeerE70;if(peer<0||peer>=6)throw std::runtime_error("Original NPC designated behavior requires a valid target");
 float angle=originalScalarSubtract(c.peers[peer].bearing,c.route.heading),scaled=originalScalarAdd(npcMul(angle,steeringF(0x3e22f983)),.5f),whole=float(int32_t(scaled));if(scaled<whole)whole=originalScalarSubtract(whole,1.f);
 angle=std::abs(originalScalarSubtract(angle,npcMul(whole,steeringF(0x40c90fdb))));float ownSpeed=npcSqrt(steeringDot(c.velocity,c.velocity)),peerSpeed=npcSqrt(steeringDot(c.peerVelocities[peer],c.peerVelocities[peer])),steer;
 if(angle>steeringF(0x3fc90fdc)){
  steer=originalNpcSteering(c.position,c.velocity,c.boardUp,originalNpcSteeringTarget(c.position,c.route.closestPoint,c.route.previousLookaheadPoint));
  if(c.peers[peer].planarDistanceCm>200&&peerSpeed<ownSpeed)word[1]=(word[1]&~63u)|31;
 }else{
  auto direction=steeringUnit(c.peerVelocities[peer]),target=c.peerPositions[peer];for(unsigned k=0;k<3;++k){float offset=npcMul(direction[k],300.f);target[k]=npcAdd(target[k],offset);}
  if(angle<steeringF(0x3f5f66f4)){
   if(angle<steeringF(0x3e860a93))word[0]=originalNpcBoostWord(word[0],c.velocity);
   else word[0]=originalNpcCrouchWord(word[0],c.velocity,state.parameterDF8);
  }else if(ownSpeed<peerSpeed)word[0]=originalNpcCrouchWord(word[0],c.velocity,state.parameterDF8);
  else if(peerSpeed<ownSpeed)word[1]=(word[1]&~63u)|31;
  steer=originalNpcSteering(c.position,c.velocity,c.boardUp,target);
 }
 word[0]=(word[0]&0xfc0fffffu)|((uint32_t(int32_t(npcMul(steer,31.f)))&63u)<<20);
 if(auto jump=originalNpcJumpZone(path,c.route.previousDistance,c.route.currentDistance)){
  state.desiredSpeedDF0=jump->speedCmps;state.regionStartE50=jump->startPoint;state.regionEndE60=jump->endPoint;state.targetPeerE70=-1;state.behavior=OriginalNpcBehavior::Jump1009E0;state.behaviorCounterF38=0;
 }else{
  int32_t marked=1;if(!originalNpcSelectPeer(state.targetPeerE70,marked,c.route.lateralDistance,c.physicalForward,c.peers,c.designatedPeer,c.tick)){state.behavior=OriginalNpcBehavior::Cruise100680;state.behaviorCounterF38=1;}
  else if(!marked&&!c.peerHuman[state.targetPeerE70]){state.behavior=OriginalNpcBehavior::Peer100F88;state.behaviorCounterF38=2;}
 }
 return word;
}
}

namespace ssx {
std::array<uint32_t,2> originalNpcManualCommand(std::array<uint32_t,2> word,const OriginalNpcDrivingContext& c,const OriginalNpcPath& path){
 Round round;auto toggle=originalNpcToggleZone(path,c.route.previousDistance,c.route.currentDistance);
 float steer=originalNpcSteering(c.position,c.velocity,c.boardUp,originalNpcSteeringTarget(c.position,c.route.closestPoint,c.route.previousLookaheadPoint));
 word[0]=(word[0]&0xff03ffffu)|((uint32_t(int32_t(npcMul(steer,31.f)))&63)<<18);
 if(toggle){word[1]=(word[1]&~63u)|(*toggle?33u:31u);float balance=std::clamp(originalScalarAdd(c.physicalRightZ,c.physicalRightZ),-1.f,1.f);
  if(std::abs(balance)>steeringF(0x3dcccccd))word[0]=(word[0]&0xc0ffffffu)|((uint32_t(int32_t(npcMul(balance,31.f)))&63)<<24);
 }else word[1]&=~63u;
 return word;
}
}

namespace ssx {
std::array<uint32_t,2> originalNpcRailCommand(std::array<uint32_t,2> word,OriginalNpcDrivingState& state,const OriginalNpcDrivingContext& c,const OriginalNpcPath& path,const std::function<uint32_t()>& next){
 Round round;auto zone=originalNpcJumpZone(path,c.route.previousDistance,c.route.currentDistance);
 if(zone){state.desiredSpeedDF0=zone->speedCmps;state.regionStartE50=zone->startPoint;state.regionEndE60=zone->endPoint;}
 state.trick.fieldE20=zone?zone->flags[1]:0; // 10AF1C: sp+0x18 is written by 10B980 only on a hit
 word[0]|=0x01fe0000u;
 if(zone&&zone->started){word[0]|=0x6000u;originalNpcPrepareTrick(state.trick,zone->flags[2]!=0,zone->flags[3]!=0,zone->flags[1],zone->flags[0],state.parameterDFC,next);return word;}
 float doubled=originalScalarAdd(c.physicalRightZ,c.physicalRightZ),balance=-1.f<=doubled?std::min(doubled,1.f):-1.f;
 if(std::abs(balance)>steeringF(0x3dcccccd))word[0]=(word[0]&0x81ffffffu)|((uint32_t(int32_t(npcMul(balance,31.f)))&63u)<<25);
 if(!(c.prewindStyle==3||c.prewindStyle==4)&&c.mainAnimationClass!=14){float spin=state.trick.spinE38<0?1.f:-1.f; /* 10B010..10B028: bc1f's delay slot loads -1, the fall-through (E38<0) +1 */word[1]=(word[1]&~63u)|(uint32_t(int32_t(npcMul(spin,31.f)))&63u);}
 if(auto speedZone=originalNpcSpeedZone(path,c.route.previousDistance,c.route.currentDistance)){
  state.desiredSpeedDF0=speedZone->speedCmps;
  if(speedZone->started){float speed=npcSqrt(steeringDot(c.velocity,c.velocity));if(speed<originalScalarSubtract(state.desiredSpeedDF0,steeringF(0x430ae38f)))word[0]|=0x10000u;}
 }
 return word;
}
}
