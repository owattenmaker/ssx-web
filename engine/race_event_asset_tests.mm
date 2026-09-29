#import "race_event_asset.h"
#include "original_float.hpp"
#include <cassert>
#include <algorithm>
#include <bit>
#include <cstdio>
int main(int argc,char**argv){@autoreleasepool {
 if(argc!=2||![[NSFileManager defaultManager] fileExistsAtPath:[NSString stringWithUTF8String:argv[1]]])return 77;
 auto asset=ssx::loadOriginalRaceEventAsset([NSString stringWithUTF8String:argv[1]]);assert(asset.paths.size()==8&&asset.participants.size()==6);
 auto found=std::find_if(asset.participants.begin(),asset.participants.end(),[](auto& x){return x.human;});assert(found!=asset.participants.end());auto rider=*found;
 ssx::OriginalRaceSession session(asset);
 assert(asset.clock.phase==ssx::RacePhase::Race&&asset.configuration[0]==0);unsigned finishCount=0,checkpointCount=0,frames=0;
 // Controlled kinematic traversal of the authored Snow Jam route. This verifies
 // loading and complete course-event orchestration, not snowboard physics/AI.
 for(int index:{3,4,5,6,7}){
  const auto& path=asset.paths[index];float length=0;for(auto segment:path.segments)length+=segment[3];
  if(index==7){auto event=std::find_if(path.events.begin(),path.events.end(),[](auto e){return e.type==1;});assert(event!=path.events.end());length=event->start+500;}
  float start=index==3?path.remainingAtOrigin-rider.progress.bestRemaining:0;
  for(float distance=start;distance<length;distance+=50){
   session.beginTick();auto clockEffects=session.state.clockEffects;
   if(clockEffects.requestResults){assert(finishCount==1);break;}
   auto point=ssx::originalRacePathSample(path,distance),next=ssx::originalRacePathSample(path,distance+50);std::array<float,3> velocity;for(int k=0;k<3;k++)velocity[k]=(next[k]-point[k])*60;
   session.endTick(point,velocity);auto effects=session.state.courseEffects;
   finishCount+=effects.finished;checkpointCount+=std::popcount(effects.checkpointMask);++frames;
  }
 }
 assert(finishCount==1&&checkpointCount==2&&session.state.clock.phase==ssx::RacePhase::EndRace);
 assert(session.state.checkpoints.humanMasks[0]==3&&session.state.finish.finishTicks>0);
 printf("Authored Snow Jam event traversal: %u ticks, two checkpoints, one finish; native results transition verified\n",frames);
}}
