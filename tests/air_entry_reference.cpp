#include "ps2_runtime_macros.h"
#include "../engine/air_entry.hpp"
#include <fstream>
#include <cstdio>
#include <random>
#include <bit>
#include <cfenv>
void sub_001162C8_0x1162c8(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0012E9B8_0x12e9b8(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x320000,g_ps2RecompiledFunctionTableSlotCount=0x88000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0x88000]={};
static int animationClass,animationIndex;static bool reverseTriggered,reverseRequested;
int main(int argc,char**argv){PS2Runtime rt;std::ifstream file(argv[1],std::ios::binary);std::vector<uint8_t> m((std::istreambuf_iterator<char>(file)),{});if(m.size()!=32*1024*1024)return 2;
 for(unsigned pc:{0x116378u,0x116120u,0x106848u,0x114130u,0x113f88u,0x113e80u,0x113f38u,0x12ee30u})rt.registerFunction(pc,[](uint8_t*,R5900Context*c,PS2Runtime*){SET_GPR_U32(c,2,0);c->pc=GPR_U32(c,31);});
 rt.registerFunction(0x311ae8,[](uint8_t*,R5900Context*c,PS2Runtime*){SET_GPR_S32(c,2,animationClass);c->pc=GPR_U32(c,31);});rt.registerFunction(0x312aa0,[](uint8_t*,R5900Context*c,PS2Runtime*){SET_GPR_S32(c,2,animationIndex);c->pc=GPR_U32(c,31);});
 rt.registerFunction(0x114cc0,[](uint8_t*,R5900Context*c,PS2Runtime*){SET_GPR_U32(c,2,reverseTriggered);c->pc=GPR_U32(c,31);});rt.registerFunction(0x3128e8,[](uint8_t*,R5900Context*c,PS2Runtime*){reverseRequested=true;c->pc=GPR_U32(c,31);});
 auto wr=[&](unsigned a,auto x){std::memcpy(m.data()+a,&x,sizeof(x));};auto rd=[&](unsigned a){float f;std::memcpy(&f,m.data()+a,4);return f;};
 std::fesetround(FE_TOWARDZERO);std::mt19937 rng(0x12e9b8);std::uniform_real_distribution<float> f(-1,1);unsigned reverseCases=0;
 for(unsigned k=0;k<20000;k++){
  ssx::OriginalAirPrewindState s;s.spin={f(rng),std::abs(f(rng)),f(rng)};s.flip={f(rng),std::abs(f(rng)),f(rng)};
  ssx::OriginalAirPrewindContext context;context.style=k%4;context.animationClass=animationClass=k%11;context.animationIndex=animationIndex=k%25;context.manualSpin=k%3?0:f(rng);context.reverseTurnTriggered=reverseTriggered=k%5==0;reverseRequested=false;
  int spin=int(rng()%64)-32,flip=int(rng()%64)-32;uint32_t command=0x2000u|((uint32_t(spin)&63)<<15)|((uint32_t(flip)&63)<<21);
  wr(0x20000,0x30000u);wr(0x40000,command);wr(0x40004,0u);wr(0x302a4,s.spin);wr(0x302b0,s.flip);wr(0x30328,context.style);wr(0x302dc,context.manualSpin);
  R5900Context c{};c.vu0_vf[0]=_mm_set_ps(1,0,0,0);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,29,0x10000);SET_GPR_U32(&c,31,0x12345678);SET_GPR_U32(&c,4,0x20000);SET_GPR_U32(&c,5,0x40000);
  sub_0012E9B8_0x12e9b8(m.data(),&c,&rt);float unit=std::bit_cast<float>(0x3d042108u);bool updated=ssx::originalAirPrewindTargets(s,float(spin)*unit,float(flip)*unit,context);
  if(c.pc!=0x12345678||updated==reverseRequested){printf("branch mismatch%u\n",k);return 3;}reverseCases+=reverseRequested;
  const float values[]={s.spin.current,s.spin.rate,s.spin.target,s.flip.current,s.flip.rate,s.flip.target};for(int i=0;i<6;i++)if(values[i]!=rd(0x302a4+i*4)){printf("prewind%u/%d expected%.9g actual%.9g\n",k,i,rd(0x302a4+i*4),values[i]);return 4;}
 }
 printf("20000 original held-control prewind target/rate cases match exactly (%u reverse-animation boundaries)\n",reverseCases);
 static bool changed;rt.registerFunction(0x2f6ac8,[](uint8_t*,R5900Context*c,PS2Runtime*){c->pc=GPR_U32(c,31);});rt.registerFunction(0x11fee8,[](uint8_t*,R5900Context*c,PS2Runtime*){SET_GPR_U32(c,2,0);c->pc=GPR_U32(c,31);});rt.registerFunction(0x11fec8,[](uint8_t*,R5900Context*c,PS2Runtime*){changed=true;c->pc=GPR_U32(c,31);});
 for(unsigned k=0;k<2000;k++){
  float gate=k%3==0?0:k%3==1?1:f(rng);bool held=k&1,pressed=k&2;wr(0x30360,gate);changed=false;
  R5900Context c{};SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,29,0x10000);SET_GPR_U32(&c,31,0x12345678);SET_GPR_U32(&c,4,0x30000);SET_GPR_U32(&c,5,held);SET_GPR_U32(&c,6,pressed);sub_001162C8_0x1162c8(m.data(),&c,&rt);bool enter=ssx::originalCrouchRequest(gate,held,pressed);auto* cp=&c;
  if(enter!=changed||enter!=bool(GPR_U32(cp,2))||gate!=rd(0x30360)){printf("jump gate%u\n",k);return 5;}
 }
 puts("2000 original held/pressed/latch crouch-entry cases match exactly");

}
