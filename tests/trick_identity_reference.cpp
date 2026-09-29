#include "ps2_runtime_macros.h"
#include "../engine/trick_identity.hpp"
#include <fstream>
#include <random>
#include <cstdio>
#include <bit>
#include <cfenv>
void sub_0011A8C8_0x11a8c8(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x410000,g_ps2RecompiledFunctionTableSlotCount=0xc4000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xc4000]={nullptr};
int main(int argc,char**argv){
 if(argc!=2)return 1;std::ifstream f(argv[1],std::ios::binary);std::vector<uint8_t> m((std::istreambuf_iterator<char>(f)),{});if(m.size()!=32*1024*1024)return 2;
 PS2Runtime rt;rt.registerFunction(0x3e6448,[](uint8_t*m,R5900Context*c,PS2Runtime*){std::memset(m+GPR_U32(c,4),GPR_U32(c,5),GPR_U32(c,6));c->pc=GPR_U32(c,31);});
 auto w=[&](unsigned a,auto v){std::memcpy(m.data()+a,&v,sizeof(v));};auto bits=[&](unsigned a){uint32_t v;std::memcpy(&v,m.data()+a,4);return v;};
 ssx::OriginalTrickIdentityProfile p;p.spinDegrees=std::bit_cast<float>(bits(0x4a30f0-0x79e4));p.flipDegrees=std::bit_cast<float>(bits(0x4a30f0-0x79e0));std::memcpy(p.ordinary.data(),m.data()+0x43d388,319);std::memcpy(p.alternate.data(),m.data()+0x43d4c8,319);
 std::fesetround(FE_TOWARDZERO);std::mt19937 rng(0x11a8c8);std::uniform_real_distribution<float> angles(-80,80),times(0,80);
 constexpr unsigned base=0x20000,out=0x30000,rider=0x40000;
 for(unsigned n=0;n<40000;n++){
  ssx::OriginalTrickIdentityState s;s.stance00=rng()%2;s.field04=rng()%2;s.field08=rng()%2;s.style0C=rng()%5;s.flag10=rng()%3;s.style20=n%4?0:rng()%6;s.flag28=n%3?0:1+rng()%2;s.active70=n%13?0:1+rng()%5;s.field7C=n%5?0:1+rng()%3;s.time24=times(rng);s.time2C=times(rng);s.spin34=n%6?angles(rng):0;s.flip38=n%7?angles(rng):0;
  for(unsigned k=0;k<n%4;k++)s.grabs60[k]=1+rng()%64;
  ssx::OriginalTrickIdentityInput in{bool(rng()%2),bool(rng()%2),int32_t(rng()%3),int32_t(rng()%3),bool(rng()%2)};
  for(auto [o,v]:{std::pair{0u,s.stance00},{4u,s.field04},{8u,s.field08},{12u,s.style0C},{16u,s.flag10},{0x20u,s.style20},{0x28u,s.flag28},{0x70u,s.active70},{0x7cu,s.field7C}})w(base+o,v);
  w(base+0x24,s.time24);w(base+0x2c,s.time2C);w(base+0x34,s.spin34);w(base+0x38,s.flip38);w(base+0x60,s.grabs60);w(base+0x1ac,rider);w(rider+0x324,uint32_t(in.riderStance));
  R5900Context c{};c.pc=0x11a8c8;SET_GPR_U32(&c,4,base);SET_GPR_U32(&c,5,out);SET_GPR_U32(&c,6,in.stanceChanged);SET_GPR_U32(&c,7,in.alternate);SET_GPR_U32(&c,8,in.style);SET_GPR_U32(&c,9,in.flag);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,29,0x10000);SET_GPR_U32(&c,31,0x12345678);
  auto before=s;sub_0011A8C8_0x11a8c8(m.data(),&c,&rt);auto r=ssx::originalTrickIdentity(s,p,in);auto*cp=&c;
  if(c.pc!=0x12345678||GPR_U32(cp,2)!=unsigned(r.valid)||std::memcmp(m.data()+out,r.identity.data(),8)||bits(base+0x34)!=std::bit_cast<uint32_t>(s.spin34)||bits(base+0x38)!=std::bit_cast<uint32_t>(s.flip38)){
   printf("Identity mismatch %u original %08x %08x valid%u native %08x %08x valid%d spin%f flip%f stance%d style%d flag%d active%d input%d/%d/%d/%d\n",n,bits(out),bits(out+4),GPR_U32(cp,2),r.identity[0],r.identity[1],r.valid,before.spin34,before.flip38,before.stance00,before.style20,before.flag28,before.active70,in.stanceChanged,in.alternate,in.style,in.flag);return 3;
  }
 }
 puts("40000 original11A8C8 cases match both identity words, validity and mutated spin/flip fields.");
}
