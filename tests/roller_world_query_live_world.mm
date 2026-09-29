// Native world for tests/roller_world_query_live.cpp (browser world data).
#import <Foundation/Foundation.h>
#include "../engine/world_collision_asset.h"
std::unique_ptr<ssx::WorldBodyCollision> loadRollerNativeWorld(const char* path){@autoreleasepool{
    NSString* folder=@(path);
    NSDictionary* terrain=[NSJSONSerialization JSONObjectWithData:[NSData dataWithContentsOfFile:[folder stringByAppendingPathComponent:@"terrain.json"]] options:0 error:nil];
    return loadWorldBodyCollision([folder stringByAppendingPathComponent:@"world_collision.json"],terrain[@"source_sha256"],[folder stringByAppendingPathComponent:@"terrain.json"]);
}}
