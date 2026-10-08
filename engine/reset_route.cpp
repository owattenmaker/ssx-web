#include "reset_route.hpp"
#include "original_float.hpp"
#include "terrain_contact_math.hpp"
#include <bit>
#include <algorithm>
#include <cfenv>
#include <stdexcept>
#pragma STDC FENV_ACCESS ON
namespace ssx {
std::array<float,3> originalResetPathSample(const OriginalRacePath& path,float& distance){
 OriginalRounding round;
 const float original=distance;float alternate=originalScalarAdd(original,50.f),selected=alternate;
 for(const auto& event:path.events){
  if(original<event.start||event.end<original)continue;
  if(event.type==16)alternate=std::max(alternate,event.end);
  else if(event.type==12)selected=std::min(selected,event.start);
  else if(event.type==14)selected=std::max(selected,event.end);
 }
 distance=selected==original?alternate:selected;return originalRacePathSample(path,distance);
}
std::array<float,3> originalResetPathDirection(const OriginalRacePath& path,float distance){
 if(path.segments.empty())throw std::runtime_error("Reset direction has no path segments");
 for(size_t i=0;i<path.segments.size();i++){const auto& segment=path.segments[i];if(i+1==path.segments.size()||distance<=segment[3])return {segment[0],segment[1],segment[2]};distance=originalScalarSubtract(distance,segment[3]);}
 throw std::runtime_error("Unreachable reset path direction");
}
OriginalResetRouteEffects originalResetRoute(std::span<const OriginalNpcPath> paths,OriginalNpcRouteState& state,
        std::array<float,3> position,bool allowPathEnd){
 OriginalRounding round;
 // VU0 horizontal dot (0x112EA0..0x112EAC): x + y, then 1.0 x z and 1.0 x w through the 1.0 vector as fs.
 auto square=[](std::array<float,3> v){
  float x=terrain_original::mul(v[0],v[0]),y=terrain_original::mul(v[1],v[1]),z=terrain_original::mul(v[2],v[2]);
  float sum=terrain_original::add(x,y);
  sum=terrain_original::add(sum,terrain_original::mul(1.f,z));
  return terrain_original::add(sum,terrain_original::mul(1.f,0.f));
 };
 float best=std::bit_cast<float>(0x7cf0bdc2u);int selected=-1;
 for(int index:originalNpcPathCandidates(paths,position,6,true)){
  const auto& path=paths[index].geometry;OriginalRacePathCache empty;
  auto projection=originalRacePathProject(path,position,empty,true);
  float length=0;for(auto segment:path.segments)length=originalScalarAdd(length,segment[3]);
  if(!allowPathEnd&&originalScalarSubtract(length,200.f)<projection.distance)continue;
  float aheadDistance=originalScalarAdd(projection.distance,796.f);auto ahead=originalResetPathSample(path,aheadDistance);
  std::array<float,3> nearDelta{},aheadDelta{};
  for(unsigned k=0;k<3;k++){nearDelta[k]=terrain_original::sub(position[k],projection.point[k]);aheadDelta[k]=terrain_original::sub(position[k],ahead[k]);}
  float score=originalScalarAdd(square(nearDelta),square(aheadDelta));
  if(score<best){best=score;selected=index==state.pathIndex?-1:index;}
 }
 //112EEC checks the old AB8 pointer before installing a candidate.
 if(state.pathIndex<0)return {};
 if(size_t(state.pathIndex)>=paths.size())throw std::runtime_error("Reset route points outside authored path table");
 OriginalResetRouteEffects result{true,selected>=0};if(selected>=0)state.pathIndex=selected;
 state.cache.segment=-1;const auto& path=paths[state.pathIndex].geometry;
 auto projection=originalRacePathProject(path,position,state.cache,true);
 state.lateralDistance=projection.lateralDistance;state.currentDistance=projection.distance;
 state.closestPoint=originalResetPathSample(path,state.currentDistance);
 float aheadDistance=originalScalarAdd(state.currentDistance,796.f);state.lookaheadPoint=originalResetPathSample(path,aheadDistance);
 state.previousDistance=state.currentDistance;return result;
}
}
