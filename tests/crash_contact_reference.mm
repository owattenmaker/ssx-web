#import <Foundation/Foundation.h>
#include "ps2_runtime_macros.h"
#include "../engine/crash_contact.hpp"
#include "../engine/world_collision_asset.h"
#include <iostream>
#include <cstring>
#define DECL(n) void n(uint8_t*,R5900Context*,PS2Runtime*)
DECL(sub_00356020_0x356020);DECL(sub_0035FE10_0x35fe10);DECL(sub_003568B0_0x3568b0);DECL(sub_00138960_0x138960);DECL(sub_003342D0_0x3342d0);DECL(sub_00333EF8_0x333ef8);DECL(sub_003279D0_0x3279d0);DECL(sub_00327AC0_0x327ac0);DECL(sub_00327B30_0x327b30);DECL(sub_00327BB0_0x327bb0);DECL(sub_00327C00_0x327c00);DECL(sub_00327C68_0x327c68);DECL(sub_00391418_0x391418);DECL(sub_00391480_0x391480);DECL(sub_003914F8_0x3914f8);DECL(sub_00340970_0x340970);DECL(sub_0032B6E0_0x32b6e0);DECL(sub_0032E100_0x32e100);DECL(sub_003E6574_0x3e6574);DECL(sub_00334888_0x334888);DECL(sub_003A6CC8_0x3a6cc8);DECL(sub_003A6D00_0x3a6d00);DECL(sub_0032C0F8_0x32c0f8);DECL(sub_0032B6A8_0x32b6a8);DECL(sub_003A6CD8_0x3a6cd8);DECL(sub_003A6CE8_0x3a6ce8);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x420000,g_ps2RecompiledFunctionTableSlotCount=(0x420000-0x100000)/4;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[(0x420000-0x100000)/4]={};
int main(int argc,char**argv){@autoreleasepool {
    using namespace ssx;using namespace terrain_original;
    if(argc!=3)return 2;auto json=[](NSString*p){return [NSJSONSerialization JSONObjectWithData:[NSData dataWithContentsOfFile:p] options:0 error:nil];};
    NSString* folder=@(argv[1]);NSDictionary* terrainData=json([folder stringByAppendingPathComponent:@"terrain.json"]);std::vector<TerrainPatch> patches;
    for(NSDictionary*d in terrainData[@"patches"]){TerrainPatch p;p.resource=[d[@"resource_id"]unsignedIntValue];p.authoredFlags=[d[@"authored_flags"]unsignedIntValue];p.authoredSurface=[d[@"authored_surface_id"]intValue];p.hasAuthoredBounds=true;
        auto vec=[](NSArray*a){return Vec3{[a[0]doubleValue],[a[1]doubleValue],[a[2]doubleValue]};};p.authoredMinimum=vec(d[@"authored_bounds_min"]);p.authoredMaximum=vec(d[@"authored_bounds_max"]);for(unsigned i=0;i<16;++i)p.coefficients[i]=vec(d[@"coefficients"][i]);patches.push_back(p);}
    CollisionWorld terrain({});terrain.setTerrainPatches(patches);auto world=loadWorldBodyCollision([folder stringByAppendingPathComponent:@"world_collision.json"],terrainData[@"source_sha256"],[folder stringByAppendingPathComponent:@"terrain.json"]);
    PS2Runtime runtime;runtime.memory().initialize();runtime.setMissingFunctionPolicy(PS2Runtime::MissingFunctionPolicy::Stop);
    auto reg=[&](uint32_t pc,PS2Runtime::RecompiledFunction fn){if(!runtime.registerFunction(pc,fn))throw std::runtime_error("Landing oracle registration");};
#define REG(pc,name) reg(pc,sub_##name)
    REG(0x356128,00356020_0x356020);REG(0x360990,0035FE10_0x35fe10);REG(0x356ae0,003568B0_0x3568b0);REG(0x356a28,003568B0_0x3568b0);REG(0x3342d0,003342D0_0x3342d0);REG(0x333ef8,00333EF8_0x333ef8);REG(0x3279d0,003279D0_0x3279d0);REG(0x327ac0,00327AC0_0x327ac0);REG(0x327b30,00327B30_0x327b30);REG(0x327bb0,00327BB0_0x327bb0);REG(0x327c00,00327C00_0x327c00);REG(0x327c68,00327C68_0x327c68);REG(0x391418,00391418_0x391418);REG(0x391480,00391480_0x391480);REG(0x3914f8,003914F8_0x3914f8);REG(0x340a10,00340970_0x340970);REG(0x32b6e0,0032B6E0_0x32b6e0);REG(0x3e6574,003E6574_0x3e6574);REG(0x334888,00334888_0x334888);REG(0x3a6cc8,003A6CC8_0x3a6cc8);REG(0x3a6d00,003A6D00_0x3a6d00);REG(0x32c0f8,0032C0F8_0x32c0f8);REG(0x32b6a8,0032B6A8_0x32b6a8);REG(0x3a6cd8,003A6CD8_0x3a6cd8);REG(0x3a6ce8,003A6CE8_0x3a6ce8);
    for(uint32_t pc:{0x32e100,0x32e398,0x32e4b8,0x32e4d0,0x32e5e8,0x32e688,0x32e690,0x32e288,0x32e9a0})reg(pc,sub_0032E100_0x32e100);
    NSArray*samples=json(@(argv[2]));unsigned cases=0,hits=0,unsupported=0;
    for(NSDictionary*sample in samples){NSData* ee=[NSData dataWithContentsOfFile:sample[@"ee"]];std::vector<uint8_t> original((uint8_t*)ee.bytes,(uint8_t*)ee.bytes+ee.length);NSData*vu=[NSData dataWithContentsOfFile:sample[@"vu"]];std::memcpy(runtime.memory().getVU0Code(),vu.bytes,vu.length);
        for(Vector offset:{Vector{0,0,0},{100,0,0},{-100,0,0},{0,100,0},{0,-100,0},{0,0,100},{0,0,-100}}){
            auto m=original;auto get=[&](uint32_t at){uint32_t x;std::memcpy(&x,m.data()+at,4);return x;};auto put=[&](uint32_t at,const auto&x){std::memcpy(m.data()+at,&x,sizeof(x));};
            constexpr uint32_t rider=0x14701a0,output=0x90000,done=0x12345678;uint32_t bone=get(get(rider+0x780)+0x2c)+get(rider+0x8a0)*32,cacheAt=get(rider+0x864);
            OriginalCrashContactProbe probe;std::memcpy(&probe.positionCm,m.data()+rider+0x110,12);std::memcpy(&probe.normal,m.data()+rider+0x370,12);
            {Rounding round;for(unsigned k=0;k<3;++k)probe.positionCm[k]=add(probe.positionCm[k],offset[k]);}put(rider+0x110,probe.positionCm);
            ContactCache cache;if(cacheAt&&get(cacheAt)&&get(cacheAt+12)==2){cache.valid=true;cache.resource=get(get(cacheAt)+0x150);cache.cellU=uint16_t(get(cacheAt+4));cache.cellV=uint16_t(get(cacheAt+4)>>16);cache.half=get(cacheAt+8);}
            auto actual=originalCrashContact(terrain,world.get(),probe,&cache);if(!actual.complete){++unsupported;continue;}
            R5900Context c{};c.vu0_vf[0]=_mm_set_ps(1,0,0,0);c.pc=0x138960;SET_GPR_U32(&c,29,0x70000);SET_GPR_U32(&c,31,done);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,4,get(rider+0x77c)+0x30);SET_GPR_U32(&c,5,output);SET_GPR_U32(&c,6,rider+0x370);
            {Rounding round;sub_00138960_0x138960(m.data(),&c,&runtime);}if(c.pc!=done){std::cerr<<"Landing original incomplete PC "<<std::hex<<c.pc<<'\n';return 3;}
            if(cacheAt){uint32_t pointer=get(cacheAt);bool expectedValid=pointer&&get(cacheAt+12)==2;if(cache.valid!=expectedValid||(expectedValid&&(cache.resource!=get(pointer+0x150)||cache.cellU!=uint16_t(get(cacheAt+4))||cache.cellV!=uint16_t(get(cacheAt+4)>>16)||cache.half!=get(cacheAt+8)))){std::cerr<<[sample[@"name"]UTF8String]<<" crash cache mismatch native "<<cache.valid<<':'<<cache.resource<<':'<<cache.cellU<<','<<cache.cellV<<'/'<<cache.half<<" original "<<expectedValid<<':'<<(pointer?get(pointer+0x150):0)<<':'<<uint16_t(get(cacheAt+4))<<','<<uint16_t(get(cacheAt+4)>>16)<<'/'<<get(cacheAt+8)<<'\n';return 5;}}
            if(c.f[0]<0){if(actual.fraction!=c.f[0])throw std::runtime_error("Native crash differs on original miss");++cases;continue;}
            Vector point,normal,surfaceVelocity;float u,v;std::memcpy(&point,m.data()+output,12);std::memcpy(&normal,m.data()+output+16,12);std::memcpy(&surfaceVelocity,m.data()+output+32,12);std::memcpy(&u,m.data()+output+0x6c,4);std::memcpy(&v,m.data()+output+0x70,4);
            int surface=int(get(output+0x4c));uint32_t patch=get(output+0x54),instance=get(output+0x50);if(patch)patch=get(patch+0x150);if(instance)instance=get(instance+0x78);
            if(actual.fraction!=c.f[0]||(actual.fraction>=0&&(actual.position!=point||actual.normal!=normal||actual.surfaceVelocityCmps!=surfaceVelocity||actual.surface!=surface||actual.instance!=instance||actual.hasPatch!=bool(patch)||(actual.hasPatch&&(uint32_t(actual.patchId)!=patch||actual.patchU!=u||actual.patchV!=v))))){std::cerr<<[sample[@"name"]UTF8String]<<" crash mismatch "<<cases<<" fraction "<<actual.fraction<<'/'<<c.f[0]<<" patch "<<actual.patchId<<'/'<<patch<<" UV "<<actual.patchU<<','<<actual.patchV<<'/'<<u<<','<<v<<'\n';for(unsigned k=0;k<3;++k)std::cerr<<"point "<<actual.position[k]<<'/'<<point[k]<<" normal "<<actual.normal[k]<<'/'<<normal[k]<<'\n';return 1;}
            ++cases;hits+=actual.fraction>=0;
        }
    }
    std::cout<<cases<<" full original physical crash queries match exactly; "<<hits<<" contacts; "<<unsupported<<" explicit unsupported callbacks\n";
}}
