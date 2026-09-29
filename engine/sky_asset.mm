#include "sky_asset.h"
#include <stdexcept>
@implementation SSXSkyAsset
+ (instancetype)skyForLocation:(NSString*)name device:(id<MTLDevice>)device {
    NSString* root=[[@(SSX_ASSET_ROOT) stringByAppendingPathComponent:name] stringByAppendingPathComponent:@""];
    NSData* manifest=[NSData dataWithContentsOfFile:[root stringByAppendingPathComponent:@"sky.json"]];
    if (!manifest) return nil;
    NSDictionary* sky=[NSJSONSerialization JSONObjectWithData:manifest options:0 error:nil];
    if (![sky isKindOfClass:NSDictionary.class]||[sky[@"version"] intValue]!=1||[sky[@"vertex_stride"] intValue]!=40||[sky[@"basis_version"] intValue]!=1)
        @throw [NSException exceptionWithName:@"Invalid sky package" reason:@"Re-run tools/import_sky.py" userInfo:nil];
    SSXSkyAsset* asset=[SSXSkyAsset new];
    asset->skyLocation=sky[@"sky_location"];
    NSData* vb=[NSData dataWithContentsOfFile:[root stringByAppendingPathComponent:@"sky-vertices.bin"]];
    NSData* ib=[NSData dataWithContentsOfFile:[root stringByAppendingPathComponent:@"sky-indices.bin"]];
    NSData* cb=[NSData dataWithContentsOfFile:[root stringByAppendingPathComponent:@"sky-colors.bin"]];
    NSUInteger vertexCount=[sky[@"vertex_count"] unsignedLongValue],indexCount=[sky[@"index_count"] unsignedLongValue];
    if (!vb||vb.length!=vertexCount*40||!ib||ib.length!=indexCount*4||!cb||cb.length!=vertexCount*16)
        @throw [NSException exceptionWithName:@"Invalid sky package" reason:@"Sky buffers do not match sky.json" userInfo:nil];
    const uint32_t* indexData=(const uint32_t*)ib.bytes;
    for (NSUInteger i=0;i<indexCount;++i) if (indexData[i]>=vertexCount) @throw [NSException exceptionWithName:@"Invalid sky package" reason:@"Index outside sky vertices" userInfo:nil];
    asset->vertices=[device newBufferWithBytes:vb.bytes length:vb.length options:MTLResourceStorageModeShared];
    asset->indices=[device newBufferWithBytes:ib.bytes length:ib.length options:MTLResourceStorageModeShared];
    asset->colors=[device newBufferWithBytes:cb.bytes length:cb.length options:MTLResourceStorageModeShared];
    asset->colorUnity=[sky[@"draw"][@"vertex_color_unity"] floatValue];
    if (!(asset->colorUnity>0)) asset->colorUnity=1;
    NSMutableDictionary* textures=[NSMutableDictionary new];
    for (NSString* key in sky[@"textures"]) {
        NSDictionary* t=sky[@"textures"][key];
        NSUInteger w=[t[@"width"] unsignedLongValue],h=[t[@"height"] unsignedLongValue];
        NSData* rgba=[NSData dataWithContentsOfFile:[root stringByAppendingPathComponent:t[@"path"]]];
        if (!w||!h||w>4096||h>4096||rgba.length!=w*h*4) @throw [NSException exceptionWithName:@"Invalid sky texture" reason:key userInfo:nil];
        MTLTextureDescriptor* td=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm width:w height:h mipmapped:NO];
        td.usage=MTLTextureUsageShaderRead;
        id<MTLTexture> texture=[device newTextureWithDescriptor:td];
        [texture replaceRegion:MTLRegionMake2D(0,0,w,h) mipmapLevel:0 withBytes:rgba.bytes bytesPerRow:w*4];
        textures[key]=texture;
    }
    // Opaque dome panels first, then the alpha horizon panels over them.
    for (int pass=0;pass<2;++pass) for (NSDictionary* b in sky[@"batches"]) {
        bool hasAlpha=[b[@"has_alpha"] boolValue];
        if (hasAlpha!=(pass==1)) continue;
        NSUInteger first=[b[@"first_index"] unsignedLongValue],count=[b[@"index_count"] unsignedLongValue];
        if (first>indexCount||count>indexCount-first||count%3) @throw [NSException exceptionWithName:@"Invalid sky batch" reason:b.description userInfo:nil];
        id<MTLTexture> texture=textures[[NSString stringWithFormat:@"9-%@",b[@"texture"]]];
        if (!texture) @throw [NSException exceptionWithName:@"Invalid sky batch" reason:@"Missing sky texture" userInfo:nil];
        asset->batches.push_back({first,count,texture,hasAlpha});
    }
    // Painted fog: the exporter names the entry confirmed for the area start;
    // its spatial quadtree is not decoded, so one entry covers the whole area.
    NSDictionary* fog=sky[@"fog"];NSDictionary* entry=nil;
    NSArray* painted=fog[@"painted"];
    if ([fog[@"start"] isKindOfClass:NSDictionary.class]&&fog[@"start"][@"entry"]) {
        NSUInteger index=[fog[@"start"][@"entry"] unsignedLongValue];
        if (index<painted.count) entry=painted[index];
    }
    if (!entry&&painted.count) entry=painted.firstObject;
    if (!entry) entry=fog[@"defaults"];
    NSArray* color=entry[@"color"];
    asset->fogColor=color.count==3?simd::float3{[color[0] floatValue],[color[1] floatValue],[color[2] floatValue]}:simd::float3{.43f,.55f,.71f};
    asset->fogNearMeters=[entry[@"near_cm"] floatValue]/100;asset->fogFarMeters=[entry[@"far_cm"] floatValue]/100;
    if (!(asset->fogFarMeters>asset->fogNearMeters)) {asset->fogNearMeters=30;asset->fogFarMeters=300;}
    return asset;
}
@end
