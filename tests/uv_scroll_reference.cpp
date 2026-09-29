// Instruction oracle for engine/uv_scroll.hpp: recompiled 0x35F6E8 (ctor),
// 0x35F7D0 (tick) and 0x35FC20 (texture matrix, zero-spin case) on synthetic
// memory against the port. 0x35FC20's only callee (0x31BE50 sin/cos) is not
// reached with angle 0.
#include "ps2_runtime_macros.h"
#include "../engine/uv_scroll.hpp"
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <random>
#include <vector>
#define ORIGINAL(name) void name(uint8_t*,R5900Context*,PS2Runtime*);
ORIGINAL(sub_0035F6E8_0x35f6e8) ORIGINAL(sub_0035F7D0_0x35f7d0) ORIGINAL(sub_0035FC20_0x35fc20)
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x440000,g_ps2RecompiledFunctionTableSlotCount=(0x440000-0x100000)/4;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[(0x440000-0x100000)/4]={};
using namespace ssx;
static uint8_t* mem;
static constexpr uint32_t gp=0x4a30f0,done=0x12345678,stackTop=0x90000,OBJ=0x1000000,ARGS=0x1001000,FPS=0x1002000,MATRIX=0x1003000;
static std::mt19937 rng(0x35F7D0);
static float uni(float a,float b){return std::uniform_real_distribution<float>(a,b)(rng);}
template<class T> static void put(uint32_t a,const T& v){std::memcpy(mem+a,&v,sizeof(v));}
static float pick(std::initializer_list<float> xs){auto it=xs.begin();std::advance(it,rng()%xs.size());return *it;}
static void store(const OriginalUvScroll& s){
    put(OBJ,s.mode);put(OBJ+4,s.timer);put(OBJ+8,s.onTime);put(OBJ+0xC,s.offTime);put(OBJ+0x10,s.angle);put(OBJ+0x14,s.spin);
    put(OBJ+0x20,s.axis);put(OBJ+0x30,s.u);put(OBJ+0x34,s.v);put(OBJ+0x38,s.stepU);put(OBJ+0x3C,s.stepV);put(OBJ+0x40,s.active);
}
static bool same(const OriginalUvScroll& s){
    auto w=[&](uint32_t a){uint32_t v;std::memcpy(&v,mem+a,4);return v;};auto b=[](float f){return std::bit_cast<uint32_t>(f);};
    return w(OBJ)==uint32_t(s.mode)&&w(OBJ+4)==b(s.timer)&&w(OBJ+8)==b(s.onTime)&&w(OBJ+0xC)==b(s.offTime)&&w(OBJ+0x10)==b(s.angle)&&w(OBJ+0x14)==b(s.spin)&&
        w(OBJ+0x20)==b(s.axis[0])&&w(OBJ+0x24)==b(s.axis[1])&&w(OBJ+0x28)==b(s.axis[2])&&w(OBJ+0x2C)==b(s.axis[3])&&
        w(OBJ+0x30)==b(s.u)&&w(OBJ+0x34)==b(s.v)&&w(OBJ+0x38)==b(s.stepU)&&w(OBJ+0x3C)==b(s.stepV)&&w(OBJ+0x40)==uint32_t(s.active);
}
static PS2Runtime* runtime;
using Original=void(*)(uint8_t*,R5900Context*,PS2Runtime*);
static void run(Original f,uint32_t pc,uint32_t a0,uint32_t a1){
    R5900Context c{};c.pc=pc;c.vu0_vf[0]=_mm_set_ps(1,0,0,0);for(unsigned i=0;i<32;i++)c.f[i]=uni(-1e3,1e3);
    for(unsigned r:{1u,2u,3u,8u,9u,10u,11u,12u,13u,14u,15u,24u,25u})SET_GPR_U64(&c,r,(uint64_t(rng())<<32)|rng());
    SET_GPR_U32(&c,4,a0);SET_GPR_U32(&c,5,a1);SET_GPR_U32(&c,28,gp);SET_GPR_U32(&c,29,stackTop);SET_GPR_U32(&c,31,done);
    OriginalRounding rounding;for(unsigned guard=0;c.pc!=done;guard++){if(guard>64)throw std::runtime_error("original incomplete");f(mem,&c,runtime);}
}
static OriginalUvScroll randomState(){
    OriginalUvScroll s;s.mode=int32_t(pick({5.f,5.f,2.f,6.f,6.f,0.f,1.f}));s.timer=rng()%3?uni(0,2):uni(.95f,1.05f);
    s.onTime=rng()%4?pick({1.f,.5f,2.f,uni(-.5f,3.f)}):0.f;s.offTime=rng()%3?0.f:pick({uni(0,2),-1.f,0.f});
    s.angle=rng()%4?0.f:uni(-10,10);s.spin=rng()%3?0.f:pick({uni(-8,8),6.2831854820251465f,-6.2831854820251465f,uni(6,7)});
    s.axis={uni(-1,1),uni(-1,1),uni(-1,1),0};
    s.u=rng()%5?uni(-1.05f,1.05f):pick({1.f,-1.f,0.f});s.v=rng()%5?uni(-1.05f,1.05f):pick({1.f,-1.f,0.f});
    s.stepU=rng()%3?pick({-0.025f,0.1f,0.f,0.01f}):uni(-.3f,.3f);s.stepV=rng()%3?pick({0.f,0.01f,-.007f,.002f}):uni(-.3f,.3f);s.active=int32_t(rng()%4!=0);
    return s;
}
int main(int argc,char** argv){
    if(argc<2)return 2;std::vector<uint8_t> memory(32*1024*1024);mem=memory.data();
    {std::ifstream f(argv[1],std::ios::binary);std::vector<uint8_t> elf((std::istreambuf_iterator<char>(f)),{});if(elf.size()<0x3b0000)return 3;std::memcpy(mem+0xFF000,elf.data(),elf.size());}
    PS2Runtime rt;runtime=&rt;rt.setMissingFunctionPolicy(PS2Runtime::MissingFunctionPolicy::Stop);
    rt.registerFunction(0x35f6e8,sub_0035F6E8_0x35f6e8);rt.registerFunction(0x35f7d0,sub_0035F7D0_0x35f7d0);rt.registerFunction(0x35fc20,sub_0035FC20_0x35fc20);
    put(gp+0x2A74,FPS);unsigned bad=0;
    {const float identity[16]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};put(0x4FF1A0,identity);}   // BSS identity (savestates)
    for(unsigned n=0;n<20000;n++){
        auto a=OriginalUvScrollArguments::defaults();for(unsigned k=1;k<13;k++)if(rng()%2)a.words[k]=k==1||k==12?rng()%8:std::bit_cast<uint32_t>(uni(-5,5));
        int32_t fps=rng()%4?60:int32_t(1+rng()%120);put(FPS+0x10,fps);put(ARGS,a.words);std::memset(mem+OBJ,0,0x50);
        run(sub_0035F6E8_0x35f6e8,0x35f6e8,OBJ,ARGS);if(!same(originalUvScroll(a,fps))){if(bad++<5)printf("35F6E8 mismatch %u\n",n);}
    }
    printf("0x35F6E8 ctor: 20000 cases, %u mismatches\n",bad);unsigned total=bad;bad=0;
    for(unsigned n=0;n<100000;n++){
        auto s=randomState();int32_t fps=rng()%4?60:int32_t(1+rng()%120);put(FPS+0x10,fps);store(s);
        unsigned steps=1+rng()%4;auto port=s;
        for(unsigned k=0;k<steps;k++){run(sub_0035F7D0_0x35f7d0,0x35f7d0,OBJ,0);originalUvScrollTick(port,fps);}
        if(!same(port)){if(bad++<5)printf("35F7D0 mismatch %u mode %d\n",n,s.mode);}
    }
    printf("0x35F7D0 tick: 100000 cases, %u mismatches\n",bad);total+=bad;bad=0;
    for(unsigned n=0;n<5000;n++){
        auto s=randomState();s.angle=0;store(s);std::memset(mem+MATRIX,0xCD,64);run(sub_0035FC20_0x35fc20,0x35fc20,OBJ,MATRIX);
        float m[16];std::memcpy(m,mem+MATRIX,64);auto o=originalUvScrollOffset(s);
        const float want[16]={1,0,0,0,0,1,0,0,0,0,1,0,o[0],o[1],0,1};
        if(std::memcmp(m,want,64)){if(bad++<5)printf("35FC20 mismatch %u\n",n);}
    }
    printf("0x35FC20 matrix (zero spin): 5000 cases, %u mismatches\n",bad);total+=bad;
    if(total){printf("FAILED %u\n",total);return 1;}puts("uv scroll oracle: all cases match");return 0;
}
