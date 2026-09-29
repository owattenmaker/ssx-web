#include <limits>
#include <algorithm>
#include "npc_path.hpp"
#include "original_float.hpp"
#include "terrain_contact_math.hpp"
#include <algorithm>
#include <bit>
#include <cfenv>
#include <stdexcept>
#pragma STDC FENV_ACCESS ON
namespace ssx {
std::vector<int> originalNpcPathCandidates(std::span<const OriginalNpcPath> paths,std::array<float,3> point,size_t maximum,bool require){
 OriginalRounding round;
 if(paths.size()>200)throw std::runtime_error("Original NPC candidate array capacity exceeded");
 maximum=std::min(maximum,paths.size());std::vector<int> result;std::vector<float> distance;
 if(!maximum)return result;
 constexpr float unavailable=std::bit_cast<float>(0x7cf0bdc2u);
 for(size_t index=0;index<paths.size();++index){
  const auto&p=paths[index];std::array<float,3> delta{};
  for(unsigned k=0;k<3;++k){if(point[k]<p.geometry.low[k])delta[k]=originalScalarSubtract(p.geometry.low[k],point[k]);else if(p.geometry.high[k]<point[k])delta[k]=originalScalarSubtract(point[k],p.geometry.high[k]);}
  float metric=std::max({delta[0],delta[1],delta[2]});if(require&&p.field3C==0)metric=unavailable;
  distance.push_back(metric);
  if(metric==0){distance.back()=unavailable;result.push_back(int(index));if(result.size()==maximum)return result;}
 }
 while(result.size()<maximum){size_t index=0;for(size_t i=1;i<distance.size();++i)if(distance[i]<distance[index])index=i;result.push_back(int(index));distance[index]=unavailable;}
 return result;
}
}
#include <cmath>
namespace ssx {namespace {
constexpr float npcF(uint32_t bits){return std::bit_cast<float>(bits);}
using NpcRound=OriginalRounding;
float npcDot(std::array<float,3> a){float x=terrain_original::mul(a[0],a[0]),y=terrain_original::mul(a[1],a[1]),z=terrain_original::mul(a[2],a[2]);float sum=terrain_original::add(x,y);sum=terrain_original::add(sum,z);return terrain_original::add(sum,0.f);}
}
float originalNpcRouteAffinity(int16_t role,bool a,bool b,bool c){
 NpcRound round;if((role==0&&!a)||(role==1&&!b)||(role==2&&!c)||(role!=0&&role!=1&&role!=2&&bool(a==b)!=c))return 0;
 return terrain_original::mul(float(4-int(a)-int(b)-int(c)),npcF(0x42053333));
}
float originalNpcPathScore(const OriginalNpcPath& path,int index,const OriginalNpcPathScoreContext& c,const std::function<uint32_t()>& nextRandom){
 NpcRound round;auto flags=path.flags38;unsigned grade=(flags>>7)&7,type=(flags>>4)&7;bool flag0=flags&1;
 float affinity=originalNpcRouteAffinity(c.roleE00,flags&8,flags&4,flags&2);
 if(affinity<=0||(!c.allowFlag0E04&&flag0))return 0;
 float speed=terrain_original::sqrt(npcDot(c.velocity)),kmh=terrain_original::mul(speed,npcF(0x3d1374bc));float threshold=grade>=1&&grade<=6?float(grade*20):0.f;
 if((type==0&&threshold<kmh)||(type==3&&kmh<threshold))return 0;
 std::array<float,3> ahead;
 if(!(speed>0)){
  // EE div.s by zero returns +/-FLT_MAX (no infinities) and mul/add overflow saturates to FLT_MAX:
  // a stationary rider (all components 0) looks 0 cm ahead; a sub-normal-speed one saturates.
  constexpr float big=std::numeric_limits<float>::max();
  auto saturate=[](double x){return float(std::clamp(x,-double(std::numeric_limits<float>::max()),double(std::numeric_limits<float>::max())));};
  for(unsigned k=0;k<3;++k){float offset=c.velocity[k]==0?0.f:saturate(double(c.velocity[k])*big);ahead[k]=saturate(double(c.position[k])+double(offset));}
 }else{
  float multiplier=originalScalarDivide(796.f,speed);
  for(unsigned k=0;k<3;++k){float offset=terrain_original::mul(c.velocity[k],multiplier);ahead[k]=terrain_original::add(c.position[k],offset);}
 }
 OriginalRacePathCache cache;auto projection=originalRacePathProject(path.geometry,ahead,cache,true);float distance=projection.lateralDistance;
 float term=terrain_original::mul(distance,distance);term=terrain_original::mul(term,npcF(0x378637bd));float proximity=originalScalarSubtract(100.f,term);if(proximity<0)proximity=0;
 if(distance>5000.f)return -1;
 std::array<float,7> terms{};terms[0]=terrain_original::mul(proximity,npcF(0x3e68ba2f));
 if(c.followedPathIndex==index)return 100.f;
 float factor=originalScalarDivide(1.f,22.f);factor=terrain_original::mul(factor,npcF(0x3eaaaaab));terms[2]=terrain_original::mul(float(type),factor);
 int occupants=0;for(int candidate:c.npcPathIndices)if(candidate==index)++occupants;if(c.currentPathIndex==index)--occupants;
 float traffic=occupants<=0?100.f:occupants==1?25.f:0.f;
 factor=originalScalarDivide(5.f,22.f);terms[3]=terrain_original::mul(traffic,factor);terms[4]=originalScalarDivide(affinity,22.f);
 factor=originalScalarDivide(5.f,22.f);if(c.allowFlag0E04&&flag0)terms[5]=terrain_original::mul(factor,100.f);
 factor=originalScalarDivide(2.f,22.f);if(c.randomizeE08){if(!nextRandom)throw std::runtime_error("Original NPC path score requires shared RNG");terms[6]=terrain_original::mul(float(nextRandom()%100),factor);}
 float result=0;for(float value:terms)result=originalScalarAdd(result,value);return result;
}
}
namespace ssx {
bool originalNpcSelectPath(std::span<const OriginalNpcPath> paths,OriginalNpcRouteState& state,OriginalNpcPathScoreContext context,bool exclude,const std::function<uint32_t()>& random){
 NpcRound round;context.currentPathIndex=state.pathIndex;
 auto look=context.position;float speed=terrain_original::sqrt(npcDot(context.velocity));
 if(speed!=0){float multiplier=originalScalarDivide(796.f,speed);for(unsigned k=0;k<3;++k)look[k]=terrain_original::add(context.position[k],terrain_original::mul(context.velocity[k],multiplier));}
 auto candidates=originalNpcPathCandidates(paths,look,6,false);int selected=-1;float best=context.computerControlled?-1.f:npcF(0x7cf0bdc2);
 for(int index:candidates){
  if(exclude&&index==state.pathIndex)continue;const auto& path=paths[index];OriginalRacePathCache empty;
  auto projection=originalRacePathProject(path.geometry,look,empty,true);float length=0;for(auto segment:path.geometry.segments)length=originalScalarAdd(length,segment[3]);
  if(originalScalarSubtract(length,200.f)<projection.distance)continue;
  float score;
  if(context.computerControlled)score=originalNpcPathScore(path,index,context,random);
  else{
   auto ahead=originalRacePathSample(path.geometry,originalScalarAdd(projection.distance,796.f));
   std::array<float,3> a{},b{};for(unsigned k=0;k<3;++k){a[k]=terrain_original::sub(look[k],ahead[k]);b[k]=terrain_original::sub(context.position[k],projection.point[k]);}
   score=originalScalarAdd(npcDot(b),npcDot(a));
  }
  if(context.computerControlled?best<score:score<best){best=score;selected=index==state.pathIndex?-1:index;}
 }
 if(selected<0)return false;
 state.pathIndex=selected;state.cache.segment=-1;const auto& path=paths[selected];
 auto projection=originalRacePathProject(path.geometry,context.position,state.cache,true);
 state.closestPoint=projection.point;state.lateralDistance=projection.lateralDistance;
 state.currentDistance=state.previousDistance=projection.distance;
 state.lookaheadPoint=originalRacePathSample(path.geometry,originalScalarAdd(projection.distance,796.f));return true;
}
}

#include "ground_motion.hpp"
namespace ssx {
bool originalNpcRouteProgress(std::span<const OriginalNpcPath> paths,OriginalNpcRouteState& state,OriginalNpcPathScoreContext context,float remaining,int32_t tick,const std::function<uint32_t()>& random){
 NpcRound round;if(state.pathIndex<0||size_t(state.pathIndex)>=paths.size())throw std::runtime_error("Missing original NPC AI path");
 state.previousDistance=state.currentDistance;const auto& path=paths[state.pathIndex];
 auto projection=originalRacePathProject(path.geometry,context.position,state.cache,true);
 state.closestPoint=projection.point;state.currentDistance=projection.distance;state.lateralDistance=projection.lateralDistance;
 state.lookaheadPoint=originalRacePathSample(path.geometry,originalScalarAdd(state.currentDistance,796.f));state.previousLookaheadPoint=state.lookaheadPoint;
 bool changed=false;
 if(remaining>0){
  if(state.lateralDistance>500.f&&tick%60==0)changed=originalNpcSelectPath(paths,state,context,false,random);
  else{
   float length=0;for(auto segment:path.geometry.segments)length=originalScalarAdd(length,segment[3]);
   if(originalScalarSubtract(length,200.f)<state.currentDistance)changed=originalNpcSelectPath(paths,state,context,true,random);
   else{bool transition=false;for(const auto&event:path.geometry.events)if(!(state.currentDistance<event.start)&&!(event.end<state.previousDistance)&&(event.type==18||event.type==20)){transition=true;break;}
    if(transition)changed=originalNpcSelectPath(paths,state,context,false,random);
   }
  }
 }
 float x=originalScalarSubtract(state.lookaheadPoint[0],state.closestPoint[0]),y=originalScalarSubtract(state.lookaheadPoint[1],state.closestPoint[1]);
 if(x==0)state.heading=y==0?y:y<0?-npcF(0x3fc90fdb):npcF(0x3fc90fdb);
 else{float angle=originalAtan(originalScalarDivide(y,x));if(x<0)angle=y>0?originalScalarAdd(angle,npcF(0x40490fdb)):originalScalarSubtract(angle,npcF(0x40490fdb));state.heading=angle;}
 return changed;
}
}
namespace ssx {
std::vector<OriginalRacePathEvent> originalNpcPathEvents(const OriginalNpcPath& path,float previous,float current,size_t maximum){
 std::vector<OriginalRacePathEvent> result;if(current<previous||!maximum)return result;
 for(auto event:path.geometry.events){if(current<event.start||event.end<previous)continue;result.push_back(event);if(result.size()==maximum)break;}return result;
}
}
namespace ssx {
std::optional<OriginalNpcSpeedZone> originalNpcSpeedZone(const OriginalNpcPath& path,float previous,float current){
 NpcRound round;for(const auto&event:originalNpcPathEvents(path,previous,originalScalarAdd(current,600.f)))if(event.type==17)return OriginalNpcSpeedZone{terrain_original::mul(float(std::bit_cast<int32_t>(event.value)),npcF(0x41de38e5)),event.start<=current};return {};
}
std::optional<OriginalNpcJumpZone> originalNpcJumpZone(const OriginalNpcPath& path,float previous,float current){
 NpcRound round;for(const auto&event:originalNpcPathEvents(path,previous,originalScalarAdd(current,300.f)))if(event.type==16){
  OriginalNpcJumpZone out;int32_t value=std::bit_cast<int32_t>(event.value);out.speedCmps=terrain_original::mul(float(value>>4),npcF(0x41de38e5));out.started=event.start<=current;
  for(unsigned bit=0;bit<4;++bit)out.flags[bit]=(event.value>>(3-bit))&1;
  out.startPoint=originalRacePathSample(path.geometry,event.start);out.endPoint=originalRacePathSample(path.geometry,event.end);return out;
 }return {};
}
std::optional<bool> originalNpcToggleZone(const OriginalNpcPath& path,float previous,float current){
 NpcRound round;for(const auto&event:originalNpcPathEvents(path,previous,originalScalarAdd(current,50.f)))if(event.type==19)return std::bit_cast<int32_t>(event.value)>0;return {};
}
}
