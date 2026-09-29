#import <Foundation/Foundation.h>
#include "../engine/replay_io.h"
#include "../engine/ground_surface.hpp"
#include <bit>
#include <iostream>
static ssx::PrototypeRider makeRider(NSDictionary* fixture,int surface,const ssx::CollisionWorld& terrain) {
    NSMutableDictionary* raw=[fixture mutableCopy];NSMutableDictionary* profile=[raw[@"profile"] mutableCopy];NSMutableDictionary* material=[profile[@"surface"] mutableCopy];material[@"id"]=@(surface);profile[@"surface"]=material;raw[@"profile"]=profile;
    NSMutableDictionary* state=[raw[@"state"] mutableCopy];
    state[@"position"]=@[@(-5.f),@0,@(-.78f)];state[@"velocity"]=@[@1000,@0,@0];
    state[@"normal"]=state[@"board_up"]=@[@0,@0,@1];state[@"forward"]=state[@"physical_forward"]=@[@1,@0,@0];state[@"lateral"]=@[@0,@1,@0];state[@"surface_velocity"]=@[@0,@0,@0];
    state[@"quaternion"]=@[@0,@0,@(-.7071067690849304f),@(.7071067690849304f)];state[@"distance"]=@(-.78f);state[@"depth1"]=@.4;state[@"depth3"]=@2.1;
    for(NSString* key in @[@"turn",@"brake",@"crouch",@"animation_turn",@"extra_lean",@"presentation_lift",@"presentation_roll"])state[key]=@{@"current":@0,@"rate":@0,@"target":@0};
    state[@"flags308"]=@0;state[@"boost"]=@0;state[@"boost_window"]=@0;state[@"boost_tier_counter"]=@0;state[@"boost_speed_floor"]=@0;
    state[@"heading_offset"]=@0;state[@"manual_spin"]=@0;state[@"mode_timing"]=@(-1);state[@"time_scale"]=@1;state[@"animation_index"]=@5;state[@"control_state"]=@0;state[@"reverse_stance"]=@NO;raw[@"state"]=state;[raw removeObjectForKey:@"cache"];
    ssx::PrototypeRider rider;initializeNativeReplay(rider,@{@"native":@{@"initial":@{@"original_ground":raw,@"grounded":@YES}}},terrain,{0,0,0},{0,1,0},0);return rider;
}
int main(int argc,char** argv){@autoreleasepool {
    if(argc!=2)return 2;NSDictionary* fixture=[NSJSONSerialization JSONObjectWithData:[NSData dataWithContentsOfFile:@(argv[1])] options:0 error:nil];
    ssx::CollisionWorld empty({});unsigned checked=0;
    for(int index=0;index<19;++index) {
        auto rider=makeRider(fixture,index,empty);const auto&p=rider.originalGroundProfile();NSArray* words=fixture[@"surface_catalog"][index][@"raw_words"];
        auto check=[&](unsigned offset,float value){if(std::bit_cast<uint32_t>(value)!=[words[offset/4]unsignedIntValue])throw std::runtime_error("Catalog field differs from original source bits");++checked;};
        check(0,p.surface.gravity);check(8,p.surface.lateralDrag);check(0x1c,p.surface.powderDamping);check(0x14,p.depthTarget1);check(0x18,p.depthTarget3);check(0xc,p.maxTurnAngle);check(0x10,p.airHeight);check(0x20,p.autoBoostSpeed);check(0x24,p.autoBoostFactor);check(0x38,p.alignmentRate);check(4,p.surfaceTerminalVelocity);
        check(0x28,p.headingProfile.surface28);check(0x2c,p.headingProfile.surface2C);check(0x30,p.headingProfile.surface30);check(0x34,p.headingProfile.surface34);
        for(unsigned i=0;i<4;++i){check(0x90+i*8,p.surface.slipFriction[i].x);check(0x94+i*8,p.surface.slipFriction[i].y);}
        if(p.surface.id!=index||rider.groundSurfaceProfileMissing)throw std::runtime_error("Catalog material binding failed");
    }
    for(auto pair:{std::pair{0,2},{2,3},{3,7},{7,0},{0,13},{13,0}}) {
        ssx::TerrainPatch before,after;before.resource=1;before.authoredSurface=pair.first;after.resource=2;after.authoredSurface=pair.second;
        before.coefficients[0]={-20,0,-20};before.coefficients[1]={20,0,0};before.coefficients[4]={0,0,40};after.coefficients=before.coefficients;after.coefficients[0].x=0;
        ssx::CollisionWorld terrain({});terrain.setTerrainPatches({before,after});auto rider=makeRider(fixture,pair.first,terrain);
        auto initial=rider.originalGroundState();auto oldProfile=rider.originalGroundProfile();auto expectedState=initial;
        oldProfile.speedLimit=ssx::originalGroundSpeedLimit(oldProfile,initial,0);auto expected=ssx::originalGroundIntegrate(oldProfile,expectedState);
        rider.advance(1./60,{},terrain,empty);
        if(!rider.grounded||rider.currentGroundSurfaceId()!=pair.second||rider.lastGroundContact.surface!=pair.second||rider.lastGroundContact.resource!=2)throw std::runtime_error("Rider did not cross authored material boundary");
        if(rider.originalGroundDiagnostics().acceleration!=expected.acceleration)throw std::runtime_error("Transition applied new material before original force phase");
        if(rider.originalGroundProfile().speedLimit!=oldProfile.speedLimit)throw std::runtime_error("Transition overwrote frame-begin cached speed limit");
        auto second=rider.originalGroundState();auto newProfile=rider.originalGroundProfile();newProfile.speedLimit=ssx::originalGroundSpeedLimit(newProfile,second,0);auto expectedSecond=ssx::originalGroundIntegrate(newProfile,second);
        rider.advance(1./60,{},terrain,empty);
        if(rider.originalGroundDiagnostics().acceleration!=expectedSecond.acceleration||rider.groundSurfaceProfileMissing)throw std::runtime_error("Next force phase did not use newly contacted material");
        std::cout<<"Material crossing "<<pair.first<<" -> "<<pair.second<<": original old/new force order and retained speed limit pass\n";
    }
    if(ssx::originalGroundSurfaceId(true,-1)!=0||ssx::originalGroundSurfaceId(false,13)!=0)throw std::runtime_error("Original default surface mapping");
    std::cout<<checked<<" loaded material floats match source words exactly across19 records\n";
}}
