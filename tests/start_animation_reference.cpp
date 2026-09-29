#include "ps2_runtime_macros.h"
#include "../engine/animation_sequence.hpp"
#include "../engine/original_float.hpp"
#include <fstream>
#include <random>
#include <cstring>
#include <cstdio>
void sub_001045D8_0x1045d8(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00313CF0_0x313cf0(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00313800_0x313800(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x420000,g_ps2RecompiledFunctionTableSlotCount=0xc8000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xc8000]={};static bool removed;
int main(int argc,char**argv){if(argc!=2)return 1;std::ifstream f(argv[1],std::ios::binary);std::vector<uint8_t>m((std::istreambuf_iterator<char>(f)),{});if(m.size()!=32*1024*1024)return 2;PS2Runtime rt;std::mt19937 rng(0x1045d8);
 auto put=[&](unsigned a,const auto&v){std::memcpy(m.data()+a,&v,sizeof(v));};auto word=[&](unsigned a){uint32_t x;std::memcpy(&x,m.data()+a,4);return x;};auto value=[&](){return float(rng()%10000)*.0001f;};
 rt.registerFunction(0x313cf0,sub_00313CF0_0x313cf0);rt.registerFunction(0x313800,sub_00313800_0x313800);
 rt.registerFunction(0x3145f8,[](uint8_t*,R5900Context*c,PS2Runtime*){removed=true;c->pc=GPR_U32(c,31);});put(0x50060,0x40000u);put(0x4077c,0x70000u);
 for(unsigned n=0;n<20000;n++){
  ssx::OriginalAnimationSequence s;s.semantic=0;s.rate=value();s.weight=value();s.targetWeight=n%2;s.fadeRemaining=n%5?value()*.2f:0;s.stopWhenFaded=n%3==0;s.completed=n%2;s.seekPending=n%2;s.slots.push_back({8192,value()*5,.1f+value()*3,1,1,false,true});float pose=value()*2-.5f,scale=.25f+value()*2;
  std::memset(m.data()+0x60000,0,0xd0);put(0x702a0,pose);put(0x60004,s.slots[0].clip);put(0x60008,s.slots[0].time);put(0x60010,s.slots[0].duration);put(0x60090,s.rate);put(0x60094,s.weight);put(0x60098,s.targetWeight);put(0x6009c,s.fadeRemaining);put(0x600a0,int(s.stopWhenFaded));put(0x600c0,int(s.completed));put(0x600c4,int(s.seekPending));
  R5900Context c{};c.pc=0x1045d8;c.f[12]=scale;SET_GPR_U32(&c,4,0x50000);SET_GPR_U32(&c,5,0x80000);SET_GPR_U32(&c,6,0x60000);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,29,0x10000);SET_GPR_U32(&c,31,0x12345678);removed=false;
  ssx::OriginalRounding rounding;sub_001045D8_0x1045d8(m.data(),&c,&rt);bool result=ssx::originalAnimationStartStep(s,pose,scale);bool okay=c.pc==0x12345678&&result==removed&&word(0x600c4)==unsigned(s.seekPending)&&word(0x600c0)==unsigned(s.completed);
  for(auto [offset,v]:{std::pair{8,s.slots[0].time},std::pair{0x90,s.rate},std::pair{0x94,s.weight},std::pair{0x98,s.targetWeight},std::pair{0x9c,s.fadeRemaining}})okay&=word(0x60000+offset)==std::bit_cast<uint32_t>(v);
  if(!okay){printf("Start animation mismatch %u\n",n);return 3;}
 }
 puts("20,000 original kind8 start-animation seeks/fades match, with no elapsed clip advance");
}
