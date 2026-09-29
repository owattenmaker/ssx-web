#import "race_event_asset.h"
#include <cmath>
#include <limits>
#include <stdexcept>
namespace ssx {namespace {
NSDictionary* object(id x,const char* label){if(![x isKindOfClass:NSDictionary.class])throw std::runtime_error(std::string("Invalid race object: ")+label);return x;}
NSArray* array(id x,const char* label,size_t maximum){if(![x isKindOfClass:NSArray.class]||[x count]>maximum)throw std::runtime_error(std::string("Invalid race array: ")+label);return x;}
double number(id x,const char* label){if(![x isKindOfClass:NSNumber.class]||!std::isfinite([x doubleValue]))throw std::runtime_error(std::string("Invalid race number: ")+label);return [x doubleValue];}
int32_t integer(id x,const char* label){double v=number(x,label);if(v<INT32_MIN||v>INT32_MAX||std::trunc(v)!=v)throw std::runtime_error(std::string("Invalid race integer: ")+label);return int32_t(v);}
uint32_t bits(id x,const char* label){double v=number(x,label);if(v<0||v>UINT32_MAX||std::trunc(v)!=v)throw std::runtime_error(std::string("Invalid race word: ")+label);return uint32_t(v);}
float scalar(id x,const char* label){float v=float(number(x,label));if(!std::isfinite(v))throw std::runtime_error(std::string("Invalid race float: ")+label);return v;}
template<size_t N>std::array<float,N> vector(id x,const char* label){auto a=array(x,label,N);if(a.count!=N)throw std::runtime_error(std::string("Race vector extent: ")+label);std::array<float,N> result;for(size_t i=0;i<N;i++)result[i]=scalar(a[i],label);return result;}
std::string hash(id x,const char* label){if(![x isKindOfClass:NSString.class]||[x length]!=64)throw std::runtime_error(std::string("Invalid race hash: ")+label);std::string s=[x UTF8String];for(char c:s)if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')))throw std::runtime_error("Race hash encoding");return s;}
RacePhase phase(id x){int n=integer(x,"phase");if(n<0||n>7)throw std::runtime_error("Race phase outside source range");return RacePhase(n);}
}
OriginalRaceEventAsset loadOriginalRaceEventAsset(NSString* path){
 NSError* error=nil;NSData* data=[NSData dataWithContentsOfFile:path options:0 error:&error];if(!data)throw std::runtime_error("Cannot read original race event asset");
 id json=[NSJSONSerialization JSONObjectWithData:data options:0 error:&error];return readOriginalRaceEventAsset(json);
}
OriginalRaceEventAsset readOriginalRaceEventAsset(NSDictionary* json){
 auto root=object(json,"root"),event=object(root[@"original_race_event"],"event"),clock=object(event[@"clock"],"clock");OriginalRaceEventAsset out;
 out.sourceEEHash=hash(root[@"ee_sha256"],"EE");auto provenance=object(event[@"provenance"],"provenance");out.courseHash=hash(provenance[@"course_sha256"],"course");
 auto config=array(provenance[@"configuration_bytes"],"configuration",4);if(config.count!=4)throw std::runtime_error("Race configuration extent");for(unsigned i=0;i<4;i++)out.configuration[i]=integer(config[i],"configuration");
 out.clock.phase=phase(clock[@"phase"]);out.clock.previous=phase(clock[@"previous"]);out.clock.previousHandler=phase(clock[@"previous_handler"]);
 out.clock.totalTicks=integer(clock[@"total_ticks"],"total ticks");out.clock.raceTicks=integer(clock[@"race_ticks"],"race ticks");out.clock.countdownTicks=integer(clock[@"countdown_ticks"],"countdown ticks");out.clock.raceEnabled=integer(clock[@"race_enabled"],"race enabled");out.clock.preRaceLocal=integer(clock[@"pre_race_local"],"pre-race local");
 for(id value in array(event[@"paths"],"paths",128)){
  auto p=object(value,"path");if(integer(p[@"index"],"path index")!=int(out.paths.size()))throw std::runtime_error("Race path ordering");OriginalRacePath path;
  path.origin=vector<3>(p[@"origin"],"origin");path.low=vector<3>(p[@"low"],"low");path.high=vector<3>(p[@"high"],"high");path.remainingAtOrigin=scalar(p[@"remaining_at_origin"],"path remaining");
  for(id point in array(p[@"segments"],"segments",100000)){auto segment=vector<4>(point,"segment");if(segment[3]<=0)throw std::runtime_error("Nonpositive source path segment");path.segments.push_back(segment);}
  if(path.segments.empty())throw std::runtime_error("Empty source path");
  for(id raw in array(p[@"events"],"events",4096)){auto e=object(raw,"course event");OriginalRacePathEvent item{bits(e[@"type"],"event type"),bits(e[@"value"],"event value"),scalar(e[@"start"],"event start"),scalar(e[@"end"],"event end")};if(item.end<item.start)throw std::runtime_error("Reversed course event");path.events.push_back(item);}
  out.paths.push_back(std::move(path));
 }
 if(out.paths.empty())throw std::runtime_error("No original course paths");
 for(id raw in array(event[@"participants"],"participants",6)){
  auto p=object(raw,"participant");if(integer(p[@"index"],"participant index")!=int(out.participants.size()))throw std::runtime_error("Race participant ordering");OriginalRaceParticipant rider;
  rider.human=number(p[@"human"],"human")!=0;rider.position=vector<3>(p[@"position"],"rider position");rider.velocity=vector<3>(p[@"velocity"],"rider velocity");rider.quaternion=vector<4>(p[@"quaternion"],"rider quaternion");
  rider.motionMode=integer(p[@"motion_mode"],"motion mode");rider.controlState=integer(p[@"control_state"],"control state");rider.dnf=integer(p[@"dnf"],"DNF");
  rider.finish={scalar(p[@"finish_elapsed"],"finish elapsed"),integer(p[@"penalty_ticks"],"penalty ticks"),integer(p[@"finish_ticks"],"finish ticks")};
  rider.progress.pathIndex=integer(p[@"path_index"],"active path");if(rider.progress.pathIndex<0||size_t(rider.progress.pathIndex)>=out.paths.size())throw std::runtime_error("Unknown participant path");
  rider.progress.remaining=scalar(p[@"remaining"],"remaining");rider.progress.bestRemaining=scalar(p[@"best_remaining"],"best remaining");auto cache=object(p[@"path_cache"],"path cache");rider.progress.cache={vector<3>(cache[@"origin"],"cache origin"),scalar(cache[@"distance"],"cache distance"),integer(cache[@"segment"],"cache segment")};
  if(rider.progress.cache.segment>=int(out.paths[rider.progress.pathIndex].segments.size()))throw std::runtime_error("Race path cache index");out.participants.push_back(rider);
 }
 auto checkpoints=object(event[@"checkpoints"],"checkpoints"),inhibited=object(checkpoints[@"inhibited_fields"],"checkpoint gates");out.checkpoints.count=integer(checkpoints[@"count"],"checkpoint count");
 auto masks=array(checkpoints[@"human_masks"],"human masks",4);for(unsigned i=0;i<masks.count;i++)out.checkpoints.humanMasks[i]=bits(masks[i],"checkpoint mask");
 out.checkpoints.pendingHumanMask=uint8_t(bits(checkpoints[@"pending_human_mask"],"pending mask"));out.checkpoints.mode=bits(inhibited[@"0"],"checkpoint mode");out.checkpoints.field60C=bits(inhibited[@"60c"],"checkpoint60c");out.checkpoints.field610=bits(inhibited[@"610"],"checkpoint610");out.checkpoints.field61C=bits(inhibited[@"61c"],"checkpoint61c");out.checkpoints.field620=bits(inhibited[@"620"],"checkpoint620");
 return out;
}
}
