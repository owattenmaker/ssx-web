#include "ps2_runtime_macros.h"
#include "../engine/rail_exit_mask.hpp"
#include <vector>
#include <cstring>
#include <fstream>
#include <cstdio>
void sub_0013BFA8_0x13bfa8(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x420000,g_ps2RecompiledFunctionTableSlotCount=0xc8000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xc8000]={};static int control;static std::vector<uint32_t> calls;
int main(int argc,char**argv){if(argc!=2)return 1;std::ifstream f(argv[1],std::ios::binary);std::vector<uint8_t>m((std::istreambuf_iterator<char>(f)),{});if(m.size()!=32*1024*1024)return 2;PS2Runtime rt;
 rt.registerFunction(0x11fee8,[](uint8_t*,R5900Context*c,PS2Runtime*){SET_GPR_U32(c,2,control);c->pc=GPR_U32(c,31);});auto put=[&](unsigned a,uint32_t v){std::memcpy(m.data()+a,&v,4);};
 for(unsigned pc:{0x105398u,0x13c140u,0x107888u})rt.registerFunction(pc,[](uint8_t*m,R5900Context*c,PS2Runtime*){
  uint32_t mask;std::memcpy(&mask,m+0x40028,4);calls.push_back(c->pc);calls.push_back(mask);
  if(c->pc==0x13c140){if(GPR_U32(c,4)!=0x10000||GPR_U32(c,5)!=3)throw std::runtime_error("Wrong physical-query owner/mode");}
  else if(GPR_U32(c,4)!=0x20000)throw std::runtime_error("Wrong rail contact rider");
  if(c->pc==0x105398&&GPR_U32(c,5)!=0)throw std::runtime_error("Wrong trigger-query mode");
  c->pc=GPR_U32(c,31);
 });
 put(0x10050,0x20000);put(0x2077c,0x30000);put(0x20aa0,0x40000);
 for(control=0;control<14;control++)for(int identity=0;identity<4;identity++){
  calls.clear();put(0x30394,identity);put(0x40028,0xffffffff);R5900Context c{};c.pc=0x13bff0;SET_GPR_U32(&c,16,0x10000);SET_GPR_U32(&c,29,0x70000);SET_GPR_U32(&c,31,0x12345678);sub_0013BFA8_0x13bfa8(m.data(),&c,&rt);
  uint32_t actual;std::memcpy(&actual,m.data()+0x40028,4);if(c.pc!=0x12345678||actual!=0xffffffffu)return 3;
  uint32_t mask=0x12345678;std::vector<uint32_t> expected;
  ssx::originalRailExitContactPhases(mask,control,identity,[&]{expected.insert(expected.end(),{0x105398u,mask});},[&]{expected.insert(expected.end(),{0x13c140u,mask});},[&]{expected.insert(expected.end(),{0x107888u,mask});});
  if(mask!=actual||calls!=expected)return 4;
 }
 puts("56 original rail-exit mask/query/rebuild sequences match");
}
