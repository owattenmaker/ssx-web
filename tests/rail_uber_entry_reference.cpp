#include "ps2_runtime_macros.h"
#include "../engine/rail_uber_entry.hpp"
#include <vector>
#include <cstring>
#include <random>
#include <cstdio>
void sub_00132620_0x132620(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x420000,g_ps2RecompiledFunctionTableSlotCount=0xc8000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xc8000]={};
static std::vector<int> calls;
static void helper(uint8_t*,R5900Context*c,PS2Runtime*){
 if(c->pc==0x11fec8){if(GPR_U32(c,4)!=0x20000||GPR_U32(c,5)!=12)throw std::runtime_error("Wrong Uber controller handoff");calls.push_back(12);}
 else if(c->pc==0x28b180){calls.push_back(1);SET_GPR_U32(c,2,0x40000);}
 else {if(GPR_U32(c,4)!=0x40000||GPR_U32(c,5)!=0x20000)throw std::runtime_error("Wrong unavailable-feedback rider");calls.push_back(2);}
 c->pc=GPR_U32(c,31);
}
int main(){std::vector<uint8_t>m(32*1024*1024);PS2Runtime rt;for(unsigned pc:{0x11fec8u,0x28b180u,0x299b70u})rt.registerFunction(pc,helper);
 auto put=[&](unsigned a,const auto&v){std::memcpy(m.data()+a,&v,sizeof(v));};auto get=[&](unsigned a){int32_t v;std::memcpy(&v,m.data()+a,4);return v;};put(0x10008,0x20000u);put(0x2077c,0x30000u);
 std::mt19937 rng(0x132620);
 for(unsigned n=0;n<20000;n++){
  int32_t requested=int(rng()%6)-1,remembered=int(rng()%6)-1,pending=int(rng());float time=float(int(rng()%100)-20)*.125f;uint32_t flags=rng();
  put(0x10004,remembered);put(0x30394,pending);put(0x202f0,time);put(0x20b2c,flags);calls.clear();
  R5900Context c{};c.pc=0x132620;SET_GPR_U32(&c,4,0x10000);SET_GPR_U32(&c,5,uint32_t(requested));SET_GPR_U32(&c,29,0x50000);SET_GPR_U32(&c,31,0x12345678);sub_00132620_0x132620(m.data(),&c,&rt);
  auto result=ssx::originalRailUberEntry(remembered,pending,requested,time,flags);std::vector<int> expected;if(result.enter)expected={12};else if(result.notifyUnavailable)expected={1,2};
  if(c.pc!=0x12345678||GPR_U32((&c),2)!=uint32_t(result.enter)||get(0x10004)!=remembered||get(0x30394)!=pending||calls!=expected){printf("Rail Uber entry mismatch %u\n",n);return 1;}
 }
 puts("20,000 original rail Uber entry gates match state and ordered callbacks");
}
