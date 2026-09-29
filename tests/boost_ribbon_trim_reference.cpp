#include "ps2_runtime_macros.h"
#include "../engine/boost_ribbon.hpp"
#include <fstream>
#include <random>
#include <cstring>
#include <cstdio>
void sub_002E6C08_0x2e6c08(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x420000,g_ps2RecompiledFunctionTableSlotCount=0xc8000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xc8000]={};
int main(int argc,char**argv){if(argc!=2)return 1;std::ifstream f(argv[1],std::ios::binary);std::vector<uint8_t>m((std::istreambuf_iterator<char>(f)),{});if(m.size()!=32*1024*1024)return 2;PS2Runtime rt;std::mt19937 rng(0x2e794c);
 auto put=[&](unsigned a,const auto&v){std::memcpy(m.data()+a,&v,sizeof(v));};auto word=[&](unsigned a){uint32_t x;std::memcpy(&x,m.data()+a,4);return x;};
 unsigned removed=0;
 for(unsigned n=0;n<20000;n++){
  ssx::OriginalBoostEffectState s;s.primaryCount=rng()%31;s.primaryDistance=float(int(rng()%200000)-1000)*.01f;s.trailLength=n%5==0?1.f:n%5==1?0.f:n%5==2?1.00001f:float(int(rng()%60000)-1000)*.01f;
  ssx::OriginalBoostRibbonHistory h;h.cursor=rng()%30;for(auto&r:h.rows){r.distance=float(int(rng()%10000)-(n%7==0?500:0))*.01f;for(auto&x:r.a)x=float(rng()%1000);for(auto&x:r.b)x=float(rng()%1000);}
  int before=s.primaryCount;put(0x10004,s.trailLength);put(0x10014,s.primaryCount);put(0x10018,s.primaryDistance);put(0x10030,h.cursor);put(0x10050,h.rows);
  R5900Context c{};c.pc=0x2e794c;SET_GPR_U32(&c,16,0x10000);SET_GPR_U32(&c,29,0x90000);
  ssx::OriginalRounding rounding;sub_002E6C08_0x2e6c08(m.data(),&c,&rt);ssx::originalBoostRibbonTrim(s,h);
  if(c.pc!=0x12345678||word(0x10014)!=uint32_t(s.primaryCount)||word(0x10018)!=std::bit_cast<uint32_t>(s.primaryDistance)||word(0x10030)!=uint32_t(h.cursor)||std::memcmp(m.data()+0x10050,h.rows.data(),sizeof(h.rows))){printf("Boost trim mismatch %u count%u/%d length%.9g/%.9g\n",n,word(0x10014),s.primaryCount,std::bit_cast<float>(word(0x10018)),s.primaryDistance);return 3;}
  removed+=before-s.primaryCount;
 }
 printf("20,000 original boost ribbon trims match count/distance and preserve samples; %u rows removed\n",removed);
}
