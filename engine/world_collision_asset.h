#pragma once
#import <Foundation/Foundation.h>
#include "world_body_collision.hpp"
std::unique_ptr<ssx::WorldBodyCollision> loadWorldBodyCollision(NSString* path,NSString* expectedSourceHash,NSString* terrainPath=nil);
