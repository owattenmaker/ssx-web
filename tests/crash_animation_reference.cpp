#include "ps2_runtime_macros.h"
#include "../engine/crash_animation.hpp"
#include <fstream>
#include <cstring>
#include <iostream>
void sub_0012DCB0_0x12dcb0(uint8_t*,R5900Context*,PS2Runtime*);void sub_0012DD98_0x12dd98(uint8_t*,R5900Context*,PS2Runtime*);void sub_0012DE80_0x12de80(uint8_t*,R5900Context*,PS2Runtime*);void sub_0012DF48_0x12df48(uint8_t*,R5900Context*,PS2Runtime*);void sub_0012E468_0x12e468(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x340000,g_ps2RecompiledFunctionTableSlotCount=(0x340000-0x100000)/4;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[(0x340000-0x100000)/4]={};
static int semantic,played;static bool updated;static void external(uint8_t*,R5900Context*c,PS2Runtime*){if(c->pc==0x312aa0)SET_GPR_U32(c,2,semantic);if(c->pc==0x3128e8){played=GPR_U32(c,5);if(c->f[12]!=-1||GPR_U32(c,6)!=0)throw std::runtime_error("Crash animation play flags differ");}if(c->pc==0x12e528)updated=true;c->pc=GPR_U32(c,31);}
int main(int argc,char**argv){using namespace ssx;if(argc!=2)return 1;PS2Runtime runtime;std::vector<uint8_t> m(32*1024*1024);std::ifstream ee(argv[1],std::ios::binary);ee.read((char*)m.data(),m.size());if(!ee)return 2;auto put=[&](uint32_t p,auto v){std::memcpy(m.data()+p,&v,sizeof(v));};put(0x20080,0x10000u);
    for(uint32_t pc:{0x312aa0,0x3128e8,0x12e528})runtime.registerFunction(pc,external);
    using Fn=PS2Runtime::RecompiledFunction;Fn functions[]={sub_0012DD98_0x12dd98,sub_0012DCB0_0x12dcb0,sub_0012DF48_0x12df48,sub_0012DE80_0x12de80,sub_0012E468_0x12e468};uint32_t pcs[]={0x12dd98,0x12dcb0,0x12df48,0x12de80,0x12e468};unsigned cases=0;
    for(unsigned kind=0;kind<5;++kind)for(semantic=300;semantic<430;++semantic)for(int detached:{0,1}){put(0x10150,detached);played=-1;updated=false;R5900Context c{};c.pc=pcs[kind];SET_GPR_U32(&c,4,0x20000);SET_GPR_U32(&c,29,0x30000);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,31,0x12345678);functions[kind](m.data(),&c,&runtime);auto result=originalCrashSelectAnimation(OriginalCrashAnimationSelection(kind),semantic,detached);if(c.pc!=0x12345678||played!=(result.play?result.semantic:-1)||updated!=result.updatePlaybackRate){std::cerr<<"Crash animation selection mismatch "<<kind<<' '<<semantic<<' '<<detached<<'\n';return 3;}++cases;}
    std::cout<<cases<<" original crash continuation/get-up/landing animation selections match, including no-op and detached cases\n";
}
