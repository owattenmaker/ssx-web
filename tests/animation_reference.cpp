// Development-only conformance check against the original PS2 pose conversion.
// No PS2 runtime is linked into the native Metal application.
#include "ps2_runtime_macros.h"
#include "../engine/skeleton.hpp"
#include "../engine/animation_motion.hpp"
#include <cstring>
#include <cstdio>
#include <random>
#include <fstream>
#include <cfenv>

void sub_00310200_0x310200(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0030ECD8_0x30ecd8(uint8_t*,R5900Context*,PS2Runtime*);
void sub_003130A8_0x3130a8(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00312C20_0x312c20(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x311318;
extern const uint32_t g_ps2RecompiledFunctionTableEnd=0x31131c;
extern const uint32_t g_ps2RecompiledFunctionTableSlotCount=1;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[1]={nullptr};

int main(int argc,char** argv) {
    PS2Runtime runtime;
    std::vector<uint8_t> ram(32*1024*1024);
    auto write=[&](uint32_t at,const auto& value){std::memcpy(ram.data()+at,&value,sizeof(value));};
    constexpr uint32_t stack=0x10000,object=0x20000,bone=0x30000,output=0x40000,done=0x12345678;
    std::mt19937 random(0x414642);
    std::uniform_real_distribution<float> coordinate(-8,8);
    const std::array<std::array<uint8_t,4>,3> maps={{{0,1,2,3},{5,4,7,6},{7,2,1,4}}};
    float maximum=0;
    std::fesetround(FE_TOWARDZERO);
    for (unsigned test=0;test<10000;++test) {
        simd::float3 input=test<100?simd::float3{float(int(test%7)-3),float(int(test%5)-2),float(int(test%3)-1)}:simd::float3{coordinate(random),coordinate(random),coordinate(random)};
        const auto& map=maps[test%maps.size()];
        bool mirrored=test%maps.size()!=0;
        R5900Context ctx{};
        ctx.vu0_vf[0]=_mm_set_ps(1,0,0,0);
        SET_GPR_U32(&ctx,29,stack);SET_GPR_U32(&ctx,16,bone);
        SET_GPR_U32(&ctx,18,object);SET_GPR_U32(&ctx,20,output);
        SET_GPR_U32(&ctx,21,stack+0x10);SET_GPR_S32(&ctx,22,-1);
        write(stack+0x70,uint64_t(done));
        write(stack+0x30,input.x);write(stack+0x34,input.y);write(stack+0x38,input.z);
        write(object+0x40,uint32_t(mirrored));
        write(bone+0x10,int16_t(0));
        std::memcpy(ram.data()+bone+0x18,map.data(),4);
        // Resume at the original post-scalar-sampling entry, execute its actual
        // vector/FPU conversion, optional mirror map and function return.
        ctx.pc=0x30efc4;
        sub_0030ECD8_0x30ecd8(ram.data(),&ctx,&runtime);
        if (ctx.pc!=done) {std::fprintf(stderr,"Unexpected reference continuation: %x\n",ctx.pc);return 1;}
        float original[4];std::memcpy(original,ram.data()+output+16,16);
        auto strict=ssx::originalAnimationQuaternion({input.x,input.y,input.z});
        std::array<float,4> mapped;
        for(unsigned k=0;k<4;++k)mapped[k]=strict[map[k]&3]*(map[k]<4?1.f:-1.f);
        if(std::memcmp(mapped.data(),original,16))throw std::runtime_error("Exact animation quaternion mismatch");
        simd::float4 expected={original[0],original[2],-original[1],original[3]};
        auto actual=ssx::afbQuaternion(input,map);
        float error=simd_length(actual-expected);
        maximum=std::max(maximum,error);
        if (!std::isfinite(error)||error>2e-4f) {
            std::fprintf(stderr,"Quaternion mismatch %u: input %f,%f,%f error %g\n",test,input.x,input.y,input.z,error);
            std::fprintf(stderr,"Original %f %f %f %f; native %f %f %f %f\n",expected.x,expected.y,expected.z,expected.w,actual.x,actual.y,actual.z,actual.w);
            return 1;
        }
    }
    std::printf("10,000 original-game quaternion/mirror cases native bit-exact; legacy preview maximum vector error %g\n",maximum);
    for(unsigned test=0;test<10000;++test) {
        constexpr uint32_t geo=0x60000,part=0x61000,record=0x62000,translation=0x63000,rotation=0x64000,world=0x65000,root=0x66000;
        ssx::AnimationTransform parent,local;ssx::AnimationVector scale;
        for(unsigned k=0;k<3;++k){parent.position[k]=coordinate(random)*100;local.position[k]=coordinate(random)*10;scale[k]=std::abs(coordinate(random));}
        for(unsigned k=0;k<4;++k){parent.rotation[k]=coordinate(random)*.1f;local.rotation[k]=coordinate(random)*.1f;}
        write(geo+8,uint32_t(1));write(geo+12,part);write(geo+0x24,translation);write(geo+0x28,rotation);write(geo+0x2c,world);
        write(part+4,uint32_t(0));write(part+0x38,record);write(part+0x44,uint32_t(1));write(record+0x12,int16_t(-1));
        write(translation,local.position);write(rotation,local.rotation);write(root,parent.position);write(root+16,parent.rotation);write(geo+0x140,scale);
        R5900Context ctx{};ctx.vu0_vf[0]=_mm_set_ps(1,0,0,0);SET_GPR_U32(&ctx,29,stack);SET_GPR_U32(&ctx,4,geo);SET_GPR_U32(&ctx,5,root);SET_GPR_U32(&ctx,31,done);ctx.pc=0x310200;
        sub_00310200_0x310200(ram.data(),&ctx,&runtime);
        auto actual=ssx::originalAnimationCompose(parent,local,scale);
        if(ctx.pc!=done||std::memcmp(actual.position.data(),ram.data()+world,12)||std::memcmp(actual.rotation.data(),ram.data()+world+16,16))throw std::runtime_error("Exact animation FK mismatch");
    }
    std::puts("10,000 original animation FK cases passed bit-identically");
    // Resume original translation conversion after scalar sampling. The next
    // scalar request is supplied an identity rotation so the original routine can
    // return normally; no reconstructed translation operations run in the oracle.
    bool registered=runtime.registerFunction(0x311318,[](uint8_t* memory,R5900Context* ctx,PS2Runtime*) {
        const float identity[3]={1,1,1};
        std::memcpy(memory+GPR_U32(ctx,5),identity,sizeof(identity));
        ctx->pc=GPR_U32(ctx,31);
    });
    if (!registered) throw std::runtime_error("Cannot register identity sample fixture");
    float maximumTranslation=0;
    for (unsigned test=0;test<10000;++test) {
        simd::float3 input={coordinate(random)*100,coordinate(random)*100,coordinate(random)*100};
        simd::float3 scale={(test&1)?-1.f:1.f,(test&2)?-1.f:1.f,(test&4)?-1.f:1.f};
        R5900Context ctx{};ctx.vu0_vf[0]=_mm_set_ps(1,0,0,0);ctx.f[20]=1;
        SET_GPR_U32(&ctx,29,stack);SET_GPR_U32(&ctx,16,bone);SET_GPR_U32(&ctx,18,object);
        SET_GPR_U32(&ctx,20,output);SET_GPR_U32(&ctx,21,stack+0x10);SET_GPR_S32(&ctx,22,-1);
        write(stack+0x70,uint64_t(done));write(bone+0x10,int16_t(0));
        write(object+0x40,uint32_t(1));
        write(bone+0x40,scale.x);write(bone+0x44,scale.y);write(bone+0x48,scale.z);write(bone+0x4c,1.f);
        write(stack+0x20,input.x);write(stack+0x24,input.y);write(stack+0x28,input.z);
        ctx.pc=0x30ee54;sub_0030ECD8_0x30ecd8(ram.data(),&ctx,&runtime);
        if (ctx.pc!=done) throw std::runtime_error("Translation reference continuation");
        float original[4];std::memcpy(original,ram.data()+output,16);
        simd::float3 expected={original[0]/100,original[2]/100,-original[1]/100};
        float error=simd_length(expected-ssx::afbTranslation(input,scale));
        maximumTranslation=std::max(maximumTranslation,error);
        if (!std::isfinite(error)||error>1e-6f) throw std::runtime_error("Mirror translation mismatch");
    }
    std::printf("10,000 original-game mirror translation cases passed; maximum vector error %g\n",maximumTranslation);
    if (argc!=3) {std::fprintf(stderr,"Expected original ELF and curve fixture arguments\n");return 1;}
    std::ifstream elfFile(argv[1],std::ios::binary);
    std::vector<uint8_t> elf((std::istreambuf_iterator<char>(elfFile)),{});
    auto u32=[&](size_t p){uint32_t x;if(p+4>elf.size())throw std::runtime_error("ELF extent");std::memcpy(&x,elf.data()+p,4);return x;};
    auto u16=[&](size_t p){uint16_t x;if(p+2>elf.size())throw std::runtime_error("ELF extent");std::memcpy(&x,elf.data()+p,2);return x;};
    for (unsigned i=0;i<u16(44);++i) {
        size_t p=u32(28)+i*u16(42);
        if (u32(p)!=1) continue;
        uint32_t offset=u32(p+4),address=u32(p+8),size=u32(p+16);
        if (size>elf.size() || offset>elf.size()-size || size>ram.size() || address>ram.size()-size) throw std::runtime_error("ELF segment extent");
        std::memcpy(ram.data()+address,elf.data()+offset,size);
    }
    std::ifstream fixture(argv[2],std::ios::binary);
    auto read=[&](auto& value){if(!fixture.read(reinterpret_cast<char*>(&value),sizeof(value)))throw std::runtime_error("Truncated fixture");};
    uint32_t packetCount;read(packetCount);
    uint64_t valuesChecked=0;
    float maxCurveError=0;
    // The reference recompilation's CVT.W.S helper follows the host rounding
    // mode. SSX3's sampling path uses truncation for nonnegative frame indices.
    std::fesetround(FE_TOWARDZERO);
    for (uint32_t packet=0;packet<packetCount;++packet) {
        uint32_t length,frames,dofs;read(length);read(frames);read(dofs);
        if (length>0x10000 || dofs>255) throw std::runtime_error("Fixture extent");
        constexpr uint32_t raw=0x60000,state=0x80000,parameters=0x90000,result=0xa0000;
        if (!fixture.read(reinterpret_cast<char*>(ram.data()+raw),length)) throw std::runtime_error("Truncated packet");
        std::memset(ram.data()+state,0,0x100);write(state,parameters);
        R5900Context ctx{};
        SET_GPR_U32(&ctx,4,state);SET_GPR_U32(&ctx,5,raw);SET_GPR_U32(&ctx,6,frames);SET_GPR_U32(&ctx,31,done);
        ctx.pc=0x3130a8;sub_003130A8_0x3130a8(ram.data(),&ctx,&runtime);
        if(ctx.pc!=done)throw std::runtime_error("Curve init continuation");
        ssx::AnimationPacket nativePacket(std::span<const uint8_t>(ram.data()+raw,length),frames,false);
        for (unsigned sample=0;sample<3;++sample) {
            float time;read(time);
            std::vector<float> expected(dofs);
            for (float& value:expected)read(value);
            ctx={};SET_GPR_U32(&ctx,4,state);SET_GPR_U32(&ctx,5,0);SET_GPR_U32(&ctx,6,dofs);
            SET_GPR_U32(&ctx,7,result);SET_GPR_U32(&ctx,29,stack);SET_GPR_U32(&ctx,31,done);SET_GPR_U32(&ctx,28,0x4a30f0);
            ctx.f[12]=time;ctx.pc=0x312c20;sub_00312C20_0x312c20(ram.data(),&ctx,&runtime);
            if(ctx.pc!=done)throw std::runtime_error("Curve sample continuation");
            auto exact=nativePacket.sample(time);
            for (unsigned i=0;i<dofs;++i) {
                float actual;std::memcpy(&actual,ram.data()+result+i*4,4);
                if(std::memcmp(&actual,&exact[i],4))throw std::runtime_error("Exact animation scalar mismatch");
                float error=std::abs(actual-expected[i]);maxCurveError=std::max(maxCurveError,error);
                if (!std::isfinite(actual)||error>2e-4f*std::max(1.f,std::abs(expected[i]))) {
                    std::fprintf(stderr,"Curve mismatch packet %u DOF %u frame %g: original %.9g imported %.9g\n",packet,i,time,actual,expected[i]);return 1;
                }
                ++valuesChecked;
            }
        }
    }
    std::fesetround(FE_TONEAREST);
    std::printf("%u original curve packets, %llu native scalar samples bit-exact; Python expanded-sample maximum absolute error %g\n",packetCount,valuesChecked,maxCurveError);
}
