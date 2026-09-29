#pragma once
#include "mesh_asset.h"
#include "sky_asset.h"
#include "riding.hpp"

static NSString* const shaderSource = @R"MSL(
#include <metal_stdlib>
using namespace metal;
struct Vertex { packed_float3 position; packed_float3 normal; packed_float2 uv; packed_float2 lightUV; };
struct Uniforms { float4x4 mvp; float4x4 model; float4 eye; float4 options; float4 fog; float4 fogRange; };
struct Skin { uint4 joints; float4 weights; };
struct Varying { float4 position [[position]]; float3 world; float3 normal; float2 uv; float2 lightUV; float4 color; };
vertex Varying terrainVertex(uint id [[vertex_id]], const device Vertex* mesh [[buffer(0)]],
                             constant Uniforms& u [[buffer(1)]], const device float4* colors [[buffer(2)]]) {
    Vertex v = mesh[id];
    return {u.mvp * float4(v.position, 1), float3(v.position), float3(v.normal), float2(v.uv), float2(v.lightUV), colors[id]};
}
vertex Varying riderVertex(uint id [[vertex_id]], const device Vertex* mesh [[buffer(0)]],
                           constant Uniforms& u [[buffer(1)]], const device float4* colors [[buffer(2)]],
                           const device Skin* skins [[buffer(3)]], constant float4x4* bones [[buffer(4)]]) {
    Vertex v=mesh[id]; Skin skin=skins[id];
    float4x4 pose=bones[skin.joints.x]*skin.weights.x+bones[skin.joints.y]*skin.weights.y
                 +bones[skin.joints.z]*skin.weights.z+bones[skin.joints.w]*skin.weights.w;
    float3 position=(pose*float4(v.position,1)).xyz;
    float3 normal=(pose*float4(v.normal,0)).xyz;
    return {u.mvp*float4(position,1),(u.model*float4(position,1)).xyz,(u.model*float4(normal,0)).xyz,float2(v.uv),float2(v.lightUV),colors[id]};
}
fragment float4 terrainFragment(Varying v [[stage_in]], constant Uniforms& u [[buffer(1)]],
                                texture2d<float> albedo [[texture(0)]], texture2d<float> lightmap [[texture(1)]]) {
    constexpr sampler tile(filter::linear, mip_filter::linear, address::repeat, max_anisotropy(8));
    constexpr sampler lightSampler(filter::linear, address::clamp_to_edge);
    float4 sampled = albedo.sample(tile, v.uv);
    if (u.options.z > 0.5 && sampled.a < 0.35) discard_fragment();
    float3 base = sampled.rgb;
    // GameCube lightmaps are scaled modulate maps: their median texel is
    // near 90, so unit gain leaves shadowed snow navy. 1.75 keeps shadow
    // detail readable and sunlit snow bright without clipping whole fields;
    // the exact original TEV scale is still an approximation to verify.
    float3 baked = lightmap.sample(lightSampler, v.lightUV).rgb * 1.75;
    float diffuse = 0.55 + 0.45 * abs(dot(normalize(v.normal), normalize(float3(0.4, 0.85, 0.3))));
    float3 color = base * v.color.rgb * mix(float3(diffuse), baked, u.options.y);
    if (u.options.x > 0.5) color = abs(normalize(v.normal));
    // Painted distance fog toward the area's horizon colour (world painter
    // kind 15). Strength fogRange.z is a preview approximation of the
    // original blend mode, which is not decoded yet.
    float depth = distance(v.world, u.eye.xyz);
    float fogAmount = clamp((depth - u.fogRange.x) / max(u.fogRange.y - u.fogRange.x, 1.0), 0.0, 1.0) * u.fogRange.z;
    color = mix(color, u.fog.rgb, fogAmount);
    // Terrain base alpha is not a blend mask: the start-area chevron/lane
    // patches are the only ground there and the original draws them opaque.
    return float4(color, 1);
}
vertex Varying skyVertex(uint id [[vertex_id]], const device Vertex* mesh [[buffer(0)]],
                         constant Uniforms& u [[buffer(1)]], const device float4* colors [[buffer(2)]]) {
    Vertex v = mesh[id];
    // 0x353b10: identity matrix with the camera position as its translation.
    float3 position = float3(v.position) + u.eye.xyz;
    return {u.mvp * float4(position, 1), position, float3(v.normal), float2(v.uv), float2(v.lightUV), colors[id]};
}
fragment float4 skyFragment(Varying v [[stage_in]], constant Uniforms& u [[buffer(1)]], texture2d<float> albedo [[texture(0)]]) {
    constexpr sampler panel(filter::linear, address::clamp_to_edge);
    float4 sampled = albedo.sample(panel, v.uv);
    float3 color = sampled.rgb * (v.color.rgb / max(u.options.x, 0.001));
    return float4(color, u.options.y > 0.5 ? sampled.a : 1.0);
}
// Retro presentation: the scene is rendered at native resolution and then
// softened as if a 2003-era ~640-wide framebuffer had been bilinearly
// upscaled, with a faint horizontal chroma smear. Deliberately subtle.
struct PostOut { float4 position [[position]]; float2 uv; };
vertex PostOut postVertex(uint id [[vertex_id]]) {
    float2 corners[3] = {float2(-1, -1), float2(3, -1), float2(-1, 3)};
    float2 c = corners[id];
    return {float4(c, 0, 1), float2(c.x * .5 + .5, .5 - c.y * .5)};
}
fragment float4 postFragment(PostOut in [[stage_in]], texture2d<float> scene [[texture(0)]], constant float4& params [[buffer(0)]]) {
    constexpr sampler smooth(filter::linear, address::clamp_to_edge);
    float2 texel = params.xy;      // 1 / drawable size
    float radius = params.z;       // softness radius in pixels
    float strength = params.w;     // 0 = off
    float3 sharp = scene.sample(smooth, in.uv).rgb;
    float2 d = texel * radius;
    float3 soft = (scene.sample(smooth, in.uv + float2( d.x,  d.y)).rgb + scene.sample(smooth, in.uv + float2(-d.x,  d.y)).rgb
                 + scene.sample(smooth, in.uv + float2( d.x, -d.y)).rgb + scene.sample(smooth, in.uv + float2(-d.x, -d.y)).rgb) * .25;
    // Composite-style chroma bleed: colour differences smear sideways a little.
    float3 left = scene.sample(smooth, in.uv - float2(d.x * 2.5, 0)).rgb, right = scene.sample(smooth, in.uv + float2(d.x * 2.5, 0)).rgb;
    float3 chroma = (left + right) * .5;
    float luma = dot(soft, float3(.299, .587, .114));
    float3 bled = luma + (chroma - dot(chroma, float3(.299, .587, .114))) * .5 + (soft - luma) * .5;
    float3 filtered = mix(soft, bled, .35);
    return float4(mix(sharp, filtered, strength), 1);
}
)MSL";

struct Uniforms { float4x4 mvp; float4x4 model; float4 eye; float4 options; float4 fog; float4 fogRange; };
static id<MTLRenderPipelineState> makePostPipeline(id<MTLDevice> device,id<MTLLibrary> library,MTLPixelFormat color,MTLPixelFormat depth) {
    MTLRenderPipelineDescriptor* d=[MTLRenderPipelineDescriptor new];
    d.vertexFunction=[library newFunctionWithName:@"postVertex"];d.fragmentFunction=[library newFunctionWithName:@"postFragment"];
    d.colorAttachments[0].pixelFormat=color;d.depthAttachmentPixelFormat=depth;d.stencilAttachmentPixelFormat=depth;
    NSError* error=nil;auto pipeline=[device newRenderPipelineStateWithDescriptor:d error:&error];
    if (!pipeline) @throw [NSException exceptionWithName:@"Metal post pipeline" reason:error.description userInfo:nil];
    return pipeline;
}
// Sky dome pipeline: alpha-blended horizon panels, drawn before the world.
static id<MTLRenderPipelineState> makeSkyPipeline(id<MTLDevice> device,id<MTLLibrary> library,MTLPixelFormat color,MTLPixelFormat depth,NSUInteger samples) {
    MTLRenderPipelineDescriptor* d=[MTLRenderPipelineDescriptor new];
    d.vertexFunction=[library newFunctionWithName:@"skyVertex"];d.fragmentFunction=[library newFunctionWithName:@"skyFragment"];
    d.colorAttachments[0].pixelFormat=color;d.depthAttachmentPixelFormat=depth;d.stencilAttachmentPixelFormat=depth;d.rasterSampleCount=samples;
    auto attachment=d.colorAttachments[0];attachment.blendingEnabled=YES;
    attachment.sourceRGBBlendFactor=MTLBlendFactorSourceAlpha;attachment.destinationRGBBlendFactor=MTLBlendFactorOneMinusSourceAlpha;
    attachment.sourceAlphaBlendFactor=MTLBlendFactorOne;attachment.destinationAlphaBlendFactor=MTLBlendFactorOneMinusSourceAlpha;
    NSError* error=nil;auto pipeline=[device newRenderPipelineStateWithDescriptor:d error:&error];
    if (!pipeline) @throw [NSException exceptionWithName:@"Metal sky pipeline" reason:error.description userInfo:nil];
    return pipeline;
}
static id<MTLDepthStencilState> makeSkyDepthState(id<MTLDevice> device) {
    MTLDepthStencilDescriptor* ds=[MTLDepthStencilDescriptor new];ds.depthCompareFunction=MTLCompareFunctionAlways;ds.depthWriteEnabled=NO;
    return [device newDepthStencilStateWithDescriptor:ds];
}
static MTLClearColor skyClearColor(SSXSkyAsset* sky) {
    if (!sky) return MTLClearColorMake(.18,.30,.44,1);
    return MTLClearColorMake(sky->fogColor.x,sky->fogColor.y,sky->fogColor.z,1);
}
// Fills the painted fog uniforms; without a sky package fog stays disabled.
static void applyFog(Uniforms& u,SSXSkyAsset* sky) {
    if (!sky) {u.fog={0,0,0,0};u.fogRange={1,2,0,0};return;}
    u.fog={sky->fogColor.x,sky->fogColor.y,sky->fogColor.z,1};
    u.fogRange={sky->fogNearMeters,sky->fogFarMeters,.5f,0};
}
static void drawSky(id<MTLRenderCommandEncoder> enc,SSXSkyAsset* sky,id<MTLRenderPipelineState> pipeline,id<MTLDepthStencilState> depthState,Uniforms u) {
    if (!sky) return;
    [enc setRenderPipelineState:pipeline];[enc setDepthStencilState:depthState];[enc setCullMode:MTLCullModeNone];
    [enc setVertexBuffer:sky->vertices offset:0 atIndex:0];[enc setVertexBuffer:sky->colors offset:0 atIndex:2];
    u.options={sky->colorUnity,0,0,0};
    for (const auto& batch:sky->batches) {
        u.options.y=batch.hasAlpha?1:0;
        [enc setVertexBytes:&u length:sizeof(u) atIndex:1];[enc setFragmentBytes:&u length:sizeof(u) atIndex:1];
        [enc setFragmentTexture:batch.texture atIndex:0];
        [enc drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:batch.count indexType:MTLIndexTypeUInt32 indexBuffer:sky->indices indexBufferOffset:batch.first*4];
    }
}
static float4x4 perspective(float aspect,float nearDistance,float verticalFovRadians=65*M_PI/180) {
    const float n = nearDistance, f = 20000, y = 1 / tanf(.5f * verticalFovRadians), x = y / aspect;
    return float4x4(float4{x,0,0,0},float4{0,y,0,0},float4{0,0,f/(n-f),-1},float4{0,0,n*f/(n-f),0});
}
static float4x4 look(float3 eye, float3 direction) {
    float3 z = -simd::normalize(direction), x = simd::normalize(simd::cross(float3{0,1,0},z));
    float3 y = simd::cross(z,x);
    return float4x4(float4{x.x,y.x,z.x,0},float4{x.y,y.y,z.y,0},float4{x.z,y.z,z.z,0},
                     float4{-simd::dot(x,eye),-simd::dot(y,eye),-simd::dot(z,eye),1});
}


static float3 nativeVector(ssx::Vec3 v) { return {float(v.x),float(v.y),float(v.z)}; }
static float4x4 riderTransform(const ssx::PrototypeRider& rider) {
    if(rider.hasOriginalAirPresentation()){
        const auto& pose=rider.originalAirPresentationPose();
        const auto& q=pose.quaternion;
        // B*Q*B^-1 for the asset import basisB(x,y,z)=(x,z,-y).
        float4x4 model=simd_matrix4x4(simd_quaternion(float4{q[0],q[2],-q[1],q[3]}));
        model.columns[3]=float4{pose.position[0]/100.f,pose.position[2]/100.f,-pose.position[1]/100.f,1};
        return model;
    }
    float3 up=nativeVector(rider.grounded?rider.normal:ssx::Vec3{0,1,0});
    float3 front={float(std::sin(rider.heading)),0,float(std::cos(rider.heading))};
    front=simd_normalize(front-up*simd_dot(front,up));
    float3 side=simd_normalize(simd_cross(up,front));
    float3 position=nativeVector(rider.position);
    return {float4{side.x,side.y,side.z,0},float4{up.x,up.y,up.z,0},
            float4{front.x,front.y,front.z,0},float4{position.x,position.y,position.z,1}};
}
struct FollowCamera { float3 target,eye; };
static FollowCamera followCamera(const ssx::PrototypeRider& rider,const ssx::CollisionWorld& world) {
    float3 up=nativeVector(rider.grounded?rider.normal:ssx::Vec3{0,1,0});
    float3 forward={float(std::sin(rider.heading)),0,float(std::cos(rider.heading))};
    forward=simd_normalize(forward-up*simd_dot(forward,up));
    float3 target=nativeVector(rider.position)+up*1.05+forward*.8;
    float3 eye=target-forward*6.5+up*2.4;
    auto delta=eye-target;
    auto hit=world.raycast({target.x,target.y,target.z},{delta.x,delta.y,delta.z},simd_length(delta));
    if (hit.hit) eye=target+simd_normalize(delta)*float(std::max(.5,hit.distance-.3));
    return {target,eye};
}
static void drawAsset(id<MTLRenderCommandEncoder> enc,SSXMeshAsset* asset,id<MTLRenderPipelineState> pipeline,
                      Uniforms u,bool baked,const std::vector<float4x4>& palette={}) {
    [enc setRenderPipelineState:pipeline];
    [enc setVertexBuffer:asset->vertices offset:0 atIndex:0];
    [enc setVertexBuffer:asset->colors offset:0 atIndex:2];
    if (asset->skeleton) {
        [enc setVertexBuffer:asset->skinBuffer offset:0 atIndex:3];
        [enc setVertexBytes:palette.data() length:palette.size()*sizeof(float4x4) atIndex:4];
    }
    [enc setVertexBytes:&u length:sizeof(u) atIndex:1];
    for (const auto& batch:asset->batches) {
        u.options.y=baked&&batch.hasLight?1:0;u.options.z=batch.instance?1:0;
        [enc setFragmentBytes:&u length:sizeof(u) atIndex:1];
        [enc setFragmentTexture:batch.base atIndex:0];[enc setFragmentTexture:batch.light atIndex:1];
        [enc drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:batch.count indexType:MTLIndexTypeUInt32
                      indexBuffer:asset->indices indexBufferOffset:batch.first*4];
    }
}
