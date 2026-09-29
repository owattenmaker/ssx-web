#import <Foundation/Foundation.h>
#include "ps2_runtime_macros.h"
#include "../engine/world_collision_asset.h"
#include "../engine/air_trajectory_world.hpp"
#include <fstream>
#include <iostream>
#include <cstring>
#define DECL(n) void n(uint8_t*,R5900Context*,PS2Runtime*)
DECL(sub_003E6574_0x3e6574);DECL(sub_00334888_0x334888);DECL(sub_0032F650_0x32f650);DECL(sub_0032F708_0x32f708);DECL(sub_00329910_0x329910);
DECL(sub_00329DC8_0x329dc8);DECL(sub_00329A28_0x329a28);DECL(sub_00329B90_0x329b90);DECL(sub_0032B2B8_0x32b2b8);DECL(sub_00329590_0x329590);
DECL(sub_003A6CC8_0x3a6cc8);DECL(sub_003A6D00_0x3a6d00);DECL(sub_0032C0F8_0x32c0f8);DECL(sub_0032A1C0_0x32a1c0);DECL(sub_0032AA28_0x32aa28);DECL(sub_0032B6A8_0x32b6a8);
DECL(sub_00327F18_0x327f18);DECL(sub_0032CDB0_0x32cdb0);DECL(sub_0032DF28_0x32df28);DECL(sub_0032B620_0x32b620);DECL(sub_003A6CD8_0x3a6cd8);DECL(sub_003A6CE8_0x3a6ce8);
static void treeOracle(uint8_t* m,R5900Context* c,PS2Runtime* r){do{sub_0032CDB0_0x32cdb0(m,c,r);}while(c->pc==0x32cf34);}
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x420000,g_ps2RecompiledFunctionTableSlotCount=(0x420000-0x100000)/4;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[(0x420000-0x100000)/4]={};
DECL(sub_0032E100_0x32e100);
int main(int argc,char**argv){@autoreleasepool {
    using namespace ssx::terrain_original;
    if(argc!=3)return 2;auto readJSON=[](NSString* path){return [NSJSONSerialization JSONObjectWithData:[NSData dataWithContentsOfFile:path] options:0 error:nil];};
    NSDictionary* package=readJSON(@(argv[1]));NSArray* samples=readJSON(@(argv[2]));auto world=loadWorldBodyCollision(@(argv[1]),package[@"source_sha256"]);
    PS2Runtime runtime;runtime.memory().initialize();runtime.setMissingFunctionPolicy(PS2Runtime::MissingFunctionPolicy::Stop);
    auto reg=[&](uint32_t pc,PS2Runtime::RecompiledFunction fn){if(!runtime.registerFunction(pc,fn))throw std::runtime_error("Original query registration failed");};
    reg(0x3e6574,sub_003E6574_0x3e6574);reg(0x3a6cc8,sub_003A6CC8_0x3a6cc8);reg(0x3a6d00,sub_003A6D00_0x3a6d00);reg(0x32c0f8,sub_0032C0F8_0x32c0f8);reg(0x32b6a8,sub_0032B6A8_0x32b6a8);reg(0x3a6cd8,sub_003A6CD8_0x3a6cd8);reg(0x3a6ce8,sub_003A6CE8_0x3a6ce8);
    for(uint32_t pc:{0x32e398,0x32e4b8,0x32e4d0,0x32e5e8,0x32e688,0x32e690,0x32e288})reg(pc,sub_0032E100_0x32e100);
    unsigned cases=0,hits=0,skipped=0;
    for(NSDictionary* sample in samples){
        NSData* bytes=[NSData dataWithContentsOfFile:sample[@"ee"]];std::vector<uint8_t> original((const uint8_t*)bytes.bytes,(const uint8_t*)bytes.bytes+bytes.length);
        NSData* vu=[NSData dataWithContentsOfFile:sample[@"vu"]];std::memcpy(runtime.memory().getVU0Code(),vu.bytes,vu.length);
        auto get=[&](uint32_t at){uint32_t x;std::memcpy(&x,original.data()+at,4);return x;};
        uint32_t rider=[sample[@"rider"] unsignedIntValue],nearby=get(rider+0x860),shape=get(rider+0xaa0);
        for(unsigned j=0;j<(sample[@"instance"]?1:get(nearby+8));++j){
            uint32_t instance=sample[@"instance"]?[sample[@"instance"] unsignedIntValue]:get(nearby+12+j*4),descriptor=get(instance+0x88),id=get(instance+0x78);
            if(!descriptor||get(descriptor)==0)continue;
            auto found=std::find_if(world->instances.begin(),world->instances.end(),[&](const auto& x){return x.resource==id;});
            if(found==world->instances.end()||!found->unsupported.empty()||get(instance+12)){++skipped;continue;}
            ssx::WorldBodyCollision isolated;isolated.instances={*found};ssx::CollisionWorld noTerrain({});
            for(Vector offset:{Vector{0,0,0},{100,0,0},{-100,0,0},{0,100,0},{0,-100,0},{0,0,100},{0,0,-100}})for(unsigned axis=0;axis<3;++axis)for(float sign:{-1.f,1.f})for(int mode:{0,2}) {
                Rounding rounding;Vector origin,end;auto shifted=offset;std::memcpy(&origin,original.data()+shape+16,12);
                for(unsigned k=0;k<3;++k){if(sample[@"offset"])shifted[k]=add(shifted[k],[sample[@"offset"][k] floatValue]);origin[k]=add(origin[k],shifted[k]);}
                end=origin;origin[axis]=sub(origin[axis],sign*500);end[axis]=add(end[axis],sign*500);
                auto memory=original;auto put=[&](uint32_t at,const auto& x){std::memcpy(memory.data()+at,&x,sizeof(x));};
                constexpr uint32_t stack=0x70000,query=0x80000,ends=0x81000,output=0x90000,done=0x12345678;
                put(ends,origin);put(ends+12,1.f);put(ends+16,end);put(ends+28,1.f);
                R5900Context c{};c.vu0_vf[0]=_mm_set_ps(1,0,0,0);c.pc=0x32e100;c.f[12]=1;
                SET_GPR_U32(&c,29,stack);SET_GPR_U32(&c,31,done);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,4,query);SET_GPR_U32(&c,5,ends);SET_GPR_U32(&c,6,ends+16);SET_GPR_U32(&c,7,1);
                sub_0032E100_0x32e100(memory.data(),&c,&runtime);if(c.pc!=done)return 3;
                c.pc=0x334888;SET_GPR_U32(&c,4,query);SET_GPR_U32(&c,5,instance);SET_GPR_U32(&c,6,output);SET_GPR_U32(&c,7,64);SET_GPR_U32(&c,8,mode==0?2:1);SET_GPR_U32(&c,31,done);
                sub_00334888_0x334888(memory.data(),&c,&runtime);if(c.pc!=done){std::cerr<<"Incomplete ray PC "<<std::hex<<c.pc<<'\n';return 4;}
                unsigned count=GPR_U32((&c),2);ssx::OriginalAirTrajectoryHit expected;float score=INFINITY;
                for(unsigned i=0;i<count;++i){ssx::OriginalAirTrajectoryHit hit;std::memcpy(&hit.position,memory.data()+output+i*128,12);std::memcpy(&hit.normal,memory.data()+output+i*128+16,12);std::memcpy(&hit.fraction,memory.data()+output+i*128+64,4);std::memcpy(&hit.surface,memory.data()+output+i*128+76,4);float metric=std::abs(ssx::originalScalarSubtract(hit.fraction,1.f));if(metric<score){score=metric;expected=hit;}}
                auto actual=ssx::queryOriginalAirTrajectoryWorld(noTerrain,&isolated,origin,end,mode);++cases;
                if(!actual.complete||actual.fraction!=expected.fraction||(actual.fraction>=0&&(actual.position!=expected.position||actual.normal!=expected.normal||actual.surface!=expected.surface))){std::cerr<<"Live ray mismatch instance "<<id<<" kind "<<found->type<<" fraction "<<actual.fraction<<'/'<<expected.fraction<<" surface "<<actual.surface<<'/'<<expected.surface<<'\n';for(unsigned k=0;k<3;++k)std::cerr<<"point "<<actual.position[k]<<'/'<<expected.position[k]<<" normal "<<actual.normal[k]<<'/'<<expected.normal[k]<<'\n';return 1;}
                hits+=actual.fraction>=0;
            }
        }
    }
    std::cout<<cases<<" original live-instance rays match exactly; "<<hits<<" contacts; "<<skipped<<" explicit dynamic skips\n";
}}
