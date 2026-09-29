#include "native_world.h"
#include "mesh_asset.h"
#include <cstdio>
namespace {
template<size_t N>NSArray* values(std::array<float,N> source){NSMutableArray* out=[NSMutableArray array];for(auto v:source)[out addObject:@(v)];return out;}
}
int main(int argc,char** argv){@autoreleasepool{try{@try{
    if(argc!=4)throw std::runtime_error("Usage: ssx3_world_audit world-start.json frames output.json");
    NSString* input=@(argv[1]);NSString* output=@(argv[3]);
    if([input.stringByStandardizingPath isEqual:output.stringByStandardizingPath])throw std::runtime_error("World audit cannot overwrite its seed");
    NSError* error=nil;NSData* data=[NSData dataWithContentsOfFile:input];
    if(!data)throw std::runtime_error("World seed is missing");
    NSDictionary* source=[NSJSONSerialization JSONObjectWithData:data options:0 error:&error];if(![source isKindOfClass:NSDictionary.class])throw std::runtime_error("Invalid world JSON");
    char* end=nullptr;long frames=strtol(argv[2],&end,10);if(!end||*end||frames<0||frames>36000)throw std::runtime_error("Invalid audit frame count");
    id<MTLDevice> device=MTLCreateSystemDefaultDevice();if(!device)throw std::runtime_error("Metal device unavailable");
    SSXMeshAsset* course=[[SSXMeshAsset alloc] initWithDevice:device queue:[device newCommandQueue] location:source[@"location"]];
    ssx::NativeRaceWorld native(source,@(SSX_ASSET_ROOT),*course->terrainCollision,*course->collision,course->bodyCollision.get(),course->spawn,course->spawnNormal,[](ssx::RiderWorld&,size_t){return ssx::RiderInput{};});
    auto& world=native.world();NSMutableArray* rows=[NSMutableArray array];NSString* failure=nil;int failedFrame=-1;
    for(int frame=0;frame<=frames;++frame){
        if(frame){try{world.tick();}catch(const std::exception& error){failure=@(error.what());failedFrame=frame;break;}}
        NSMutableArray* actors=[NSMutableArray array];
        for(size_t slot=0;slot<world.size();++slot){auto& actor=world.actor(slot);auto& rider=*actor.rider;auto& ground=rider.originalGroundState();auto& route=native.npcContext(slot).driving.route;
            auto physical=ssx::OriginalAirState::fromNative(rider.position,rider.velocity);auto commands=native.commands(slot);auto animation=native.animator(slot)->mainAnimationState();
            auto quaternion=rider.grounded?ground.quaternion:rider.originalAirPhysicalPose().quaternion;
            NSMutableArray* spheres=[NSMutableArray array];if(rider.lastBodyVolume)for(unsigned i=0;i<rider.lastBodyVolume->count;++i)[spheres addObject:values(rider.lastBodyVolume->spheres[i].centerCm)];
            [actors addObject:@{@"slot":@(slot),@"position_cm":values(physical.position),@"velocity_cmps":values(physical.velocity),@"quaternion":values(quaternion),@"control_state":@(rider.currentControlState()),@"grounded":@(rider.grounded),@"time_scale":@(ground.timeScale),@"commands":@[@(commands[0]),@(commands[1])],@"body_available":@(rider.lastBodyVolume.has_value()),@"body_spheres_cm":spheres,@"animation":@(animation?animation->semantic:-1),@"course_path":@(actor.progress.pathIndex),@"course_remaining":@(actor.progress.remaining),@"course_best_remaining":@(actor.progress.bestRemaining),@"ai_path":@(route.pathIndex),@"ai_distance":@(route.currentDistance),@"ai_previous_distance":@(route.previousDistance),@"ai_closest":values(route.closestPoint),@"ai_lookahead":values(route.lookaheadPoint),@"ai_previous_lookahead":values(route.previousLookaheadPoint),@"ai_lateral":@(route.lateralDistance),@"ai_heading":@(route.heading)}];
        }
        NSMutableArray* random=[NSMutableArray array];for(auto word:world.randomState()->words)[random addObject:@(word)];
        [rows addObject:@{@"frame":@(frame),@"tick":@(world.clock.totalTicks),@"race_tick":@(world.clock.raceTicks),@"random_state":random,@"participants":actors}];
    }
    auto encoded=[NSJSONSerialization dataWithJSONObject:@{@"provenance":source[@"provenance"],@"frames":rows,@"completed":@(!failure),@"failed_frame":@(failedFrame),@"failure":failure?:NSNull.null} options:NSJSONWritingPrettyPrinted error:&error];
    if(!encoded||![encoded writeToFile:output atomically:YES])throw std::runtime_error("Could not write native world audit");
    if(failure){std::fprintf(stderr,"Native shared world stopped at frame%d: %s (completed frames retained)\n",failedFrame,failure.UTF8String);return 1;}
    std::printf("Native shared world: %ld frames, %zu participants\n",frames,world.size());
} @catch(NSException* error){std::fprintf(stderr,"World audit: %s\n",error.reason.UTF8String);return 1;}}catch(const std::exception& error){std::fprintf(stderr,"World audit: %s\n",error.what());return 1;}return 0;}}
