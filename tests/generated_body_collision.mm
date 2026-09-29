#import <Foundation/Foundation.h>
#include "../engine/rider_animation_player.h"
#include "../engine/world_collision_asset.h"
#include <iostream>
#include <numeric>
int main(int argc,char**argv){@autoreleasepool {
    if(argc!=5)return 2;auto json=[](NSString*path){return [NSJSONSerialization JSONObjectWithData:[NSData dataWithContentsOfFile:path] options:0 error:nil];};
    auto rig=std::make_shared<ssx::AnimationRigAsset>(ssx::loadOriginalAnimationRig(@(argv[1])));NSDictionary*package=json(@(argv[2]));auto world=loadWorldBodyCollision(@(argv[2]),package[@"source_sha256"]);NSArray*inputs=json(@(argv[3]));NSMutableArray*reports=[NSMutableArray array];
    auto vec=[](NSArray*a){return ssx::terrain_original::Vector{[a[0]floatValue],[a[1]floatValue],[a[2]floatValue]};};
    for(NSDictionary*item in inputs){
        NSDictionary*p=item[@"pose_inputs"],*g=item[@"ground_inputs"],*b=item[@"reference_body"];
        if(p[@"bones"])throw std::runtime_error("Captured bones must never enter the native pose factory");
        ssx::OriginalGroundState state;state.position=vec(g[@"position"]);state.velocity=vec(g[@"velocity"]);state.normal=vec(g[@"normal"]);state.lateral=vec(g[@"lateral"]);state.controlState=[g[@"control_state"]intValue];state.animationIndex=[g[@"animation_index"]intValue];
        for(unsigned k=0;k<4;++k)state.quaternion[k]=[g[@"quaternion"][k]floatValue];
        state.animationSelectionSupported=true;state.turn.current=[g[@"turn"][@"current"]floatValue];state.brake.current=[g[@"brake"][@"current"]floatValue];state.crouch.current=[g[@"crouch"][@"current"]floatValue];
        ssx::PrototypeRider rider;rider.seedOriginalGround({},state);rider.grounded=true;
        auto animation=ssx::makeOriginalRiderAnimation(rig,p);auto provider=animation->bodyProvider();auto generated=provider(rider,0);if(!generated)throw std::runtime_error("Verified neutral pose provider rejected fixture");
        ssx::BodyCollisionVolume reference;reference.broadCenterCm=vec(b[@"broad_center_cm"]);reference.broadRadiusCm=[b[@"broad_radius_cm"]floatValue];reference.activeMask=[b[@"mask"]unsignedIntValue];NSArray*spheres=b[@"spheres"];reference.count=unsigned(spheres.count);
        for(unsigned i=0;i<reference.count;++i)reference.spheres[i]={vec(spheres[i][@"source_center_cm"]),[spheres[i][@"radius_cm"]floatValue],[spheres[i][@"bone"]unsignedIntValue]};
        float maxCenter=0;unsigned exactSpheres=0;
        for(unsigned i=0;i<reference.count;++i){bool exact=true;for(unsigned k=0;k<3;++k){float error=std::abs(reference.spheres[i].centerCm[k]-generated->spheres[i].centerCm[k]);maxCenter=std::max(maxCenter,error);exact&=error==0;}exactSpheres+=exact;}
        bool broadExact=reference.broadCenterCm==generated->broadCenterCm&&reference.broadRadiusCm==generated->broadRadiusCm;
        unsigned queries=0,hits=0,hitDisagreements=0,resourceDisagreements=0,unsupported=0;float maxPoint=0,maxDepth=0,maxNormal=0;
        auto shifted=[](ssx::BodyCollisionVolume body,ssx::terrain_original::Vector offset){ssx::terrain_original::Rounding rounding;for(unsigned k=0;k<3;++k){body.broadCenterCm[k]=ssx::terrain_original::add(body.broadCenterCm[k],offset[k]);for(unsigned i=0;i<body.count;++i)body.spheres[i].centerCm[k]=ssx::terrain_original::add(body.spheres[i].centerCm[k],offset[k]);}return body;};
        auto compare=[&](ssx::terrain_original::Vector offset,const std::vector<uint32_t>&filter){
            world->resetSphereTreeCache();auto a=world->query(shifted(*generated,offset),state.normal,&filter,true);world->resetSphereTreeCache();auto e=world->query(shifted(reference,offset),state.normal,&filter,true);++queries;
            if(!a.complete()||!e.complete()){++unsupported;return;}if(a.best.hit!=e.best.hit){++hitDisagreements;return;}if(!a.best.hit)return;++hits;
            if(a.best.instance!=e.best.instance||a.best.node!=e.best.node){++resourceDisagreements;return;}
            maxDepth=std::max(maxDepth,std::abs(a.best.penetrationCm-e.best.penetrationCm));for(unsigned k=0;k<3;++k){maxPoint=std::max(maxPoint,std::abs(a.best.pointCm[k]-e.best.pointCm[k]));maxNormal=std::max(maxNormal,std::abs(a.best.normal[k]-e.best.normal[k]));}
        };
        std::vector<uint32_t> nearby;for(NSNumber*i in item[@"nearby_instances"])nearby.push_back(i.unsignedIntValue);compare({},nearby);unsigned capturedSceneQueries=queries;
        // Stress authored nearby resources by translating both bodies identically;
        // these offsets are comparison probes, never native gameplay inputs.
        std::vector<const ssx::WorldCollisionInstance*> targets;for(const auto&i:world->instances)if(i.type&&i.unsupported.empty()&&std::any_of(i.nodes.begin(),i.nodes.end(),[](const auto&n){return n.surface!=-1;}))targets.push_back(&i);
        auto distance=[&](const auto* i){double d=0;for(unsigned k=0;k<3;++k){double x=(i->low[k]+i->high[k])*.5-reference.broadCenterCm[k];d+=x*x;}return d;};
        std::sort(targets.begin(),targets.end(),[&](auto a,auto b){return distance(a)<distance(b);});if(targets.size()>15)targets.resize(15);
        for(auto target:targets){ssx::terrain_original::Vector base;for(unsigned k=0;k<3;++k)base[k]=float((double(target->low[k])+target->high[k])*.5-reference.broadCenterCm[k]);std::vector<uint32_t> filter{target->resource};
            for(unsigned axis=0;axis<3;++axis)for(int step=-10;step<=10;++step){auto delta=base;delta[axis]+=step*20;compare(delta,filter);}
        }
        NSDictionary*report=@{@"name":item[@"name"],@"native_generated":@YES,@"captured_bones_as_input":@NO,@"exact_sphere_centers":@(exactSpheres),@"sphere_count":@(reference.count),@"broad_sphere_exact":@(broadExact),@"maximum_sphere_center_error_cm":@(maxCenter),@"queries":@(queries),@"captured_scene_queries":@(capturedSceneQueries),@"matched_contacts":@(hits),@"hit_disagreements":@(hitDisagreements),@"resource_or_node_disagreements":@(resourceDisagreements),@"unsupported_queries":@(unsupported),@"max_point_error_cm":@(maxPoint),@"max_normal_error":@(maxNormal),@"max_penetration_error_cm":@(maxDepth)};[reports addObject:report];
        std::cout<<[item[@"name"]UTF8String]<<": "<<exactSpheres<<'/'<<reference.count<<" exact sphere centers, maximum errorcm "<<maxCenter<<", "<<queries<<" scene/probe queries, "<<hits<<" contacts, "<<hitDisagreements<<" hit and "<<resourceDisagreements<<" selection disagreements; depthcm "<<maxDepth<<'\n';
    }
    NSData*out=[NSJSONSerialization dataWithJSONObject:reports options:NSJSONWritingPrettyPrinted error:nil];if(![out writeToFile:@(argv[4]) atomically:YES])return 3;
}}
