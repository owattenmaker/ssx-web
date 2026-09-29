#include "ps2_runtime_macros.h"
#include "../engine/original_camera.hpp"
#include <fstream>
#include <random>
#include <vector>
#include <cstring>
#include <cstdio>
void sub_001635F8_0x1635f8(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x420000,g_ps2RecompiledFunctionTableSlotCount=0xc8000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xc8000]={};
static float suppliedAngle;
static unsigned angleCalls;
static void angleGetter(uint8_t*,R5900Context* c,PS2Runtime*){
    ++angleCalls;c->f[0]=suppliedAngle;c->pc=GPR_U32(c,31);
}
int main(int argc,char** argv){
    if(argc!=2)return 2;
    std::ifstream file(argv[1],std::ios::binary);
    std::vector<uint8_t> elf((std::istreambuf_iterator<char>(file)),{}),memory(32*1024*1024);
    std::memcpy(memory.data()+0xff000,elf.data(),elf.size());
    auto put=[&](unsigned at,auto value){std::memcpy(memory.data()+at,&value,sizeof(value));};
    PS2Runtime runtime;runtime.registerFunction(0x110000,angleGetter);
    put(0x20030,0x40000u);put(0x40000,0x41000u);put(0x410b8,int16_t(0));put(0x410bc,0x110000u);
    std::mt19937 rng(0x163e8c);unsigned firstLandings=0,decayFrames=0;
    for(unsigned n=0;n<20000;++n){
        ssx::OriginalChaseAlgorithmState state;
        const bool airborne=n%3==0;
        state.landed=n%2;state.phaseAActive=rng()%2;state.phaseBActive=rng()%2;
        state.phaseAClock=float(rng()%5001)/1000;
        if(n%11==0)state.phaseAClock=1.2f;
        if(n%11==1)state.phaseAClock=2.2f;
        state.landingDecay=float(rng()%1001)/1000;
        state.lookOffsetA=float(int(rng()%20001)-10000)/10;
        state.eyeOffsetA=float(int(rng()%20001)-10000)/10;
        state.lookOffsetB=float(int(rng()%20001)-10000)/10;
        state.eyeOffsetB=float(int(rng()%20001)-10000)/10;
        suppliedAngle=float(rng()%3142)/1000;
        if(n%11==2)suppliedAngle=1.05f;
        if(n%11==3)suppliedAngle=1.4f;
        put(0x30060,unsigned(airborne));
        auto fields=[&](auto fn){
            fn(0x218,state.phaseAClock);fn(0x224,state.landingDecay);
            fn(0x20c,state.lookOffsetB);fn(0x210,state.eyeOffsetB);
            fn(0x21c,state.lookOffsetA);fn(0x220,state.eyeOffsetA);
            fn(0x2e0,state.phaseBActive);fn(0x2e4,state.landed);fn(0x2e8,state.phaseAActive);
        };
        fields([&](unsigned offset,auto value){put(0x20000+offset,value);});
        angleCalls=0;unsigned nativeCalls=0;
        R5900Context ctx{};ctx.pc=0x163e8c;ctx.f[31]=ssx::original_camera::default3::jumpLandDecay;
        SET_GPR_U32(&ctx,18,0x20000);SET_GPR_U32(&ctx,19,0x30000);
        SET_GPR_U32(&ctx,28,0x4a30f0);SET_GPR_U32(&ctx,29,0x10000);
        {
            ssx::original_camera::Rounding rounding;
            sub_001635F8_0x1635f8(memory.data(),&ctx,&runtime);
            ssx::original_camera::stepJumpLanding(state,airborne,[&](){++nativeCalls;return suppliedAngle;});
        }
        if(ctx.pc!=0x163fd8||nativeCalls!=angleCalls)throw std::runtime_error("Landing control flow differs");
        fields([&](unsigned offset,auto value){
            if(std::memcmp(memory.data()+0x20000+offset,&value,4)){
                std::printf("Camera landing mismatch case %u offset %x\n",n,offset);
                throw std::runtime_error("Original camera landing field differs");
            }
        });
        firstLandings+=nativeCalls;decayFrames+=state.landed!=0;
    }
    std::printf("20000 original camera landing stages match all nine state fields; %u first landings, %u offset-decay frames. Angle getter is a controlled input.\n",firstLandings,decayFrames);
}
