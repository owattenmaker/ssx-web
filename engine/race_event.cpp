#include "race_event.hpp"
#include "original_float.hpp"
#include "terrain_contact_math.hpp"
#include <bit>
#include <cfenv>
#include <stdexcept>
namespace ssx { namespace {
int32_t addTicks(int32_t a,int32_t b){return std::bit_cast<int32_t>(uint32_t(a)+uint32_t(b));}
void valid(RacePhase p){if(int(p)<0||int(p)>7)throw std::runtime_error("Invalid original race phase");}
}
void originalRaceSelect(OriginalRaceClock& s,RacePhase next){
 valid(next);if(s.phase==next)return;s.previous=s.phase;s.previousHandler=s.phase;s.phase=next;
}
bool originalRaceHumansFinished(std::span<const float> values){for(float value:values)if(!(value>=0))return false;return true;}
RaceClockEffects originalRaceClockBeginTick(OriginalRaceClock& s,std::span<const float> humans){
 valid(s.phase);valid(s.previous);RaceClockEffects out;
 if(s.phase==RacePhase::None)throw std::runtime_error("Original race dispatch requires a selected phase");
 if(s.previous!=s.phase){
  out.exited=s.previousHandler;
  if(out.exited==RacePhase::Countdown)s.countdownTicks=0;
  out.entered=s.phase;
  switch(s.phase){
   case RacePhase::Freeride:s.raceEnabled=0;break;
   case RacePhase::PreRace:s.raceTicks=0;s.preRaceLocal=0;break;
   case RacePhase::Countdown:s.countdownTicks=180;break;
   case RacePhase::Race:out.raceStartNotification=true;break;
   case RacePhase::EndRace:out.raceFinishNotification=true;break;
   case RacePhase::Shutdown:out.shutdown=true;break;
   default:break;
  }
  s.previousHandler=RacePhase::None;s.previous=s.phase;
 }
 switch(s.phase){
  case RacePhase::GameInit:originalRaceSelect(s,RacePhase::Freeride);break;
  case RacePhase::Freeride:originalRaceSelect(s,RacePhase::Race);break;
  case RacePhase::Countdown:if(s.countdownTicks>0)s.countdownTicks=addTicks(s.countdownTicks,-1);else originalRaceSelect(s,RacePhase::Race);break;
  case RacePhase::Race:s.raceTicks=addTicks(s.raceTicks,1);if(originalRaceHumansFinished(humans)){originalRaceSelect(s,RacePhase::EndRace);out.requestResults=true;}break;
  default:break;
 }
 return out;
}
void originalRaceClockEndTick(OriginalRaceClock& s){s.totalTicks=addTicks(s.totalTicks,1);}
RaceClockEffects originalRaceClockStep(OriginalRaceClock& s,std::span<const float> humans){auto out=originalRaceClockBeginTick(s,humans);originalRaceClockEndTick(s);return out;}
void originalRaceMarkFinished(OriginalRiderRaceFinish& s,int32_t ticks){s.finishTicks=addTicks(ticks,s.penaltyTicks);if(s.elapsed<0)s.elapsed=0;}
void originalRaceFinishElapsedStep(OriginalRiderRaceFinish& s){
 if(!(s.elapsed>=0))return;OriginalRounding rounding;
 s.elapsed=originalScalarAdd(s.elapsed,std::bit_cast<float>(0x3c888889u));
}
}
#include <algorithm>
#include <cmath>
namespace ssx {namespace {
using V=std::array<float,3>;
float A(float a,float b){return originalScalarAdd(a,b);}
float S(float a,float b){return originalScalarSubtract(a,b);}
using Round=OriginalRounding;
float vuDot(V a,V b){float x=terrain_original::mul(a[0],b[0]),y=terrain_original::mul(a[1],b[1]),z=terrain_original::mul(a[2],b[2]);return terrain_original::add((terrain_original::add(x,y)),z);}
}
OriginalRacePathProjection originalRacePathProject(const OriginalRacePath& path,V position,OriginalRacePathCache& cache,bool horizontal){
 Round round;OriginalRacePathProjection result;float best=std::bit_cast<float>(0x7cf0bdc2u),lateralSquared=0;
 bool cached=cache.segment>=0;int index=cached?cache.segment:0;V origin=cached?cache.origin:path.origin;
 float distance=cached?cache.distance:0,limit=cached?A(distance,3000.f):best;
 for(;index<int(path.segments.size());index++){
  const auto& segment=path.segments[index];V direction={segment[0],segment[1],segment[2]},relative;
  for(unsigned i=0;i<3;i++)relative[i]=terrain_original::sub(position[i],origin[i]);
  float projection=horizontal?A(terrain_original::mul(relative[0],direction[0]),terrain_original::mul(relative[1],direction[1])):vuDot(relative,direction);
  float clamped=projection>=0?std::min(projection,segment[3]):0;V point,residual;
  for(unsigned i=0;i<3;i++){point[i]=terrain_original::add(origin[i],terrain_original::mul(direction[i],clamped));residual[i]=terrain_original::sub(position[i],point[i]);}
  float metric=horizontal?A(terrain_original::mul(residual[0],residual[0]),terrain_original::mul(residual[1],residual[1])):vuDot(residual,residual);
  if(metric<best){
   best=metric;lateralSquared=horizontal?metric:A(terrain_original::mul(residual[0],residual[0]),terrain_original::mul(residual[1],residual[1]));
   result.distance=A(distance,clamped);result.point=point;cache.origin=origin;cache.distance=distance;cache.segment=index;
   if(cached&&clamped==projection)break;
  }
  if(distance>limit)break;
  for(unsigned i=0;i<3;i++)origin[i]=terrain_original::add(origin[i],terrain_original::mul(direction[i],segment[3]));distance=A(distance,segment[3]);
 }
 result.lateralDistance=originalScalarSqrt(lateralSquared);return result;
}
std::vector<OriginalRacePathEvent> originalRacePathEvents(const OriginalRacePath& path,float previous,float current,size_t maximum){
 Round round;float low=S(path.remainingAtOrigin,previous),high=S(path.remainingAtOrigin,current);std::vector<OriginalRacePathEvent> result;
 if(high<low||maximum==0)return result;
 for(auto event:path.events){if(high<event.start||event.end<low)continue;result.push_back(event);if(result.size()==maximum)break;}return result;
}
}
namespace ssx {
bool originalRaceRecordCheckpoint(OriginalRaceCheckpoints& s,int human,int checkpoint){
 if(s.mode||s.field60C||s.field610||s.field61C||s.field620||human<0||checkpoint<0||checkpoint>=s.count)return false;
 if(human>=int(s.humanMasks.size()))throw std::runtime_error("Original checkpoint human index out of range");
 uint32_t bit=1u<<(unsigned(checkpoint)&31u);if(s.humanMasks[human]&bit)return false;
 s.pendingHumanMask=uint8_t(s.pendingHumanMask|(1u<<(unsigned(human)&31u)));s.humanMasks[human]|=bit;return true;
}
}

namespace ssx {
V originalRacePathSample(const OriginalRacePath& path,float distance){
 Round round;V p=path.origin;
 for(auto segment:path.segments){float length=segment[3];if(distance<length){for(unsigned i=0;i<3;i++)p[i]=terrain_original::add(p[i],terrain_original::mul(segment[i],distance));return p;}
  distance=S(distance,length);for(unsigned i=0;i<3;i++)p[i]=terrain_original::add(p[i],terrain_original::mul(segment[i],length));}
 return p;
}
void originalRaceSelectPath(std::span<const OriginalRacePath> paths,OriginalRaceProgress& state,V position,V velocity,bool exclude){
 Round round;constexpr float huge=std::bit_cast<float>(0x7cf0bdc2u);float speed=terrain_original::sqrt(vuDot(velocity,velocity));V look=position;
 if(speed!=0){float scale=originalScalarDivide(796.f,speed);for(unsigned i=0;i<3;i++)look[i]=terrain_original::add(position[i],terrain_original::mul(velocity[i],scale));}
 std::vector<float> bounds;std::vector<int> candidates;size_t maximum=std::min<size_t>(3,paths.size());
 for(size_t n=0;n<paths.size();n++){
  const auto& path=paths[n];std::array<float,3> d{};for(unsigned i=0;i<3;i++){if(look[i]<path.low[i])d[i]=S(path.low[i],look[i]);else if(path.high[i]<look[i])d[i]=S(look[i],path.high[i]);}
  float metric=std::max({d[0],d[1],d[2]});bounds.push_back(metric);
  if(metric==0){bounds.back()=huge;candidates.push_back(int(n));if(candidates.size()==maximum)break;}
 }
 while(candidates.size()<maximum){size_t index=0;for(size_t i=1;i<bounds.size();i++)if(bounds[i]<bounds[index])index=i;candidates.push_back(int(index));bounds[index]=huge;}
 float best=huge;int selected=state.pathIndex;
 for(int index:candidates){if(exclude&&index==state.pathIndex)continue;const auto& path=paths[index];OriginalRacePathCache empty;
  auto projected=originalRacePathProject(path,look,empty,true);float distance=projected.distance;
  if(index==state.pathIndex&&std::abs(S(S(path.remainingAtOrigin,state.remaining),distance))>1592.f)state.cache.segment=-1;
  auto ahead=originalRacePathSample(path,A(distance,796.f));V a,b;for(unsigned i=0;i<3;i++){a[i]=terrain_original::sub(position[i],projected.point[i]);b[i]=terrain_original::sub(look[i],ahead[i]);}
  float metric=A(vuDot(a,a),vuDot(b,b));if(metric<best){best=metric;selected=index;}
 }
 state.pathIndex=selected;
}
}
namespace ssx {
std::vector<int32_t> originalRaceBonusCrossings(const OriginalRaceBonusTable& t,float best,float remaining){
 std::vector<int32_t> out;if(!(remaining<best))return out;
 if(t.distance[0]==0||t.distance[0]<remaining)return out; //0x11302C / 0x11303C
 for(unsigned k=0;;){
  if(t.distance[k]<best)out.push_back(t.value[k]); //0x11305C: crossed this tick (earlier entries were passed before)
  if(++k>=6)break;
  if(t.distance[k]==0||t.distance[k]<remaining)break; //0x113088 / 0x11309C
 }
 return out;
}
std::vector<OriginalRacePathEvent> originalRaceProgressStep(std::span<const OriginalRacePath> paths,OriginalRaceProgress& state,V position,V velocity,int32_t ticks,const OriginalRaceBonusTable* bonus,std::vector<int32_t>* bonusCrossings){
 Round round;if(state.pathIndex<0||size_t(state.pathIndex)>=paths.size())throw std::runtime_error("Missing original race path");
 std::vector<OriginalRacePathEvent> events;
 auto project=[&](){const auto& path=paths[state.pathIndex];auto p=originalRacePathProject(path,position,state.cache,true);state.remaining=S(path.remainingAtOrigin,p.distance);
  if(bonus&&bonusCrossings){auto crossed=originalRaceBonusCrossings(*bonus,state.bestRemaining,state.remaining);bonusCrossings->insert(bonusCrossings->end(),crossed.begin(),crossed.end());}
  float endpoint=std::min(state.bestRemaining,state.remaining);events=originalRacePathEvents(path,state.bestRemaining,endpoint);if(state.remaining<state.bestRemaining)state.bestRemaining=state.remaining;return p;};
 int old=state.pathIndex;auto projection=project();
 if(state.bestRemaining>0){
  if(projection.lateralDistance>500.f&&ticks%60==0)originalRaceSelectPath(paths,state,position,velocity,false);
  else{
   const auto& path=paths[state.pathIndex];float length=0;for(auto segment:path.segments)length=A(length,segment[3]);
   if(projection.distance>S(length,200.f))originalRaceSelectPath(paths,state,position,velocity,true);
   else{
    float low=state.remaining>=0?S(path.remainingAtOrigin,state.remaining):0.f;
    float high=state.bestRemaining>=0?S(path.remainingAtOrigin,state.bestRemaining):length;
    bool change=false;for(auto event:path.events)if(!(high<event.start||event.end<low)&&(event.type==18||event.type==20)){change=true;break;}
    if(change)originalRaceSelectPath(paths,state,position,velocity,false);
   }
  }
 }
 if(state.pathIndex!=old){state.cache.segment=-1;project();}
 return events;
}
}
namespace ssx {
RaceCourseEffects originalRaceApplyCourseEvents(std::span<const OriginalRacePathEvent> events,OriginalRiderRaceFinish& finish,OriginalRaceCheckpoints& checkpoints,int human,int32_t ticks,bool eligible){
 RaceCourseEffects out;for(auto event:events){
  if(event.type==1&&eligible&&finish.elapsed<0){originalRaceMarkFinished(finish,ticks);out.finished=true;}
  if(event.type==11&&originalRaceRecordCheckpoint(checkpoints,human,int(event.value)))out.checkpointMask|=1u<<(event.value&31u);
 }return out;
}
}
