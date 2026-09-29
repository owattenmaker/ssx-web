#include "ps2_runtime_macros.h"
#include "../engine/crash_detached.hpp"
#include <fstream>
#include <cstring>
#include <iostream>
#include <random>
#define DECL(n) void sub_##n(uint8_t*,R5900Context*,PS2Runtime*)
DECL(00137550_0x137550);DECL(00137138_0x137138);DECL(00329A90_0x329a90);DECL(0032F650_0x32f650);DECL(00329910_0x329910);DECL(00329A28_0x329a28);DECL(003E6574_0x3e6574);DECL(00329DC8_0x329dc8);DECL(00317830_0x317830);DECL(0031BE50_0x31be50);DECL(00329B40_0x329b40);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000;
extern const uint32_t g_ps2RecompiledFunctionTableEnd=0x420000;
extern const uint32_t g_ps2RecompiledFunctionTableSlotCount=(0x420000-0x100000)/4;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[(0x420000-0x100000)/4]={};
using namespace ssx;using namespace ssx::terrain_original;
static float penetration;static Vector hitNormal,actualForce;static bool hasEntity,entityCalled;static std::mt19937 originalRandom;
template<class T>static void write(uint8_t*m,uint32_t p,T v){std::memcpy(m+p,&v,sizeof(v));}
template<class T>static T read(uint8_t*m,uint32_t p){T v;std::memcpy(&v,m+p,sizeof(v));return v;}
static void vec(uint8_t*m,uint32_t p,Vector v){write(m,p,v);write(m,p+12,0.f);}
static void external(uint8_t*m,R5900Context*c,PS2Runtime*){if(c->pc==0x336850){uint32_t p=GPR_U32(c,6);std::memset(m+p,0,0x80);vec(m,p+16,hitNormal);write(m,p+0x50,hasEntity?0x65000u:0u);c->f[0]=penetration;}if(c->pc==0x317810)SET_GPR_U32(c,2,originalRandom());if(c->pc==0x33ffe8){entityCalled=true;actualForce=read<Vector>(m,GPR_U32(c,6));}c->pc=GPR_U32(c,31);}
int main(int argc,char**argv){if(argc!=2)return 1;PS2Runtime runtime;std::vector<uint8_t>memory(32*1024*1024);auto*m=memory.data();std::ifstream ee(argv[1],std::ios::binary);ee.read((char*)m,memory.size());if(!ee)return 2;
#define REG(pc,n) runtime.registerFunction(pc,sub_##n)
    REG(0x137550,00137550_0x137550);REG(0x329a90,00329A90_0x329a90);REG(0x32f650,0032F650_0x32f650);REG(0x329910,00329910_0x329910);REG(0x329a28,00329A28_0x329a28);REG(0x3e6574,003E6574_0x3e6574);REG(0x329dc8,00329DC8_0x329dc8);REG(0x317830,00317830_0x317830);REG(0x31be50,0031BE50_0x31be50);REG(0x329b40,00329B40_0x329b40);
    for(uint32_t pc:{0x336850,0x32f708,0x317810,0x33ffe8})runtime.registerFunction(pc,external);
    constexpr uint32_t actor=0x10000,state=0x20000,geometry=0x30000,bone=0x31000,body=0x40000,stack=0x50000,done=0x12345678;
    write(m,state+0x40,actor);write(m,state+0x44,body);write(m,actor+0x780,geometry);write(m,geometry+0x2c,bone);write(m,actor+0x8a4,0u);write(m,body+0x28,0xffffffffu);
    write(m,0x4a30f0-0x848,0x60000u);write(m,0x60084,0x61000u);write(m,0x60010,60);write(m,0x6500c,0x65100u);write(m,0x6510c,0x65200u);write(m,0x65358,int16_t(0));write(m,0x6535c,0x33ffe8u);
    std::mt19937 random(0x424f4152);std::uniform_real_distribution<float>coord(-100000,100000),speed(-2500,2500),unit(-1,1),scale(.75f,1.5f);
    auto vector=[&](auto&d){return Vector{d(random),d(random),d(random)};};auto normal=[&](){auto v=vector(unit);float l=std::sqrt(dot(v,v));for(auto&x:v)x/=l;return v;};auto pose=[&](){AnimationTransform p;p.position=vector(coord);p.rotation={unit(random),unit(random),unit(random),unit(random)};float l=0;for(auto x:p.rotation)l+=x*x;l=std::sqrt(l);for(auto&x:p.rotation)x/=l;return p;};
    auto context=[&](uint32_t pc){R5900Context c{};c.pc=pc;c.vu0_vf[0]=_mm_set_ps(1,0,0,0);SET_GPR_U32(&c,4,state);SET_GPR_U32(&c,29,stack);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,31,done);return c;};
    for(unsigned n=0;n<20000;++n){auto p=pose();float s=scale(random);vec(m,bone,p.position);write(m,bone+16,p.rotation);write(m,geometry+0x140,s);auto c=context(0x137550);Rounding rounding;sub_00137550_0x137550(m,&c,&runtime);auto native=originalCrashDetachedBody(p,s);if(c.pc!=done||read<Vector>(m,body+16)!=native.broadCenterCm||read<float>(m,body+32)!=native.broadRadiusCm||read<unsigned>(m,body+44)!=native.count)throw std::runtime_error("Detached body header mismatch");for(unsigned j=0;j<4;++j)if(read<Vector>(m,body+48+j*32)!=native.spheres[j].centerCm||read<float>(m,body+64+j*32)!=20)throw std::runtime_error("Detached body sphere mismatch");}
    std::cout<<"20000 original detached-board body volumes match all sphere centers/radii exactly\n";
    unsigned bounces=0,stops=0,draws=0;
    for(unsigned n=0;n<20000;++n){auto p=pose();float scaleValue=scale(random);vec(m,bone,p.position);write(m,bone+16,p.rotation);write(m,geometry+0x140,scaleValue);OriginalCrashMotionState s;s.detachedVelocity=vector(speed);s.detachedAngularVelocity=vector(unit);for(auto&x:s.detachedAngularVelocity)x*=n%2?20:1;OriginalCrashActorState a;a.detached=n%11!=0;a.detachedPosition=vector(coord);vec(m,state+16,s.detachedVelocity);vec(m,state+32,s.detachedAngularVelocity);vec(m,actor+0x130,a.detachedPosition);write(m,actor+0x150,int(a.detached));vec(m,state+80,s.low);vec(m,state+96,s.high);penetration=n%13?std::abs(unit(random))*80:-1.f;hitNormal=n%3?normal():Vector{0,0,1};hasEntity=n%2;entityCalled=false;auto seed=random();originalRandom.seed(seed);std::mt19937 nativeRandom(seed);auto c=context(0x137138);Rounding rounding;sub_00137138_0x137138(m,&c,&runtime);
        auto volume=originalCrashDetachedBody(p,scaleValue);if(a.detached){s.low={INFINITY,INFINITY,INFINITY};s.high={-INFINITY,-INFINITY,-INFINITY};for(unsigned j=0;j<4;++j)for(unsigned k=0;k<3;++k){s.low[k]=std::min(s.low[k],sub(volume.spheres[j].centerCm[k],20));s.high[k]=std::max(s.high[k],add(volume.spheres[j].centerCm[k],20));}}
        auto result=originalCrashDetachedBodyResponse(s,a,penetration,hitNormal,hasEntity,60,[&](){return nativeRandom();});
        if(c.pc!=done||read<Vector>(m,actor+0x130)!=a.detachedPosition||read<Vector>(m,state+16)!=s.detachedVelocity||read<Vector>(m,state+32)!=s.detachedAngularVelocity||read<Vector>(m,state+80)!=s.low||read<Vector>(m,state+96)!=s.high||entityCalled!=result.dynamicCallbackRequired||(entityCalled&&actualForce!=result.entityForce)||originalRandom!=nativeRandom){std::cerr<<"Detached collision mismatch "<<n<<" root "<<(read<Vector>(m,actor+0x130)==a.detachedPosition)<<" velocity "<<(read<Vector>(m,state+16)==s.detachedVelocity)<<" angular "<<(read<Vector>(m,state+32)==s.detachedAngularVelocity)<<" bounds "<<(read<Vector>(m,state+80)==s.low)<<" rng "<<(originalRandom==nativeRandom)<<'\n';return 3;}
        bounces+=result.bounced;stops+=result.stopped;draws+=result.randomDraws;
    }
    std::cout<<"20000 complete original detached-board contact responses match; "<<bounces<<" bounces, "<<stops<<" stops, "<<draws<<" shared RNG draws; root, bounds, angular jitter and entity forces exact\n";
}
