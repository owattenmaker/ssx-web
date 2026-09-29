#include "ps2_runtime_macros.h"
#include "../engine/boost_draw.hpp"
#include <fstream>
#include <random>
#include <cstring>
#include <cstdio>
void sub_002E7A10_0x2e7a10(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x420000,g_ps2RecompiledFunctionTableSlotCount=0xc8000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xc8000]={};
static std::vector<std::vector<ssx::OriginalBoostDrawVertex>> submitted;static unsigned mainCount=0;
int main(int argc,char**argv){if(argc!=2)return 1;std::ifstream f(argv[1],std::ios::binary);std::vector<uint8_t>m((std::istreambuf_iterator<char>(f)),{});if(m.size()!=32*1024*1024)return 2;PS2Runtime rt;std::mt19937 rng(0x2e7c3c);
 auto put=[&](unsigned a,const auto&v){std::memcpy(m.data()+a,&v,sizeof(v));};auto quad=[&](float w){ssx::BoostHistoryQuad v;for(auto&x:v)x=float(int(rng()%800000)-400000)*.01f;v[3]=w;return v;};
 rt.registerFunction(0x140000,[](uint8_t*m,R5900Context*c,PS2Runtime*){auto count=GPR_U32(c,5),p=GPR_U32(c,6);if(count>40)throw std::runtime_error("Side vertex overflow");submitted.emplace_back(count);std::memcpy(submitted.back().data(),m+p,count*48);c->pc=GPR_U32(c,31);});
 rt.registerFunction(0x37a260,[](uint8_t*,R5900Context*c,PS2Runtime*){mainCount=GPR_U32(c,5);SET_GPR_U32(c,2,0x60000);c->pc=GPR_U32(c,31);});
 rt.registerFunction(0x37a430,[](uint8_t*m,R5900Context*c,PS2Runtime*){if(mainCount>60)throw std::runtime_error("Main vertex overflow");submitted.emplace_back(mainCount);std::memcpy(submitted.back().data(),m+0x60000,mainCount*48);c->pc=GPR_U32(c,31);});
 put(0x10000,0x20000u);put(0x103ac,0x50000u);put(0x20780,0x40000u);put(0x40030,0x41000u);put(0x2089c,0u);put(0x4a289c,0x30000u);put(0x310d8,0x32000u);put(0x32238,uint16_t(0));put(0x3223c,0x140000u);put(0x4a5b80,0x70000u);
 static_assert(sizeof(ssx::OriginalBoostDrawVertex)==48);
 unsigned total=0;
 for(unsigned n=0;n<20000;n++){
  ssx::OriginalBoostEffectState s;s.trailCount=1+rng()%20;s.primaryCount=rng()%31;s.trailLength=3+float(rng()%10000)*.1f;s.primaryDistance=1+float(rng()%10000)*.1f;s.alpha=float(rng()%1001)*.001f;s.width=float(rng()%501)*.1f;
  ssx::OriginalBoostSideHistory side;side.cursor=rng()%20;side.scroll=float(int(rng()%2000)-1000)*.001f;for(auto&r:side.samples){r.left=quad(.3f);r.right=quad(.3f);}
  ssx::OriginalBoostRibbonHistory main;main.cursor=rng()%30;for(auto&r:main.rows){r.distance=float(rng()%2000)*.1f;auto a=quad(1),b=quad(1);for(unsigned k=0;k<3;k++){r.a[k]=a[k];r.b[k]=b[k];}}
  auto head=quad(1),right=quad(0);put(0x10004,s.trailLength);put(0x10008,s.alpha);put(0x1000c,s.width);put(0x10010,side.scroll);put(0x10014,s.primaryCount);put(0x10018,s.primaryDistance);put(0x10030,main.cursor);put(0x10050,main.rows);put(0x1039c,side.cursor);put(0x103a0,s.trailCount);put(0x50000,side.samples);put(0x41030,head);put(0x201a0,right);
  std::memset(m.data()+0x90000,0,0x1040);std::memset(m.data()+0x60000,0,60*48);submitted.clear();mainCount=0;
  R5900Context c{};c.pc=0x2e7c3c;c.vu0_vf[0]=_mm_set_ps(1,0,0,0);SET_GPR_U32(&c,18,0x10000);SET_GPR_U32(&c,6,0x20000);SET_GPR_U32(&c,4,s.trailCount);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,29,0x90000);
  ssx::OriginalRounding rounding;sub_002E7A10_0x2e7a10(m.data(),&c,&rt);auto out=ssx::originalBoostDrawVertices(s,side,main,head,right);std::vector<std::vector<ssx::OriginalBoostDrawVertex>> expected;
  if(!out.left.empty()){expected.push_back(out.left);expected.push_back(out.right);}if(!out.main.empty())expected.push_back(out.main);
  bool okay=c.pc==0x12345678&&submitted.size()==expected.size();if(okay)for(unsigned p=0;p<expected.size();p++)okay&=submitted[p].size()==expected[p].size()&&!std::memcmp(submitted[p].data(),expected[p].data(),expected[p].size()*48);
  if(!okay){printf("Boost draw mismatch %u side%d main%d submits%zu/%zu pc%x\n",n,s.trailCount,s.primaryCount,submitted.size(),expected.size(),c.pc);return 3;}for(auto&p:expected)total+=p.size();
 }
 printf("20,000 original boost draw packets match position/UV/colour; %u vertices\n",total);
}
