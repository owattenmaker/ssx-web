#include "mesh_asset.h"
#include "world_collision_asset.h"
using simd::float3;
using simd::float4;
using simd::float4x4;

@implementation SSXMeshAsset
- (instancetype)initWithDevice:(id<MTLDevice>)device queue:(id<MTLCommandQueue>)queue location:(NSString*)name {
    self=[super init]; if (!self) return nil;
    NSString* root=[@(SSX_ASSET_ROOT) stringByAppendingPathComponent:name];
    NSData* riderData=[NSData dataWithContentsOfFile:[root stringByAppendingPathComponent:@"rider.json"]];
    NSDictionary* rider=riderData ? [NSJSONSerialization JSONObjectWithData:riderData options:0 error:nil] : nil;
    NSError* error=nil;
    NSData* manifest = [NSData dataWithContentsOfFile:[root stringByAppendingPathComponent:@"world.json"]];
    NSDictionary* world = manifest ? [NSJSONSerialization JSONObjectWithData:manifest options:0 error:&error] : nil;
    if (!world || [world[@"version"] intValue] != 1 || [world[@"vertex_stride"] intValue] != 40)
        @throw [NSException exceptionWithName:@"Missing terrain" reason:@"Run tools/import_world.py before launching this location." userInfo:nil];
    if (!rider && (![world[@"up_axis"] isEqualToString:@"Y"] || [world[@"basis_version"] intValue]!=1))
        @throw [NSException exceptionWithName:@"World basis needs reimport" reason:@"Run tools/import_world.py: original world assets are Z-up; native assets must convert (x,z,-y)." userInfo:nil];
    NSData* vb = [NSData dataWithContentsOfFile:[root stringByAppendingPathComponent:@"vertices.bin"]];
    NSData* ib = [NSData dataWithContentsOfFile:[root stringByAppendingPathComponent:@"indices.bin"]];
    if (!vb.length || vb.length != [world[@"vertex_count"] unsignedLongValue]*40 ||
        !ib.length || ib.length%12 || ib.length != [world[@"index_count"] unsignedLongValue]*4)
        @throw [NSException exceptionWithName:@"Invalid terrain" reason:@"Mesh buffers do not match manifest" userInfo:nil];
    const uint32_t* indexData = (const uint32_t*)ib.bytes;
    for (NSUInteger i=0; i<ib.length/4; ++i)
        if (indexData[i] >= vb.length/40) @throw [NSException exceptionWithName:@"Invalid terrain" reason:@"Index outside vertex buffer" userInfo:nil];
    const float* vertexData=(const float*)vb.bytes;
    for (NSUInteger i=0;i<vb.length/4;++i)
        if (!std::isfinite(vertexData[i])) @throw [NSException exceptionWithName:@"Invalid mesh" reason:@"Nonfinite vertex data" userInfo:nil];
    std::vector<ssx::Triangle> collisionTriangles;
    collisionTriangles.reserve(ib.length/12);
    auto position=[&](uint32_t index){const float* p=vertexData+index*10;return ssx::Vec3{p[0],p[1],p[2]};};
    for (NSUInteger i=0;i<ib.length/4;i+=3)
        collisionTriangles.push_back({position(indexData[i]),position(indexData[i+1]),position(indexData[i+2]),uint32_t(i/3)});
    collision=std::make_unique<ssx::CollisionWorld>(std::move(collisionTriangles));
    std::vector<ssx::Triangle> terrainTriangles;
    for (NSDictionary* batch in world[@"batches"]) {
        if ([batch[@"instance"] boolValue]) continue;
        NSUInteger first=[batch[@"first_index"] unsignedLongValue],count=[batch[@"index_count"] unsignedLongValue];
        if (first>ib.length/4 || count>ib.length/4-first || count%3)
            @throw [NSException exceptionWithName:@"Invalid terrain batch" reason:@"Collision range" userInfo:nil];
        for (NSUInteger i=first;i<first+count;i+=3)
            terrainTriangles.push_back({position(indexData[i]),position(indexData[i+1]),position(indexData[i+2]),uint32_t(i/3)});
    }
    terrainCollision=std::make_unique<ssx::CollisionWorld>(terrainTriangles);
    if (!rider) {
        NSData* analyticData=[NSData dataWithContentsOfFile:[root stringByAppendingPathComponent:@"terrain.json"]];
        NSDictionary* analytic=analyticData?[NSJSONSerialization JSONObjectWithData:analyticData options:0 error:nil]:nil;
        if([analytic[@"version"] intValue]!=1 || ![analytic[@"up_axis"] isEqualToString:@"Y"] ||
           ![analytic[@"source_sha256"] isEqual:world[@"source_sha256"]] ||
           [analytic[@"patches"] count]!=[world[@"patches"] count])
            @throw [NSException exceptionWithName:@"Terrain contact needs reimport" reason:@"Run tools/terrain_contact.py for this location or re-run tools/import_world.py." userInfo:nil];
        std::vector<ssx::TerrainPatch> surfaces;
        for(NSDictionary* entry in analytic[@"patches"]) {
            NSArray* coefficients=entry[@"coefficients"];
            if(coefficients.count!=16) @throw [NSException exceptionWithName:@"Invalid patch" reason:@"Coefficient count" userInfo:nil];
            ssx::TerrainPatch patch;patch.resource=[entry[@"resource_id"] unsignedIntValue];
            if(!entry[@"authored_flags"]) @throw [NSException exceptionWithName:@"Missing terrain flags" reason:@"Reimport terrain collision metadata" userInfo:nil];
            patch.authoredFlags=[entry[@"authored_flags"] unsignedIntValue];
            if(!entry[@"authored_surface_id"]) @throw [NSException exceptionWithName:@"Missing terrain surface" reason:@"Reimport terrain collision metadata" userInfo:nil];
            patch.authoredSurface=[entry[@"authored_surface_id"] intValue];
            NSArray* contactMin=entry[@"authored_bounds_min"],*contactMax=entry[@"authored_bounds_max"];
            if(contactMin.count!=3||contactMax.count!=3) @throw [NSException exceptionWithName:@"Missing terrain bounds" reason:@"Reimport terrain collision metadata" userInfo:nil];
            patch.hasAuthoredBounds=true;
            patch.authoredMinimum={[contactMin[0] doubleValue],[contactMin[1] doubleValue],[contactMin[2] doubleValue]};
            patch.authoredMaximum={[contactMax[0] doubleValue],[contactMax[1] doubleValue],[contactMax[2] doubleValue]};
            bool found=false;
            for(NSDictionary* source in world[@"collision_sources"])
                if([source[@"kind"] isEqualToString:@"terrain"] &&
                   [source[@"track"] unsignedIntValue]==(patch.resource&255) && [source[@"rid"] unsignedIntValue]==(patch.resource>>8)) {
                    patch.source=[source[@"first_triangle"] unsignedIntValue];found=true;break;
                }
            if(!found) @throw [NSException exceptionWithName:@"Invalid patch" reason:@"Missing render provenance" userInfo:nil];
            for(NSUInteger i=0;i<16;++i) {
                NSArray* c=coefficients[i];
                if(c.count!=3) @throw [NSException exceptionWithName:@"Invalid patch" reason:@"Coefficient dimensions" userInfo:nil];
                patch.coefficients[i]={[c[0] doubleValue],[c[1] doubleValue],[c[2] doubleValue]};
            }
            surfaces.push_back(patch);
        }
        terrainCollision->setTerrainPatches(std::move(surfaces));
    }
    if(!rider) {
        NSString* bodyPath=[root stringByAppendingPathComponent:@"world_collision.json"];
        // Package preparation publishes this together with regenerated scale-
        // correct world geometry. A missing backend remains explicit as nullptr.
        if([NSFileManager.defaultManager fileExistsAtPath:bodyPath])
            bodyCollision=loadWorldBodyCollision(bodyPath,world[@"source_sha256"],[root stringByAppendingPathComponent:@"terrain.json"]);
    }
    if(!rider){
        NSData* railData=[NSData dataWithContentsOfFile:[root stringByAppendingPathComponent:@"rails.json"]];
        NSDictionary* railJson=railData?[NSJSONSerialization JSONObjectWithData:railData options:0 error:nil]:nil;
        if(railJson){
            if([railJson[@"version"] intValue]!=1||![railJson[@"source_sha256"] isEqual:world[@"source_sha256"]])
                @throw [NSException exceptionWithName:@"Rail package needs reimport" reason:@"Run tools/import_rails.py for this location" userInfo:nil];
            for(NSDictionary* r in railJson[@"rails"]){
                ssx::OriginalRailRecord record;record.packedId=[r[@"packed_id"] unsignedIntValue];if(!r[@"runtime_flags"])throw std::runtime_error("rails.json lacks runtime_flags (tools/export_rail_runtime_flags.py)");record.flags=[r[@"runtime_flags"] unsignedIntValue];record.surface=[r[@"header_words"][@"surface_id"] intValue];
                for(NSDictionary* seg in r[@"segments"]){
                    ssx::OriginalRailSegment s;NSDictionary* src=seg[@"source"];NSArray* rows=src[@"coefficients"];
                    if(rows.count!=4||[src[@"bounds_min"] count]!=3||[src[@"bounds_max"] count]!=3)@throw [NSException exceptionWithName:@"Invalid rail segment" reason:r[@"name"] userInfo:nil];
                    for(unsigned k=0;k<4;++k){NSArray* row=rows[k];if(row.count!=3)@throw [NSException exceptionWithName:@"Invalid rail row" reason:r[@"name"] userInfo:nil];for(unsigned j=0;j<3;++j)s.coefficients[k][j]=[row[j] floatValue];}
                    for(unsigned j=0;j<3;++j){s.boundsMin[j]=[src[@"bounds_min"][j] floatValue];s.boundsMax[j]=[src[@"bounds_max"][j] floatValue];}
                    s.length=[seg[@"length_cm"] floatValue];s.distance=[seg[@"distance_cm"] floatValue];s.index=[seg[@"index"] intValue];s.previous=[seg[@"previous_segment"] intValue];s.next=[seg[@"next_segment"] intValue];s.flags=[seg[@"flags"] unsignedIntValue];
                    record.segments.push_back(s);
                }
                rails.push_back(std::move(record));
            }
        }
    }
    // Geometric bring-up spawn, not the original event start. A high triangle
    // alone can select decorative summit geometry, so require a continuous broad
    // downhill surface in front before selecting a candidate.
    std::vector<size_t> candidates;
    for (size_t i=0;i<terrainTriangles.size();i+=7) {
        const auto& t=terrainTriangles[i];auto cross=ssx::cross(t.b-t.a,t.c-t.a);
        if (std::abs(ssx::unit(cross).y)>.85 && ssx::dot(cross,cross)>4) candidates.push_back(i);
    }
    std::sort(candidates.begin(),candidates.end(),[&](size_t a,size_t b){
        const auto& x=terrainTriangles[a];const auto& y=terrainTriangles[b];
        return x.a.y+x.b.y+x.c.y>y.a.y+y.b.y+y.c.y;
    });
    for (size_t index:candidates) {
        const auto& t=terrainTriangles[index];auto n=ssx::unit(ssx::cross(t.b-t.a,t.c-t.a));if (n.y<0) n=n*-1;
        auto point=(t.a+t.b+t.c)*(1.0/3);
        auto direction=ssx::unit(ssx::Vec3{n.x,0,n.z});
        if (ssx::dot(direction,direction)<.5) continue;
        auto side=ssx::cross({0,1,0},direction);
        bool suitable=true;
        for (double ahead:{0.,8.,20.,40.}) for (double lateral:{-3.,0.,3.}) {
            auto at=point+direction*ahead+side*lateral;
            auto hit=terrainCollision->raycast(at+ssx::Vec3{0,3,0},{0,-1,0},ahead*.7+6);
            if (!hit.hit || hit.normal.y<.7 || hit.position.y>point.y+1) suitable=false;
        }
        if (suitable) {spawn=point;spawnNormal=n;hasSpawn=true;break;}
    }
    // Explicit development placement accepts native meters. The reference harness
    // can provide a measured start without baking guessed course coordinates here.
    NSString* placement=NSProcessInfo.processInfo.environment[@"SSX_SPAWN"];
    if (placement) {
        double x,y,z;
        if (sscanf(placement.UTF8String,"%lf,%lf,%lf",&x,&y,&z)!=3 || !std::isfinite(x)||!std::isfinite(y)||!std::isfinite(z))
            @throw [NSException exceptionWithName:@"Invalid SSX_SPAWN" reason:@"Expected x,y,z in native meters" userInfo:nil];
        auto hit=terrainCollision->raycast({x,y+5,z},{0,-1,0},30);
        if (hit.hit) {spawn=hit.position;spawnNormal=hit.normal;hasSpawn=true;}
    }
    vertices = [device newBufferWithBytes:vb.bytes length:vb.length options:MTLResourceStorageModeShared];
    indices = [device newBufferWithBytes:ib.bytes length:ib.length options:MTLResourceStorageModeShared];
    diagnosticBone=-1;
    if (rider) {
        if([NSFileManager.defaultManager fileExistsAtPath:[root stringByAppendingPathComponent:@"animation-packets.json"]])
            originalAnimation=std::make_shared<ssx::AnimationRigAsset>(ssx::loadOriginalAnimationRig(root));
        animationBindings=originalAnimation?originalAnimation:std::make_shared<ssx::AnimationRigAsset>(ssx::loadOriginalAnimationBindings(root));
        std::vector<ssx::Bone> bones;
        for (NSDictionary* b in rider[@"bones"]) {
            NSArray* t=b[@"translation"],*q=b[@"rotation"];
            if (t.count!=3 || q.count!=4) @throw [NSException exceptionWithName:@"Invalid rider" reason:@"Bone transform size" userInfo:nil];
            if ([b[@"name"] isEqualToString:@"bicepleft"]) diagnosticBone=int(bones.size());
            int animationBone=[b[@"index"] intValue];
            if (animationBone<0 || animationBone>=22) @throw [NSException exceptionWithName:@"Unsupported animation skeleton" reason:@"Body channel mapping" userInfo:nil];
            if (!b[@"animation_translation_channel"] || !b[@"animation_rotation_channel"])
                @throw [NSException exceptionWithName:@"Missing bone DOF layout" reason:@"Re-run tools/rider_assets.py" userInfo:nil];
            translationChannels.push_back([b[@"animation_translation_channel"] intValue]);
            rotationChannels.push_back([b[@"animation_rotation_channel"] intValue]);
            animationBoneIds.push_back(animationBone);
            animationParts.push_back([b[@"file"] intValue]);
            int mirrorBone=[b[@"mirror_index"] intValue];
            if (mirrorBone<0 || mirrorBone>=22) @throw [NSException exceptionWithName:@"Invalid mirror mapping" reason:@"Body channel index" userInfo:nil];
            mirrorBoneIds.push_back(mirrorBone);
            NSArray* mapping=b[@"mirror_quaternion_map"];
            if (mapping.count!=4) @throw [NSException exceptionWithName:@"Missing bone mapping" reason:@"Re-run tools/rider_assets.py" userInfo:nil];
            std::array<uint8_t,4> map;
            for (unsigned i=0;i<4;++i) {
                int value=[mapping[i] intValue];
                if (value<0 || value>7) @throw [NSException exceptionWithName:@"Invalid bone mapping" reason:@"Component index" userInfo:nil];
                map[i]=value;
            }
            quaternionMaps.push_back(map);
            NSArray* mirrorScale=b[@"mirror_translation_scale"];
            if (mirrorScale.count!=4) @throw [NSException exceptionWithName:@"Missing translation mirror map" reason:@"Re-run tools/rider_assets.py" userInfo:nil];
            translationMaps.push_back(simd::float3{[mirrorScale[0] floatValue],[mirrorScale[1] floatValue],[mirrorScale[2] floatValue]});
            bones.push_back({[b[@"parent"] intValue],{[t[0] floatValue],[t[1] floatValue],[t[2] floatValue]},
                             {[q[0] floatValue],[q[1] floatValue],[q[2] floatValue],[q[3] floatValue]}});
        }
        for (size_t i=0;i<bones.size();++i) {
            int source=-1;
            for (size_t j=0;j<bones.size();++j)
                if (animationParts[i]==animationParts[j] && mirrorBoneIds[i]==animationBoneIds[j]) source=int(j);
            if (source<0) @throw [NSException exceptionWithName:@"Invalid mirror source" reason:@"Bone not in rig" userInfo:nil];
            mirrorSources.push_back(source);
        }
        if (bones.empty() || bones.size()>64) @throw [NSException exceptionWithName:@"Invalid rider" reason:@"Bone count exceeds current palette capacity" userInfo:nil];
        try { skeleton=std::make_unique<ssx::Skeleton>(std::move(bones)); }
        catch (const std::exception& e) { @throw [NSException exceptionWithName:@"Invalid rider skeleton" reason:@(e.what()) userInfo:nil]; }
        NSArray* skin=rider[@"skin"];
        if (skin.count!=vb.length/40) @throw [NSException exceptionWithName:@"Invalid rider" reason:@"Skin/vertex count mismatch" userInfo:nil];
        std::vector<SkinVertex> skinVertices;
        for (NSArray* influences in skin) {
            if (!influences.count || influences.count>4) @throw [NSException exceptionWithName:@"Invalid rider" reason:@"Skin influence count" userInfo:nil];
            SkinVertex vertex;float sum=0;
            for (NSUInteger i=0;i<influences.count;++i) {
                NSArray* pair=influences[i];
                if (pair.count!=2) @throw [NSException exceptionWithName:@"Invalid rider" reason:@"Skin influence layout" userInfo:nil];
                vertex.joints[i]=[pair[0] unsignedIntValue];vertex.weights[i]=[pair[1] floatValue];
                if (vertex.joints[i]>=skeleton->size() || !std::isfinite(vertex.weights[i]) || vertex.weights[i]<0)
                    @throw [NSException exceptionWithName:@"Invalid rider" reason:@"Skin influence value" userInfo:nil];
                sum+=vertex.weights[i];
            }
            if (std::abs(sum-1)>1e-4) @throw [NSException exceptionWithName:@"Invalid rider" reason:@"Unnormalized skin weights" userInfo:nil];
            skinVertices.push_back(vertex);
        }
        skinBuffer=[device newBufferWithBytes:skinVertices.data() length:skinVertices.size()*sizeof(SkinVertex) options:MTLResourceStorageModeShared];
        NSData* samples=[NSData dataWithContentsOfFile:[root stringByAppendingPathComponent:@"animation-samples.json"]];
        if (samples) {
            NSDictionary* decoded=[NSJSONSerialization JSONObjectWithData:samples options:0 error:nil];
            animationClips=decoded[@"clips"];
            for (NSDictionary* clip in animationClips) {
                if (![clip[@"frames"] count] || [clip[@"fps"] floatValue]<=0)
                    @throw [NSException exceptionWithName:@"Invalid animation" reason:@"Empty clip or invalid cadence" userInfo:nil];
                for (NSArray* frame in clip[@"frames"])
                    if (frame.count!=69) @throw [NSException exceptionWithName:@"Invalid animation" reason:@"Body curve count" userInfo:nil];
                NSDictionary* parts=clip[@"parts"];
                if (![parts isKindOfClass:NSDictionary.class])
                    @throw [NSException exceptionWithName:@"Invalid animation" reason:@"Missing part streams" userInfo:nil];
                for (NSString* part in parts) {
                    NSArray* partFrames=parts[part];NSUInteger channels=[partFrames.firstObject count];
                    if (partFrames.count!=[clip[@"frames"] count] || !channels)
                        @throw [NSException exceptionWithName:@"Invalid animation" reason:@"Part frame count mismatch" userInfo:nil];
                    for (NSArray* frame in partFrames) {
                        if (frame.count!=channels) @throw [NSException exceptionWithName:@"Invalid animation" reason:@"Part channel count mismatch" userInfo:nil];
                        for (NSNumber* value in frame) if (!std::isfinite(value.floatValue))
                            @throw [NSException exceptionWithName:@"Invalid animation" reason:@"Nonfinite curve sample" userInfo:nil];
                    }
                }
            }
        }
        // All riders share one memory-mapped copy of the complete source library.
        // Legacy small JSON clip packs remain a fallback for standalone assets.
        static NSDictionary* fullLibrary=nil;static NSData* fullSamples=nil;
        if(!fullLibrary){
            NSString* libraryRoot=[@(SSX_ASSET_ROOT) stringByAppendingPathComponent:@"ANIMATIONS"];
            NSData* metadata=[NSData dataWithContentsOfFile:[libraryRoot stringByAppendingPathComponent:@"library.json"]];
            if(metadata){
                NSDictionary* library=[NSJSONSerialization JSONObjectWithData:metadata options:0 error:nil];
                NSData* bytes=[NSData dataWithContentsOfFile:[libraryRoot stringByAppendingPathComponent:@"samples.f32"] options:NSDataReadingMappedIfSafe error:nil];
                if([library[@"version"] intValue]!=1||![library[@"clips"] isKindOfClass:NSArray.class]||!bytes||bytes.length!=[library[@"byte_length"] unsignedLongLongValue])
                    @throw [NSException exceptionWithName:@"Animation library" reason:@"Invalid shared clip library" userInfo:nil];
                for(NSDictionary* clip in library[@"clips"]){
                    NSUInteger frames=[clip[@"frame_count"] unsignedIntegerValue];
                    if(!frames||frames>65535||[clip[@"fps"] intValue]!=30)@throw [NSException exceptionWithName:@"Animation library" reason:@"Invalid clip cadence" userInfo:nil];
                    for(NSDictionary* stream in [clip[@"streams"] allValues]){
                        NSUInteger offset=[stream[@"offset"] unsignedIntegerValue],channels=[stream[@"channels"] unsignedIntegerValue];
                        if(!channels||channels>512||offset%4||offset>bytes.length||[stream[@"frames"] unsignedIntegerValue]!=frames||frames*channels*4>bytes.length-offset)
                            @throw [NSException exceptionWithName:@"Animation library" reason:@"Clip stream outside sample buffer" userInfo:nil];
                    }
                }
                fullSamples=bytes;fullLibrary=library;
            }
        }
        if(fullLibrary){animationClips=fullLibrary[@"clips"];animationSampleData=fullSamples;}
    }
    NSData* colorData=[NSData dataWithContentsOfFile:[root stringByAppendingPathComponent:@"colors.bin"]];
    if (colorData && colorData.length!=vb.length/40*16)
        @throw [NSException exceptionWithName:@"Invalid vertex colors" reason:@"Color buffer size mismatch" userInfo:nil];
    if (colorData) colors=[device newBufferWithBytes:colorData.bytes length:colorData.length options:MTLResourceStorageModeShared];
    else {
        std::vector<float4> whiteColors(vb.length/40,float4{1,1,1,1});
        colors=[device newBufferWithBytes:whiteColors.data() length:whiteColors.size()*16 options:MTLResourceStorageModeShared];
    }
    NSMutableDictionary<NSString*, id<MTLTexture>>* textures = [NSMutableDictionary new];
    id<MTLCommandBuffer> upload = [queue commandBuffer];
    id<MTLBlitCommandEncoder> blit = [upload blitCommandEncoder];
    for (NSString* key in world[@"textures"]) {
        NSDictionary* t = world[@"textures"][key];
        NSUInteger w = [t[@"width"] unsignedLongValue], h = [t[@"height"] unsignedLongValue];
        NSData* rgba = [NSData dataWithContentsOfFile:[root stringByAppendingPathComponent:t[@"path"]]];
        if (!w || !h || w>4096 || h>4096 || rgba.length != w*h*4)
            @throw [NSException exceptionWithName:@"Invalid texture" reason:key userInfo:nil];
        MTLTextureDescriptor* td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                                     width:w height:h mipmapped:YES];
        td.usage = MTLTextureUsageShaderRead;
        id<MTLTexture> texture = [device newTextureWithDescriptor:td];
        [texture replaceRegion:MTLRegionMake2D(0,0,w,h) mipmapLevel:0 withBytes:rgba.bytes bytesPerRow:w*4];
        [blit generateMipmapsForTexture:texture];
        textures[key] = texture;
    }
    [blit endEncoding];
    [upload commit];
    [upload waitUntilCompleted];
    MTLTextureDescriptor* whiteDescriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm width:1 height:1 mipmapped:NO];
    id<MTLTexture> white = [device newTextureWithDescriptor:whiteDescriptor];
    uint32_t pixel = 0xffffffff;
    [white replaceRegion:MTLRegionMake2D(0,0,1,1) mipmapLevel:0 withBytes:&pixel bytesPerRow:4];
    for (NSDictionary* b in world[@"batches"]) {
        NSUInteger first = [b[@"first_index"] unsignedLongValue], count = [b[@"index_count"] unsignedLongValue];
        if (first > ib.length/4 || count > ib.length/4-first || count%3)
            @throw [NSException exceptionWithName:@"Invalid mesh batch" reason:b.description userInfo:nil];
        id<MTLTexture> base = textures[[NSString stringWithFormat:@"9-%@",b[@"texture"]]];
        id<MTLTexture> light = textures[[NSString stringWithFormat:@"10-%@",b[@"lightmap"]]];
        batches.push_back({first,count,base ?: white,light ?: white,light != nil,[b[@"instance"] boolValue]});
    }
    NSArray* low = world[@"bounds"][0], *high = world[@"bounds"][1];
    float3 minimum = {[low[0] floatValue],[low[1] floatValue],[low[2] floatValue]};
    float3 maximum = {[high[0] floatValue],[high[1] floatValue],[high[2] floatValue]};
    center = (minimum+maximum)*.5f;
    radius = simd::length(maximum-minimum)*.6f;
    minimumBounds=minimum; maximumBounds=maximum;
    lightingVerified=[world[@"lighting_verified"] boolValue];
    return self;
}
- (std::vector<simd::float4x4>)paletteAt:(double)seconds clip:(NSUInteger)index playing:(bool)playing mirror:(bool)mirror diagnostic:(bool)diagnostic {
    if (!skeleton) return {};
    std::vector<simd::float4> pose=playing ? skeleton->restRotations() : std::vector<simd::float4>(skeleton->size(),simd::float4{0,0,0,1});
    auto translations=skeleton->restTranslations();
    if (playing && animationClips.count) {
        NSDictionary* clip=animationClips[index%animationClips.count];
        NSArray* clipFrames=clip[@"frames"];
        NSUInteger frameCount=animationSampleData?[clip[@"frame_count"] unsignedIntegerValue]:clipFrames.count;
        bool loop=clip[@"loop"]?[clip[@"loop"] boolValue]:true;
        double rawSample=std::max(0.0,seconds)*[clip[@"fps"] doubleValue];
        double sample=loop?std::fmod(rawSample,double(frameCount)):std::min(rawSample,double(frameCount-1));
        NSUInteger first=NSUInteger(sample),second=loop?(first+1)%frameCount:std::min(first+1,frameCount-1);
        float blend=float(sample-first);
        for (size_t i=0;i<pose.size();++i) {
            NSArray* partFrames=clip[@"parts"][[NSString stringWithFormat:@"%d",animationParts[i]]];
            NSDictionary* stream=clip[@"streams"][[NSString stringWithFormat:@"%d",animationParts[i]]];
            if (!partFrames&&!stream) continue;
            NSArray* a=partFrames?partFrames[first]:nil,*b=partFrames?partFrames[second]:nil;
            NSUInteger channels=stream?[stream[@"channels"] unsignedIntegerValue]:a.count;
            const float* packed=stream?reinterpret_cast<const float*>(static_cast<const uint8_t*>(animationSampleData.bytes)+[stream[@"offset"] unsignedIntegerValue]):nullptr;
            auto value=[&](NSUInteger channel){return packed?packed[first*channels+channel]*(1-blend)+packed[second*channels+channel]*blend:[a[channel] floatValue]*(1-blend)+[b[channel] floatValue]*blend;};
            int source=mirror?mirrorSources[i]:int(i);
            int t=translationChannels[source],r=rotationChannels[source];
            if ((t>=0 && NSUInteger(t+3)>channels)||(r>=0 && NSUInteger(r+3)>channels))
                @throw [NSException exceptionWithName:@"Invalid animation DOF layout" reason:@"Bone channels outside sampled part" userInfo:nil];
            if (t>=0) translations[i]=ssx::afbTranslation({value(t),value(t+1),value(t+2)},mirror?translationMaps[i]:simd::float3{1,1,1});
            if (r>=0) pose[i]=ssx::afbQuaternion(simd::float3{value(r),value(r+1),value(r+2)},mirror ? quaternionMaps[i] : std::array<uint8_t,4>{0,1,2,3});
        }
    } else if (diagnostic && diagnosticBone>=0)
        pose[diagnosticBone]=simd_quaternion(float(std::sin(seconds)*.6),simd::float3{0,0,1}).vector;
    return skeleton->palette(pose,{0,0,0},playing,{},translations);
}
- (std::vector<simd::float4x4>)paletteForOriginalPose:(const std::vector<ssx::AnimationTransform>&)pose scale:(ssx::AnimationVector)scale {
    std::vector<simd::float4x4> transforms;
    for(const auto&bone:pose){const auto&q=bone.rotation;simd::float4x4 m=simd_matrix4x4(simd_quaternion(simd::float4{q[0],q[2],-q[1],q[3]}));
        m.columns[0]*=scale[0];m.columns[1]*=scale[2];m.columns[2]*=scale[1];
        m.columns[3]={bone.position[0]/100.f,bone.position[2]/100.f,-bone.position[1]/100.f,1};transforms.push_back(m);}
    return skeleton->paletteFromGlobals(std::move(transforms));
}
@end
