#include "ps2_runtime_macros.h"
#include "../engine/pickup_instance_flags.hpp"
#include <vector>
#include <random>
#include <cstring>
#include <cstdio>
void sub_00342D10_0x342d10(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00350F60_0x350f60(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x420000,g_ps2RecompiledFunctionTableSlotCount=0xc8000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xc8000]={};
static bool veto;static std::vector<int> calls;
static uint32_t get(uint8_t*m,unsigned a){uint32_t v;std::memcpy(&v,m+a,4);return v;}
static void put(uint8_t*m,unsigned a,uint32_t v){std::memcpy(m+a,&v,4);}
static void callback(uint8_t*m,R5900Context*c,PS2Runtime*){
 if(c->pc==0x34fcc0){calls.push_back(0);SET_GPR_U32(c,2,veto);}
 else if(c->pc==0x123400){
  if(GPR_U32(c,4)!=0x20000||GPR_U32(c,5)!=3)throw std::runtime_error("Wrong old component destruction");
  calls.push_back(1);put(m,0x30008,ssx::originalPickupRestoredFlags(get(m,0x30008)));
 }else if(c->pc==0x317d70){
  if(GPR_U32(c,4)!=0x1c||GPR_U32(c,6)!=0x20000000||GPR_U32(c,7)!=0)throw std::runtime_error("Wrong replacement allocation");
  calls.push_back(2);SET_GPR_U32(c,2,0x40000);
 }else if(c->pc==0x34fb00){
  if(GPR_U32(c,4)!=0x40000||GPR_U32(c,5)!=1||GPR_U32(c,6)!=19||GPR_U32(c,7)!=0x30000)throw std::runtime_error("Wrong replacement class/owner");
  calls.push_back(3);put(m,0x40018,0x30000);
 }else throw std::runtime_error("Unexpected completion helper");
 c->pc=GPR_U32(c,31);
}
int main(){std::vector<uint8_t>m(32*1024*1024);PS2Runtime rt;
 for(unsigned pc:{0x34fcc0u,0x123400u,0x317d70u,0x34fb00u})rt.registerFunction(pc,callback);
 rt.registerFunction(0x350f60,sub_00350F60_0x350f60);
 put(m.data(),0x2000c,0x50000);put(m.data(),0x5000c,0x123400);put(m.data(),0x20018,0x30000);put(m.data(),0x20030,3);
 std::mt19937 rng(0x342e98);unsigned replacements=0;
 for(unsigned n=0;n<20000;n++){
  uint32_t flags=rng();int32_t ticks=int(n%7)-1;bool notify=n%3!=0;veto=n%5==0;calls.clear();
  put(m.data(),0x30008,flags);put(m.data(),0x2002c,uint32_t(ticks));put(m.data(),0x4000c,0);
  R5900Context c{};c.pc=0x342e98;SET_GPR_U32(&c,4,0x20000);SET_GPR_U32(&c,5,notify);SET_GPR_U32(&c,29,0x10000);SET_GPR_U32(&c,31,0x12345678);
  sub_00342D10_0x342d10(m.data(),&c,&rt);
  bool replace=!(notify&&veto)&&ticks>=0;std::vector<int> expected;if(notify)expected.push_back(0);if(replace)expected.insert(expected.end(),{1,2,3});
  uint32_t final=replace?ssx::originalPickupRestorePendingFlags(ssx::originalPickupRestoredFlags(flags)):flags;
  if(c.pc!=0x12345678||calls!=expected||get(m.data(),0x30008)!=final||get(m.data(),0x4000c)!=(replace?0x491680u:0)){printf("Completion mismatch %u\n",n);return 1;}replacements+=replace;
 }
 printf("20,000 original mode3 completion paths verified; %u type19 replacements, no automatic restoration callback\n",replacements);
}
