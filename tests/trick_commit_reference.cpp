#include "ps2_runtime_macros.h"
#include "../engine/trick_commit.hpp"
#include <fstream>
#include <random>
#include <cstdio>
#include <bit>
#include <cfenv>
#include "commit_registry.inc"
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x410000,g_ps2RecompiledFunctionTableSlotCount=0xc4000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xc4000]={nullptr};
int main(int argc,char**argv){
 if(argc!=2)return 1;std::ifstream file(argv[1],std::ios::binary);std::vector<uint8_t> m((std::istreambuf_iterator<char>(file)),{});if(m.size()!=32*1024*1024)return 2;
 PS2Runtime rt;registerCommit(rt);
 rt.registerFunction(0x3e6448,[](uint8_t*m,R5900Context*c,PS2Runtime*){std::memset(m+GPR_U32(c,4),GPR_U32(c,5),GPR_U32(c,6));c->pc=GPR_U32(c,31);});
 for(unsigned at:{0x1179e0,0x12a250,0x117b88,0x28b180,0x29b7e0,0x118ff8,0x14dc80,0x14dd58,0x29b430,0x119ef8})rt.registerFunction(at,[](uint8_t*,R5900Context*c,PS2Runtime*){SET_GPR_U32(c,2,0);c->pc=GPR_U32(c,31);});
 auto w=[&](unsigned a,auto v){std::memcpy(m.data()+a,&v,sizeof(v));};auto bits=[&](unsigned a){uint32_t v;std::memcpy(&v,m.data()+a,4);return v;};auto f=[&](unsigned a){return std::bit_cast<float>(bits(a));};
 ssx::OriginalTrickCommitProfile p;p.identity.spinDegrees=f(0x49b70c);p.identity.flipDegrees=f(0x49b710);p.namedPointScale=f(0x49b6fc);p.scoreScale=f(0x49b5ec);p.spinScale=f(0x49b6dc);p.flipScale=f(0x49b6e0);std::memcpy(p.identity.ordinary.data(),m.data()+0x43d388,319);std::memcpy(p.identity.alternate.data(),m.data()+0x43d4c8,319);
 for(unsigned n=0;n<24;n++){auto a=0x43d608+n*16;p.named[n].id=bits(a);p.named[n].points=int32_t(bits(a+4));std::memcpy(p.named[n].identityFields.data(),m.data()+a+8,7);}
 std::fesetround(FE_TOWARDZERO);std::mt19937 rng(0x11a228);std::uniform_real_distribution<float> amount(.001,3),angle(-30,30);
 constexpr unsigned base=0x20000;
 for(unsigned n=0;n<6000;n++){
  std::memset(m.data()+base,0,0x1c8);w(0x535c10,uint8_t(0));
  ssx::OriginalGrabScoreState s;s.accumulated14=n%11?amount(rng):0;s.multiplier1C4=.5f+float(n%5)/2;s.bonusPoints84=int(n%100)*10;s.uberCount54=n%3;
  ssx::OriginalTrickIdentityState id;id.spin34=n%3?angle(rng):0;id.flip38=n%4?angle(rng):0;id.stance00=n%2;id.field04=n%3==0;
  for(unsigned k=0;k<n%4;k++)id.grabs60[k]=1+rng()%60;
  ssx::OriginalTrickIdentityInput input;input.alternate=n%2;input.stanceChanged=n%3==0;
  auto copy=id;auto identity=ssx::originalTrickIdentity(copy,p.identity,input);ssx::originalNamedTrickBonus(identity.identity,p.named);
  ssx::OriginalTrickHistory history;history.next=n%10;for(unsigned k=0;k<n%11;k++)history.entries[k]=identity.identity;
  float multiplier=1.f+float(n%3)/2;
  w(base,id.stance00);w(base+4,id.field04);w(base+0x14,s.accumulated14);w(base+0x18,multiplier);w(base+0x34,id.spin34);w(base+0x38,id.flip38);w(base+0x54,s.uberCount54);w(base+0x60,id.grabs60);w(base+0x84,s.bonusPoints84);w(base+0xa4,-1.f);w(base+0xa8,history.entries);w(base+0xf8,history.next);w(base+0x1c4,s.multiplier1C4);w(base+0x1ac,0x40000u);
  R5900Context c{};c.pc=0x11a228;SET_GPR_U32(&c,4,base);SET_GPR_U32(&c,5,input.stanceChanged);SET_GPR_U32(&c,6,input.alternate);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,29,0x10000);SET_GPR_U32(&c,31,0x12345678);
  float scoredSpin=n%2?angle(rng):id.spin34,scoredFlip=n%2?angle(rng):id.flip38;
  for(auto [pc,value]:{std::pair{0x119898u,scoredSpin},std::pair{0x1198d8u,scoredFlip}}){auto rotation=c;rotation.pc=pc;rotation.f[12]=value;rt.lookupFunction(pc)(m.data(),&rotation,&rt);}
  ssx::originalTrickRotationScore(s,id,p,scoredSpin,scoredFlip);
  sub_0011A228_0x11a228(m.data(),&c,&rt);auto result=ssx::originalOrdinaryTrickCommit(s,id,history,p,input,multiplier);
  if(c.pc!=0x12345678||std::bit_cast<uint32_t>(c.f[0])!=std::bit_cast<uint32_t>(result.meterDelta)||bits(base+0x198)!=uint32_t(result.points)||bits(base+0x114)!=uint32_t(result.committedUbers)||bits(base+0x14)!=std::bit_cast<uint32_t>(s.accumulated14)||std::memcmp(m.data()+base+0xa8,history.entries.data(),80)||bits(base+0xf8)!=history.next){printf("Commit mismatch %u delta%08x/%08x points%d/%d repeat%d\n",n,std::bit_cast<uint32_t>(c.f[0]),std::bit_cast<uint32_t>(result.meterDelta),int(bits(base+0x198)),result.points,result.repeats);return 3;}
 }
 puts("6000 complete original11A228 ordinary calls match meter delta, banked points, normalized score, repeat history and cursor; external notification callbacks isolated.");
}
