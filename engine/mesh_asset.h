#pragma once
#import <MetalKit/MetalKit.h>
#include "collision.hpp"
#include "world_body_collision.hpp"
#include "skeleton.hpp"
#include "animation_asset.h"
#include "rail_motion.hpp"
#include <memory>

struct alignas(16) SkinVertex { uint32_t joints[4]={0,0,0,0}; float weights[4]={0,0,0,0}; };
static_assert(sizeof(SkinVertex)==32);
struct Batch { NSUInteger first, count; __strong id<MTLTexture> base, light; bool hasLight, instance; };

// Asset ownership and decoding are independent of AppKit views. One course view
// composes static world assets and a skinned rider without hidden renderer views.
@interface SSXMeshAsset : NSObject {
@public
    id<MTLBuffer> vertices,indices,colors,skinBuffer;
    std::vector<Batch> batches;
    std::unique_ptr<ssx::CollisionWorld> collision,terrainCollision;
    std::unique_ptr<ssx::WorldBodyCollision> bodyCollision;
    std::vector<ssx::OriginalRailRecord> rails; // Authored kind-8 grind rails, source cm/Z-up rows
    std::unique_ptr<ssx::Skeleton> skeleton;
    std::shared_ptr<const ssx::AnimationRigAsset> originalAnimation;
    std::shared_ptr<const ssx::AnimationRigAsset> animationBindings;
    simd::float3 center,minimumBounds,maximumBounds;
    float radius;
    bool lightingVerified,hasSpawn;
    ssx::Vec3 spawn,spawnNormal;
    int diagnosticBone;
    NSArray* animationClips;
    NSData* animationSampleData;
    std::vector<int> animationBoneIds,mirrorBoneIds,animationParts;
    std::vector<int> translationChannels,rotationChannels,mirrorSources;
    std::vector<std::array<uint8_t,4>> quaternionMaps;
    std::vector<simd::float3> translationMaps;
}
- (instancetype)initWithDevice:(id<MTLDevice>)device queue:(id<MTLCommandQueue>)queue location:(NSString*)name;
- (std::vector<simd::float4x4>)paletteForOriginalPose:(const std::vector<ssx::AnimationTransform>&)pose scale:(ssx::AnimationVector)scale;
- (std::vector<simd::float4x4>)paletteAt:(double)seconds clip:(NSUInteger)index playing:(bool)playing mirror:(bool)mirror diagnostic:(bool)diagnostic;
@end
