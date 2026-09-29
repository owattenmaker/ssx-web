#include "gameplay_effects.h"
#include <stdexcept>
namespace ssx {namespace {
using simd::float3;using simd::float4;using simd::float4x4;
struct FxFrame {float4x4 vp;float4 right,up,options;};
struct Sprite {float4 positionSize,colour;};
float num(NSDictionary* d,NSString* key){id value=d[key];if(![value isKindOfClass:NSNumber.class]||!std::isfinite([value doubleValue]))throw std::runtime_error("Invalid source effect parameter");return [value floatValue];}
template<size_t N>std::array<float,N> vec(id a){if(![a isKindOfClass:NSArray.class]||[a count]!=N)throw std::runtime_error("Invalid source effect vector");std::array<float,N> out;for(unsigned i=0;i<N;++i)out[i]=[a[i] floatValue];return out;}
NSDictionary* read(NSString* path){NSData* bytes=[NSData dataWithContentsOfFile:path];if(!bytes)throw std::runtime_error("Missing original effect asset");auto result=[NSJSONSerialization JSONObjectWithData:bytes options:0 error:nil];if(![result isKindOfClass:NSDictionary.class])throw std::runtime_error("Invalid original effect asset");return result;}
OriginalSnowParticleProfile particleProfile(NSDictionary* d){
    OriginalSnowParticleProfile p;p.particlesPerBirth=unsigned(num(d,@"NumParticles"));p.life=num(d,@"Life");p.lifeRange=num(d,@"LifeR");p.damping=num(d,@"Damp");p.size=num(d,@"Size");p.sizeRange=num(d,@"SizeR");p.finalSize=num(d,@"SizeFinal");
    auto xyz=[&](NSString* prefix){return SnowVector{num(d,[prefix stringByAppendingString:@"X"]),num(d,[prefix stringByAppendingString:@"Y"]),num(d,[prefix stringByAppendingString:@"Z"])};};
    p.offset=xyz(@"Off");p.positionRange0=xyz(@"R0");p.positionRange1=xyz(@"R1");p.velocity=xyz(@"Vel");p.force=xyz(@"Force");p.velocityRanges={xyz(@"R0V"),xyz(@"R1V"),xyz(@"R2V")};
    auto rgba=[&](NSString* prefix){return SnowColour{num(d,[prefix stringByAppendingString:@"R"]),num(d,[prefix stringByAppendingString:@"G"]),num(d,[prefix stringByAppendingString:@"B"]),num(d,[prefix stringByAppendingString:@"A"])};};
    p.startColour=rgba(@"StartCol");p.endColour=rgba(@"EndCol");p.colourRange0=rgba(@"R0");p.colourRange1=rgba(@"R1");return p;
}
static NSString* const shader=@R"MSL(
#include <metal_stdlib>
using namespace metal;
struct Frame {float4x4 vp;float4 right;float4 up;float4 options;};
struct Sprite {float4 positionSize;float4 colour;};
struct Track {float4 uvq;uint4 rgba;float4 position;};
struct Out {float4 position [[position]];float2 uv;float4 colour;};
vertex Out fxSpriteVertex(uint vertexId [[vertex_id]],uint instance [[instance_id]],const device Sprite* sprites [[buffer(0)]],constant Frame& f [[buffer(1)]]){
    constexpr float2 corners[6]={float2(-1,-1),float2(1,-1),float2(-1,1),float2(-1,1),float2(1,-1),float2(1,1)};
    auto s=sprites[instance];auto c=corners[vertexId];float3 p=s.positionSize.xyz+(f.right.xyz*c.x+f.up.xyz*c.y)*s.positionSize.w;
    return {f.vp*float4(p,1),float2(c.x*.5+.5,.5-c.y*.5),s.colour};
}
vertex Out fxTrackVertex(uint vertexId [[vertex_id]],const device Track* tracks [[buffer(0)]],constant Frame& f [[buffer(1)]]){
    auto t=tracks[vertexId];float3 p=float3(t.position.x,t.position.z,-t.position.y)*.01;
    float4 clip=f.vp*float4(p,1);if(f.options.y>.5)clip.z=clip.w;
    return {clip,t.uvq.xy,float4(t.rgba)/128.f};
}
fragment float4 fxMaskFragment(Out in [[stage_in]]){return float4(0);}
fragment float4 fxFragment(Out in [[stage_in]],texture2d<float> tex [[texture(0)]],constant Frame& f [[buffer(1)]]){
    constexpr sampler source(filter::linear,address::repeat);
    float4 t=tex.sample(source,in.uv);float alpha=clamp(t.a*f.options.x*in.colour.a,0.f,1.f);
    return float4(t.rgb*in.colour.rgb,alpha);
}
)MSL";
}
GameplayEffects::GameplayEffects(id<MTLDevice> gpu,MTLPixelFormat colour,MTLPixelFormat depthFormat,NSUInteger samples,NSString* root,NSString* course,NSDictionary* initial,std::shared_ptr<OriginalRiderAnimation> animation):player(std::move(animation)),device(gpu){
    auto trailConfig=initial[@"original_board_trail"];auto snowConfig=initial[@"original_snow"];
    if(!trailConfig||!snowConfig)throw std::runtime_error("Original snow/track initialization is missing");
    NSString* snowFolder=[root stringByAppendingPathComponent:@"SNOW_FX"];NSString* trackFolder=[root stringByAppendingPathComponent:@"BOARD_TRAIL"];
    auto snowAssets=read([snowFolder stringByAppendingPathComponent:@"snow-fx.json"]);auto trackAssets=read([trackFolder stringByAppendingPathComponent:@"board_trail.json"]);
    auto tp=trackAssets[@"profile"];trailProfile.innerWidth=num(tp,@"inner_width_scale");trailProfile.outerWidth=num(tp,@"outer_width_scale");trailProfile.height=num(tp,@"height_scale");trailProfile.depthHeight=num(tp,@"depth_to_height_scale");trailProfile.innerJitter=num(tp,@"inner_position_jitter");trailProfile.fadeSegments=int(num(tp,@"fade_segments"));
    trailProfile.depths={num(tp,@"packed_depth_scale"),num(tp,@"loose_depth_scale"),num(tp,@"powder_depth_scale"),num(tp,@"deep_powder_depth_scale")};trailProfile.planeOffset=num(tp,@"plane_offset");trailProfile.backwardsOffset=num(tp,@"backwards_offset");trailProfile.middleU=num(tp,@"middle_texture_u");trailProfile.innerU=num(tp,@"inner_texture_u");trailProfile.outerU=num(tp,@"outer_texture_u");trail=board_trail::State(trailProfile);
    trailInput.environmentARGB=vec<4>(trailConfig[@"environment_argb"]);auto ti=trailConfig[@"initial_context"];
    trailInput.flagAC4=[ti[@"flagAC4"]boolValue];trailInput.flagAD0=[ti[@"flagAD0"]boolValue];trailInput.flagAFC=[ti[@"flagAFC"]boolValue];trailInput.flagB00=[ti[@"flagB00"]boolValue];trailInput.detached=[ti[@"detached"]boolValue];
    visualRandom.word=[snowConfig[@"shared_visual_lcg"]unsignedIntValue];NSArray* rng=snowConfig[@"particle_random_state"];if(rng.count!=6)throw std::runtime_error("Missing particle random seed");for(unsigned i=0;i<6;++i)particleRandom.words[i]=[rng[i]unsignedIntValue];
    boardIndex=[snowConfig[@"board_bone_index"]unsignedIntValue];
    // 2DF4D0 BodySnow bone table resolved for the shipped 22-bone biped rigs (SNOW_CRASH_RECOVERY.md).
    bodyBones={17,18,20,21,16,19,0,1,2,3,4,6,15,7,8,17,18,20,21,16,19,0,1,2,9,10,11,12,13,14};snowInput.environmentARGB=vec<4>(snowConfig[@"environment_argb"]);snowInput.suppressed=[snowConfig[@"suppressed"]boolValue];
    environment=loadOriginalEnvironmentAsset([root stringByAppendingPathComponent:course]);
    if(environment){
        NSDictionary* seed=initial[@"original_environment"];
        if(!seed)throw std::runtime_error("Original environment initialization is missing");
        environmentState.ambient=vec<4>(seed[@"ambient"]);environmentState.ratio=vec<4>(seed[@"ratio"]);
        environment->globals.forceNext=[seed[@"force_next"]boolValue];
        environment->globals.multiplier=vec<4>(seed[@"multiplier"]);environment->globals.airAmbient=vec<4>(seed[@"air_ambient"]);environment->globals.airRatio=vec<4>(seed[@"air_ratio"]);
    }
    auto ri=snowConfig[@"initial_rider"];snowInput.secondaryBrake274=num(ri,@"secondary_brake274");snowInput.visibilityMode=int(num(ri,@"visibility_mode"));
    snowInput.rider.trackingInhibited=trailInput.flagAC4;snowInput.rider.trackingAD0=trailInput.flagAD0;snowInput.rider.trackingAFC=trailInput.flagAFC;snowInput.rider.trackingB00=trailInput.flagB00;
    auto state=snowConfig[@"state"];snowState.previousSpeedCmps=num(state,@"previous_speed_cmps");snowState.visibilityMode=int(num(state,@"visibility_mode"));snowState.impactSurface=int(num(state,@"impact_surface"));
    auto impact=state[@"impact"];snowState.impact={num(impact,@"strength"),num(impact,@"buildup"),num(impact,@"alpha"),vec<3>(impact[@"position_cm"]),vec<3>(impact[@"normal"]),[impact[@"kind"]boolValue],[impact[@"wide_scatter"]boolValue]};
    NSArray* enabled=state[@"emitter_enabled"];if(enabled.count!=10)throw std::runtime_error("Invalid original emitter enable state");for(unsigned i=0;i<10;++i)snowState.emitterEnabled[i]=[enabled[i]boolValue];
    NSArray* materialList=snowConfig[@"surfaces"];if(materialList.count!=19)throw std::runtime_error("Missing source surface FX profiles");
    for(NSDictionary* d in materialList){int id=int(num(d,@"id"));if(id<0||id>=19)throw std::runtime_error("Invalid snow surface");auto& s=surfaces[id];s.id=id;s.cloudActive=[d[@"cloud_active"]boolValue];s.trailActive=[d[@"trail_active"]boolValue];s.impactActive=[d[@"impact_active"]boolValue];s.largeImpactActive=[d[@"large_impact_active"]boolValue];s.cloudMaxHeightCm=num(d,@"cloud_max_height_cm");s.impactMultiplier=num(d,@"impact_multiplier");s.rockChance=num(d,@"rock_chance");s.smallChunkChance=num(d,@"small_chunk_chance");s.largeChunkChance=num(d,@"large_chunk_chance");s.chunksUseWakeVelocity=[d[@"chunks_use_wake_velocity"]boolValue];s.minWakeVelocityScale=num(d,@"min_wake_velocity_scale");s.maxWakeVelocityScale=num(d,@"max_wake_velocity_scale");}
    textures=[NSMutableDictionary dictionary];std::map<int,float> textureAlpha;
    auto loadTexture=[&](NSDictionary* t,NSString* folder,NSString* file){NSUInteger w=[t[@"width"]unsignedIntegerValue],h=[t[@"height"]unsignedIntegerValue];NSData* bytes=[NSData dataWithContentsOfFile:[folder stringByAppendingPathComponent:file]];if(!w||!h||bytes.length!=w*h*4)throw std::runtime_error("Missing original snow texture");auto descriptor=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm width:w height:h mipmapped:NO];auto texture=[device newTextureWithDescriptor:descriptor];[texture replaceRegion:MTLRegionMake2D(0,0,w,h) mipmapLevel:0 withBytes:bytes.bytes bytesPerRow:w*4];return texture;};
    for(NSDictionary* t in snowAssets[@"textures"]){NSNumber* id=t[@"id"];textures[id]=loadTexture(t,snowFolder,t[@"gs_alpha_file"]?:t[@"file"]);textureAlpha[id.intValue]=t[@"gs_alpha_file"]?[t[@"gs_alpha_scale"]floatValue]:1.f;}
    trailTexture=loadTexture(trackAssets[@"texture"],trackFolder,trackAssets[@"texture"][@"file"]);
    for(NSDictionary* entry in snowAssets[@"profiles"]){unsigned id=[entry[@"emitter_index"]unsignedIntValue];if(id>=10)throw std::runtime_error("Invalid original emitter index");NSDictionary* p=entry[@"parameters"];emitters[id]=std::make_unique<OriginalSnowParticles>(particleProfile(p));profiles[id].velocityScale=num(p,@"VelScale");profiles[id].normalScale=num(p,@"NormScale");profiles[id].normalSpeedScale=num(p,@"NormSpeedScale");profiles[id].sideScale=num(p,@"SideScale");textureIds[id]=int(num(p,@"TextureId"));alphaScales[id]=textureAlpha.contains(textureIds[id])?textureAlpha[textureIds[id]]:1;}
    NSError* error=nil;auto library=[device newLibraryWithSource:shader options:nil error:&error];if(!library)throw std::runtime_error(error.description.UTF8String);
    auto pd=[MTLRenderPipelineDescriptor new];pd.colorAttachments[0].pixelFormat=colour;pd.depthAttachmentPixelFormat=depthFormat;pd.stencilAttachmentPixelFormat=depthFormat;pd.rasterSampleCount=samples;pd.fragmentFunction=[library newFunctionWithName:@"fxFragment"];
    auto blend=pd.colorAttachments[0];blend.blendingEnabled=YES;blend.sourceRGBBlendFactor=MTLBlendFactorSourceAlpha;blend.destinationRGBBlendFactor=MTLBlendFactorOneMinusSourceAlpha;blend.sourceAlphaBlendFactor=MTLBlendFactorOne;blend.destinationAlphaBlendFactor=MTLBlendFactorOneMinusSourceAlpha;
    pd.vertexFunction=[library newFunctionWithName:@"fxSpriteVertex"];spritePipeline=[device newRenderPipelineStateWithDescriptor:pd error:&error];pd.vertexFunction=[library newFunctionWithName:@"fxTrackVertex"];trailPipeline=[device newRenderPipelineStateWithDescriptor:pd error:&error];if(!spritePipeline||!trailPipeline)throw std::runtime_error(error.description.UTF8String);
    pd.colorAttachments[0].writeMask=MTLColorWriteMaskNone;pd.colorAttachments[0].blendingEnabled=NO;pd.fragmentFunction=[library newFunctionWithName:@"fxMaskFragment"];maskPipeline=[device newRenderPipelineStateWithDescriptor:pd error:&error];if(!maskPipeline)throw std::runtime_error(error.description.UTF8String);
    auto ds=[MTLDepthStencilDescriptor new];ds.depthCompareFunction=MTLCompareFunctionLessEqual;ds.depthWriteEnabled=NO;depth=[device newDepthStencilStateWithDescriptor:ds];
    auto makeDepthState=[&](MTLCompareFunction test,bool write,MTLCompareFunction stencil,MTLStencilOperation operation){auto d=[MTLDepthStencilDescriptor new];d.depthCompareFunction=test;d.depthWriteEnabled=write;auto s=[MTLStencilDescriptor new];s.stencilCompareFunction=stencil;s.depthStencilPassOperation=operation;s.readMask=255;s.writeMask=255;d.frontFaceStencil=d.backFaceStencil=s;return [device newDepthStencilStateWithDescriptor:d];};
    trailDepthStates=@[makeDepthState(MTLCompareFunctionAlways,false,MTLCompareFunctionAlways,MTLStencilOperationZero),makeDepthState(MTLCompareFunctionLessEqual,false,MTLCompareFunctionAlways,MTLStencilOperationReplace),makeDepthState(MTLCompareFunctionAlways,true,MTLCompareFunctionEqual,MTLStencilOperationZero),makeDepthState(MTLCompareFunctionLessEqual,true,MTLCompareFunctionAlways,MTLStencilOperationReplace),makeDepthState(MTLCompareFunctionLessEqual,true,MTLCompareFunctionEqual,MTLStencilOperationKeep)];
}
void GameplayEffects::step(const PrototypeRider& rider){
    terrain_original::Rounding sourceRounding;
    const auto& pose=player->currentWorldPose();if(!pose||boardIndex>=pose->size())return;
    auto physical=OriginalAirState::fromNative(rider.position,rider.velocity);const auto& g=rider.originalGroundState();auto animation=player->mainAnimationState();
    if(environment){
        OriginalEnvironmentFrame frame;frame.motionMode=rider.currentMotionMode();
        const auto& contact=rider.lastGroundContact;
        frame.groundPatch=contact.hit?environment->patch(contact.resource):nullptr;frame.u=float(contact.u);frame.v=float(contact.v);
        const auto& predicted=rider.originalAirTrajectoryState();
        frame.predictionStatus=predicted.status;frame.predictedPatch=environment->patch(predicted.patchId);
        frame.predictedU=predicted.patchU;frame.predictedV=predicted.patchV;frame.predictedTime=predicted.predictedTime;frame.elapsed=predicted.elapsed;
        try {environment->update(environmentState,frame);}
        catch(const OriginalEnvironmentUnavailable& error){
            if(!reportedLightingGap){NSLog(@"Snow lighting retained its last valid sample: %s",error.what());reportedLightingGap=true;}
        }
        trailInput.environmentARGB=snowInput.environmentARGB=environmentState.ambient;
    }
    auto board=originalSnowUnitBoardFrame(pose->at(boardIndex));auto scale=player->modelScale();
    trailInput.motion=rider.currentMotionMode();trailInput.surface=rider.currentGroundSurfaceId();trailInput.semantic=animation?animation->semantic:g.animationIndex;trailInput.marker0=animation&&(animation->flags&1);trailInput.marker1=animation&&(animation->flags&2);trailInput.reverseStance=g.reverseStance;trailInput.manual=rider.originalLandingRuntime().manualState330!=0;
    trailInput.board={board.right,board.up,board.originCm};for(unsigned k=0;k<3;++k){trailInput.board.axis0[k]*=scale[0];trailInput.board.axis2[k]*=scale[2];}
    bool landed=rider.lastLandingTick&&rider.lastLandingTick!=lastLandingSeen;
    trailInput.contact=landed?rider.lastLandingContact.position:OriginalAirState::fromNative(rider.lastGroundContact.position,{}).position;
    trailInput.normal=g.normal;trailInput.velocity=physical.velocity;trailInput.contactDistance=g.distance;
    board_trail::update(trail,trailInput,visualRandom.word,trailProfile); //11199C before1119CC spray.
    auto& r=snowInput.rider;r.velocityCmps=physical.velocity;r.turn=g.turn.current;r.brake=g.brake.current;r.manualState=rider.originalLandingRuntime().manualState330;r.animationSemantic=trailInput.semantic;r.motionMode=trailInput.motion;r.controlState=rider.currentControlState();r.reverse=g.reverseStance;
    snowInput.board=board;snowInput.groundNormal=g.normal;snowInput.lateral=g.lateral;
    if(landed){originalSnowImpactTrigger(snowState,rider.lastLandingContact.position,rider.lastLandingContact.normal,rider.lastLandingNormalSpeedCmps,rider.lastLandingContact.surface,false,r);lastLandingSeen=rider.lastLandingTick;}
    // Crash landings and slide impacts (motion2) reach the same impact emitter,
    // which scatters them with the original motion-2 jitter and buildup.
    if(rider.lastCrashTick&&rider.lastCrashTick!=lastCrashSeen){
        //111AA0 from 10EB30 / 137860 / 137D18: impact point, normal, |strength|, surface, kind=false.
        originalSnowCrashImpactTrigger(snowState,rider.lastCrashImpactPointCm,rider.lastCrashImpactNormal,rider.lastCrashImpactCmps,rider.lastCrashImpactSurface,r);lastCrashSeen=rider.lastCrashTick;
    }
    if(trailInput.surface<0||trailInput.surface>=19)return;
    auto context=originalSnowContextStep(snowState,snowInput,surfaces[trailInput.surface]);
    auto submit=[&](OriginalSnowEmission birth,unsigned index){birth.emitter=index;birth.active=birth.active&&snowState.emitterEnabled[index];emitters[index]->emit(birth,particleRandom);};
    auto chunks=originalSnowChunkEmission({profiles[1],profiles[2]},context.chunks,visualRandom);for(auto birth:chunks)submit(birth,birth.emitter);
    OriginalSnowEmission inactive;inactive.positionCm=board.originCm;submit(inactive,3);
    submit(originalSnowTrailEmission(profiles[0],context.trail,visualRandom),0);submit(inactive,4);
    for(auto birth:originalSnowImpactEmission(snowState.impact,context.impact,visualRandom).births)submit(birth,birth.emitter);
    submit(originalSnowCloudEmission(profiles[7],context.cloud,visualRandom),7);
    //2E2260 BodySnow: cycles a body bone while the impact buildup is positive; no LCG draws.
    OriginalSnowBodyContext body;for(unsigned i=0;i<30;++i)body.boneOriginsCm[i]=pose->at(bodyBones[i]).position;
    body.primaryBoneOriginCm=pose->at(0).position;body.velocityCmps=physical.velocity;body.speedCmps=context.rider.speedCmps;body.colour=context.trail.colour;body.motionMode=r.motionMode;
    submit(originalSnowBodyEmission(profiles[9],bodyState,snowState.impact,body),9);submit(inactive,8);
}
size_t GameplayEffects::visibleParticles()const{size_t total=0;for(const auto& e:emitters)if(e)total+=e->particles().size();return total;}
void GameplayEffects::draw(id<MTLRenderCommandEncoder> enc,float4x4 vp,float3 forward){
    auto right=simd_normalize(simd_cross(forward,float3{0,1,0}));auto up=simd_normalize(simd_cross(right,forward));FxFrame frame{vp,{right.x,right.y,right.z,0},{up.x,up.y,up.z,0},{255.f/128,0,0,0}};
    [enc setDepthStencilState:depth];[enc setCullMode:MTLCullModeNone];[enc setTriangleFillMode:MTLTriangleFillModeFill];
    auto window=board_trail::drawWindow(trail,trailProfile);std::vector<board_trail::Vertex> ribbon;
    auto vertex=[&](int band,int slice){auto v=trail.bands[band][(window.start+slice)%54];v.rgba[3]=board_trail::fadedAlpha(v.rgba[3],float(slice)*window.fadeStep);return v;};
    for(int band=0;band<5;++band)for(int i=0;i+1<window.count;++i){int a=board_trail::bandOrder[band],b=board_trail::bandOrder[band+1];auto v0=vertex(a,i),v1=vertex(b,i),v2=vertex(a,i+1),v3=vertex(b,i+1);ribbon.insert(ribbon.end(),{v0,v1,v2,v2,v1,v3});}
    if(!ribbon.empty()){
        std::vector<board_trail::Vertex> top;for(int i=0;i+1<window.count;++i){auto a=vertex(0,i),b=vertex(1,i),c=vertex(0,i+1),d=vertex(1,i+1);top.insert(top.end(),{a,b,c,c,b,d});}
        auto buffer=[device newBufferWithBytes:ribbon.data() length:ribbon.size()*sizeof(board_trail::Vertex) options:MTLResourceStorageModeShared];auto roof=[device newBufferWithBytes:top.data() length:top.size()*sizeof(board_trail::Vertex) options:MTLResourceStorageModeShared];
        //386E78: clear mask, mark visible roof, clear its depth, draw trough,
        //then fix holes. GS destination-alpha masks become Metal stencil.
        for(unsigned pass=0;pass<5;++pass){frame.options.y=pass==2?1:0;[enc setRenderPipelineState:pass<3?maskPipeline:trailPipeline];[enc setDepthStencilState:trailDepthStates[pass]];[enc setStencilReferenceValue:pass==1||pass==2||pass==3?1:0];[enc setVertexBuffer:pass==3?buffer:roof offset:0 atIndex:0];[enc setVertexBytes:&frame length:sizeof(frame) atIndex:1];[enc setFragmentBytes:&frame length:sizeof(frame) atIndex:1];[enc setFragmentTexture:trailTexture atIndex:0];[enc drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:pass==3?ribbon.size():top.size()];}
        frame.options.y=0;
    }
    [enc setDepthStencilState:depth];
    for(unsigned i:{0,1,2,5,6,7,9}){
        std::vector<Sprite> sprites;for(const auto& p:emitters[i]->particles()){if(!p.colourGs[3])continue;sprites.push_back({{p.positionCm[0]*.01f,p.positionCm[2]*.01f,-p.positionCm[1]*.01f,p.halfExtentCm*.01f},{p.colourGs[0]/128.f,p.colourGs[1]/128.f,p.colourGs[2]/128.f,p.colourGs[3]/128.f}});}
        if(sprites.empty())continue;auto buffer=[device newBufferWithBytes:sprites.data() length:sprites.size()*sizeof(Sprite) options:MTLResourceStorageModeShared];frame.options.x=alphaScales[i];
        [enc setRenderPipelineState:spritePipeline];[enc setVertexBuffer:buffer offset:0 atIndex:0];[enc setVertexBytes:&frame length:sizeof(frame) atIndex:1];[enc setFragmentBytes:&frame length:sizeof(frame) atIndex:1];[enc setFragmentTexture:textures[@(textureIds[i])] atIndex:0];[enc drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:6 instanceCount:sprites.size()];
    }
}
}
