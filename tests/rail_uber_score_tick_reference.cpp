#include "ps2_runtime_macros.h"
#include "../engine/score_boundary.hpp"
#include <fstream>
#include <random>
#include <cstring>
#include <cstdio>
void sub_00117C28_0x117c28(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x420000,g_ps2RecompiledFunctionTableSlotCount=0xc8000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xc8000]={};
int main(int argc,char**argv){if(argc!=2)return 1;std::ifstream f(argv[1],std::ios::binary);std::vector<uint8_t>m((std::istreambuf_iterator<char>(f)),{});if(m.size()!=32*1024*1024)return 2;PS2Runtime rt;std::mt19937 random(0x117e58);
 auto put=[&](unsigned a,float x){std::memcpy(m.data()+a,&x,4);};auto bits=[&](unsigned a){uint32_t x;std::memcpy(&x,m.data()+a,4);return x;};
 for(unsigned n=0;n<20000;n++){
  ssx::OriginalScoreBoundaryState s;s.activeSeconds6C=n%5==0?-1.f:n%5==1?0.f:float(random()%100000)*.001f;s.score.accumulated14=float(random()%100000)*.0001f;
  float dt=float(random()%121)/3600.f;put(0x2006c,s.activeSeconds6C);put(0x20014,s.score.accumulated14);
  R5900Context c{};c.pc=0x117e58;SET_GPR_U32(&c,16,0x20000);SET_GPR_U32(&c,28,0x4a30f0);c.f[20]=dt;c.f[21]=0;
  ssx::OriginalRounding rounding;sub_00117C28_0x117c28(m.data(),&c,&rt);ssx::originalRailUberScoreTick(s,dt);
  if(c.pc!=0x12345678||bits(0x2006c)!=std::bit_cast<uint32_t>(s.activeSeconds6C)||bits(0x20014)!=std::bit_cast<uint32_t>(s.score.accumulated14)){printf("Rail Uber accrual mismatch %u pc%x time%08x/%08x score%08x/%08x\n",n,c.pc,bits(0x2006c),std::bit_cast<uint32_t>(s.activeSeconds6C),bits(0x20014),std::bit_cast<uint32_t>(s.score.accumulated14));return 3;}
 }
 puts("20,000 original active rail-Uber score ticks match elapsed time and points");
}
