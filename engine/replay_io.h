#pragma once
#import <Foundation/Foundation.h>
#include "replay.hpp"
struct NativeReplaySpec {
    ssx::InputReplay input;
    __strong NSDictionary* source;
    __strong NSString* location;
    NativeReplaySpec(ssx::InputReplay value,NSDictionary* json,NSString* area):input(std::move(value)),source(json),location(area) {}
};
NativeReplaySpec readNativeReplay(NSString* path);
NSDictionary* readNativeRideStart(NSString* path);
void initializeNativeReplay(ssx::PrototypeRider& rider,NSDictionary* source,const ssx::CollisionWorld& terrain,
                           ssx::Vec3 spawn,ssx::Vec3 normal,double heading);
void writeNativeReplay(NSString* prefix,const NativeReplaySpec& spec,const std::vector<ssx::ReplayRecord>& records);
