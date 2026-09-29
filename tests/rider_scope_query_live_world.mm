// Native terrain for tests/rider_scope_query_live.cpp (browser world data, as tests/landing_contact_reference.mm).
#import <Foundation/Foundation.h>
#include "../engine/collision.hpp"
#include <memory>
std::unique_ptr<ssx::CollisionWorld> loadScopeNativeTerrain(const char* path){@autoreleasepool{
    using namespace ssx;NSString* folder=@(path);
    NSDictionary* terrainData=[NSJSONSerialization JSONObjectWithData:[NSData dataWithContentsOfFile:[folder stringByAppendingPathComponent:@"terrain.json"]] options:0 error:nil];
    std::vector<TerrainPatch> patches;
    for(NSDictionary*d in terrainData[@"patches"]){TerrainPatch p;p.resource=[d[@"resource_id"]unsignedIntValue];p.authoredFlags=[d[@"authored_flags"]unsignedIntValue];p.authoredSurface=[d[@"authored_surface_id"]intValue];p.hasAuthoredBounds=true;
        auto vec=[](NSArray*a){return Vec3{[a[0]doubleValue],[a[1]doubleValue],[a[2]doubleValue]};};p.authoredMinimum=vec(d[@"authored_bounds_min"]);p.authoredMaximum=vec(d[@"authored_bounds_max"]);for(unsigned i=0;i<16;++i)p.coefficients[i]=vec(d[@"coefficients"][i]);patches.push_back(p);}
    auto terrain=std::make_unique<CollisionWorld>(std::vector<Triangle>{});terrain->setTerrainPatches(std::move(patches));return terrain;
}}
