#include "ps2_runtime_macros.h"
#include "../engine/crash_bake.hpp"
#include <fstream>
#include <cstring>
#include <iostream>
#include <random>
void sub_0010EB30_0x10eb30(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0012E010_0x12e010(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0011E098_0x11e098(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000;
extern const uint32_t g_ps2RecompiledFunctionTableEnd=0x340000;
extern const uint32_t g_ps2RecompiledFunctionTableSlotCount=(0x340000-0x100000)/4;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[(0x340000-0x100000)/4]={};
static ssx::AnimationTransform offset;
static void capture(uint8_t*m,R5900Context*c,PS2Runtime*){uint32_t p=GPR_U32(c,5);std::memcpy(&offset.position,m+p,12);std::memcpy(&offset.rotation,m+p+16,16);throw 1;}
int main(int argc,char**argv){using namespace ssx;using namespace ssx::terrain_original;if(argc!=2)return 1;PS2Runtime runtime;std::vector<uint8_t> memory(32*1024*1024);std::ifstream ee(argv[1],std::ios::binary);ee.read((char*)memory.data(),memory.size());if(!ee)return 2;
    auto write=[&](uint32_t p,const auto&v){std::memcpy(memory.data()+p,&v,sizeof(v));};auto pose=[&](uint32_t p,AnimationTransform t){write(p,t.position);write(p+12,1.f);write(p+16,t.rotation);};
    runtime.registerFunction(0x311bf0,capture);runtime.registerFunction(0x11e098,sub_0011E098_0x11e098);
    constexpr uint32_t actor=0x10000,geometry=0x20000,stack=0x30000,positions=0x40000,rotations=0x50000;
    write(actor+0x780,geometry);write(actor+0x89c,0u);write(geometry+0x24,positions);write(geometry+0x28,rotations);
    std::mt19937 random(0x42414b45);std::uniform_real_distribution<float> coord(-10000,10000),local(-100,100),unit(-1,1),scaling(.75f,1.5f);
    auto transform=[&](auto& d){AnimationTransform t;t.position={d(random),d(random),d(random)};t.rotation={unit(random),unit(random),unit(random),unit(random)};float length=0;for(auto v:t.rotation)length+=v*v;length=std::sqrt(length);for(auto&v:t.rotation)v/=length;return t;};
    write(0x21080,actor);
    for(unsigned variant=0;variant<2;++variant)for(unsigned n=0;n<20000;++n){auto physical=transform(coord),current=transform(local),sampled=transform(local);Vector scale{scaling(random),scaling(random),scaling(random)};pose(actor+0x110,physical);pose(stack+(variant?0x50:0x60),sampled);write(positions,current.position);write(positions+12,1.f);write(rotations,current.rotation);write(geometry+0x140,scale);write(geometry+0x14c,1.f);
        R5900Context c{};c.pc=variant?0x12e080:0x10edd4;c.f[20]=-1;c.vu0_vf[0]=_mm_set_ps(1,0,0,0);SET_GPR_U32(&c,16,0x21000);SET_GPR_U32(&c,17,0x500000);SET_GPR_U32(&c,19,actor);SET_GPR_U32(&c,20,0x500000);SET_GPR_U32(&c,29,stack);SET_GPR_U32(&c,28,0x4a30f0);bool captured=false;Rounding rounding;try{if(variant)sub_0012E010_0x12e010(memory.data(),&c,&runtime);else sub_0010EB30_0x10eb30(memory.data(),&c,&runtime);}catch(int code){if(code!=1)throw;captured=true;}
        for(unsigned k=0;k<3;++k)current.position[k]=mul(current.position[k],scale[k]);auto expected=originalCrashRootBake(physical,current,sampled);AnimationTransform actual;std::memcpy(&actual.position,memory.data()+actor+0x110,12);std::memcpy(&actual.rotation,memory.data()+actor+0x120,16);
        if(!captured||actual.position!=expected.physical.position||actual.rotation!=expected.physical.rotation||offset.position!=expected.animationRootOffset.position||offset.rotation!=expected.animationRootOffset.rotation){std::cerr<<"Crash root bake mismatch "<<n<<" pos "<<(actual.position==expected.physical.position)<<" rotation "<<(actual.rotation==expected.physical.rotation)<<" offset "<<(offset.position==expected.animationRootOffset.position)<<'\n';return 3;}
    }
    std::cout<<"40000 original hard-entry/reset-clip physical/root-offset bakes match exactly\n";
}
