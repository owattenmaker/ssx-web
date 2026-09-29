#include "rider_pose_stages.h"
#include "preview_rider_physics.h"
#include "gameplay_animation.h"
#include "gameplay_effects.h"
#include "original_camera.hpp"
// Headless Metal integration check: loads actual world + rider, runs native motion,
// and optionally writes a deterministic render. It never creates or focuses a window.
#import <MetalKit/MetalKit.h>
#import <ImageIO/ImageIO.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#include <cstdio>
using simd::float3;
using simd::float4;
using simd::float4x4;
#include "renderer.h"
#include "replay_io.h"
#include "rider_animation_player.h"

int main(int argc,char** argv) {
    @autoreleasepool {
        try { @try {
            std::optional<NativeReplaySpec> replay;
            NSString* telemetry=nil;NSString* imagePath=nil;NSString* animationStatePath=nil;
            NSString* location=argc>1?@(argv[1]):@"ARA1";
            if (argc>1 && std::string(argv[1])=="--replay") {
                if (argc<5) throw std::runtime_error("Usage: ssx3_scene_audit --replay scenario.json --telemetry output-prefix [--image final.png]");
                replay=readNativeReplay(@(argv[2]));location=replay->location;
                for (int i=3;i<argc;i+=2) {
                    if (i+1>=argc) throw std::runtime_error("Missing replay option value");
                    std::string option=argv[i];
                    if (option=="--telemetry") telemetry=@(argv[i+1]);
                    else if (option=="--image") imagePath=@(argv[i+1]);
                    else if(option=="--animation-state")animationStatePath=@(argv[i+1]);
                    else throw std::runtime_error("Unknown replay option: "+option);
                }
                if (!telemetry) throw std::runtime_error("Replay requires --telemetry output-prefix");
                NSString* inputPath=[NSURL fileURLWithPath:@(argv[2])].URLByResolvingSymlinksInPath.path;
                for (NSString* output in @[[telemetry stringByAppendingString:@".json"],[telemetry stringByAppendingString:@".csv"],imagePath?:@"",animationStatePath?:@""]) {
                    if ([[NSURL fileURLWithPath:output].URLByResolvingSymlinksInPath.path isEqualToString:inputPath])
                        throw std::runtime_error("Replay output must not overwrite its scenario");
                }
            } else if (argc>2) imagePath=@(argv[2]);
            id<MTLDevice> device=MTLCreateSystemDefaultDevice();
            if (!device) @throw [NSException exceptionWithName:@"Metal unavailable" reason:@"No headless device" userInfo:nil];
            id<MTLCommandQueue> queue=[device newCommandQueue];
            SSXMeshAsset* world=[[SSXMeshAsset alloc] initWithDevice:device queue:queue location:location];
            SSXSkyAsset* sky=world->skeleton?nil:[SSXSkyAsset skyForLocation:location device:device];
            NSString* riderPackage=NSProcessInfo.processInfo.environment[@"SSX_RIDER_PACKAGE"] ?: @"RIDER_ZOE";
            if (![@[@"RIDER_MAC",@"RIDER_SAM",@"RIDER_SAM_PACKERS",@"RIDER_SAM_FISHING",@"RIDER_SAM_FISHING_TUBE",@"RIDER_ZOE",@"RIDER_PSYMON",@"RIDER_ALLEGRA",@"RIDER_MOBY",@"RIDER_GRIFF",@"RIDER_LUTHER"] containsObject:riderPackage])
                throw std::runtime_error("Unsupported rider package");
            SSXMeshAsset* mesh=world->skeleton?world:[[SSXMeshAsset alloc] initWithDevice:device queue:queue location:riderPackage];
            ssx::PrototypeRider rider;
            std::shared_ptr<ssx::OriginalRiderAnimation> originalAnimator,collisionAnimator;ssx::BodyPoseProvider bodyPoseProvider;ssx::RiderPoseStages poseStages;
            std::shared_ptr<ssx::GameplayAnimation> gameplay;
            std::shared_ptr<ssx::GameplayEffects> effects;
            ssx::OriginalCameraState cameraState;ssx::OriginalCameraOutput cameraOutput;bool cameraValid=false;
            auto cameraInput=[&](const ssx::PrototypeRider& r){
                ssx::OriginalCameraInput in;auto physical=ssx::OriginalAirState::fromNative(r.position,r.velocity);
                auto head=collisionAnimator?collisionAnimator->posedBone(0):std::nullopt;auto hp=head?head->position:physical.position;
                in.headPosition={hp[0],hp[1],hp[2],1};auto frame=r.currentPhysicalFrame();in.riderForward={frame.forward[0],frame.forward[1],frame.forward[2],0};
                in.velocity={physical.velocity[0],physical.velocity[1],physical.velocity[2],0};const auto& g=r.originalGroundState();in.previousContactNormal={g.previousNormal[0],g.previousNormal[1],g.previousNormal[2],0};
                in.motionMode=r.currentMotionMode();in.boostLevel=g.boost;in.jumpCharge=float(r.jumpCharge);in.surfaceId=r.currentGroundSurfaceId();
                const auto& t=r.originalAirTrajectoryState();in.trajectoryStatusActive=t.status==1||t.status==3;in.predictedAirTime=t.predictedTime;
                in.trajectoryHeading={t.heading[0],t.heading[1],t.heading[2],0};in.trajectoryNormal={t.normal[0],t.normal[1],t.normal[2],0};in.tick=r.originalLandingRuntime().tick;
                ssx::CollisionWorld* cw=world->collision.get();
                in.terrainProbe=[cw](const ssx::OriginalCameraQuad& s,const ssx::OriginalCameraQuad& e)->std::optional<ssx::OriginalCameraProbeHit>{
                    ssx::Vec3 a{s[0]*.01,s[2]*.01,-s[1]*.01},b{e[0]*.01,e[2]*.01,-e[1]*.01};ssx::Vec3 d=b-a;double length=std::sqrt(ssx::dot(d,d));if(length<=0)return std::nullopt;
                    auto hit=cw->raycast(a,d,length);if(!hit.hit)return std::nullopt;return ssx::OriginalCameraProbeHit{float(hit.distance/length),{float(hit.normal.x),float(-hit.normal.z),float(hit.normal.y),0}};};
                return in;};
            if (replay) {
                if (world->skeleton) throw std::runtime_error("Native replay requires a course area");
                auto n=world->spawnNormal;
                double heading=std::atan2(n.x,n.z);
                initializeNativeReplay(rider,replay->source,*world->terrainCollision,world->spawn,n,heading);rider.bindOriginalRails(world->rails);
                NSDictionary* animation=replay->source[@"native"][@"initial"][@"original_animation"];
                collisionAnimator=makePreviewCollisionAnimation(@(SSX_ASSET_ROOT),mesh->originalAnimation,animation);
                if(collisionAnimator){bodyPoseProvider=collisionAnimator->bodyProvider();poseStages=ssx::originalPoseStages(collisionAnimator);if(mesh->originalAnimation&&mesh->originalAnimation->character==[animation[@"character"] UTF8String])originalAnimator=collisionAnimator;}
                if(collisionAnimator&&replay->source[@"native"][@"initial"][@"original_grab_control"]){
                    gameplay=std::make_shared<ssx::GameplayAnimation>(collisionAnimator,replay->source[@"native"][@"initial"]);gameplay->bind(rider);
                    if(replay->source[@"native"][@"initial"][@"original_snow"]){effects=std::make_shared<ssx::GameplayEffects>(device,MTLPixelFormatBGRA8Unorm,MTLPixelFormatDepth32Float_Stencil8,1,@(SSX_ASSET_ROOT),location,replay->source[@"native"][@"initial"],collisionAnimator);}
                    cameraState.begin(cameraInput(rider));cameraOutput=ssx::originalChaseCameraStep(cameraState,cameraInput(rider));cameraValid=true;
                    gameplay->afterFrame=[&](const ssx::PrototypeRider& r){if(effects)effects->step(r);if(cameraValid)cameraOutput=ssx::originalChaseCameraStep(cameraState,cameraInput(r));};
                }
                auto records=ssx::runReplay(replay->input,rider,*world->terrainCollision,*world->collision,world->bodyCollision.get(),bodyPoseProvider?&bodyPoseProvider:nullptr,collisionAnimator?&poseStages:nullptr);
                writeNativeReplay(telemetry,*replay,records);
                if(animationStatePath){NSMutableArray*sequences=[NSMutableArray array];if(originalAnimator)for(const auto&s:originalAnimator->sequenceState()){NSMutableArray*slots=[NSMutableArray array];for(const auto&slot:s.slots)[slots addObject:@{@"clip":@(slot.clip),@"time":@(slot.time),@"duration":@(slot.duration),@"rate":@(slot.rate),@"weight":@(slot.weight),@"loop":@(slot.loop)}];[sequences addObject:@{@"semantic":@(s.semantic),@"channel":@(s.channel),@"rate":@(s.rate),@"weight":@(s.weight),@"fade_remaining":@(s.fadeRemaining),@"flags":@(s.flags),@"slots":slots}];}NSMutableArray*bones=[NSMutableArray array];if(originalAnimator)if(auto pose=originalAnimator->poseAt(rider,rider.elapsed))for(const auto&b:*pose)[bones addObject:@{@"position_cm":@[@(b.position[0]),@(b.position[1]),@(b.position[2])],@"rotation":@[@(b.rotation[0]),@(b.rotation[1]),@(b.rotation[2]),@(b.rotation[3])]}];NSDictionary*state=@{@"supported":@(originalAnimator&&originalAnimator->isSupported()),@"sequences":sequences,@"bones":bones};NSData*encoded=[NSJSONSerialization dataWithJSONObject:state options:NSJSONWritingPrettyPrinted error:nil];if(![encoded writeToFile:animationStatePath atomically:YES])throw std::runtime_error("Could not write animation state audit");}

                std::printf("Native replay: %d frames, %zu telemetry rows, PROVISIONAL physics\n",replay->input.frames,records.size());
            } else if (world->hasSpawn) {
                auto n=world->spawnNormal;
                double heading=std::hypot(n.x,n.z)>.01?std::atan2(n.x,n.z):std::atan2(world->center.x-world->spawn.x,world->center.z-world->spawn.z);
                rider.reset(world->spawn,n,heading);
                NSString* startPath=[[@(SSX_ASSET_ROOT) stringByAppendingPathComponent:location] stringByAppendingPathComponent:@"riding-start.json"];
                NSDictionary* start=readNativeRideStart(startPath);
                if(start){initializeNativeReplay(rider,start,*world->terrainCollision,world->spawn,n,heading);
                    originalAnimator=ssx::makeOriginalRiderAnimation(mesh->originalAnimation,start[@"native"][@"initial"][@"original_animation"]);
                    if(originalAnimator){bodyPoseProvider=originalAnimator->bodyProvider();poseStages=ssx::originalPoseStages(originalAnimator);}}
                ssx::RiderInput input;input.crouch=1;
                int frames=argc>3?atoi(argv[3]):0;
                for (int i=0;i<frames;++i) rider.advance(1.0/60,input,*world->terrainCollision,*world->collision,world->bodyCollision.get(),bodyPoseProvider?&bodyPoseProvider:nullptr,originalAnimator?&poseStages:nullptr);
                if (!std::isfinite(rider.position.y)) @throw [NSException exceptionWithName:@"Nonfinite riding state" reason:@"Simulation integration failed" userInfo:nil];
                std::printf("Course %s spawn %.3f %.3f %.3f normal %.3f %.3f %.3f; after %d frames %.3f %.3f %.3f speed %.3f grounded %d\n",
                    location.UTF8String,world->spawn.x,world->spawn.y,world->spawn.z,n.x,n.y,n.z,frames,
                    rider.position.x,rider.position.y,rider.position.z,rider.speed(),rider.grounded);
            }
            for (NSUInteger clip=0;clip<mesh->animationClips.count;++clip) {
                double duration=[mesh->animationClips[clip][@"duration"] doubleValue];
                std::vector<double> times={0};if(NSProcessInfo.processInfo.environment[@"SSX_AUDIT_ALL_CLIP_TIMES"])times={0,duration*.5,duration};
                for(double time:times){
                auto palette=[mesh paletteAt:time clip:clip playing:true mirror:false diagnostic:false];
                const float* vertices=(const float*)mesh->vertices.contents;
                const SkinVertex* skin=(const SkinVertex*)mesh->skinBuffer.contents;
                float3 low={INFINITY,INFINITY,INFINITY},high={-INFINITY,-INFINITY,-INFINITY};
                for (size_t i=0;i<mesh->vertices.length/40;++i) {
                    float4 value={vertices[i*10],vertices[i*10+1],vertices[i*10+2],1},point={0,0,0,0};
                    for (unsigned j=0;j<4;++j) point+=palette[skin[i].joints[j]]*value*skin[i].weights[j];
                    for (unsigned j=0;j<3;++j) {if(!std::isfinite(point[j]))@throw [NSException exceptionWithName:@"Invalid skinned vertex" reason:mesh->animationClips[clip][@"name"] userInfo:nil];low[j]=std::min(low[j],point[j]);high[j]=std::max(high[j],point[j]); }
                }
                // Full banks intentionally move unused equipment offstage:
                // FE_GEAR_KAORI_CYC places its board12.14m below the menu floor.
                // A fixed5m combined body/board bound was only valid for the old eight-clip set.
                if (!std::isfinite(low.y)) @throw [NSException exceptionWithName:@"Invalid skinned bounds" reason:@"Original pose integration" userInfo:nil];
                std::printf("%s: posed bounds [%.3f %.3f %.3f] [%.3f %.3f %.3f], %zu bones\n",
                            [mesh->animationClips[clip][@"name"] UTF8String],low.x,low.y,low.z,high.x,high.y,high.z,palette.size());
                }
            }
            NSError* error=nil;
            id<MTLLibrary> library=[device newLibraryWithSource:shaderSource options:nil error:&error];
            if (!library) @throw [NSException exceptionWithName:@"Metal shader" reason:error.description userInfo:nil];
            MTLRenderPipelineDescriptor* pd=[MTLRenderPipelineDescriptor new];
            pd.colorAttachments[0].pixelFormat=MTLPixelFormatBGRA8Unorm;pd.depthAttachmentPixelFormat=MTLPixelFormatDepth32Float_Stencil8;pd.stencilAttachmentPixelFormat=MTLPixelFormatDepth32Float_Stencil8;
            pd.fragmentFunction=[library newFunctionWithName:@"terrainFragment"];
            pd.vertexFunction=[library newFunctionWithName:@"terrainVertex"];
            id<MTLRenderPipelineState> terrain=[device newRenderPipelineStateWithDescriptor:pd error:&error];
            pd.vertexFunction=[library newFunctionWithName:@"riderVertex"];
            id<MTLRenderPipelineState> skinned=[device newRenderPipelineStateWithDescriptor:pd error:&error];
            if (!terrain||!skinned) @throw [NSException exceptionWithName:@"Metal pipeline" reason:error.description userInfo:nil];
            id<MTLRenderPipelineState> skyPipeline=makeSkyPipeline(device,library,MTLPixelFormatBGRA8Unorm,MTLPixelFormatDepth32Float_Stencil8,1);
            id<MTLDepthStencilState> skyDepth=makeSkyDepthState(device);
            const NSUInteger width=1280,height=800;
            MTLTextureDescriptor* td=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm width:width height:height mipmapped:NO];
            td.usage=MTLTextureUsageRenderTarget|MTLTextureUsageShaderRead;td.storageMode=MTLStorageModeShared;
            id<MTLTexture> color=[device newTextureWithDescriptor:td];
            td.pixelFormat=MTLPixelFormatDepth32Float_Stencil8;td.storageMode=MTLStorageModePrivate;
            id<MTLTexture> depth=[device newTextureWithDescriptor:td];
            MTLRenderPassDescriptor* pass=[MTLRenderPassDescriptor new];
            pass.colorAttachments[0].texture=color;pass.colorAttachments[0].loadAction=MTLLoadActionClear;
            pass.colorAttachments[0].storeAction=MTLStoreActionStore;pass.colorAttachments[0].clearColor=skyClearColor(sky);
            pass.depthAttachment.texture=depth;pass.depthAttachment.loadAction=MTLLoadActionClear;pass.depthAttachment.clearDepth=1;pass.stencilAttachment.texture=depth;pass.stencilAttachment.loadAction=MTLLoadActionClear;pass.stencilAttachment.clearStencil=0;
            MTLDepthStencilDescriptor* ds=[MTLDepthStencilDescriptor new];ds.depthCompareFunction=MTLCompareFunctionLessEqual;ds.depthWriteEnabled=YES;
            id<MTLCommandBuffer> command=[queue commandBuffer];id<MTLRenderCommandEncoder> enc=[command renderCommandEncoderWithDescriptor:pass];
            [enc setDepthStencilState:[device newDepthStencilStateWithDescriptor:ds]];[enc setCullMode:MTLCullModeNone];
            auto camera=followCamera(rider,*world->collision);
            float3 target=camera.target,eye=camera.eye;float fov=65*M_PI/180;
            if(cameraValid){auto v=ssx::nativeCameraView(cameraOutput);eye={v.eye[0],v.eye[1],v.eye[2]};target={v.lookAt[0],v.lookAt[1],v.lookAt[2]};fov=v.fovRadians;std::printf("Original camera: eye %.2f %.2f %.2f fov %.3f rad\n",eye.x,eye.y,eye.z,fov);}
            if (world->skeleton) {
                target={0,.8,0};eye={3,2.2,-4};
                if (NSProcessInfo.processInfo.environment[@"SSX_AUDIT_CLOSEUP"]) {target={0,1.32,0};eye={.65,1.44,-.9};}
                NSString* modelView=NSProcessInfo.processInfo.environment[@"SSX_AUDIT_MODEL_VIEW"];
                if ([modelView isEqual:@"full"]) {target={0,.87,0};eye={1.05,1.55,-2.05};}
                if ([modelView isEqual:@"back"]) {target={0,.90,0};eye={-.75,1.45,2.1};}
            }
            float4x4 vp=perspective(float(width)/height,.1,fov)*look(eye,target-eye);
            Uniforms u{vp,matrix_identity_float4x4,{eye.x,eye.y,eye.z,1},{0,0,0,0}};applyFog(u,sky);
            id<MTLDepthStencilState> worldDepth=[device newDepthStencilStateWithDescriptor:ds];
            drawSky(enc,sky,skyPipeline,skyDepth,u);[enc setDepthStencilState:worldDepth];[enc setCullMode:MTLCullModeNone];
            NSUInteger renderClip=0;
            NSString* requestedClip=NSProcessInfo.processInfo.environment[@"SSX_AUDIT_CLIP"];
            if (requestedClip) {
                BOOL found=NO;
                for (NSUInteger i=0;i<mesh->animationClips.count;++i) if ([mesh->animationClips[i][@"name"] isEqual:requestedClip]) {renderClip=i;found=YES;break;}
                if (!found) throw std::runtime_error("Requested render clip is not available");
            }
            double renderTime=rider.elapsed;
            if(NSString* time=NSProcessInfo.processInfo.environment[@"SSX_AUDIT_CLIP_TIME"]){renderTime=time.doubleValue;if(!std::isfinite(renderTime)||renderTime<0)throw std::runtime_error("Invalid render clip time");}
            auto palette=[mesh paletteAt:renderTime clip:renderClip playing:true mirror:false diagnostic:false];
            bool originalWorldPose=false;
            if(collisionAnimator){auto sourcePose=collisionAnimator->poseAt(rider,rider.elapsed);bool same=mesh->originalAnimation==nullptr?false:mesh->originalAnimation->character==[replay->source[@"native"][@"initial"][@"original_animation"][@"character"] UTF8String];
                auto scale=same?collisionAnimator->modelScale():ssx::AnimationVector{1,1,1};auto pose=same?sourcePose:collisionAnimator->poseForRig(rider,*mesh->animationBindings,scale);
                if(pose){palette=[mesh paletteForOriginalPose:*pose scale:scale];originalWorldPose=true;}}
            else if(originalAnimator){auto pose=originalAnimator->poseAt(rider,rider.elapsed);if(pose){palette=[mesh paletteForOriginalPose:*pose scale:originalAnimator->modelScale()];originalWorldPose=true;}}
            if (!world->skeleton) drawAsset(enc,world,terrain,u,true);
            u.model=(world->skeleton||originalWorldPose)?matrix_identity_float4x4:riderTransform(rider);u.mvp=vp*u.model;
            drawAsset(enc,mesh,skinned,u,false,palette);
            if(effects){effects->draw(enc,vp,target-eye);std::printf("Original snow FX: %zu trail slices, %zu particles\n",effects->trackSlices(),effects->visibleParticles());}
            [enc endEncoding];
            if (NSProcessInfo.processInfo.environment[@"SSX_AUDIT_RETRO"]) {
                // Same retro presentation pass as the app, for headless review.
                MTLTextureDescriptor* pt=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm width:width height:height mipmapped:NO];
                pt.usage=MTLTextureUsageRenderTarget;pt.storageMode=MTLStorageModeShared;id<MTLTexture> presented=[device newTextureWithDescriptor:pt];
                MTLRenderPassDescriptor* postPass=[MTLRenderPassDescriptor new];postPass.colorAttachments[0].texture=presented;postPass.colorAttachments[0].loadAction=MTLLoadActionDontCare;postPass.colorAttachments[0].storeAction=MTLStoreActionStore;
                td.pixelFormat=MTLPixelFormatDepth32Float_Stencil8;td.storageMode=MTLStorageModePrivate;td.usage=MTLTextureUsageRenderTarget;id<MTLTexture> postDepth=[device newTextureWithDescriptor:td];
                postPass.depthAttachment.texture=postDepth;postPass.stencilAttachment.texture=postDepth;
                id<MTLRenderPipelineState> postPipeline=makePostPipeline(device,library,MTLPixelFormatBGRA8Unorm,MTLPixelFormatDepth32Float_Stencil8);
                id<MTLRenderCommandEncoder> post=[command renderCommandEncoderWithDescriptor:postPass];
                float radius=std::clamp(float(width)/640.f*.55f,.4f,2.f);float params[4]={1.f/width,1.f/height,radius,1.f};
                [post setRenderPipelineState:postPipeline];[post setFragmentTexture:color atIndex:0];[post setFragmentBytes:params length:sizeof(params) atIndex:0];
                [post drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];[post endEncoding];
                color=presented;
            }
            [command commit];[command waitUntilCompleted];
            if (command.error) @throw [NSException exceptionWithName:@"Metal render failed" reason:command.error.description userInfo:nil];
            if (imagePath) {
                NSMutableData* pixels=[NSMutableData dataWithLength:width*height*4];
                [color getBytes:pixels.mutableBytes bytesPerRow:width*4 fromRegion:MTLRegionMake2D(0,0,width,height) mipmapLevel:0];
                CGColorSpaceRef space=CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
                CGContextRef context=CGBitmapContextCreate(pixels.mutableBytes,width,height,8,width*4,space,CGBitmapInfo(kCGImageAlphaPremultipliedFirst)|kCGBitmapByteOrder32Little);
                CGImageRef image=CGBitmapContextCreateImage(context);
                NSURL* url=[NSURL fileURLWithPath:imagePath];
                CGImageDestinationRef destination=CGImageDestinationCreateWithURL((__bridge CFURLRef)url,(__bridge CFStringRef)UTTypePNG.identifier,1,nullptr);
                if (!destination) @throw [NSException exceptionWithName:@"Cannot save render" reason:url.path userInfo:nil];
                CGImageDestinationAddImage(destination,image,nullptr);
                bool saved=CGImageDestinationFinalize(destination);
                CFRelease(destination);CGImageRelease(image);CGContextRelease(context);CGColorSpaceRelease(space);
                if (!saved) @throw [NSException exceptionWithName:@"Cannot save render" reason:url.path userInfo:nil];
            }
            std::puts("Headless native course/rider Metal render passed");
        } @catch(NSException* error) {std::fprintf(stderr,"%s: %s\n",error.name.UTF8String,error.reason.UTF8String);return 1;}
        } catch(const std::exception& error) {std::fprintf(stderr,"Native audit: %s\n",error.what());return 1;}
    }
}
