#import <Foundation/Foundation.h>
#include "../engine/world_collision_asset.h"
#include <iostream>
#include <map>
int main(int argc,char**argv) {@autoreleasepool {
    if(argc!=4&&argc!=2)return 2;
    auto read=[](NSString* path){return [NSJSONSerialization JSONObjectWithData:[NSData dataWithContentsOfFile:path] options:0 error:nil];};
    NSDictionary* package=read(@(argv[1]));
    auto world=loadWorldBodyCollision(@(argv[1]),package[@"source_sha256"]);
    if(argc==2){std::map<std::string,unsigned> reasons;for(const auto& i:world->instances)if(!i.unsupported.empty())++reasons[i.unsupported];std::cout<<world->instances.size()<<" instances\n";for(const auto& [reason,count]:reasons)std::cout<<count<<" "<<reason<<'\n';return 0;}
    NSDictionary* fixtures=read(@(argv[2]));
    auto vec=[](NSArray* a){return ssx::terrain_original::Vector{[a[0] floatValue],[a[1] floatValue],[a[2] floatValue]};};
    NSMutableArray* results=[NSMutableArray array];
    for(NSDictionary* item in fixtures[@"cases"])for(NSDictionary* volume in item[@"rider_volumes"]) {
        ssx::BodyCollisionVolume body;body.broadCenterCm=vec(volume[@"broad_center_cm"]);body.broadRadiusCm=[volume[@"broad_radius_cm"] floatValue];body.activeMask=[volume[@"mask"] unsignedIntValue];
        NSArray* spheres=volume[@"spheres"];body.count=unsigned(spheres.count);
        for(unsigned i=0;i<body.count;++i){NSDictionary* s=spheres[i];body.spheres[i]={vec(s[@"source_center_cm"]),[s[@"radius_cm"] floatValue],[s[@"bone"] unsignedIntValue]};}
        std::vector<uint32_t> filter;
        for(NSDictionary* binding in item[@"verified_instance_bindings"])if([binding[@"rider"] isEqual:volume[@"rider"]])filter.push_back([binding[@"resource"] unsignedIntValue]);
        auto query=world->query(body,vec(volume[@"source_ground_normal"]),&filter,[volume[@"query_flag874"] boolValue]);const auto& hit=query.best;
        NSMutableArray* unknown=[NSMutableArray array];for(auto id:query.unsupportedInstances)[unknown addObject:@(id)];
        [results addObject:@{@"state":item[@"state"],@"rider":volume[@"rider"],@"hit":@(hit.hit),@"instance":@(hit.instance),@"depth_cm":@(hit.penetrationCm),@"point_cm":@[@(hit.pointCm[0]),@(hit.pointCm[1]),@(hit.pointCm[2])],@"normal":@[@(hit.normal[0]),@(hit.normal[1]),@(hit.normal[2])],@"candidates":@(query.candidates),@"contacts":@(query.contacts),@"unsupported":unknown}];
    }
    NSData* output=[NSJSONSerialization dataWithJSONObject:results options:NSJSONWritingPrettyPrinted error:nil];[output writeToFile:@(argv[3]) atomically:YES];
    unsigned unsupported=0;for(const auto& instance:world->instances)unsupported+=!instance.unsupported.empty();
    std::cout<<world->instances.size()<<" authoredinstances loaded; "<<unsupported<<" explicitly unsupported; "<<results.count<<" capturedbodyqueries executed\n";
}}
