#pragma once
#import <Metal/Metal.h>
#include <simd/simd.h>
#include "rider_animation_player.h"
#include "board_trail.hpp"
#include "snow_context.hpp"
#include "snow_particles.hpp"
#include "environment_asset.h"
#include "snow_crash.hpp"
namespace ssx {
class GameplayEffects {
    std::shared_ptr<OriginalRiderAnimation> player;
    board_trail::Profile trailProfile;
    board_trail::State trail;
    board_trail::Input trailInput;
    OriginalSnowContextInput snowInput;
    OriginalSnowContextState snowState;
    std::unique_ptr<OriginalEnvironmentAsset> environment;
    OriginalEnvironmentState environmentState;
    bool reportedLightingGap=false;
    OriginalSnowRandom visualRandom;
    OriginalRandomState particleRandom;
    std::array<OriginalSnowSurface,19> surfaces;
    std::array<std::unique_ptr<OriginalSnowParticles>,10> emitters;
    std::array<OriginalSnowChunkProfile,10> profiles;
    std::array<int,10> textureIds{};
    std::array<float,10> alphaScales{};
    unsigned boardIndex=23;
    uint32_t lastLandingSeen=0,lastCrashSeen=0;
    OriginalSnowBodyState bodyState;OriginalSnowBodyBoneTable bodyBones{};
    __strong id<MTLDevice> device;
    __strong id<MTLRenderPipelineState> spritePipeline,trailPipeline,maskPipeline;
    __strong NSArray* trailDepthStates;
    __strong id<MTLDepthStencilState> depth;
    __strong NSMutableDictionary* textures;
    __strong id<MTLTexture> trailTexture;
public:
    GameplayEffects(id<MTLDevice>,MTLPixelFormat colour,MTLPixelFormat depth,NSUInteger samples,
                    NSString* assetRoot,NSString* course,NSDictionary* initial,std::shared_ptr<OriginalRiderAnimation>);
    void step(const PrototypeRider&);
    void draw(id<MTLRenderCommandEncoder>,simd::float4x4 viewProjection,simd::float3 viewDirection);
    size_t trackSlices()const{return size_t(trail.count);}
    size_t visibleParticles()const;
};
}
