#import "race_event_asset.h"
#include <cctype>
#include "replay_io.h"
#include <fstream>
#include <iomanip>
#include <sstream>

namespace {
double number(id value,const char* field) {
    if (![value isKindOfClass:NSNumber.class] || CFGetTypeID((__bridge CFTypeRef)value)==CFBooleanGetTypeID() || !std::isfinite([value doubleValue]))
        throw std::runtime_error(std::string("Expected finite number: ")+field);
    return [value doubleValue];
}
int integer(id value,const char* field,int maximum) {
    double n=number(value,field);
    if (n<0||n>maximum||std::floor(n)!=n) throw std::runtime_error(std::string("Invalid integer: ")+field);
    return int(n);
}
uint32_t unsigned32(id value,const char* field) {
    double n=number(value,field);
    if(n<0||n>double(UINT32_MAX)||std::floor(n)!=n) throw std::runtime_error(std::string("Invalid uint32: ")+field);
    return uint32_t(n);
}
ssx::Vec3 vector(id value,const char* field) {
    if (![value isKindOfClass:NSArray.class]||[value count]!=3) throw std::runtime_error(std::string("Expected 3-vector: ")+field);
    return {number(value[0],field),number(value[1],field),number(value[2],field)};
}
std::array<uint8_t,2> stick(id value) {
    if (![value isKindOfClass:NSArray.class]||[value count]!=2) throw std::runtime_error("Each stick needs two byte axes");
    return {uint8_t(integer(value[0],"stick",255)),uint8_t(integer(value[1],"stick",255))};
}
NSDictionary* native(NSDictionary* source) {
    id value=source[@"native"]?:@{};
    if (![value isKindOfClass:NSDictionary.class]) throw std::runtime_error("native must be an object");
    return value;
}
NSArray* array(ssx::Vec3 v) {return @[@(v.x),@(v.y),@(v.z)];}
NSArray* bytes(std::array<uint8_t,2> pair) {return @[@(pair[0]),@(pair[1])];}
NSString* hex(const ssx::ReplayPad& pad) {
    std::ostringstream out;out<<std::hex<<std::setfill('0');for (uint8_t byte:pad.movieBytes()) out<<std::setw(2)<<unsigned(byte);
    return @(out.str().c_str());
}
}
NSDictionary* readNativeRideStart(NSString* path) {
    if(![NSFileManager.defaultManager fileExistsAtPath:path])return nil;
    NSData* data=[NSData dataWithContentsOfFile:path];NSError* error=nil;
    id source=data?[NSJSONSerialization JSONObjectWithData:data options:0 error:&error]:nil;
    if(![source isKindOfClass:NSDictionary.class]||![source[@"native"] isKindOfClass:NSDictionary.class]||
       ![source[@"native"][@"initial"] isKindOfClass:NSDictionary.class])
        throw std::runtime_error("Invalid native riding-start package");
    return source;
}
NativeReplaySpec readNativeReplay(NSString* path) {
    NSData* data=[NSData dataWithContentsOfFile:path];NSError* error=nil;
    id source=data?[NSJSONSerialization JSONObjectWithData:data options:0 error:&error]:nil;
    if (![source isKindOfClass:NSDictionary.class]) throw std::runtime_error("Cannot read replay JSON object");
    int frames=integer(source[@"frames"],"frames",36000);
    id rawEvents=source[@"events"]?:@[];
    if (![rawEvents isKindOfClass:NSArray.class]) throw std::runtime_error("events must be an array");
    std::vector<ssx::ReplayEvent> events;
    for (id entry in rawEvents) {
        if (![entry isKindOfClass:NSDictionary.class]) throw std::runtime_error("event must be an object");
        ssx::ReplayEvent event;event.start=integer(entry[@"start"],"start",frames);event.end=integer(entry[@"end"],"end",frames);
        id buttons=entry[@"buttons"]?:@[];
        if (![buttons isKindOfClass:NSArray.class]) throw std::runtime_error("buttons must be an array");
        for (id button in buttons) {
            if (![button isKindOfClass:NSString.class]) throw std::runtime_error("button names must be strings");
            event.buttons|=ssx::replayButton([button UTF8String]);
        }
        if (entry[@"left"]) event.left=stick(entry[@"left"]);
        if (entry[@"right"]) event.right=stick(entry[@"right"]);
        events.push_back(event);
    }
    id location=native(source)[@"location"]?:@"ARA1";
    if (![location isKindOfClass:NSString.class]||![location length]||[location containsString:@"/"]||[location isEqualToString:@".."])
        throw std::runtime_error("Invalid native location");
    ssx::InputReplay replay(frames,std::move(events));
    id initialConfig=native(source)[@"initial"];
    if(initialConfig&&![initialConfig isKindOfClass:NSDictionary.class])throw std::runtime_error("native.initial must be an object");
    id initialJumpConfig=initialConfig[@"original_jump"];
    if(initialJumpConfig&&![initialJumpConfig isKindOfClass:NSDictionary.class])throw std::runtime_error("original_jump must be an object");
    id initialJump=initialJumpConfig[@"previousHeld"];
    if(initialJump)replay.initialJumpHeld=[initialJump boolValue];
    if(id accepted=native(source)[@"accepted_input"]) {
        if(![accepted isKindOfClass:NSDictionary.class]||integer(accepted[@"version"],"accepted version",1)!=1||integer(accepted[@"frames"],"accepted frames",36000)!=frames)
            throw std::runtime_error("Unsupported accepted-input version or frame count");
        id decoding=accepted[@"decoding"]?:@"verified_controls";
        if(![decoding isKindOfClass:NSString.class]||![@[@"verified_controls",@"runtime_control_state"] containsObject:decoding])throw std::runtime_error("Unsupported original command decoding policy");
        replay.decodeAcceptedAtRuntime=[decoding isEqualToString:@"runtime_control_state"];
        if(replay.decodeAcceptedAtRuntime)replay.expectedInitialControlState=integer(accepted[@"initial_control_state"],"initial controller",255);
        id previous=accepted[@"previous_jump_held"];
        if(!previous||CFGetTypeID((__bridge CFTypeRef)previous)!=CFBooleanGetTypeID())throw std::runtime_error("Accepted previous_jump_held must be boolean");
        replay.initialJumpHeld=[previous boolValue];
        id segments=accepted[@"segments"];
        if(![segments isKindOfClass:NSArray.class]||[segments count]==0)throw std::runtime_error("Accepted input requires segments");
        auto word=[](id text) {
            if(![text isKindOfClass:NSString.class]||![text hasPrefix:@"0x"])throw std::runtime_error("Accepted command must be hexadecimal text");
            const char* begin=[text UTF8String];char* end=nullptr;unsigned long long value=strtoull(begin,&end,16);
            if(end==begin||*end||value>UINT32_MAX)throw std::runtime_error("Invalid accepted command word");
            return uint32_t(value);
        };
        int cursor=0;
        for(id segment in segments) {
            if(![segment isKindOfClass:NSDictionary.class])throw std::runtime_error("Accepted segment must be an object");
            if(replay.decodeAcceptedAtRuntime){if(segment[@"controls"]||segment[@"control_state"])throw std::runtime_error("Raw command segments must not prescribe decoded controls or inferred states");}
            else if(![segment[@"controls"] isKindOfClass:NSDictionary.class])throw std::runtime_error("Accepted segment requires controls object");
            ssx::AcceptedReplayEvent e;e.start=integer(segment[@"start"],"accepted start",frames);e.end=integer(segment[@"end"],"accepted end",frames);
            if(e.start!=cursor||e.end<=e.start)throw std::runtime_error("Accepted segments must cover each frame once in order");
            cursor=e.end;if(!replay.decodeAcceptedAtRuntime)e.controlState=integer(segment[@"control_state"],"control state",255);
            e.word0=word(segment[@"word0"]);e.word1=word(segment[@"word1"]);
            if(e.word0&0xfff)throw std::runtime_error("Accepted word0 must have duration bits removed");
            if(replay.decodeAcceptedAtRuntime){replay.accepted.push_back(e);continue;}
            NSDictionary* c=segment[@"controls"];NSMutableSet* known=[NSMutableSet new];
            auto axis=[&](NSString* key,float& target) {
                [known addObject:key];if(id v=c[key]){double n=number(v,key.UTF8String);if(n< -1||n>1)throw std::runtime_error("Accepted axis outside -1..1");target=float(n);}
            };
            auto flag=[&](NSString* key,bool& target) {
                [known addObject:key];if(id v=c[key]){if(CFGetTypeID((__bridge CFTypeRef)v)!=CFBooleanGetTypeID())throw std::runtime_error("Accepted flag must be boolean");target=[v boolValue];}
            };
            axis(@"turn",e.input.turn);axis(@"crouch",e.input.crouch);axis(@"brake",e.input.brake);axis(@"boardPress",e.input.boardPress);axis(@"boardPivot",e.input.boardPivot);
            axis(@"prewindTurn",e.input.prewindTurn);axis(@"spin",e.input.spin);axis(@"flip",e.input.flip);axis(@"airAdjustLR",e.input.airAdjustLR);axis(@"airAdjustFB",e.input.airAdjustFB);
            flag(@"jumpHeld",e.input.jumpHeld);flag(@"jumpPressed",e.input.jumpPressed);flag(@"boostHeld",e.input.boostHeld);flag(@"boostPressed",e.input.boostPressed);
            flag(@"handplant",e.input.handplant);flag(@"ollieHeld",e.input.ollieHeld);flag(@"recoverPressed",e.input.recoverPressed);flag(@"pausePressed",e.input.pausePressed);
            [known addObject:@"grabMask"];if(c[@"grabMask"])e.input.grabMask=uint8_t(integer(c[@"grabMask"],"grabMask",15));
            for(id key in c)if(![known containsObject:key])throw std::runtime_error("Unknown accepted control field");
            replay.accepted.push_back(e);
        }
        if(cursor!=frames)throw std::runtime_error("Accepted input does not cover the replay");
    }
    return {std::move(replay),source,location};
}
static ssx::OriginalGroundState readOriginalPoseState(NSDictionary* s){
    if(![s isKindOfClass:NSDictionary.class])throw std::runtime_error("Original pose state must be an object");
        auto value=[](NSDictionary* object,NSString* key) {
            float result=float(number(object[key],key.UTF8String));
            if(!std::isfinite(result)) throw std::runtime_error("original_ground scalar exceeds float range");
            return result;
        };
        auto vec=[](NSDictionary* object,NSString* key) {
            auto v=vector(object[key],key.UTF8String);std::array<float,3> result={float(v.x),float(v.y),float(v.z)};
            for(float component:result) if(!std::isfinite(component)) throw std::runtime_error("original_ground vector exceeds float range");
            return result;
        };
        auto control=[&](NSString* key) {
            id c=s[key];if(![c isKindOfClass:NSDictionary.class]) throw std::runtime_error("Ground control must be an object");
            return ssx::GroundControlValue{value(c,@"current"),value(c,@"rate"),value(c,@"target")};
        };
        auto boolean=[](NSDictionary* object,NSString* key) {
            id v=object[key];if(!v||CFGetTypeID((__bridge CFTypeRef)v)!=CFBooleanGetTypeID()) throw std::runtime_error("Ground state flag must be boolean");
            return bool([v boolValue]);
        };
        auto signedInteger=[](id object,const char* name) {
            double n=number(object,name);if(n<INT32_MIN||n>INT32_MAX||std::floor(n)!=n) throw std::runtime_error("Ground integer outside int32 range");
            return int32_t(n);
        };
        ssx::OriginalGroundState state;
        state.position=vec(s,@"position");state.velocity=vec(s,@"velocity");state.normal=vec(s,@"normal");state.forward=vec(s,@"forward");
        state.lateral=vec(s,@"lateral");state.surfaceVelocity=vec(s,@"surface_velocity");state.physicalForward=vec(s,@"physical_forward");
        state.turn=control(@"turn");state.brake=control(@"brake");state.crouch=control(@"crouch");
        state.depth1=value(s,@"depth1");state.depth3=value(s,@"depth3");state.distance=value(s,@"distance");state.timeScale=value(s,@"time_scale");
        state.boost=value(s,@"boost");state.boostWindow=value(s,@"boost_window");state.modeTiming=value(s,@"mode_timing");state.headingOffset=value(s,@"heading_offset");
        state.flags308=unsigned32(s[@"flags308"],"flags308");state.riderType=signedInteger(s[@"rider_type"],"rider_type");
        state.animationIndex=signedInteger(s[@"animation_index"],"animation_index");state.forceHeadingBoost=boolean(s,@"force_heading_boost");state.state320Equals324=boolean(s,@"state320_equals324");
        id quaternion=s[@"quaternion"];
        if(![quaternion isKindOfClass:NSArray.class]||[quaternion count]!=4) throw std::runtime_error("Ground quaternion requires four numbers");
        for(unsigned n=0;n<4;++n) {
            state.quaternion[n]=float(number(quaternion[n],"quaternion"));
            if(!std::isfinite(state.quaternion[n])) throw std::runtime_error("Ground quaternion exceeds float range");
        }
        state.presentationUp=vec(s,@"presentation_up");state.boardUp=vec(s,@"board_up");state.manualSpin=value(s,@"manual_spin");
        state.prewindStyle=signedInteger(s[@"prewind_style"],"prewind_style");
        state.controlState=signedInteger(s[@"control_state"],"control_state");state.reverseStance=boolean(s,@"reverse_stance");
        state.boardBouncePhase=value(s,@"board_bounce_phase");state.boardLift=value(s,@"board_lift");
        state.previousNormal=vec(s,@"previous_normal");state.boardNormal=vec(s,@"board_normal");
        state.balance280=control(@"balance280");state.adjustment28C=control(@"adjustment28c");state.adjustment298=control(@"adjustment298");
        state.animationTurn=control(@"animation_turn");state.extraLean=control(@"extra_lean");
        state.boardAlignment=control(@"board_alignment");state.presentationRoll=control(@"presentation_roll");
        state.presentationLift=control(@"presentation_lift");state.animationClass=signedInteger(s[@"animation_class"],"animation_class");
        state.boostTierCounter=signedInteger(s[@"boost_tier_counter"],"boost_tier_counter");state.boostSpeedFloor=value(s,@"boost_speed_floor");
    return state;
}
void initializeNativeReplay(ssx::PrototypeRider& rider,NSDictionary* source,const ssx::CollisionWorld& terrain,
                           ssx::Vec3 spawn,ssx::Vec3 normal,double heading) {
    NSDictionary* config=native(source);
    id initial=config[@"initial"]?:@{};
    if (![initial isKindOfClass:NSDictionary.class]) throw std::runtime_error("native.initial must be an object");
    if (config[@"spawn"]) {
        auto requested=vector(config[@"spawn"],"native.spawn");
        auto hit=terrain.raycast(requested+ssx::Vec3{0,5,0},{0,-1,0},30);
        if (!hit.hit) throw std::runtime_error("native.spawn has no terrain within projection range");
        spawn=hit.position;normal=hit.normal;heading=std::atan2(normal.x,normal.z);
    }
    auto environment=NSProcessInfo.processInfo.environment;
    if (environment[@"SSX_HEADING"]) {
        NSString* raw=environment[@"SSX_HEADING"];char* end=nullptr;
        heading=strtod(raw.UTF8String,&end);
        if (!end||end==raw.UTF8String||*end||!std::isfinite(heading)) throw std::runtime_error("Invalid SSX_HEADING");
    }
    if (initial[@"heading"]) heading=number(initial[@"heading"],"heading");
    if (initial[@"normal"]) {
        normal=vector(initial[@"normal"],"normal");
        if (ssx::dot(normal,normal)<1e-12) throw std::runtime_error("Initial normal cannot be zero");
    }
    if (ssx::dot(normal,normal)<1e-12) normal={0,1,0};
    rider.reset(spawn,normal,heading);
    bool isHuman=true;
    if(id value=initial[@"is_human"]){
        if(CFGetTypeID((__bridge CFTypeRef)value)!=CFBooleanGetTypeID())throw std::runtime_error("is_human must be boolean");
        isHuman=[value boolValue];
    }
    rider.setDetailedBodyQueries(isHuman);
    if (environment[@"SSX_VELOCITY"]) {
        double x,y,z;char trailing;
        if (sscanf([environment[@"SSX_VELOCITY"] UTF8String],"%lf,%lf,%lf%c",&x,&y,&z,&trailing)!=3 || !std::isfinite(x)||!std::isfinite(y)||!std::isfinite(z))
            throw std::runtime_error("SSX_VELOCITY must be finite x,y,z in meters/second");
        rider.velocity={x,y,z};
    }
    if (initial[@"position"]) {
        rider.position=vector(initial[@"position"],"position"); // Exact state; no implicit offset or projection.
        auto hit=terrain.raycast(rider.position+ssx::Vec3{0,.1,0},{0,-1,0},.2);
        rider.grounded=hit.hit&&hit.normal.y>.2;
        if (hit.hit&&!initial[@"normal"]) rider.normal=hit.normal;
    }
    if (initial[@"velocity"]) rider.velocity=vector(initial[@"velocity"],"velocity");
    if (initial[@"grounded"]) {
        if (CFGetTypeID((__bridge CFTypeRef)initial[@"grounded"])!=CFBooleanGetTypeID()) throw std::runtime_error("grounded must be boolean");
        rider.grounded=[initial[@"grounded"] boolValue];
    }
    if (id raw=initial[@"original_jump"]) {
        if (![raw isKindOfClass:NSDictionary.class]) throw std::runtime_error("original_jump must be an object");
        ssx::OriginalJumpState state;
        auto sourceVector=[&](NSString* key) {
            auto value=vector(raw[key],key.UTF8String);
            std::array<float,3> result={float(value.x),float(value.y),float(value.z)};
            for(float component:result) if(!std::isfinite(component)) throw std::runtime_error("original_jump vector exceeds float range");
            return result;
        };
        state.position=sourceVector(@"position");state.velocity=sourceVector(@"velocity");
        state.normal=sourceVector(@"normal");state.forward=sourceVector(@"forward");
        state.takeoffNormal=sourceVector(@"takeoffNormal");state.boardUp=sourceVector(@"boardUp");
        state.speedLimit=float(number(raw[@"speedLimit"],"speedLimit"));
        state.charge=float(number(raw[@"charge"],"charge"));
        if(!std::isfinite(state.speedLimit)||!std::isfinite(state.charge)) throw std::runtime_error("original_jump scalar exceeds float range");
        state.ticksSinceGroundFocus=unsigned32(raw[@"ticksSinceGroundFocus"],"ticksSinceGroundFocus");
        state.riderState=unsigned32(raw[@"riderState"],"riderState");
        state.flags=uint16_t(integer(raw[@"flags"],"flags",UINT16_MAX));
        state.motionMode=int(integer(raw[@"motionMode"],"motionMode",5));
        id held=raw[@"previousHeld"];
        if(!held||CFGetTypeID((__bridge CFTypeRef)held)!=CFBooleanGetTypeID()) throw std::runtime_error("previousHeld must be boolean");
        id acceptedPrevious=config[@"accepted_input"][@"previous_jump_held"];
        rider.seedOriginalJump(state,acceptedPrevious?[acceptedPrevious boolValue]:[held boolValue]);
    }
    if (id raw=initial[@"original_ground"]) {
        if(![raw isKindOfClass:NSDictionary.class]||![raw[@"profile"] isKindOfClass:NSDictionary.class]||![raw[@"state"] isKindOfClass:NSDictionary.class])
            throw std::runtime_error("original_ground requires profile and state objects");
        NSDictionary* p=raw[@"profile"];NSDictionary* s=raw[@"state"];
        auto value=[](NSDictionary* object,NSString* key) {
            float result=float(number(object[key],key.UTF8String));
            if(!std::isfinite(result)) throw std::runtime_error("original_ground scalar exceeds float range");
            return result;
        };
        auto curve=[&](NSDictionary* object,NSString* key) {
            id list=object[key];ssx::GroundCurve result;
            if(![list isKindOfClass:NSArray.class]||[list count]!=4) throw std::runtime_error("Ground curve requires four points");
            for(unsigned n=0;n<4;++n) {
                id point=list[n];
                if(![point isKindOfClass:NSArray.class]||[point count]!=2) throw std::runtime_error("Ground curve point requires two numbers");
                result[n]={float(number(point[0],"curve x")),float(number(point[1],"curve y"))};
                if(!std::isfinite(result[n].x)||!std::isfinite(result[n].y)||(n&&result[n].x<=result[n-1].x)) throw std::runtime_error("Invalid ground curve ordering/range");
            }
            return result;
        };
        auto boolean=[](NSDictionary* object,NSString* key) {
            id v=object[key];if(!v||CFGetTypeID((__bridge CFTypeRef)v)!=CFBooleanGetTypeID()) throw std::runtime_error("Ground state flag must be boolean");
            return bool([v boolValue]);
        };
        auto signedInteger=[](id object,const char* name) {
            double n=number(object,name);if(n<INT32_MIN||n>INT32_MAX||std::floor(n)!=n) throw std::runtime_error("Ground integer outside int32 range");
            return int32_t(n);
        };
        NSDictionary* surface=p[@"surface"];
        if(![surface isKindOfClass:NSDictionary.class]) throw std::runtime_error("Ground surface must be an object");
        ssx::OriginalGroundProfile profile;
        profile.surface.id=integer(surface[@"id"],"surface id",255);
        profile.surface.gravity=value(surface,@"gravity");profile.surface.lateralDrag=value(surface,@"lateral_drag");
        profile.surface.powderDamping=value(surface,@"powder_damping");profile.surface.slipFriction=curve(surface,@"slip_friction");
        profile.extraLeanCurve=curve(p,@"extra_lean_curve");profile.lateralSpeedCurve=curve(p,@"lateral_speed_curve");profile.turnMinCurve=curve(p,@"turn_min_curve");profile.turnMaxCurve=curve(p,@"turn_max_curve");
        profile.depthTarget1=value(p,@"depth_target1");profile.depthTarget3=value(p,@"depth_target3");profile.maxTurnAngle=value(p,@"max_turn_angle");
        profile.airHeight=value(p,@"air_height");
        profile.autoBoostSpeed=value(p,@"auto_boost_speed");profile.autoBoostFactor=value(p,@"auto_boost_factor");
        profile.bodyScale=value(p,@"body_scale");profile.speedLimit=value(p,@"speed_limit");profile.speedStat=value(p,@"speed_stat");profile.edgeStat=value(p,@"edge_stat");
        profile.alignmentRate=value(p,@"alignment_rate");
        profile.surfaceTerminalVelocity=value(p,@"surface_terminal_velocity");profile.topSpeedStat=value(p,@"top_speed_stat");
        id speedTable=p[@"speed_limit_table"];
        if(![speedTable isKindOfClass:NSArray.class]||[speedTable count]!=48) throw std::runtime_error("Original speed table requires 48 numbers");
        for(unsigned n=0;n<48;++n) {
            profile.speedLimitTable[n]=float(number(speedTable[n],"speed table"));
            if(!std::isfinite(profile.speedLimitTable[n])) throw std::runtime_error("Speed table exceeds float range");
        }
        NSDictionary* h=p[@"heading_profile"];
        if(![h isKindOfClass:NSDictionary.class]) throw std::runtime_error("Ground heading profile must be an object");
        profile.headingProfile.surface28=value(h,@"surface28");profile.headingProfile.surface2C=value(h,@"surface2c");
        profile.headingProfile.surface30=value(h,@"surface30");profile.headingProfile.surface34=value(h,@"surface34");
        profile.headingProfile.crouchTurnCurve=curve(h,@"crouch_turn_curve");profile.headingProfile.crouchSpeedCurve=curve(h,@"crouch_speed_curve");profile.headingProfile.directSteerCurve=curve(h,@"direct_steer_curve");
        if(profile.bodyScale<=0||profile.speedLimit<=0||profile.depthTarget1<=0||profile.depthTarget3<=profile.depthTarget1)
            throw std::runtime_error("Invalid ground profile scale/speed/depth");
        auto state=readOriginalPoseState(s);
        bool active=raw[@"active_physics"]?boolean(raw,@"active_physics"):true;
        if(active)rider.seedOriginalGround(profile,state);
        else{rider.seedOriginalGroundProfile(profile);rider.seedOriginalPoseControls(state);}
        if(id catalog=raw[@"surface_catalog"]) {
            if(![catalog isKindOfClass:NSArray.class]||[catalog count]!=19)throw std::runtime_error("Original ground material catalog requires 19 entries");
            std::array<ssx::OriginalGroundMaterial,19> materials;
            for(unsigned index=0;index<materials.size();++index) {
                NSDictionary* entry=catalog[index];NSDictionary* material=entry[@"surface"];NSDictionary* heading=entry[@"heading_profile"];
                if(![entry isKindOfClass:NSDictionary.class]||![material isKindOfClass:NSDictionary.class]||![heading isKindOfClass:NSDictionary.class])throw std::runtime_error("Invalid original ground material");
                auto& target=materials[index];target.surface.id=int(integer(material[@"id"],"material id",18));
                target.surface.gravity=value(material,@"gravity");target.surface.lateralDrag=value(material,@"lateral_drag");
                target.surface.powderDamping=value(material,@"powder_damping");target.surface.slipFriction=curve(material,@"slip_friction");
                target.depthTarget1=value(entry,@"depth_target1");target.depthTarget3=value(entry,@"depth_target3");target.maxTurnAngle=value(entry,@"max_turn_angle");
                target.airHeight=value(entry,@"air_height");target.autoBoostSpeed=value(entry,@"auto_boost_speed");target.autoBoostFactor=value(entry,@"auto_boost_factor");
                target.alignmentRate=value(entry,@"alignment_rate");target.terminalVelocity=value(entry,@"surface_terminal_velocity");
                target.heading={value(heading,@"surface28"),value(heading,@"surface2c"),value(heading,@"surface30"),value(heading,@"surface34")};
            }
            rider.bindOriginalGroundSurfaceCatalog(std::move(materials));
        }
        if(id properties=raw[@"surface_properties44"]) {
            if(![properties isKindOfClass:NSArray.class]||[properties count]!=19)throw std::runtime_error("Original surface catalog must contain 19 entries");
            std::vector<int> values;for(id property in properties)values.push_back(signedInteger(property,"surface property44"));
            rider.bindOriginalAirSurfaceProperties(std::move(values));
        }
        if(id cache=raw[@"cache"]) {
            if(![cache isKindOfClass:NSDictionary.class]) throw std::runtime_error("Ground cache must be an object");
            ssx::terrain_original::ContactCache c;
            c.valid=boolean(cache,@"valid");c.resource=unsigned32(cache[@"resource"],"cache resource");
            c.cellU=unsigned(integer(cache[@"cell_u"],"cache cell_u",8));c.cellV=unsigned(integer(cache[@"cell_v"],"cache cell_v",8));
            c.half=unsigned(integer(cache[@"half"],"cache half",1));rider.seedOriginalGroundCache(c);
        }
    }
    auto readContactCache=[](id raw,int expectedKind){
        if(![raw isKindOfClass:NSDictionary.class])throw std::runtime_error("Original contact cache must be an object");
        id valid=raw[@"valid"];if(!valid||CFGetTypeID((__bridge CFTypeRef)valid)!=CFBooleanGetTypeID())throw std::runtime_error("Contact cache validity must be boolean");
        ssx::terrain_original::ContactCache cache;cache.valid=[valid boolValue];
        if(cache.valid&&integer(raw[@"query_kind"],"cache query kind",2)!=expectedKind)throw std::runtime_error("Contact cache kind differs from native query");
        cache.resource=unsigned32(raw[@"resource"],"cache resource");cache.cellU=unsigned(integer(raw[@"cell_u"],"cache U",8));cache.cellV=unsigned(integer(raw[@"cell_v"],"cache V",8));cache.half=unsigned(integer(raw[@"half"],"cache half",1));return cache;
    };
    if(id body=initial[@"original_ground"][@"body_cache"])rider.seedOriginalBodyCache(readContactCache(body,isHuman?1:0));
    if(id animation=initial[@"original_animation"]){
        if(![animation isKindOfClass:NSDictionary.class])throw std::runtime_error("Original animation must be an object");
        if(!initial[@"original_ground"]){
            if(animation[@"pose_state"])rider.seedOriginalPoseControls(readOriginalPoseState(animation[@"pose_state"]));
            if(animation[@"contact_cache"])rider.seedOriginalGroundCache(readContactCache(animation[@"contact_cache"],2));
            if(animation[@"body_cache"])rider.seedOriginalBodyCache(readContactCache(animation[@"body_cache"],isHuman?1:0));
        }
    }
    if(id race=initial[@"original_race_event"])rider.seedOriginalRaceSession(ssx::readOriginalRaceEventAsset(race));
    if(id raw=initial[@"original_boost"]){
        if(![raw isKindOfClass:NSDictionary.class]||![raw[@"profile"] isKindOfClass:NSDictionary.class]||![raw[@"state"] isKindOfClass:NSDictionary.class])throw std::runtime_error("Boost requires profile/state objects");
        NSDictionary* p=raw[@"profile"];NSDictionary* s=raw[@"state"];
        auto value=[](id object,const char* label){float v=float(number(object,label));if(!std::isfinite(v))throw std::runtime_error("Boost scalar exceeds float range");return v;};
        auto signedValue=[](id object,const char* label){double v=number(object,label);if(v<INT32_MIN||v>INT32_MAX||std::floor(v)!=v)throw std::runtime_error("Boost integer out of range");return int(v);};
        ssx::OriginalBoostProfile profile{value(p[@"full_threshold"],"full threshold"),value(p[@"medium_threshold"],"medium threshold"),value(p[@"drain_per_tick"],"boost drain"),
            value(p[@"tick_seconds"],"tick seconds"),value(p[@"modifier_threshold"],"modifier threshold"),value(p[@"super_timer_floor"],"timer floor"),value(p[@"normal_decay"],"normal decay"),value(p[@"fast_decay"],"fast decay")};
        ssx::OriginalBoostState state{value(s[@"meter"],"boost meter"),value(s[@"amount"],"boost amount"),value(s[@"window"],"boost window"),signedValue(s[@"tier"],"boost tier"),signedValue(s[@"drain_enabled"],"drain policy"),
            uint8_t(integer(s[@"feedback_flags"],"feedback flags",255)),value(s[@"modifier"],"boost modifier"),value(s[@"super_time"],"super time")};
        rider.seedOriginalBoost(profile,state);
    }
    if(id raw=initial[@"original_landing"]){
        if(![raw isKindOfClass:NSDictionary.class])throw std::runtime_error("Original landing must be an object");
        NSDictionary* p=raw[@"profile"];NSDictionary* r=raw[@"runtime"];
        if(![p isKindOfClass:NSDictionary.class]||![r isKindOfClass:NSDictionary.class])throw std::runtime_error("Landing requires profile/runtime objects");
        auto value=[](id object,const char* label){float f=float(number(object,label));if(!std::isfinite(f))throw std::runtime_error("Landing scalar exceeds float range");return f;};
        auto signedValue=[](id object,const char* label){double v=number(object,label);if(v<INT32_MIN||v>INT32_MAX||std::floor(v)!=v)throw std::runtime_error("Landing integer out of range");return int(v);};
        id materials=p[@"materials"];if(![materials isKindOfClass:NSArray.class]||[materials count]!=19)throw std::runtime_error("Landing requires19 materials");
        ssx::OriginalLandingProfile profile;
        for(unsigned i=0;i<19;++i){id m=materials[i];if(![m isKindOfClass:NSDictionary.class])throw std::runtime_error("Landing material must be an object");
            profile.materials[i]={value(m[@"depth1"],"depth1"),value(m[@"depth3"],"depth3"),value(m[@"normal_impulse_factor"],"normal impulse"),value(m[@"maximum_normal_speed"],"normal speed"),unsigned32(m[@"recovery"],"recovery")};}
        profile.landingStat=value(p[@"landing_stat"],"landing stat");profile.bodyScale=value(p[@"body_scale"],"landing body scale");if(profile.bodyScale<=0)throw std::runtime_error("Landing body scale must be positive");
        ssx::OriginalLandingRuntime runtime;runtime.tick=unsigned32(r[@"tick"],"landing tick");runtime.lastGroundLeaveTick=unsigned32(r[@"last_ground_leave_tick"],"ground leave tick");runtime.groundFocusTick=unsigned32(r[@"ground_focus_tick"],"ground focus tick");
        runtime.manualState330=signedValue(r[@"manual_state330"],"manual state");runtime.animationClass=signedValue(r[@"animation_class"],"animation class");
        id flags=r[@"animation_flags"];if(![flags isKindOfClass:NSString.class]||[flags length]!=18||![flags hasPrefix:@"0x"])throw std::runtime_error("Landing animation flags must be16 hexadecimal digits");
        const char* digits=[flags UTF8String];for(unsigned i=2;i<18;++i)if(!std::isxdigit(static_cast<unsigned char>(digits[i])))throw std::runtime_error("Invalid landing animation flags");
        runtime.animationFlags=strtoull(digits,nullptr,16);rider.seedOriginalLanding(profile,runtime);
    }
    if(id raw=initial[@"original_air_entry"]){
        if(![raw isKindOfClass:NSDictionary.class])throw std::runtime_error("Air entry must be an object");
        NSDictionary* profile=raw[@"profile"];NSDictionary* prewind=raw[@"prewind"];
        if(![profile isKindOfClass:NSDictionary.class]||![prewind isKindOfClass:NSDictionary.class])throw std::runtime_error("Air entry requires profile and prewind objects");
        auto value=[](id object,const char* label){float f=float(number(object,label));if(!std::isfinite(f))throw std::runtime_error("Air entry scalar exceeds float range");return f;};
        auto flag=[](id object){if(!object||CFGetTypeID((__bridge CFTypeRef)object)!=CFBooleanGetTypeID())throw std::runtime_error("Air entry flag must be boolean");return bool([object boolValue]);};
        auto triplet=[&](id object){if(![object isKindOfClass:NSDictionary.class])throw std::runtime_error("Air prewind control must be an object");
            return ssx::GroundControlValue{value(object[@"current"],"prewind current"),value(object[@"rate"],"prewind rate"),value(object[@"target"],"prewind target")};};
        ssx::OriginalAirControlProfile p{value(profile[@"trick_stat"],"trick stat"),flag(profile[@"boost_modifier"]),flag(profile[@"landing_animation"])};
        auto rawPivot=vector(raw[@"pivot"],"air entry pivot");std::array<float,3> pivot={float(rawPivot.x),float(rawPivot.y),float(rawPivot.z)};
        for(float f:pivot)if(!std::isfinite(f))throw std::runtime_error("Air entry pivot exceeds float range");
        rider.seedOriginalAirEntry(p,{triplet(prewind[@"spin"]),triplet(prewind[@"flip"]),value(prewind[@"jump_gate"],"jump gate")},pivot);
    }
    if(id raw=initial[@"original_air_control"]) {
        if(![raw isKindOfClass:NSDictionary.class])throw std::runtime_error("original_air_control must be an object");
        NSDictionary* p=raw[@"profile"];NSDictionary* s=raw[@"state"];NSDictionary* physical=raw[@"physical"];NSDictionary* context=raw[@"context"];
        for(id object in @[p?:NSNull.null,s?:NSNull.null,physical?:NSNull.null,context?:NSNull.null])
            if(![object isKindOfClass:NSDictionary.class])throw std::runtime_error("Air control requires profile/state/physical/context objects");
        auto value=[](NSDictionary* object,NSString* key) {
            float n=float(number(object[key],key.UTF8String));if(!std::isfinite(n))throw std::runtime_error("Air scalar exceeds float range");return n;
        };
        auto boolean=[](NSDictionary* object,NSString* key) {
            id v=object[key];if(!v||CFGetTypeID((__bridge CFTypeRef)v)!=CFBooleanGetTypeID())throw std::runtime_error("Air flag must be boolean");return bool([v boolValue]);
        };
        auto sourceVector=[](id object,const char* label) {
            auto v=vector(object,label);std::array<float,3> result={float(v.x),float(v.y),float(v.z)};
            for(float n:result)if(!std::isfinite(n))throw std::runtime_error("Air vector exceeds float range");return result;
        };
        ssx::OriginalAirControlProfile profile;profile.trickStat=value(p,@"trick_stat");
        profile.boostModifier=boolean(p,@"boost_modifier");profile.landingAnimation=boolean(p,@"landing_animation");
        ssx::OriginalAirControlState state;state.mode=integer(s[@"mode"],"air mode",3);state.phase=integer(s[@"phase"],"air phase",3);
        state.extended=integer(s[@"extended"],"air extended",1);
        for(auto [name,member]:std::initializer_list<std::pair<const char*,float ssx::OriginalAirControlState::*>>{
            {"target_flip",&ssx::OriginalAirControlState::targetFlip},{"target_spin",&ssx::OriginalAirControlState::targetSpin},
            {"progress_flip",&ssx::OriginalAirControlState::progressFlip},{"progress_spin",&ssx::OriginalAirControlState::progressSpin},
            {"total_spin",&ssx::OriginalAirControlState::totalSpin},{"total_flip",&ssx::OriginalAirControlState::totalFlip},
            {"adjust_spin",&ssx::OriginalAirControlState::adjustSpin},{"adjust_flip",&ssx::OriginalAirControlState::adjustFlip},
            {"scored_spin",&ssx::OriginalAirControlState::scoredSpin},{"scored_flip",&ssx::OriginalAirControlState::scoredFlip},
            {"max_spin",&ssx::OriginalAirControlState::maxSpin},{"max_flip",&ssx::OriginalAirControlState::maxFlip},
            {"axis_blend",&ssx::OriginalAirControlState::axisBlend},{"hold_spin",&ssx::OriginalAirControlState::holdSpin},
            {"hold_flip",&ssx::OriginalAirControlState::holdFlip},{"input_angle",&ssx::OriginalAirControlState::inputAngle},
            {"idle_time",&ssx::OriginalAirControlState::idleTime},{"spin_rate",&ssx::OriginalAirControlState::spinRate},{"flip_rate",&ssx::OriginalAirControlState::flipRate}})
            state.*member=value(s,@(name));
        ssx::OriginalAirPresentation pose;pose.position=sourceVector(physical[@"position"],"air physical position");
        id q=physical[@"quaternion"];if(![q isKindOfClass:NSArray.class]||[q count]!=4)throw std::runtime_error("Air physical quaternion requires four numbers");
        for(unsigned n=0;n<4;++n){pose.quaternion[n]=float(number(q[n],"air quaternion"));if(!std::isfinite(pose.quaternion[n]))throw std::runtime_error("Air quaternion exceeds float range");}
        rider.seedOriginalAirControl(profile,state,pose,sourceVector(raw[@"pivot"],"air pivot"),boolean(context,@"no_grab_context"));
        auto signedValue=[](NSDictionary* object,NSString* key) {
            double n=number(object[key],key.UTF8String);if(n<INT32_MIN||n>INT32_MAX||std::floor(n)!=n)throw std::runtime_error("Air integer outside int32 range");return int32_t(n);
        };
        if(id properties=raw[@"surface_properties44"]) {
            if(![properties isKindOfClass:NSArray.class]||[properties count]!=19)throw std::runtime_error("Original surface catalog must contain 19 entries");
            std::vector<int> values;for(id property in properties) {
                double n=number(property,"surface property44");if(n<INT32_MIN||n>INT32_MAX||std::floor(n)!=n)throw std::runtime_error("Invalid surface property44");values.push_back(int(n));
            }
            rider.bindOriginalAirSurfaceProperties(std::move(values));
        }
        if(id t=raw[@"trajectory"]) {
            NSDictionary* c=raw[@"alignment_context"];
            if(![t isKindOfClass:NSDictionary.class]||![c isKindOfClass:NSDictionary.class])throw std::runtime_error("Trajectory requires state and alignment context objects");
            ssx::OriginalAirTrajectory trajectory;
            trajectory.hitPosition=sourceVector(t[@"hit_position"],"trajectory hit position");trajectory.heading=sourceVector(t[@"heading"],"trajectory heading");
            trajectory.normal=sourceVector(t[@"normal"],"trajectory normal");trajectory.apexPosition=sourceVector(t[@"apex_position"],"trajectory apex");
            trajectory.patchId=signedValue(t,@"patch_id");trajectory.patchU=value(t,@"patch_u");trajectory.patchV=value(t,@"patch_v");
            for(auto [name,target]:std::initializer_list<std::pair<NSString*,ssx::OriginalAirState*>>{{@"prediction",&trajectory.prediction},{@"integrated",&trajectory.integrated}}) {
                id data=t[name];if(![data isKindOfClass:NSDictionary.class])throw std::runtime_error("Trajectory integration state must be an object");
                target->position=sourceVector(data[@"position"],"trajectory position");target->velocity=sourceVector(data[@"velocity"],"trajectory velocity");
            }
            trajectory.surface=signedValue(t,@"surface");trajectory.patchFlags=signedValue(t,@"patch_flags");
            trajectory.predictedTime=value(t,@"predicted_time");trajectory.apexTime=value(t,@"apex_time");trajectory.elapsed=value(t,@"elapsed");
            trajectory.integratedTime=value(t,@"integrated_time");trajectory.speedLimit=value(t,@"speed_limit");trajectory.status=signedValue(t,@"status");
            if(trajectory.speedLimit<=0||trajectory.status<0||trajectory.status>3)throw std::runtime_error("Invalid trajectory speed/status");
            ssx::OriginalAirAlignmentContext alignment;
            alignment.normal=sourceVector(c[@"normal"],"alignment normal");alignment.heading=sourceVector(c[@"heading"],"alignment heading");
            alignment.physicalForward=sourceVector(c[@"physical_forward"],"alignment forward");
            alignment.predictedTime=value(c,@"predicted_time");alignment.elapsedTime=value(c,@"elapsed_time");alignment.timeScale=value(c,@"time_scale");alignment.adjustSpin=value(c,@"adjust_spin");
            alignment.trajectoryStatus=signedValue(c,@"trajectory_status");alignment.surfaceIndex=signedValue(c,@"surface_index");alignment.surfaceFlags=signedValue(c,@"surface_flags");
            alignment.surfaceProperty44=signedValue(c,@"surface_property44");alignment.controlState=signedValue(c,@"control_state");alignment.airModeFlag=signedValue(c,@"air_mode_flag");
            rider.seedOriginalAirTrajectory(trajectory,alignment);
        }
    }
}
void writeNativeReplay(NSString* prefix,const NativeReplaySpec& spec,const std::vector<ssx::ReplayRecord>& records) {
    NSMutableArray* rows=[NSMutableArray arrayWithCapacity:records.size()];
    std::ostringstream csv;csv<<std::setprecision(17);
    csv<<"frame,input_frame,time_s,simulation_time_s,position_x,position_y,position_z,velocity_x,velocity_y,velocity_z,speed_mps,heading_rad,normal_x,normal_y,normal_z,grounded,jump_held,jump_pressed,jump_released,jump_charge,paused,obstacle_contact,ps2_buttons,grab_mask,turn,crouch,brake,boost_held,left_x_byte,left_y_byte,right_x_byte,right_y_byte,ps2_pad_hex\n";
    for (const auto& r:records) {
        for (auto v:{r.position,r.velocity,r.normal}) if (!std::isfinite(v.x)||!std::isfinite(v.y)||!std::isfinite(v.z)) throw std::runtime_error("Nonfinite native telemetry");
        NSString* raw=hex(r.pad);
        NSMutableArray* contacts=[NSMutableArray new];
        for (const auto& c:r.obstacleContacts)
            [contacts addObject:@{@"triangle":@(c.hit.source),@"position":array(c.hit.position),@"normal":array(c.hit.normal),
                @"distance_m":@(c.hit.distance),@"origin":array(c.origin),@"probe_height_m":@(c.probeHeight),@"simulation_time_s":@(c.simulationTime),
                @"velocity_before":array(c.velocityBefore),@"velocity_after":array(c.velocityAfter)}];
        id originalGround=NSNull.null;
        if(r.originalGround) {
            const auto& g=*r.originalGround;const auto& d=r.groundDiagnostics;
            auto sourceArray=[](const std::array<float,3>& v){return @[@(v[0]),@(v[1]),@(v[2])];};
            auto control=[](const ssx::GroundControlValue& c){return @{@"current":@(c.current),@"rate":@(c.rate),@"target":@(c.target)};};
            originalGround=@{@"ticks":@(r.groundTicks),@"surface_id":@(r.groundSurfaceId),@"surface_profile_missing":@(r.groundSurfaceProfileMissing),@"position_cm":sourceArray(g.position),@"velocity_cmps":sourceArray(g.velocity),
                @"normal":sourceArray(g.normal),@"forward":sourceArray(g.forward),@"lateral":sourceArray(g.lateral),@"physical_forward":sourceArray(g.physicalForward),
                @"quaternion":@[@(g.quaternion[0]),@(g.quaternion[1]),@(g.quaternion[2]),@(g.quaternion[3])],@"board_up":sourceArray(g.boardUp),
                @"balance280":control(g.balance280),@"adjustment28c":control(g.adjustment28C),@"adjustment298":control(g.adjustment298),
                @"reverse_stance":@(g.reverseStance),@"heading_offset":@(g.headingOffset),@"previous_normal":sourceArray(g.previousNormal),@"board_normal":sourceArray(g.boardNormal),@"presentation_up":sourceArray(g.presentationUp),@"animation_turn":control(g.animationTurn),@"extra_lean":control(g.extraLean),
                @"board_alignment":control(g.boardAlignment),@"presentation_roll":control(g.presentationRoll),@"presentation_lift":control(g.presentationLift),
                @"board_bounce_phase":@(g.boardBouncePhase),@"board_lift_cm":@(g.boardLift),@"animation_index":@(g.animationIndex),@"animation_class":@(g.animationClass),
                @"turn":control(g.turn),@"brake":control(g.brake),@"crouch":control(g.crouch),@"depth1_cm":@(g.depth1),@"depth3_cm":@(g.depth3),
                @"distance_cm":@(g.distance),@"manual_spin":@(g.manualSpin),@"control_state":@(g.controlState),
                @"acceleration_cmps2":sourceArray(d.acceleration),@"normal_acceleration":@(d.normalAcceleration),@"forward_friction":@(d.forwardFriction),
                @"forward_drive":@(d.forwardDrive),@"lateral_acceleration":@(d.lateralAcceleration),@"compression":@(d.compression),
                @"contact_hit":@(r.groundContact.hit),@"contact_point_m":array(r.groundContact.position),@"contact_resource":@(r.groundContact.resource),
                @"contact_surface":@(r.groundContact.surface),@"contact_u":@(r.groundContact.u),@"contact_v":@(r.groundContact.v)};
        }
        auto sourceVec=[](const std::array<float,3>& v){return @[@(v[0]),@(v[1]),@(v[2])];};
        auto sourceQuat=[](const std::array<float,4>& q){return @[@(q[0]),@(q[1]),@(q[2]),@(q[3])];};
        id airControl=NSNull.null,airTrajectory=NSNull.null,airAlignment=NSNull.null,airPresentation=NSNull.null;
        if(r.originalAirControl) {
            const auto& a=*r.originalAirControl;
            airControl=@{@"target_flip":@(a.targetFlip),
                @"target_spin":@(a.targetSpin),
                @"progress_flip":@(a.progressFlip),
                @"progress_spin":@(a.progressSpin),
                @"total_spin":@(a.totalSpin),
                @"total_flip":@(a.totalFlip),
                @"adjust_spin":@(a.adjustSpin),
                @"adjust_flip":@(a.adjustFlip),
                @"scored_spin":@(a.scoredSpin),
                @"scored_flip":@(a.scoredFlip),
                @"max_spin":@(a.maxSpin),
                @"max_flip":@(a.maxFlip),
                @"axis_blend":@(a.axisBlend),
                @"hold_spin":@(a.holdSpin),
                @"hold_flip":@(a.holdFlip),
                @"input_angle":@(a.inputAngle),
                @"idle_time":@(a.idleTime),
                @"spin_rate":@(a.spinRate),
                @"flip_rate":@(a.flipRate),
                @"mode":@(a.mode),
                @"phase":@(a.phase),
                @"extended":@(a.extended)};
        }
        if(r.originalAirTrajectory) {
            const auto& t=*r.originalAirTrajectory;
            airTrajectory=@{@"hit_position":sourceVec(t.hitPosition),@"heading":sourceVec(t.heading),@"normal":sourceVec(t.normal),@"apex_position":sourceVec(t.apexPosition),
                @"patch_id":@(t.patchId),@"patch_u":@(t.patchU),@"patch_v":@(t.patchV),@"surface":@(t.surface),@"patch_flags":@(t.patchFlags),
                @"predicted_time":@(t.predictedTime),@"apex_time":@(t.apexTime),@"elapsed":@(t.elapsed),@"integrated_time":@(t.integratedTime),@"speed_limit":@(t.speedLimit),@"status":@(t.status),
                @"prediction":@{@"position":sourceVec(t.prediction.position),@"velocity":sourceVec(t.prediction.velocity)},
                @"integrated":@{@"position":sourceVec(t.integrated.position),@"velocity":sourceVec(t.integrated.velocity)}};
        }
        if(r.originalAirAlignment) {
            const auto& a=*r.originalAirAlignment;
            airAlignment=@{@"normal":sourceVec(a.normal),@"heading":sourceVec(a.heading),@"physical_forward":sourceVec(a.physicalForward),
                @"predicted_time":@(a.predictedTime),@"elapsed_time":@(a.elapsedTime),@"time_scale":@(a.timeScale),@"adjust_spin":@(a.adjustSpin),
                @"trajectory_status":@(a.trajectoryStatus),@"surface_index":@(a.surfaceIndex),@"surface_flags":@(a.surfaceFlags),@"surface_property44":@(a.surfaceProperty44),
                @"control_state":@(a.controlState),@"air_mode_flag":@(a.airModeFlag)};
        }
        if(r.originalAirPresentation) {
            const auto& p=*r.originalAirPresentation;
            airPresentation=@{@"position_cm":sourceVec(p.position),@"quaternion":sourceQuat(p.quaternion)};
        }
        id prewind=NSNull.null;
        if(r.originalAirPrewind){
            auto control=[](const ssx::GroundControlValue& c){return @{@"current":@(c.current),@"rate":@(c.rate),@"target":@(c.target)};};
            const auto& p=*r.originalAirPrewind;prewind=@{@"spin":control(p.spin),@"flip":control(p.flip),@"jump_gate":@(p.jumpGate)};
        }
        NSDictionary* originalAir=@{@"prewind":prewind,@"control":airControl,@"trajectory":airTrajectory,@"alignment":airAlignment,@"presentation":airPresentation,
            @"physical_quaternion":sourceQuat(r.airPhysicalQuaternion),@"angular_supported":@(r.airAngularSupported),
            @"physical_orientation_pending":@(r.airPhysicalOrientationPending),@"control_ticks":@(r.airControlTicks)};
        id originalLanding=NSNull.null;
        if(r.originalLanding){const auto& l=*r.originalLanding;const auto& h=r.landingContact;
            originalLanding=@{@"tick":@(l.tick),@"last_ground_leave_tick":@(l.lastGroundLeaveTick),@"ground_focus_tick":@(l.groundFocusTick),
                @"last_landing_tick":@(r.lastLandingTick),@"transition_pending":@(r.landingPending),@"score_event_pending":@(r.landingScorePending),
                @"probe_complete":@(h.complete),@"probe_fraction":@(h.fraction),@"probe_position_cm":sourceVec(h.position),@"probe_normal":sourceVec(h.normal),
                @"probe_surface_velocity_cmps":sourceVec(h.surfaceVelocityCmps),@"surface":@(h.surface),@"patch_id":@(h.patchId),@"instance":@(h.instance),@"has_instance":@(h.hasInstance)};
        }
        id originalBoost=NSNull.null;
        if(r.originalBoost){const auto& b=*r.originalBoost;const auto& e=r.boostEffects;
            originalBoost=@{@"meter":@(b.meter),@"amount":@(b.amount),@"window":@(b.window),@"tier":@(b.tier),@"drain_enabled":@(b.drainEnabled),
                @"feedback_flags":@(b.feedbackFlags),@"modifier":@(b.modifier),@"super_time":@(b.superTime),
                @"started":@(e.started),@"denied":@(e.denied),@"stopped":@(e.stopped),@"timer_expired":@(e.timerExpired),
                @"event_callbacks_pending":@(e.started||e.denied||e.stopped||e.timerExpired)};
        }
        id originalRace=NSNull.null;
        if(r.originalRace){
            const auto& race=*r.originalRace;const auto& c=race.clock;const auto& p=race.progress;const auto& f=race.finish;
            NSMutableArray* unhandled=[NSMutableArray new];for(const auto& e:race.unhandledEvents)
                [unhandled addObject:@{@"type":@(e.type),@"value":@(e.value),@"start":@(e.start),@"end":@(e.end)}];
            originalRace=@{@"clock":@{@"phase":@(int(c.phase)),@"previous":@(int(c.previous)),@"previous_handler":@(int(c.previousHandler)),
                    @"total_ticks":@(c.totalTicks),@"race_ticks":@(c.raceTicks),@"countdown_ticks":@(c.countdownTicks),@"race_enabled":@(c.raceEnabled),@"pre_race_local":@(c.preRaceLocal)},
                @"human":@{@"path_index":@(p.pathIndex),@"remaining":@(p.remaining),@"best_remaining":@(p.bestRemaining),
                    @"path_cache":@{@"origin":sourceVec(p.cache.origin),@"distance":@(p.cache.distance),@"segment":@(p.cache.segment)},
                    @"finish_elapsed":@(f.elapsed),@"finish_ticks":@(f.finishTicks),@"penalty_ticks":@(f.penaltyTicks)},
                @"checkpoint_mask":@(race.checkpoints.humanMasks[0]),@"pending_human_mask":@(race.checkpoints.pendingHumanMask),
                @"race_start_notification":@(race.clockEffects.raceStartNotification),@"request_results":@(race.clockEffects.requestResults),
                @"finished_this_tick":@(race.courseEffects.finished),@"unhandled_events":unhandled,@"opponents_simulated":@NO};
        }
        id acceptedCommand=NSNull.null;
        if(r.accepted) {
            const auto& a=*r.accepted;const auto& c=r.input;
            acceptedCommand=@{@"word0":[NSString stringWithFormat:@"0x%08x",a.word0],@"word1":[NSString stringWithFormat:@"0x%08x",a.word1],@"control_state":@(a.controlState),
                @"controls":@{@"turn":@(c.turn),@"crouch":@(c.crouch),@"brake":@(c.brake),@"boardPress":@(c.boardPress),@"boardPivot":@(c.boardPivot),
                    @"prewindTurn":@(c.prewindTurn),@"spin":@(c.spin),@"flip":@(c.flip),@"airAdjustLR":@(c.airAdjustLR),@"airAdjustFB":@(c.airAdjustFB),
                    @"jumpHeld":@(c.jumpHeld),@"jumpPressed":@(c.jumpPressed),@"boostHeld":@(c.boostHeld),@"boostPressed":@(c.boostPressed),
                    @"handplant":@(c.handplant),@"ollieHeld":@(c.ollieHeld),@"recoverPressed":@(c.recoverPressed),@"pausePressed":@(c.pausePressed),@"grabMask":@(c.grabMask)}};
        }
        NSMutableArray* unsupported=[NSMutableArray new];for(auto resource:r.bodyQuery.unsupportedInstances)[unsupported addObject:@(resource)];
        id bodyHit=NSNull.null;
        if(r.bodyQuery.best.hit) {
            const auto& h=r.bodyQuery.best;
            bodyHit=@{@"instance":@(h.instance),@"node":@(h.node),@"triangle":@(h.triangle),@"surface":@(h.surface),@"terrain":@(h.terrain),
                @"penetration_cm":@(h.penetrationCm),@"point_cm":@[@(h.pointCm[0]),@(h.pointCm[1]),@(h.pointCm[2])],@"normal":@[@(h.normal[0]),@(h.normal[1]),@(h.normal[2])]};
        }
        NSMutableArray* spheres=[NSMutableArray new];
        if(r.bodyVolume)for(unsigned n=0;n<r.bodyVolume->count;++n) {
            const auto& sphere=r.bodyVolume->spheres[n];
            [spheres addObject:@{@"index":@(n),@"center_cm":@[@(sphere.centerCm[0]),@(sphere.centerCm[1]),@(sphere.centerCm[2])],@"radius_cm":@(sphere.radiusCm)}];
        }
        NSDictionary* bodyCollision=@{@"missing_world":@(r.bodyMissingWorld),@"missing_pose":@(r.bodyMissingPose),@"orientation_response_pending":@(r.bodyOrientationPending),@"event_response_pending":@(r.bodyEventPending),
            @"unsupported_instances":unsupported,@"candidate_count":@(r.bodyQuery.candidates),@"contact_count":@(r.bodyQuery.contacts),@"hit":bodyHit,@"spheres":spheres};
        [rows addObject:@{@"frame":@(r.frame),@"input_frame":@(r.inputFrame),@"time_s":@(double(r.frame)/60),@"simulation_time_s":@(r.simulationTime),
            @"position":array(r.position),@"velocity":array(r.velocity),@"normal":array(r.normal),@"speed_mps":@(r.speed),@"heading_rad":@(r.heading),
            @"airborne_ticks":@(r.airborneTicks),@"air_speed_capped":@(r.airSpeedCapped),@"grounded":@(r.grounded),@"control_state":@(r.riderControlState),@"motion_mode":@(r.motionMode),@"crash_phase":@(r.crashPhase),@"crash_submode":@(r.crashSubmode),@"crash_recovery":@(r.crashRecovery),@"reset_requested":@(r.resetRequested),@"jump_held":@(r.input.jumpHeld),@"jump_pressed":@(r.input.jumpPressed),@"jump_released":@(r.jumpReleased),
            @"jump_charge":@(r.jumpCharge),@"paused":@(r.paused),@"obstacle_contact":@(r.obstacleContact),@"obstacle_contacts":contacts,
            @"original_ground":originalGround,@"original_air":originalAir,@"original_race":originalRace,@"original_boost":originalBoost,@"original_landing":originalLanding,
            @"accepted_command":acceptedCommand,
            @"body_collision":bodyCollision,
            @"ps2_buttons":@(r.pad.buttons),@"grab_mask":@(r.input.grabMask),@"turn":@(r.input.turn),@"crouch":@(r.input.crouch),@"brake":@(r.input.brake),
            @"boost_held":@(r.input.boostHeld),@"left":bytes(r.pad.left),@"right":bytes(r.pad.right),@"ps2_pad_hex":raw}];
        csv<<r.frame<<','<<r.inputFrame<<','<<double(r.frame)/60<<','<<r.simulationTime<<','<<r.position.x<<','<<r.position.y<<','<<r.position.z<<','
           <<r.velocity.x<<','<<r.velocity.y<<','<<r.velocity.z<<','<<r.speed<<','<<r.heading<<','<<r.normal.x<<','<<r.normal.y<<','<<r.normal.z<<','
           <<r.grounded<<','<<r.input.jumpHeld<<','<<r.input.jumpPressed<<','<<r.jumpReleased<<','<<r.jumpCharge<<','<<r.paused<<','<<r.obstacleContact<<','
           <<r.pad.buttons<<','<<unsigned(r.input.grabMask)<<','<<r.input.turn<<','<<r.input.crouch<<','<<r.input.brake<<','<<r.input.boostHeld<<','
           <<unsigned(r.pad.left[0])<<','<<unsigned(r.pad.left[1])<<','<<unsigned(r.pad.right[0])<<','<<unsigned(r.pad.right[1])<<','<<raw.UTF8String<<'\n';
    }
    NSDictionary* result=@{@"version":@1,@"physics":@"Recovered source-float ground/launch/air math when original profile is present; full gameplay fidelity still under validation",@"fps":@60,@"physics_hz":@60,@"airborne_hz":@60,@"airborne_model":@"Recovered0x1139A0 source-float step and114298 launch; collision/event integration remains incomplete.",
        @"input_source":spec.input.accepted.empty()?@"requested_pad_timeline":(spec.input.decodeAcceptedAtRuntime?@"original_raw_commands":@"original_accepted_commands"),
        @"location":spec.location,@"frames":@(spec.input.frames),@"units":@"meters, meters/second, radians; Y up",
        @"frame_semantics":@"Row0 is initial state. RowN+1 follows input_frameN. Events are [start,end); overlapping buttons union, last active axis pair wins.",
        @"analog_normalization":@"Original PS2 axial response/deadzones followed by original signed six-bit gameplay quantization. Accepted-command controls bypass device normalization.",
        @"environment":@{@"SSX_SPAWN":NSProcessInfo.processInfo.environment[@"SSX_SPAWN"]?:@"",@"SSX_VELOCITY":NSProcessInfo.processInfo.environment[@"SSX_VELOCITY"]?:@"",@"SSX_HEADING":NSProcessInfo.processInfo.environment[@"SSX_HEADING"]?:@""},
        @"spec":spec.source,@"records":rows};
    NSError* error=nil;
    NSData* json=[NSJSONSerialization dataWithJSONObject:result options:NSJSONWritingPrettyPrinted|NSJSONWritingSortedKeys error:&error];
    NSString* directory=[prefix stringByDeletingLastPathComponent];
    if (directory.length&&![NSFileManager.defaultManager createDirectoryAtPath:directory withIntermediateDirectories:YES attributes:nil error:&error])
        throw std::runtime_error("Cannot create telemetry directory");
    if (!json||![json writeToFile:[prefix stringByAppendingString:@".json"] options:NSDataWritingAtomic error:&error]) throw std::runtime_error("Cannot write telemetry JSON");
    auto csvText=csv.str();
    NSData* text=[NSData dataWithBytes:csvText.data() length:csvText.size()];
    if (![text writeToFile:[prefix stringByAppendingString:@".csv"] options:NSDataWritingAtomic error:&error]) throw std::runtime_error("Cannot write telemetry CSV");
}
