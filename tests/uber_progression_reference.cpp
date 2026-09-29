#include "ps2_runtime_macros.h"
#include "../engine/boost_award.hpp"
#include <fstream>
#include <random>
#include <cstdio>
#include <bit>
void sub_0010E910_0x10e910(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x410000,g_ps2RecompiledFunctionTableSlotCount=0xc4000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xc4000]={nullptr};
static int32_t afterCount;
int main(int argc,char**argv){
 if(argc!=2)return 1;std::ifstream file(argv[1],std::ios::binary);std::vector<uint8_t> m((std::istreambuf_iterator<char>(file)),{});if(m.size()!=32*1024*1024)return 2;
 PS2Runtime rt;rt.registerFunction(0x119d40,[](uint8_t*m,R5900Context*c,PS2Runtime*){std::memcpy(m+GPR_U32(c,4)+0x114,&afterCount,4);c->f[0]=0;c->pc=GPR_U32(c,31);});
 for(unsigned a:{0x270870,0x111aa0,0x10e028,0x14dc80,0x14dd58,0x149778,0x10e098,0x2948d0,0x100000,0x100008})rt.registerFunction(a,[](uint8_t*,R5900Context*c,PS2Runtime*){SET_GPR_U32(c,2,0);c->pc=GPR_U32(c,31);});
 auto w=[&](unsigned a,auto v){std::memcpy(m.data()+a,&v,sizeof(v));};auto u=[&](unsigned a){uint32_t v;std::memcpy(&v,m.data()+a,4);return v;};
 constexpr unsigned rider=0x14701a0,score=0x20000,vtable=0x50000;
 w(rider+0x790,score);w(rider+0x6c0,vtable);w(vtable+0x40,int16_t(0));w(vtable+0x44,0x100000u);w(vtable+0x88,int16_t(0));w(vtable+0x8c,0x100008u);
 std::mt19937 rng(0x10e9b4);
 for(unsigned n=0;n<20000;n++){
  int32_t before=int(rng()%200);afterCount=before+int(rng()%40)-10;
  ssx::OriginalBoostState s;s.tier=int(n%16)-2;s.superTime=float(rng()%100);
  w(score+0x114,before);w(rider+0x2f4,s.tier);w(rider+0x2f0,s.superTime);
  R5900Context c{};c.pc=0x10e910;SET_GPR_U32(&c,4,rider);SET_GPR_U32(&c,8,0x60000);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,29,0x10000);SET_GPR_U32(&c,31,0x12345678);
  sub_0010E910_0x10e910(m.data(),&c,&rt);ssx::originalLandingUberProgression(s,before,afterCount);
  if(c.pc!=0x12345678||u(rider+0x2f4)!=uint32_t(s.tier)||u(rider+0x2f0)!=std::bit_cast<uint32_t>(s.superTime)){printf("Uber tier mismatch %u\n",n);return 3;}
 }
 puts("20000 original10E910 calls match committed-Uber tier and timer changes; unrelated score/effect callbacks isolated.");
}
