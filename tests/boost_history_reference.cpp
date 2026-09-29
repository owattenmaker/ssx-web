#include "ps2_runtime_macros.h"
#include "../engine/boost_history.hpp"
#include <fstream>
#include <random>
#include <cstring>
#include <cstdio>
void sub_002E66B8_0x2e66b8(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x420000,g_ps2RecompiledFunctionTableSlotCount=0xc8000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xc8000]={};
int main(int argc,char**argv){if(argc!=2)return 1;std::ifstream f(argv[1],std::ios::binary);std::vector<uint8_t>m((std::istreambuf_iterator<char>(f)),{});if(m.size()!=32*1024*1024)return 2;PS2Runtime rt;std::mt19937 rng(0x2e68a8);
 auto put=[&](unsigned a,const auto&v){std::memcpy(m.data()+a,&v,sizeof(v));};auto word=[&](unsigned a){uint32_t x;std::memcpy(&x,m.data()+a,4);return x;};
 auto quad=[&](float w){ssx::BoostHistoryQuad v;for(auto&x:v)x=float(int(rng()%800000)-400000)*.01f;v[3]=w;return v;};
 put(0x10000,0x20000u);put(0x20780,0x30000u);put(0x30030,0x40000u);put(0x2089c,0u);put(0x208b0,1u);put(0x208b8,2u);put(0x103ac,0x50000u);
 for(unsigned n=0;n<20000;n++){
  ssx::OriginalBoostEffectState state;state.trailCount=int(rng()%26)-3;
  ssx::OriginalBoostSideHistory history;history.capacity=ssx::originalBoostHistoryCapacity(n%3==0?-1:0);history.cursor=rng()%20;history.scroll=float(int(rng()%6000)-3000)*.001f;
  for(auto&pair:history.samples){pair.left=quad(1);pair.right=quad(1);}
  auto right=quad(0),head=quad(1),a=quad(1),b=n%7?quad(1):a;
  ssx::OriginalBoostEffectParameters params;params.emitting=n%2;params.speed=float(rng()%30000);params.scrollStep=n%3==0?.002f:n%3==1?.005f:.01f;bool enabled=n%11!=0;
  put(0x1039c,history.cursor);put(0x103a0,state.trailCount);put(0x103a4,history.capacity);put(0x10010,history.scroll);put(0x50000,history.samples);
  put(0x201a0,right);put(0x40030,head);put(0x40070,a);put(0x400b0,b);put(0x4a4700,int(enabled));
  R5900Context c{};c.pc=0x2e68a8;c.vu0_vf[0]=_mm_set_ps(1,0,0,0);SET_GPR_U32(&c,16,0x10000);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,29,0x90000);SET_GPR_U32(&c,6,params.emitting);c.f[4]=params.scrollStep;c.f[5]=params.speed;
  ssx::OriginalRounding rounding;sub_002E66B8_0x2e66b8(m.data(),&c,&rt);ssx::originalBoostSideHistory(state,history,params,right,head,a,b,enabled);
  if(c.pc!=0x12345678||word(0x1039c)!=uint32_t(history.cursor)||word(0x103a0)!=uint32_t(state.trailCount)||word(0x10010)!=std::bit_cast<uint32_t>(history.scroll)||std::memcmp(m.data()+0x50000,history.samples.data(),sizeof(history.samples))){printf("Boost history mismatch %u pc%x count%d/%d cursor%u/%d scroll%.9g/%.9g\n",n,c.pc,int(word(0x103a0)),state.trailCount,word(0x1039c),history.cursor,std::bit_cast<float>(word(0x10010)),history.scroll);return 3;}
 }
 puts("20,000 original boost side-history updates match all ring samples, cursor, count and scroll");
}
