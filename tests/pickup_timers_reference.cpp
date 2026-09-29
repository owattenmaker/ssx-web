#include "ps2_runtime_macros.h"
#include "../engine/pickup_timers.hpp"
#include <vector>
#include <cstring>
#include <cstdio>
#include <random>
void sub_00356198_0x356198(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00342D10_0x342d10(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x420000,g_ps2RecompiledFunctionTableSlotCount=0xc8000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xc8000]={};
static int32_t expectedCooldown,expectedDebounce;static int phase,completion;
static int32_t read(uint8_t*m,unsigned at){int32_t v;std::memcpy(&v,m+at,4);return v;}
static void callback(uint8_t*m,R5900Context*c,PS2Runtime*){
 if(GPR_U32(c,4)!=0x20000)throw std::runtime_error("Wrong pickup timer owner");
 if(read(m,0x20020)!=expectedCooldown)throw std::runtime_error("Cooldown tick did not precede callbacks");
 if(c->pc==0x123400){if(phase++!=0)throw std::runtime_error("Wrong predicate order");SET_GPR_U32(c,2,0);}
 else if(c->pc==0x3556f8){if(phase++!=1)throw std::runtime_error("Wrong refresh order");}
 else if(c->pc==0x123404){if(phase!=2||GPR_U32(c,5)!=1||read(m,0x2002c)!=0)throw std::runtime_error("Wrong completion contract");++completion;}
 else if(c->pc==0x355748){if(phase!=2||read(m,0x2002c)!=expectedDebounce)throw std::runtime_error("Post-update ordering mismatch");++phase;}
 c->pc=GPR_U32(c,31);
}
int main(){std::vector<uint8_t> m(32*1024*1024);PS2Runtime rt;
 auto put=[&](unsigned at,uint32_t v){std::memcpy(m.data()+at,&v,4);};
 put(0x2000c,0x30000);put(0x300a4,0x123400);put(0x3007c,0x342d88);put(0x30114,0x123404);put(0x2001c,0);
 for(unsigned pc:{0x123400u,0x123404u,0x3556f8u,0x355748u})rt.registerFunction(pc,callback);
 rt.registerFunction(0x342d88,sub_00342D10_0x342d10);
 std::mt19937 random(0x356198);unsigned completed=0;
 for(unsigned n=0;n<20000;n++){
  int32_t cooldown=int(random()%100)-4,debounce=int(random()%190)-4;
  if(n%11==0)cooldown=INT32_MIN;if(n%13==0)cooldown=INT32_MAX;if(n%7==0)debounce=1;
  expectedCooldown=cooldown;expectedDebounce=debounce;bool complete=ssx::originalPickupTimersTick(expectedCooldown,expectedDebounce);
  put(0x20020,uint32_t(cooldown));put(0x2002c,uint32_t(debounce));phase=completion=0;
  R5900Context c{};c.pc=0x356198;SET_GPR_U32(&c,4,0x20000);SET_GPR_U32(&c,29,0x10000);SET_GPR_U32(&c,31,0x12345678);sub_00356198_0x356198(m.data(),&c,&rt);
  if(c.pc!=0x12345678||read(m.data(),0x20020)!=expectedCooldown||read(m.data(),0x2002c)!=expectedDebounce||completion!=int(complete)||phase!=(complete?2:3)){printf("Timer mismatch %u\n",n);return 1;}completed+=completion;
 }
 printf("20,000 original356198+342D88 composed timer ticks match state/callback order; %u completions\n",completed);
}
