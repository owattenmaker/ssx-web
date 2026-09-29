#import <Foundation/Foundation.h>
#include "ps2_runtime_macros.h"
#include "../engine/landing_contact.hpp"
#include "../engine/world_collision_asset.h"
#include <iostream>
#include <cstring>
#define DECL(n) void n(uint8_t*,R5900Context*,PS2Runtime*)
DECL(sub_00334458_0x334458);DECL(sub_0032F650_0x32f650);DECL(sub_0032F708_0x32f708);DECL(sub_00329910_0x329910);DECL(sub_00329DC8_0x329dc8);DECL(sub_00329A28_0x329a28);DECL(sub_00329B90_0x329b90);DECL(sub_0032B2B8_0x32b2b8);DECL(sub_00329590_0x329590);DECL(sub_0032A1C0_0x32a1c0);DECL(sub_0032AA28_0x32aa28);DECL(sub_00356020_0x356020);DECL(sub_0035FE10_0x35fe10);DECL(sub_003568B0_0x3568b0);DECL(sub_0013A7B0_0x13a7b0);DECL(sub_003342D0_0x3342d0);DECL(sub_00333EF8_0x333ef8);DECL(sub_003279D0_0x3279d0);DECL(sub_00327AC0_0x327ac0);DECL(sub_00327B30_0x327b30);DECL(sub_00327BB0_0x327bb0);DECL(sub_00327C00_0x327c00);DECL(sub_00327C68_0x327c68);DECL(sub_00391418_0x391418);DECL(sub_00391480_0x391480);DECL(sub_003914F8_0x3914f8);DECL(sub_00340970_0x340970);DECL(sub_0032B6E0_0x32b6e0);DECL(sub_0032E100_0x32e100);DECL(sub_003E6574_0x3e6574);DECL(sub_00334888_0x334888);DECL(sub_003A6CC8_0x3a6cc8);DECL(sub_003A6D00_0x3a6d00);DECL(sub_0032C0F8_0x32c0f8);DECL(sub_0032B6A8_0x32b6a8);DECL(sub_003A6CD8_0x3a6cd8);DECL(sub_003A6CE8_0x3a6ce8);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x420000,g_ps2RecompiledFunctionTableSlotCount=(0x420000-0x100000)/4;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[(0x420000-0x100000)/4]={};
int main(int argc,char**argv){@autoreleasepool {
    using namespace ssx;using namespace terrain_original;
    if(argc!=4)return 2;auto json=[](NSString*p){return [NSJSONSerialization JSONObjectWithData:[NSData dataWithContentsOfFile:p] options:0 error:nil];};
    NSString* folder=@(argv[1]);NSDictionary* terrainData=json([folder stringByAppendingPathComponent:@"terrain.json"]);std::vector<TerrainPatch> patches;
    for(NSDictionary*d in terrainData[@"patches"]){TerrainPatch p;p.resource=[d[@"resource_id"]unsignedIntValue];p.authoredFlags=[d[@"authored_flags"]unsignedIntValue];p.authoredSurface=[d[@"authored_surface_id"]intValue];p.hasAuthoredBounds=true;
        auto vec=[](NSArray*a){return Vec3{[a[0]doubleValue],[a[1]doubleValue],[a[2]doubleValue]};};p.authoredMinimum=vec(d[@"authored_bounds_min"]);p.authoredMaximum=vec(d[@"authored_bounds_max"]);for(unsigned i=0;i<16;++i)p.coefficients[i]=vec(d[@"coefficients"][i]);patches.push_back(p);}
    CollisionWorld terrain({});terrain.setTerrainPatches(patches);auto world=loadWorldBodyCollision([folder stringByAppendingPathComponent:@"world_collision.json"],terrainData[@"source_sha256"],[folder stringByAppendingPathComponent:@"terrain.json"]);
    PS2Runtime runtime;runtime.memory().initialize();runtime.setMissingFunctionPolicy(PS2Runtime::MissingFunctionPolicy::Stop);
    auto reg=[&](uint32_t pc,PS2Runtime::RecompiledFunction fn){if(!runtime.registerFunction(pc,fn))throw std::runtime_error("Landing oracle registration");};
#define REG(pc,name) reg(pc,sub_##name)
    REG(0x356128,00356020_0x356020);REG(0x360990,0035FE10_0x35fe10);REG(0x356ae0,003568B0_0x3568b0);REG(0x356a28,003568B0_0x3568b0);REG(0x3342d0,003342D0_0x3342d0);REG(0x333ef8,00333EF8_0x333ef8);REG(0x3279d0,003279D0_0x3279d0);REG(0x327ac0,00327AC0_0x327ac0);REG(0x327b30,00327B30_0x327b30);REG(0x327bb0,00327BB0_0x327bb0);REG(0x327c00,00327C00_0x327c00);REG(0x327c68,00327C68_0x327c68);REG(0x391418,00391418_0x391418);REG(0x391480,00391480_0x391480);REG(0x3914f8,003914F8_0x3914f8);REG(0x340a10,00340970_0x340970);REG(0x32b6e0,0032B6E0_0x32b6e0);REG(0x3e6574,003E6574_0x3e6574);REG(0x334888,00334888_0x334888);REG(0x3a6cc8,003A6CC8_0x3a6cc8);REG(0x3a6d00,003A6D00_0x3a6d00);REG(0x32c0f8,0032C0F8_0x32c0f8);REG(0x32b6a8,0032B6A8_0x32b6a8);REG(0x3a6cd8,003A6CD8_0x3a6cd8);REG(0x3a6ce8,003A6CE8_0x3a6ce8);
    for(uint32_t pc:{0x32e100,0x32e398,0x32e4b8,0x32e4d0,0x32e5e8,0x32e688,0x32e690,0x32e288,0x32e9a0})reg(pc,sub_0032E100_0x32e100);
    REG(0x329910,00329910_0x329910);REG(0x329dc8,00329DC8_0x329dc8);REG(0x329a28,00329A28_0x329a28);REG(0x329b90,00329B90_0x329b90);REG(0x32b2b8,0032B2B8_0x32b2b8);REG(0x329590,00329590_0x329590);REG(0x32a1c0,0032A1C0_0x32a1c0);REG(0x32aa28,0032AA28_0x32aa28);
    for(uint32_t pc:{0x32f760,0x32f840,0x32f8b0,0x32f8c0,0x32f8f0,0x32f990,0x32fa30,0x32fac0})reg(pc,sub_0032F708_0x32f708);
    REG(0x334458,00334458_0x334458);
    NSArray*samples=json(@(argv[2]));unsigned cases=0,hits=0,unsupported=0;
    for(NSDictionary*sample in samples){NSData* ee=[NSData dataWithContentsOfFile:sample[@"ee"]];std::vector<uint8_t> original((uint8_t*)ee.bytes,(uint8_t*)ee.bytes+ee.length);NSData*vu=[NSData dataWithContentsOfFile:sample[@"vu"]];std::memcpy(runtime.memory().getVU0Code(),vu.bytes,vu.length);
        NSArray* bindings=json(@(argv[3]))[@"instances"];
        for(NSDictionary* binding in bindings)for(Vector displacement:{Vector{0,0,0},{100,0,0},{-100,0,0},{0,100,0},{0,-100,0},{0,0,100},{0,0,-100},{1000,0,0}}){
            auto m=original;auto get=[&](uint32_t at){uint32_t x;std::memcpy(&x,m.data()+at,4);return x;};auto put=[&](uint32_t at,const auto&x){std::memcpy(m.data()+at,&x,sizeof(x));};
            constexpr uint32_t rider=0x14701a0,query=0x80000,output=0x90000,done=0x12345678;
            unsigned address=[binding[@"instance_address"]unsignedIntValue],resource=[binding[@"resource"]unsignedIntValue];if(get(address+0x78)!=resource)throw std::runtime_error("Pickup binding moved in source sample");
            auto found=std::find_if(world->instances.begin(),world->instances.end(),[&](const auto&i){return i.resource==resource;});if(found==world->instances.end()||!found->unsupported.empty())throw std::runtime_error("Unsupported authored pickup");
            WorldBodyCollision isolated;isolated.instances.push_back(*found);
            uint32_t shape=get(rider+0xaa0);BodyCollisionVolume body;std::memcpy(&body.broadCenterCm,m.data()+shape+16,12);std::memcpy(&body.broadRadiusCm,m.data()+shape+32,4);body.activeMask=get(shape+40);body.count=get(shape+44);
            Vector offset;{Rounding round;for(unsigned k=0;k<3;k++){float target=add(mul(add(found->low[k],found->high[k]),.5f),displacement[k]);offset[k]=sub(target,body.broadCenterCm[k]);body.broadCenterCm[k]=add(body.broadCenterCm[k],offset[k]);}}
            put(shape+16,body.broadCenterCm);
            for(unsigned i=0;i<body.count;i++){std::memcpy(&body.spheres[i].centerCm,m.data()+shape+48+i*32,12);std::memcpy(&body.spheres[i].radiusCm,m.data()+shape+64+i*32,4);{Rounding round;for(unsigned k=0;k<3;k++)body.spheres[i].centerCm[k]=add(body.spheres[i].centerCm[k],offset[k]);}put(shape+48+i*32,body.spheres[i].centerCm);}
            auto actual=isolated.query(body,{0,0,1},nullptr,false,1);if(!actual.complete())throw std::runtime_error("Incomplete native pickup query");
            uint32_t nearby=get(rider+0x860);put(nearby+8,1u);put(nearby+12,address);
            R5900Context c{};c.vu0_vf[0]=_mm_set_ps(1,0,0,0);c.pc=0x32f650;SET_GPR_U32(&c,29,0x70000);SET_GPR_U32(&c,31,done);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,4,query);SET_GPR_U32(&c,5,shape);SET_GPR_U32(&c,6,1);
            {Rounding round;sub_0032F650_0x32f650(m.data(),&c,&runtime);}if(c.pc!=done)return 3;
            c.pc=0x334458;SET_GPR_U32(&c,4,nearby);SET_GPR_U32(&c,5,query);SET_GPR_U32(&c,6,output);SET_GPR_U32(&c,7,64);SET_GPR_U32(&c,8,0);SET_GPR_U32(&c,31,done);
            {Rounding round;sub_00334458_0x334458(m.data(),&c,&runtime);}if(c.pc!=done){std::cerr<<"Incomplete source pickup query "<<std::hex<<c.pc<<'\n';return 4;}
            unsigned count=GPR_U32((&c),2);if(count!=actual.instanceContacts.size()){std::cerr<<"Pickup count mismatch "<<[binding[@"name"]UTF8String]<<" source "<<count<<" native "<<actual.instanceContacts.size()<<'\n';return 5;}
            for(unsigned i=0;i<count;i++){
                unsigned at=output+i*128;const auto&hit=actual.instanceContacts[i];Vector point,normal;float depth;std::memcpy(&point,m.data()+at,12);std::memcpy(&normal,m.data()+at+16,12);std::memcpy(&depth,m.data()+at+0x40,4);
                if(hit.pointCm!=point||hit.normal!=normal||hit.penetrationCm!=depth||hit.instance!=get(get(at+0x50)+0x78)||hit.node!=get(at+0x5c)){std::cerr<<"Pickup contact mismatch "<<[binding[@"name"]UTF8String]<<" sample "<<[sample[@"name"]UTF8String]<<" depth "<<depth<<" native "<<hit.penetrationCm<<'\n';return 6;}
            }
            ++cases;hits+=count;
        }
    }
    std::cout<<cases<<" original334458 pickup collector queries match ordered contacts from captured posed bodies; "<<hits<<" contacts\n";
}}
