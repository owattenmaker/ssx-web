#include "npc_air.hpp"
#include "original_float.hpp"
#include <bit>
#include <cmath>
#include <cfenv>
#include <stdexcept>
#pragma STDC FENV_ACCESS ON
#include "npc_float.hpp"
namespace ssx {namespace {
constexpr float F(uint32_t bits){return std::bit_cast<float>(bits);}
struct Round {int old=std::fegetround();Round(){if(std::fesetround(FE_TOWARDZERO))throw std::runtime_error("NPC airborne rounding");}~Round(){std::fesetround(old);}};
float add(float a,float b){return originalScalarAdd(a,b);}float sub(float a,float b){return originalScalarSubtract(a,b);}float div(float a,float b){return originalScalarDivide(a,b);}
int32_t increment(int32_t value){return std::bit_cast<int32_t>(uint32_t(value)+1u);}
const OriginalNpcGrabCatalog& catalog(const OriginalNpcAirContext& c){if(!c.grabs)throw std::runtime_error("Original NPC grab catalog missing");return *c.grabs;}
uint32_t draw(const std::function<uint32_t()>& random){if(!random)throw std::runtime_error("Original NPC air shared RNG missing");return random();}
float grabRate(const OriginalNpcGrabCatalog& g){return add(npcMul(g.grabStat,F(0x3e998a4c)),1.f);}
int normalGrab(OriginalNpcTrickPlan& s,float remaining,const OriginalNpcAirContext& c,const std::function<uint32_t()>& random){
 const auto&g=catalog(c);s.fieldE24=s.fieldE28=0;s.startE30=remaining;int index=draw(random)%15;float rate=grabRate(g);
 float end=div(g.normal[index].marker2,rate),begin=div(g.normal[index].marker1,rate);
 if(remaining<end)return -1;
 s.releaseE2C=sub(end,begin);s.remainingE34=sub(remaining,end);s.normalCounts[index]=increment(s.normalCounts[index]);return index;
}
int advancedGrab(OriginalNpcTrickPlan& s,float remaining,const OriginalNpcAirContext& c,bool uber){
 const auto&g=catalog(c);if(uber){s.fieldE28=0;if(c.superTime<=0)return -1;}else s.fieldE24=0;
 s.startE30=remaining;float rate=grabRate(g);int selected=-1,least=10000;auto& counts=uber?s.uberCounts:s.tweakCounts;
 for(unsigned index=uber?0:1;index<15;++index){
  const auto& entry=uber?g.uber[c.boostTierCounter>=5][index]:g.tweak[index];if(entry.semantic==438)continue;
  if(!uber&&c.uberEnabled&&c.superTime>0&&g.uber[c.boostTierCounter>=5][index].semantic!=438)continue;
  float normalBegin=div(g.normal[index].marker1,rate),begin=div(entry.marker1,rate),end=div(entry.marker2,rate);
  float duration=add(end,normalBegin);
  if(duration<remaining&&counts[index]<least){
   if(uber)s.fieldE24=1;else s.fieldE28=1;
   selected=int(index);s.releaseE2C=sub(end,begin);s.remainingE34=sub(remaining,duration);least=counts[index];
  }
 }
 if(selected>=0)counts[selected]=increment(counts[selected]);return selected;
}
}
std::array<float,2> originalNpcRotationTimes(const OriginalAirControlState& a,bool flip,float stat,bool boosted){
 Round round;float whole=F(flip?0x40c90fdc:0x40490fdc),maximum=flip?a.maxFlip:a.maxSpin;
 float delta=std::abs(sub(flip?a.progressFlip:a.progressSpin,flip?a.targetFlip:a.targetSpin));if(whole<delta)delta=sub(delta,whole);
 float current=div(delta,maximum),full=div(whole,maximum);if(boosted){full=npcMul(full,F(0x3f000418));current=npcMul(current,F(0x3f000418));}
 float factor=div(1.f,add(npcMul(stat,F(0x3f0010fc)),1.f));full=npcMul(full,factor);current=npcMul(current,factor);return {current,full};
}
void originalNpcStopRotations(OriginalNpcTrickPlan& s,float remaining,const OriginalNpcAirContext& c){
 Round round;auto spin=originalNpcRotationTimes(c.angular,false,c.trickStat,c.boostModifier>0),flip=originalNpcRotationTimes(c.angular,true,c.trickStat,c.boostModifier>0);
 if(s.fieldE20&&remaining<1){s.spinE38=s.flipE3C=0;return;}if(!s.enabledE10)return;
 if(s.spinE38!=0){float horizon=add(remaining,F(0x3dcccccd));if(horizon<spin[0]||(horizon<spin[1]&&sub(spin[0],F(0x3ecccccd))<0))s.spinE38=0;}
 if(s.flipE3C!=0){float horizon=add(remaining,F(0x3dcccccd));if(horizon<flip[0])s.spinE38=s.flipE3C=0;else if(horizon<flip[1]&&sub(flip[0],F(0x3ecccccd))<0)s.spinE38=s.flipE3C=0;}
}
int originalNpcChooseGrab(OriginalNpcTrickPlan& s,float remaining,float parameter,const OriginalNpcAirContext& c,const std::function<uint32_t()>& random){
 Round round;if((s.fieldE20&&remaining<1)||!(remaining>.75f)||remaining==100.f)return -1;
 const auto&g=catalog(c);int index=-1;
 if(c.uberEnabled&&g.eventId!=14&&float(draw(random)%100)<npcMul(parameter,100.f))index=advancedGrab(s,remaining,c,true);
 if(index==-1&&float(draw(random)%100)<add(npcMul(parameter,70.f),30.f))index=advancedGrab(s,remaining,c,false);
 if(index==-1&&float(draw(random)%100)<add(npcMul(parameter,40.f),60.f))index=normalGrab(s,remaining,c,random);
 return index;
}
std::array<uint32_t,2> originalNpcPassiveAirCommand(std::array<uint32_t,2> words,OriginalNpcDrivingState& state,const OriginalNpcDrivingContext& d,const OriginalNpcAirContext& c,const std::function<uint32_t()>& random){
 Round round;auto&s=state.trick;float remaining=(c.trajectoryStatus==1||c.trajectoryStatus==3)?sub(c.predictedTime,c.elapsed):0;
 s.enabledE10=0;s.indexE0C=-1;s.decisionE14=0;s.phaseE18=1;s.fieldE20=0;
 if(remaining>1){s.enabledE10=1;s.spinE38=1;if(draw(random)&1)s.spinE38=-s.spinE38;}else s.spinE38=0;
 if(remaining>3){s.enabledE10=1;s.flipE3C=1;if(draw(random)&1)s.flipE3C=-s.flipE3C;}else s.flipE3C=0;
 if(remaining<.5f&&s.spinE38==0&&s.flipE3C==0){
  auto target=originalNpcSteeringTarget(d.position,d.route.closestPoint,d.route.previousLookaheadPoint);
  float steer=-originalNpcSteering(d.position,d.velocity,d.boardUp,target);
  words[0]=(words[0]&0xc0ffffffu)|((uint32_t(int32_t(npcMul(steer,31.f)))&63)<<24);s.indexE0C=-1;
 }
 words[0]=(words[0]&0xff00ffffu)|((uint32_t(s.indexE0C)&255)<<16);return words;
}
std::array<uint32_t,2> originalNpcAirCommand(std::array<uint32_t,2> words,OriginalNpcDrivingState& state,const OriginalNpcAirContext& c,const std::function<uint32_t()>& random){
 Round round;auto&s=state.trick;float remaining=100;
 if(c.trajectoryStatus==1||c.trajectoryStatus==3){remaining=sub(c.predictedTime,c.elapsed);if(remaining>15)state.longFlightTicks484=increment(state.longFlightTicks484);else state.longFlightTicks484=0;}else state.longFlightTicks484=0;
 if(state.longFlightTicks484>=31){state.longFlightTicks484=0;words[0]|=0x1000;return words;}
 if(!s.decisionE14){
  originalNpcStopRotations(s,remaining,c);if(s.remainingE34<=0)s.remainingE34=15;
  if(s.phaseE18){s.remainingE34=remaining;s.phaseE18=0;}
  else if(s.indexE0C==-1&&remaining>.5f&&remaining<s.remainingE34&&c.mainAnimationClass!=20)s.indexE0C=originalNpcChooseGrab(s,remaining,state.parameterDFC,c,random);
  else if(s.fieldE20&&remaining<1){s.fieldE28=0;s.indexE0C=-1;}
  else if(remaining<=add(s.releaseE2C,F(0x3e99999a))){s.fieldE28=0;s.indexE0C=-1;s.fieldE24=0;} //10B404 bc1tl -> 10B43C..10B444 also clears E24
  else if(sub(s.startE30,remaining)>2&&s.fieldE24==0){s.fieldE28=0;s.indexE0C=-1;s.fieldE24=0;}
 }else if(s.flipE3C==0&&s.indexE0C==-1){s.indexE0C=draw(random)%15;if(draw(random)&1)s.fieldE28=1;}
 words[0]=(words[0]&0xc0ffffffu)|((uint32_t(int32_t(npcMul(s.spinE38,31.f)))&63)<<24);
 words[1]=(words[1]&~63u)|(uint32_t(int32_t(npcMul(s.flipE3C,31.f)))&63);
 words[0]=(words[0]&0xff00ffffu)|((uint32_t(s.indexE0C)&255)<<16);
 if(s.fieldE24||s.fieldE28)words[0]|=0x4000;else words[0]&=~0x4000u;
 if(s.fieldE20==1)words[1]=(words[1]&0xfffc0fffu)|0x21000;
 return words;
}
}
