#import <Foundation/Foundation.h>
#include "ps2_runtime_macros.h"
#include "../engine/world_collision_asset.h"
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
int main(int argc,char**argv){@autoreleasepool {
    if(argc!=3)return 2;
    auto readJSON=[](NSString* path){return [NSJSONSerialization JSONObjectWithData:[NSData dataWithContentsOfFile:path] options:0 error:nil];};
    NSDictionary* package=readJSON(@(argv[1]));NSArray* samples=readJSON(@(argv[2]));auto world=loadWorldBodyCollision(@(argv[1]),package[@"source_sha256"]);
    PS2Runtime runtime;runtime.memory().initialize();runtime.setMissingFunctionPolicy(PS2Runtime::MissingFunctionPolicy::Stop);
    auto reg=[&](uint32_t pc,PS2Runtime::RecompiledFunction fn){if(!runtime.registerFunction(pc,fn))throw std::runtime_error("Original query registration failed");};
    reg(0x3e6574,sub_003E6574_0x3e6574);reg(0x329910,sub_00329910_0x329910);reg(0x329dc8,sub_00329DC8_0x329dc8);reg(0x329a28,sub_00329A28_0x329a28);reg(0x329b90,sub_00329B90_0x329b90);
    reg(0x32b2b8,sub_0032B2B8_0x32b2b8);reg(0x329590,sub_00329590_0x329590);reg(0x3a6cc8,sub_003A6CC8_0x3a6cc8);reg(0x3a6d00,sub_003A6D00_0x3a6d00);
    reg(0x32c0f8,sub_0032C0F8_0x32c0f8);reg(0x32a1c0,sub_0032A1C0_0x32a1c0);reg(0x32aa28,sub_0032AA28_0x32aa28);reg(0x32b6a8,sub_0032B6A8_0x32b6a8);
    for(uint32_t pc:{0x32f840,0x32f8b0,0x32fac0,0x32f760,0x32f8f0,0x32f990,0x32fa30})reg(pc,sub_0032F708_0x32f708);
    reg(0x327f18,sub_00327F18_0x327f18);reg(0x32cdb0,treeOracle);reg(0x32df28,sub_0032DF28_0x32df28);reg(0x32b620,sub_0032B620_0x32b620);reg(0x3a6cd8,sub_003A6CD8_0x3a6cd8);reg(0x3a6ce8,sub_003A6CE8_0x3a6ce8);
    unsigned cases=0,hits=0,skipped=0;float maximumPoint=0,maximumNormal=0,maximumDepth=0;
    for(NSDictionary* sample in samples){
        NSData* bytes=[NSData dataWithContentsOfFile:sample[@"ee"]];std::vector<uint8_t> original((const uint8_t*)bytes.bytes,(const uint8_t*)bytes.bytes+bytes.length);
        NSData* vu=[NSData dataWithContentsOfFile:sample[@"vu"]];std::memcpy(runtime.memory().getVU0Code(),vu.bytes,vu.length);
        auto get=[&](uint32_t at){uint32_t x;std::memcpy(&x,original.data()+at,4);return x;};
        uint32_t rider=[sample[@"rider"] unsignedIntValue],nearby=get(rider+0x860),shape=get(rider+0xaa0);bool detailed=get(rider+0x874)!=0;
        for(unsigned instanceIndex=0;instanceIndex<(sample[@"instance"]?1:get(nearby+8));++instanceIndex){
            uint32_t instance=sample[@"instance"]?[sample[@"instance"] unsignedIntValue]:get(nearby+12+instanceIndex*4),descriptor=get(instance+0x88),id=get(instance+0x78);
            if(!descriptor||get(descriptor)==0)continue;
            auto found=std::find_if(world->instances.begin(),world->instances.end(),[&](const auto& x){return x.resource==id;});
            if(found==world->instances.end()||!found->unsupported.empty()||get(instance+12)){++skipped;continue;}
            for(auto offset:{ssx::terrain_original::Vector{0,0,0},{100,0,0},{-100,0,0},{0,100,0},{0,-100,0},{0,0,100},{0,0,-100}}){
                if(sample[@"offset"]){ssx::terrain_original::Rounding rounding;for(unsigned k=0;k<3;++k)offset[k]=ssx::terrain_original::add(offset[k],[sample[@"offset"][k] floatValue]);}
                auto memory=original;auto put=[&](uint32_t at,const auto& x){std::memcpy(memory.data()+at,&x,sizeof(x));};
                ssx::BodyCollisionVolume body;std::memcpy(&body.broadCenterCm,memory.data()+shape+16,12);std::memcpy(&body.broadRadiusCm,memory.data()+shape+32,4);body.activeMask=get(shape+40);body.count=get(shape+44);
                ssx::terrain_original::Vector ground;std::memcpy(&ground,memory.data()+rider+0x370,12);
                {ssx::terrain_original::Rounding rounding;for(unsigned k=0;k<3;++k)body.broadCenterCm[k]=ssx::terrain_original::add(body.broadCenterCm[k],offset[k]);}
                put(shape+16,body.broadCenterCm);
                for(unsigned i=0;i<body.count;++i){std::memcpy(&body.spheres[i].centerCm,memory.data()+shape+48+i*32,12);std::memcpy(&body.spheres[i].radiusCm,memory.data()+shape+64+i*32,4);{ssx::terrain_original::Rounding rounding;for(unsigned k=0;k<3;++k)body.spheres[i].centerCm[k]=ssx::terrain_original::add(body.spheres[i].centerCm[k],offset[k]);}put(shape+48+i*32,body.spheres[i].centerCm);}
                constexpr uint32_t stack=0x70000,query=0x80000,output=0x90000,done=0x12345678;
                R5900Context c{};c.vu0_vf[0]=_mm_set_ps(1,0,0,0);c.pc=0x32f650;SET_GPR_U32(&c,29,stack);SET_GPR_U32(&c,31,done);SET_GPR_U32(&c,28,0x4a30f0);
                SET_GPR_U32(&c,4,query);SET_GPR_U32(&c,5,shape);SET_GPR_U32(&c,6,detailed?0:1);
                {ssx::terrain_original::Rounding rounding;sub_0032F650_0x32f650(memory.data(),&c,&runtime);}if(c.pc!=done)return 3;
                put(query+8,uint32_t(1));put(query+16,ground);put(query+28,0.f);
                c.pc=0x334888;SET_GPR_U32(&c,4,query);SET_GPR_U32(&c,5,instance);SET_GPR_U32(&c,6,output);SET_GPR_U32(&c,7,64);SET_GPR_U32(&c,8,0);SET_GPR_U32(&c,31,done);
                {ssx::terrain_original::Rounding rounding;sub_00334888_0x334888(memory.data(),&c,&runtime);}if(c.pc!=done){std::cerr<<"Original query incomplete PC "<<std::hex<<c.pc<<" instance "<<id<<"\n";return 4;}
                unsigned count=GPR_U32((&c),2);ssx::WorldBodyHit expected;float score=INFINITY;
                for(unsigned i=0;i<count;++i){ssx::WorldBodyHit hit;hit.hit=true;hit.instance=id;std::memcpy(&hit.pointCm,memory.data()+output+i*128,12);std::memcpy(&hit.normal,memory.data()+output+i*128+16,12);std::memcpy(&hit.penetrationCm,memory.data()+output+i*128+64,4);float metric=std::abs(hit.penetrationCm-(detailed?body.broadRadiusCm*2:-1));if(metric<score){score=metric;expected=hit;}}
                world->resetSphereTreeCache();std::vector<uint32_t> filter{id};auto actual=world->query(body,ground,&filter,detailed,0).best;++cases;
                float point=0,normal=0,depth=std::abs(actual.penetrationCm-expected.penetrationCm);
                for(unsigned k=0;k<3;++k){point=std::max(point,std::abs(actual.pointCm[k]-expected.pointCm[k]));normal=std::max(normal,std::abs(actual.normal[k]-expected.normal[k]));}
                if(actual.hit!=expected.hit||(actual.hit&&(point>.0001f||normal>2e-6f||depth>.0001f))){std::cerr<<"Live world collision mismatch instance "<<id<<" rider "<<rider<<" hit "<<actual.hit<<'/'<<expected.hit<<" pointcm "<<point<<" normal "<<normal<<" depth "<<depth<<'\n';return 1;}
                if(actual.hit){++hits;maximumPoint=std::max(maximumPoint,point);maximumNormal=std::max(maximumNormal,normal);maximumDepth=std::max(maximumDepth,depth);}
            }
        }
    }
    std::cout<<cases<<" original live-instance queries passed; "<<hits<<" contacts; "<<skipped<<" explicit dynamic/missing-resource skips; maxerrors pointcm "<<maximumPoint<<" normal "<<maximumNormal<<" depthcm "<<maximumDepth<<'\n';
}}
