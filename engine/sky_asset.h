#pragma once
#import <MetalKit/MetalKit.h>
#include <simd/simd.h>
#include <vector>

struct SkyBatch { NSUInteger first, count; __strong id<MTLTexture> texture; bool hasAlpha; };
// Original per-area sky dome (tools/import_sky.py): a 3 m model drawn around
// the camera position before the world, plus the painted fog for the start.
@interface SSXSkyAsset : NSObject {
@public
    id<MTLBuffer> vertices,indices,colors;
    std::vector<SkyBatch> batches;
    float colorUnity;
    simd::float3 fogColor;
    float fogNearMeters,fogFarMeters;
    NSString* skyLocation;
}
// Returns nil when the area has no exported sky package.
+ (instancetype)skyForLocation:(NSString*)name device:(id<MTLDevice>)device;
@end
