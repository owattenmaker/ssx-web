#include "rider_pose_stages.h"
#include "preview_rider_physics.h"
#include "gameplay_animation.h"
#include "gameplay_effects.h"
#include "original_camera.hpp"
#import <AppKit/AppKit.h>
#import <MetalKit/MetalKit.h>
#import <simd/simd.h>
#import <GameController/GameController.h>
#include "input.hpp"
#include "collision.hpp"
#include "skeleton.hpp"
#include "riding.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <set>
#include <vector>
#include <memory>
using simd::float3;
using simd::float4;
using simd::float4x4;
#include "mesh_asset.h"

#include "renderer.h"
#include "preview_presentation.h"
#include "replay_io.h"
#include "rider_animation_player.h"

static constexpr NSUInteger sceneSamples=4;
@interface EngineView : MTKView <MTKViewDelegate> {
    id<MTLCommandQueue> queue;
    id<MTLRenderPipelineState> terrainPipeline,riderPipeline,skyPipeline,postPipeline;
    id<MTLDepthStencilState> depth,skyDepth;
    id<MTLTexture> sceneMSAA,sceneColor,sceneDepth;
    bool retroFilter;
    SSXMeshAsset* scene;
    SSXSkyAsset* sky;
    SSXMeshAsset* riderMesh;
    std::set<unsigned short> keys;
    float3 eye,center;
    float yaw,pitch,radius;
    double lastFrame,animationTime;
    bool wireframe,normals,bakedLighting,probeCollision;
    bool riding,paused,diagnosticPose,playDecoded,mirrorDecoded;
    NSUInteger frames,animationIndex;
    NSTextField* hud;
    NSString* location;
    NSString* riderPackage;
    ssx::InputMapper inputMapper;
    ssx::RiderInput riderInput;
    ssx::PrototypeRider rider;
    std::shared_ptr<ssx::GameplayAnimation> gameplayAnimation;
    std::shared_ptr<ssx::OriginalRiderAnimation> collisionAnimator;
    std::string collisionCharacter;
    ssx::BodyPoseProvider bodyPoseProvider;ssx::RiderPoseStages poseStages;
    std::vector<float4x4> displayedPalette;
    NSString* controllerName;
    NSString* gameplayError;
    std::shared_ptr<ssx::GameplayEffects> gameplayEffects;
    // Original DEFAULT_3 chase camera (engine/CAMERA_RECOVERY.md), stepped once per 60 Hz tick.
    ssx::OriginalCameraState cameraState;ssx::OriginalCameraOutput cameraOutput;bool cameraValid;
}
- (ssx::OriginalCameraInput)cameraInputFor:(const ssx::PrototypeRider&)r;
- (void)stepCamera:(const ssx::PrototypeRider&)r;
- (instancetype)initWithFrame:(NSRect)frame location:(NSString*)name;
- (void)resetCamera;
- (void)resetRider;
- (void)changeLocation:(NSPopUpButton*)sender;
- (void)changeRider:(NSPopUpButton*)sender;
@end

@implementation EngineView
- (instancetype)initWithFrame:(NSRect)frame location:(NSString*)name {
    self=[super initWithFrame:frame device:MTLCreateSystemDefaultDevice()];
    if (!self) return nil;
    if (!self.device) @throw [NSException exceptionWithName:@"Metal unavailable" reason:@"No Metal device" userInfo:nil];
    location=name;
    self.colorPixelFormat=MTLPixelFormatBGRA8Unorm;
    self.depthStencilPixelFormat=MTLPixelFormatDepth32Float_Stencil8;
    self.sampleCount=1;self.clearColor=MTLClearColorMake(.18,.30,.44,1);retroFilter=![NSUserDefaults.standardUserDefaults boolForKey:@"SSXRetroFilterOff"];
    self.preferredFramesPerSecond=60;self.delegate=self;
    queue=[self.device newCommandQueue];
    NSError* error=nil;
    id<MTLLibrary> library=[self.device newLibraryWithSource:shaderSource options:nil error:&error];
    if (!library) @throw [NSException exceptionWithName:@"Metal shader" reason:error.description userInfo:nil];
    MTLRenderPipelineDescriptor* descriptor=[MTLRenderPipelineDescriptor new];
    descriptor.fragmentFunction=[library newFunctionWithName:@"terrainFragment"];
    descriptor.colorAttachments[0].pixelFormat=self.colorPixelFormat;
    descriptor.depthAttachmentPixelFormat=self.depthStencilPixelFormat;descriptor.stencilAttachmentPixelFormat=self.depthStencilPixelFormat;descriptor.rasterSampleCount=sceneSamples;
    descriptor.vertexFunction=[library newFunctionWithName:@"terrainVertex"];
    terrainPipeline=[self.device newRenderPipelineStateWithDescriptor:descriptor error:&error];
    descriptor.vertexFunction=[library newFunctionWithName:@"riderVertex"];
    riderPipeline=[self.device newRenderPipelineStateWithDescriptor:descriptor error:&error];
    if (!terrainPipeline||!riderPipeline) @throw [NSException exceptionWithName:@"Metal pipeline" reason:error.description userInfo:nil];
    skyPipeline=makeSkyPipeline(self.device,library,self.colorPixelFormat,self.depthStencilPixelFormat,sceneSamples);skyDepth=makeSkyDepthState(self.device);
    postPipeline=makePostPipeline(self.device,library,self.colorPixelFormat,self.depthStencilPixelFormat);
    MTLDepthStencilDescriptor* ds=[MTLDepthStencilDescriptor new];
    ds.depthCompareFunction=MTLCompareFunctionLessEqual;ds.depthWriteEnabled=YES;
    depth=[self.device newDepthStencilStateWithDescriptor:ds];
    scene=[[SSXMeshAsset alloc] initWithDevice:self.device queue:queue location:name];
    sky=[SSXSkyAsset skyForLocation:name device:self.device];self.clearColor=skyClearColor(sky);
    riderPackage=NSProcessInfo.processInfo.environment[@"SSX_RIDER_PACKAGE"] ?: [NSUserDefaults.standardUserDefaults stringForKey:@"SSXSelectedRiderPackage"] ?: @"RIDER_ZOE";
    NSArray* allowedRiders=@[@"RIDER_MAC",@"RIDER_SAM",@"RIDER_SAM_PACKERS",@"RIDER_SAM_FISHING",@"RIDER_SAM_FISHING_TUBE",@"RIDER_ZOE"];
    if (![allowedRiders containsObject:riderPackage]) riderPackage=@"RIDER_MAC";
    if (!scene->skeleton && scene->hasSpawn) {
        @try {riderMesh=[[SSXMeshAsset alloc] initWithDevice:self.device queue:queue location:riderPackage];}
        @catch(NSException* error) {
            if ([riderPackage isEqualToString:@"RIDER_MAC"]) @throw;
            NSLog(@"Selected rider package unavailable: %@ — %@",riderPackage,error.reason);
            riderPackage=@"RIDER_MAC";
            riderMesh=[[SSXMeshAsset alloc] initWithDevice:self.device queue:queue location:riderPackage];
        }
    }
    center=scene->center;radius=scene->radius;bakedLighting=scene->lightingVerified;
    riding=riderMesh!=nil && ![NSProcessInfo.processInfo.environment[@"SSX_MODE"] isEqualToString:@"inspect"];
    if (riding) [self resetRider];else [self resetCamera];
    hud=[NSTextField labelWithString:@""];
    hud.textColor=NSColor.whiteColor;hud.backgroundColor=[NSColor colorWithWhite:0 alpha:.7];hud.drawsBackground=YES;
    hud.font=[NSFont monospacedSystemFontOfSize:12 weight:NSFontWeightMedium];
    hud.frame=NSMakeRect(18,18,frame.size.width-36,92);hud.autoresizingMask=NSViewMaxYMargin|NSViewWidthSizable;
    [self addSubview:hud];
    NSPopUpButton* locations=[[NSPopUpButton alloc] initWithFrame:NSMakeRect(18,frame.size.height-48,200,30) pullsDown:NO];
    locations.autoresizingMask=NSViewMinYMargin;
    NSArray<NSString*>* names=[[NSFileManager.defaultManager contentsOfDirectoryAtPath:@(SSX_ASSET_ROOT) error:nil] sortedArrayUsingSelector:@selector(compare:)];
    for (NSString* candidate in names) {
        if ([candidate hasPrefix:@"."]) continue;
        NSString* path=[[@(SSX_ASSET_ROOT) stringByAppendingPathComponent:candidate] stringByAppendingPathComponent:@"world.json"];
        if ([NSFileManager.defaultManager fileExistsAtPath:path]) [locations addItemWithTitle:candidate];
    }
    [locations selectItemWithTitle:location];locations.target=self;locations.action=@selector(changeLocation:);
    locations.accessibilityLabel=@"Imported course area";[self addSubview:locations];
    if (riderMesh) {
        NSPopUpButton* riders=[[NSPopUpButton alloc] initWithFrame:NSMakeRect(230,frame.size.height-48,260,30) pullsDown:NO];
        riders.autoresizingMask=NSViewMinYMargin;
        NSArray* labels=@[@"Mac",@"Sam · Rope Tow Regular",@"Sam · Packers",@"Sam · Fly Fishing / Net",@"Sam · Fly Fishing / Rod Kit",@"Zoe"];
        for (NSUInteger i=0;i<allowedRiders.count;++i) {
            NSString* path=[[@(SSX_ASSET_ROOT) stringByAppendingPathComponent:allowedRiders[i]] stringByAppendingPathComponent:@"rider.json"];
            if (![NSFileManager.defaultManager fileExistsAtPath:path]) continue;
            [riders addItemWithTitle:labels[i]];riders.lastItem.representedObject=allowedRiders[i];
            if ([allowedRiders[i] isEqual:riderPackage]) [riders selectItem:riders.lastItem];
        }
        riders.target=self;riders.action=@selector(changeRider:);riders.accessibilityLabel=@"Playable rider and outfit";[self addSubview:riders];

    }
    NSLog(@"Native Metal %@: %zu triangles, riding prototype %@",name,scene->collision->triangleCount(),riding?@"on":@"off");
    return self;
}
- (BOOL)acceptsFirstResponder { return YES; }
- (ssx::OriginalCameraInput)cameraInputFor:(const ssx::PrototypeRider&)r {
    ssx::OriginalCameraInput in;
    auto physical=ssx::OriginalAirState::fromNative(r.position,r.velocity);
    auto head=collisionAnimator?collisionAnimator->posedBone(0):std::nullopt; // rider+89C primary bone (root), 11FF48
    auto headPosition=head?head->position:physical.position;
    in.headPosition={headPosition[0],headPosition[1],headPosition[2],1};
    auto frame=r.currentPhysicalFrame();in.riderForward={frame.forward[0],frame.forward[1],frame.forward[2],0};
    in.velocity={physical.velocity[0],physical.velocity[1],physical.velocity[2],0};
    const auto& g=r.originalGroundState();in.previousContactNormal={g.previousNormal[0],g.previousNormal[1],g.previousNormal[2],0};
    in.motionMode=r.currentMotionMode();in.boostLevel=g.boost;in.jumpCharge=float(r.jumpCharge);in.surfaceId=r.currentGroundSurfaceId();
    const auto& trajectory=r.originalAirTrajectoryState();in.trajectoryStatusActive=trajectory.status==1||trajectory.status==3;in.predictedAirTime=trajectory.predictedTime;
    in.trajectoryHeading={trajectory.heading[0],trajectory.heading[1],trajectory.heading[2],0};in.trajectoryNormal={trajectory.normal[0],trajectory.normal[1],trajectory.normal[2],0};
    in.tick=r.originalLandingRuntime().tick;
    ssx::CollisionWorld* world=scene->collision.get();
    in.terrainProbe=[world](const ssx::OriginalCameraQuad& start,const ssx::OriginalCameraQuad& end)->std::optional<ssx::OriginalCameraProbeHit>{
        ssx::Vec3 a{start[0]*.01,start[2]*.01,-start[1]*.01},b{end[0]*.01,end[2]*.01,-end[1]*.01};
        ssx::Vec3 d=b-a;double length=std::sqrt(ssx::dot(d,d));if(length<=0)return std::nullopt;
        auto hit=world->raycast(a,d,length);if(!hit.hit)return std::nullopt;
        return ssx::OriginalCameraProbeHit{float(hit.distance/length),{float(hit.normal.x),float(-hit.normal.z),float(hit.normal.y),0}};
    };
    return in;
}
- (void)stepCamera:(const ssx::PrototypeRider&)r {
    if(!cameraValid)return;
    try {cameraOutput=ssx::originalChaseCameraStep(cameraState,[self cameraInputFor:r]);}
    catch(const std::exception& error){cameraValid=false;NSLog(@"Original camera stopped: %s",error.what());}
}
- (void)changeRider:(NSPopUpButton*)sender {
    NSString* selected=sender.selectedItem.representedObject;
    if (!selected || [selected isEqual:riderPackage]) return;
    @try {
        SSXMeshAsset* candidate=[[SSXMeshAsset alloc] initWithDevice:self.device queue:queue location:selected];
        if (!candidate->skeleton || !candidate->animationClips.count)
            @throw [NSException exceptionWithName:@"Invalid rider" reason:@"The rider needs a rig and animation clips." userInfo:nil];
        riderMesh=candidate;riderPackage=selected;displayedPalette.clear();animationTime=0;
        keys.clear();rider.cancelInput();
        [NSUserDefaults.standardUserDefaults setObject:selected forKey:@"SSXSelectedRiderPackage"];
    } @catch(NSException* error) {
        for (NSMenuItem* item in sender.itemArray) if ([item.representedObject isEqual:riderPackage]) [sender selectItem:item];
        NSAlert* alert=[NSAlert new];alert.messageText=@"Could not load this rider";alert.informativeText=error.reason;
        [alert beginSheetModalForWindow:self.window completionHandler:nil];
    }
    [self.window makeFirstResponder:self];
}
- (void)changeLocation:(NSPopUpButton*)sender {
    NSString* selected=sender.titleOfSelectedItem;if ([selected isEqualToString:location]) return;
    NSWindow* window=self.window;
    @try {
        EngineView* next=[[EngineView alloc] initWithFrame:self.frame location:selected];
        window.contentView=next;[window makeFirstResponder:next];
    } @catch(NSException* error) { [sender selectItemWithTitle:location];NSLog(@"Course load failed: %@",error.reason); }
}
- (void)resetCamera { eye=center+float3{0,radius*.45f,radius*.85f};yaw=M_PI;pitch=-.487f; }
- (void)resetRider {
    if (!riderMesh) return;
    ssx::Vec3 downhill={scene->spawnNormal.x,-(1-scene->spawnNormal.y),scene->spawnNormal.z};
    if (std::hypot(downhill.x,downhill.z)<.01) downhill={center.x-scene->spawn.x,0,center.z-scene->spawn.z};
    rider.reset(scene->spawn,scene->spawnNormal,std::atan2(downhill.x,downhill.z));
    NSString* startPath=[[@(SSX_ASSET_ROOT) stringByAppendingPathComponent:location] stringByAppendingPathComponent:@"riding-start.json"];
    try {
        NSDictionary* start=readNativeRideStart(startPath);
        gameplayAnimation.reset();gameplayEffects.reset();collisionAnimator.reset();collisionCharacter.clear();bodyPoseProvider={};poseStages={};
        if(start){ initializeNativeReplay(rider,start,*scene->terrainCollision,scene->spawn,scene->spawnNormal,rider.heading);
            NSDictionary* animation=start[@"native"][@"initial"][@"original_animation"];
            collisionAnimator=makePreviewCollisionAnimation(@(SSX_ASSET_ROOT),riderMesh->originalAnimation,animation);
            if(collisionAnimator){collisionCharacter=[animation[@"character"] UTF8String];bodyPoseProvider=collisionAnimator->bodyProvider();poseStages=previewCollisionStages(collisionAnimator);
                gameplayAnimation=std::make_shared<ssx::GameplayAnimation>(collisionAnimator,start[@"native"][@"initial"]);gameplayAnimation->bind(rider);
                gameplayEffects=std::make_shared<ssx::GameplayEffects>(self.device,self.colorPixelFormat,self.depthStencilPixelFormat,sceneSamples,@(SSX_ASSET_ROOT),location,start[@"native"][@"initial"],collisionAnimator);
                __unsafe_unretained EngineView* view=self;
                gameplayAnimation->afterFrame=[fx=gameplayEffects,view](const ssx::PrototypeRider& r){fx->step(r);[view stepCamera:r];};}}
    } catch(const std::exception& error) {
        @throw [NSException exceptionWithName:@"Invalid native riding start" reason:@(error.what()) userInfo:nil];
    }
    rider.bindOriginalRails(scene->rails);
    yaw=rider.heading;pitch=0;paused=false;gameplayError=nil;animationTime=0;displayedPalette.clear();
    inputMapper.reset();
    eye=previewFollowCamera(rider,*scene->collision).eye;
    cameraValid=false;
    if(collisionAnimator){
        try {cameraState={};cameraState.begin([self cameraInputFor:rider]);cameraOutput=ssx::originalChaseCameraStep(cameraState,[self cameraInputFor:rider]);cameraValid=true;}
        catch(const std::exception& error){NSLog(@"Original camera unavailable: %s",error.what());}
    }
}
- (void)keyDown:(NSEvent*)event {
    keys.insert(event.keyCode);if (event.isARepeat) return;
    if (event.keyCode==9 && riderMesh) { riding=!riding;if (riding) [self resetRider];else [self resetCamera]; }
    if (event.keyCode==15) { if (riding) [self resetRider];else [self resetCamera]; }
    if (event.keyCode==3) wireframe=!wireframe;
    if (event.keyCode==45) normals=!normals;
    if (event.keyCode==37) bakedLighting=!bakedLighting;
    if (event.keyCode==11) probeCollision=!probeCollision;
    if (event.keyCode==4) {retroFilter=!retroFilter;[NSUserDefaults.standardUserDefaults setBool:!retroFilter forKey:@"SSXRetroFilterOff"];}
    if (riding && (event.keyCode==35||event.keyCode==53)) paused=!paused;
    if (!riding && scene->skeleton) {
        if (event.keyCode==17) diagnosticPose=!diagnosticPose;
        if (event.keyCode==46) mirrorDecoded=!mirrorDecoded;
        if (event.keyCode==35 && scene->animationClips.count) {
            playDecoded=!playDecoded;animationTime=0;
            center.y+=playDecoded?1:-1;eye.y+=playDecoded?1:-1;
        }
        if ((event.keyCode==30||event.keyCode==33)&&scene->animationClips.count) {
            animationIndex=(animationIndex+scene->animationClips.count+(event.keyCode==30?1:-1))%scene->animationClips.count;animationTime=0;
        }
    }
    if (event.keyCode==40) {
        NSWindow* window=self.window;
        @try {
            EngineView* next=[[EngineView alloc] initWithFrame:self.frame location:location];
            next->eye=eye;next->yaw=yaw;next->pitch=pitch;next->riding=riding;next->rider=rider;
            next->paused=paused;next->probeCollision=probeCollision;
            window.contentView=next;[window makeFirstResponder:next];
        } @catch(NSException* error) { NSLog(@"Asset reload failed: %@",error.reason); }
    }
}
- (void)keyUp:(NSEvent*)event { keys.erase(event.keyCode); }
- (BOOL)resignFirstResponder { keys.clear();return [super resignFirstResponder]; }
- (void)mouseDown:(NSEvent*)event { (void)event;[self.window makeFirstResponder:self]; }
- (void)mouseDragged:(NSEvent*)event { yaw+=event.deltaX*.004f;pitch=std::clamp(pitch-float(event.deltaY)*.004f,-1.1f,1.1f); }
- (void)scrollWheel:(NSEvent*)event {
    if (riding) return;
    float3 forward={sinf(yaw)*cosf(pitch),sinf(pitch),cosf(yaw)*cosf(pitch)};
    eye+=forward*(float)event.scrollingDeltaY*std::max(.005f,radius*.002f);
}
- (void)mtkView:(MTKView*)view drawableSizeWillChange:(CGSize)size { (void)view;(void)size; }
- (void)drawInMTKView:(MTKView*)view {
    double now=CACurrentMediaTime(),dt=lastFrame?std::min(now-lastFrame,.05):0;lastFrame=now;
    bool active=self.window.isKeyWindow;
    if (!active) { keys.clear();rider.cancelInput(); }
    ssx::InputSample input;
    GCController* controller=GCController.current?:GCController.controllers.firstObject;
    GCExtendedGamepad* pad=controller.extendedGamepad;
    controllerName=pad?(controller.vendorName?:@"Gamepad"):@"Keyboard";
    if (active && pad) {
        input.leftX=pad.leftThumbstick.xAxis.value;input.leftY=pad.leftThumbstick.yAxis.value;
        input.rightX=pad.rightThumbstick.xAxis.value;input.rightY=pad.rightThumbstick.yAxis.value;
        input.dpadX=pad.dpad.xAxis.value;input.dpadY=pad.dpad.yAxis.value;
        auto button=[&](GCControllerButtonInput* b,ssx::Button name){if (b.isPressed) input.buttons|=ssx::bit(name);};
        button(pad.buttonA,ssx::Button::South);button(pad.buttonX,ssx::Button::West);
        button(pad.buttonB,ssx::Button::East);button(pad.buttonY,ssx::Button::North);
        button(pad.leftShoulder,ssx::Button::L1);button(pad.leftTrigger,ssx::Button::L2);
        button(pad.rightShoulder,ssx::Button::R1);button(pad.rightTrigger,ssx::Button::R2);
        button(pad.leftThumbstickButton,ssx::Button::L3);button(pad.rightThumbstickButton,ssx::Button::R3);
        button(pad.buttonMenu,ssx::Button::Pause);
        yaw+=input.rightX*dt*1.7;pitch=std::clamp(pitch+input.rightY*dt*1.7,-1.1,1.1);
    }
    for (const auto& pair:std::array<std::pair<unsigned short,ssx::Button>,4>{{
        {12,ssx::Button::L1},{14,ssx::Button::R1},{6,ssx::Button::L2},{7,ssx::Button::R2}}})
        if (keys.count(pair.first)) input.buttons|=ssx::bit(pair.second);
    if (riding) {
        input.leftX=std::clamp(input.leftX+float(keys.count(2)||keys.count(124))-float(keys.count(0)||keys.count(123)),-1.f,1.f);
        input.leftY=std::clamp(input.leftY+float(keys.count(13)||keys.count(126))-float(keys.count(1)||keys.count(125)),-1.f,1.f);
        if (keys.count(49)) input.buttons|=ssx::bit(ssx::Button::South);
        if (active && ([NSEvent modifierFlags]&NSEventModifierFlagShift)) input.buttons|=ssx::bit(ssx::Button::West);
    }
    riderInput=inputMapper.update(input);
    if (riderInput.pausePressed) paused=!paused;
    float3 forward;
    if (riding) {
        if (active&&!paused&&!gameplayError) {
            try {rider.advance(dt,riderInput,*scene->terrainCollision,*scene->collision,scene->bodyCollision.get(),bodyPoseProvider?&bodyPoseProvider:nullptr,collisionAnimator?&poseStages:nullptr);}
            catch(const std::exception& error){gameplayError=@(error.what());paused=true;NSLog(@"Preview paused: %@",gameplayError);}
            animationTime+=dt;
            if (rider.position.y<scene->minimumBounds.y-100) [self resetRider];
            else if (rider.resetRequested) [self resetRider]; // Original116120 reset request honoured as a respawn.
        }
        if(cameraValid){
            auto view=ssx::nativeCameraView(cameraOutput);
            eye={view.eye[0],view.eye[1],view.eye[2]};float3 target={view.lookAt[0],view.lookAt[1],view.lookAt[2]};
            forward=simd_normalize(target-eye);
        } else {
            auto camera=previewFollowCamera(rider,*scene->collision);
            eye+=(camera.eye-eye)*float(1-std::exp(-dt*7));
            auto delta=eye-camera.target;
            auto hit=scene->collision->raycast({camera.target.x,camera.target.y,camera.target.z},{delta.x,delta.y,delta.z},simd_length(delta));
            if (hit.hit) eye=camera.target+simd_normalize(delta)*float(std::max(.5,hit.distance-.3));
            forward=simd_normalize(camera.target-eye);
        }
    } else {
        pitch=std::clamp(pitch+(keys.count(126)-double(keys.count(125)))*dt,-1.5,1.5);
        yaw+=(keys.count(124)-double(keys.count(123)))*dt;
        forward={sinf(yaw)*cosf(pitch),sinf(pitch),cosf(yaw)*cosf(pitch)};
        float3 right={-cosf(yaw),0,sinf(yaw)};
        float speed=std::max(.25f,radius*.15f)*(([NSEvent modifierFlags]&NSEventModifierFlagShift)?4:1);
        eye+=(forward*float(keys.count(13)-double(keys.count(1))+input.leftY)+right*float(keys.count(2)-double(keys.count(0))+input.leftX)
              +float3{0,float(keys.count(49)-double(keys.count(8))),0})*float(dt)*speed;
        animationTime+=dt;
    }
    MTLRenderPassDescriptor* present=view.currentRenderPassDescriptor;id<CAMetalDrawable> drawable=view.currentDrawable;
    if (!present||!drawable) return;
    NSUInteger width=NSUInteger(view.drawableSize.width),height=NSUInteger(view.drawableSize.height);
    if (!sceneColor||sceneColor.width!=width||sceneColor.height!=height) {
        MTLTextureDescriptor* td=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:self.colorPixelFormat width:width height:height mipmapped:NO];
        td.usage=MTLTextureUsageRenderTarget|MTLTextureUsageShaderRead;td.storageMode=MTLStorageModePrivate;sceneColor=[self.device newTextureWithDescriptor:td];
        td.usage=MTLTextureUsageRenderTarget;td.textureType=MTLTextureType2DMultisample;td.sampleCount=sceneSamples;sceneMSAA=[self.device newTextureWithDescriptor:td];
        td.pixelFormat=self.depthStencilPixelFormat;sceneDepth=[self.device newTextureWithDescriptor:td];
    }
    MTLRenderPassDescriptor* pass=[MTLRenderPassDescriptor new];
    pass.colorAttachments[0].texture=sceneMSAA;pass.colorAttachments[0].resolveTexture=sceneColor;pass.colorAttachments[0].loadAction=MTLLoadActionClear;pass.colorAttachments[0].storeAction=MTLStoreActionMultisampleResolve;pass.colorAttachments[0].clearColor=self.clearColor;
    pass.depthAttachment.texture=sceneDepth;pass.depthAttachment.loadAction=MTLLoadActionClear;pass.depthAttachment.clearDepth=1;pass.depthAttachment.storeAction=MTLStoreActionDontCare;
    pass.stencilAttachment.texture=sceneDepth;pass.stencilAttachment.loadAction=MTLLoadActionClear;pass.stencilAttachment.clearStencil=0;pass.stencilAttachment.storeAction=MTLStoreActionDontCare;
    id<MTLCommandBuffer> command=[queue commandBuffer];command.label=@"Native course and rider";
    id<MTLRenderCommandEncoder> enc=[command renderCommandEncoderWithDescriptor:pass];
    [enc setDepthStencilState:depth];[enc setCullMode:MTLCullModeNone];
    [enc setTriangleFillMode:wireframe?MTLTriangleFillModeLines:MTLTriangleFillModeFill];
    float fov=riding&&cameraValid?cameraOutput.fov:float(65*M_PI/180);
    float4x4 vp=perspective(view.drawableSize.width/std::max(1.0,view.drawableSize.height),riding?.1f:std::clamp(radius*.0002f,.01f,.5f),fov)*look(eye,forward);
    Uniforms u{vp,matrix_identity_float4x4,{eye.x,eye.y,eye.z,1},{float(normals),0,0,0}};applyFog(u,sky);
    if (!scene->skeleton) drawSky(enc,sky,skyPipeline,skyDepth,u);
    [enc setDepthStencilState:depth];[enc setCullMode:MTLCullModeNone];
    auto scenePalette=[scene paletteAt:animationTime clip:animationIndex playing:playDecoded mirror:mirrorDecoded diagnostic:diagnosticPose];
    drawAsset(enc,scene,scene->skeleton?riderPipeline:terrainPipeline,u,bakedLighting,scenePalette);
    NSString* clipName=@"";
    if (riding) {
        if(collisionAnimator&&!gameplayError){try{
            auto sourcePose=collisionAnimator->poseAt(rider,rider.elapsed);
            bool same=riderMesh->animationBindings&&riderMesh->animationBindings->character==collisionCharacter;
            ssx::AnimationVector scale=same?collisionAnimator->modelScale():ssx::AnimationVector{1,1,1};
            auto pose=same?sourcePose:collisionAnimator->poseForRig(rider,*riderMesh->animationBindings,scale);
            if(!pose)throw std::runtime_error("Original gameplay pose unavailable");
            displayedPalette=[riderMesh paletteForOriginalPose:*pose scale:scale];
            auto state=collisionAnimator->mainAnimationState();
            clipName=rider.isGrinding()?@"Grinding":rider.isCrashing()?[NSString stringWithFormat:@"Crashed · recovery %d%% · hold jump to recover",int(std::clamp(rider.originalCrashControlState().recovery70,0.f,1.f)*100)]:state&&state->animationClass>=18&&state->animationClass<=20?@"Grab":rider.grounded?@"Riding":@"Airborne";
        }catch(const std::exception& error){gameplayError=@(error.what());paused=true;}}
        u.model=matrix_identity_float4x4;u.mvp=vp;
        if(!displayedPalette.empty())drawAsset(enc,riderMesh,riderPipeline,u,false,displayedPalette);
        if(gameplayEffects)gameplayEffects->draw(enc,vp,forward);
    }
    [enc endEncoding];
    // Retro presentation pass: softness sized to a 640-wide framebuffer.
    id<MTLRenderCommandEncoder> post=[command renderCommandEncoderWithDescriptor:present];
    float radius=std::clamp(float(width)/640.f*.55f,.4f,2.f);
    float params[4]={1.f/width,1.f/height,radius,retroFilter?1.f:0.f};
    [post setRenderPipelineState:postPipeline];[post setFragmentTexture:sceneColor atIndex:0];[post setFragmentBytes:params length:sizeof(params) atIndex:0];
    [post drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];[post endEncoding];
    [command presentDrawable:drawable];[command commit];
    if (++frames%15==1) {
        NSString* raceDetail=@"";
        if(rider.hasOriginalRaceSession()){
            const auto& race=rider.originalRaceState();
            int ticks=race.finish.elapsed>=0?race.finish.finishTicks:race.clock.raceTicks;
            int64_t hundredths=int64_t(std::max(0,ticks))*100/60;
            raceDetail=[NSString stringWithFormat:@" · %@ %02lld:%02lld.%02lld · Splits %u/%d",
                race.finish.elapsed>=0?@"Finish":@"Time",hundredths/6000,(hundredths/100)%60,hundredths%100,
                unsigned(std::popcount(race.checkpoints.humanMasks[0])),std::max(0,race.checkpoints.count-1)];
        }
        if (riding) hud.stringValue=[NSString stringWithFormat:
            @"  SSX 3 · Native riding prototype · %@ · %@ · %@\n  %.1f km/h · %@ · Shoulder inputs %X · %@%@\n  WASD / left stick steer, crouch, brake · Hold / release Space or Cross/A jump · Shift or Square/X boost\n  Q/Z/E/X = L1/L2/R1/R2 · R respawn · P pause · V inspect · K reload · H retro filter %@ · Native development build",
            location,controllerName,paused?@"PAUSED":(!active?@"FOCUS PAUSED":@"RUNNING"),rider.speed()*3.6,
            rider.grounded?@"Grounded":@"Airborne",unsigned(riderInput.grabMask),clipName,raceDetail,retroFilter?@"on":@"off"];
        else {
            NSString* detail=@"";
            if (probeCollision) {
                auto hit=scene->collision->raycast({eye.x,eye.y,eye.z},{forward.x,forward.y,forward.z},20000);
                detail=hit.hit?[NSString stringWithFormat:@" · Mesh hit %.1f m",hit.distance]:@" · No mesh hit";
            } else if (scene->skeleton) detail=playDecoded?[NSString stringWithFormat:@" · %@%@",scene->animationClips[animationIndex][@"name"],mirrorDecoded?@" (mirror)":@""]:@" · P clip · T rig · [ ] clips · M mirror";
            hud.stringValue=[NSString stringWithFormat:
                @"  SSX 3 · Native Metal inspection · %@ · %@ · Shoulder inputs %X%@\n  WASD / sticks fly · Drag / right stick look · Space / C up / down · Shift faster · Scroll zoom\n  R reset · F wireframe · N normals · L lighting · B collision · K reload · V riding prototype\n  %.1f, %.1f, %.1f m",
                location,controllerName,unsigned(riderInput.grabMask),detail,eye.x,eye.y,eye.z];
        }
        if(riding&&gameplayError)hud.stringValue=[NSString stringWithFormat:@"  This gameplay path is still being implemented. Press R to restart.\n  %@",gameplayError];
    }
}
@end

@interface AppDelegate : NSObject <NSApplicationDelegate>
@property(strong) NSWindow* window;
@end
@implementation AppDelegate
- (void)applicationDidFinishLaunching:(NSNotification*)note {
    (void)note;
    NSString* location=NSProcessInfo.processInfo.environment[@"SSX_LOCATION"] ?: @"ARA1";
    @try {
        self.window=[[NSWindow alloc] initWithContentRect:NSMakeRect(100,100,1280,800)
                    styleMask:NSWindowStyleMaskTitled|NSWindowStyleMaskClosable|NSWindowStyleMaskMiniaturizable|NSWindowStyleMaskResizable
                    backing:NSBackingStoreBuffered defer:NO];
        self.window.title=@"SSX 3 Metal Engine — Native Riding Prototype";
        EngineView* view=[[EngineView alloc] initWithFrame:self.window.contentView.bounds location:location];
        self.window.contentView=view;
        [self.window makeFirstResponder:view];
        [self.window center];
        [self.window makeKeyAndOrderFront:nil];
        [NSApp activateIgnoringOtherApps:YES];
    } @catch(NSException* error) {
        NSLog(@"Engine startup failed: %@: %@",error.name,error.reason);
        NSAlert* alert=[NSAlert new]; alert.messageText=error.name; alert.informativeText=error.reason;
        [alert runModal]; [NSApp terminate:nil];
    }
}
- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication*)app { (void)app; return YES; }
@end
int main() {
    @autoreleasepool {
        NSApplication* app=NSApplication.sharedApplication;
        [app setActivationPolicy:NSApplicationActivationPolicyRegular];
        NSMenu* menu=[NSMenu new]; NSMenuItem* item=[NSMenuItem new]; [menu addItem:item];
        NSMenu* submenu=[NSMenu new];
        [submenu addItemWithTitle:@"Quit SSX 3 Metal Engine" action:@selector(terminate:) keyEquivalent:@"q"];
        item.submenu=submenu; app.mainMenu=menu;
        AppDelegate* delegate=[AppDelegate new]; app.delegate=delegate;
        [app run];
    }
}
