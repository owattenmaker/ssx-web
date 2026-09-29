#include "ps2_runtime_macros.h"
#include "../engine/upper_reaction.hpp"
#include "../engine/original_random.hpp"
#include <fstream>
#include <cstring>
#include <random>
#include <cstdio>
#include <cfenv>
#include <bit>
void sub_00115D48_0x115d48(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0031C228_0x31c228(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x340000,g_ps2RecompiledFunctionTableSlotCount=0x90000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0x90000]={};
static int cls,semantic;static uint32_t now;static uint64_t mask;static unsigned draws;static ssx::OriginalRandomState rng;
static void dependency(uint8_t*m,R5900Context*c,PS2Runtime*){
 switch(c->pc){
 case 0x311ae8:SET_GPR_U32(c,2,cls);break;
 case 0x1298c8:SET_GPR_U32(c,2,now);break;
 case 0x317810:++draws;SET_GPR_U32(c,2,rng.next());break;
 case 0x3128e8:semantic=GPR_U32(c,5);memcpy(&mask,m+GPR_U32(c,4)+0x20,8);break;
 default:throw std::runtime_error("Unknown upper oracle dependency");
 }c->pc=GPR_U32(c,31);
}
int main(int argc,char**argv){
 if(argc!=2)return 2;std::ifstream in(argv[1],std::ios::binary);std::vector<uint8_t> m((std::istreambuf_iterator<char>(in)),{});if(m.size()!=32*1024*1024)return 3;
 PS2Runtime rt;for(unsigned pc:{0x311ae8,0x1298c8,0x317810,0x3128e8})rt.registerFunction(pc,dependency);rt.registerFunction(0x31c228,sub_0031C228_0x31c228);
 constexpr unsigned actor=0x30000,anim=0x40000,done=0x12345678;
 auto put=[&](unsigned at,const auto&v){memcpy(m.data()+at,&v,sizeof(v));};auto u=[&](unsigned at){uint32_t v;memcpy(&v,m.data()+at,4);return v;};
 put(actor+0x784,anim);std::mt19937 gen(0x115d48);std::uniform_real_distribution<float> unit(-1,1);
 unsigned requests[5]={};
 for(unsigned i=0;i<30000;++i){std::fesetround(FE_TOWARDZERO);ssx::OriginalUpperReactionContext context;context.upperClass=cls=i%9==0?3:0;context.reverseStance=i&1;context.clockTick=now=gen();context.physicalForward={i%17?unit(gen):0,i%19?unit(gen):0,0};context.reactionMask=(uint64_t(gen())<<32)|gen();context.lookbackMask=(uint64_t(gen())<<32)|gen();float idle=i%7==0?0:unit(gen)+2;
  std::array<ssx::OriginalUpperPeer,6> peers;for(unsigned k=0;k<6;++k){auto&p=peers[k];p.enabled=gen()%3;p.attackEligible=gen()%2;p.distanceCm=std::abs(unit(gen))*1600;p.bearing=unit(gen)*20;p.lastReactionTick=now-uint32_t(gen()%1200);put(actor+k*36,uint32_t(p.enabled));put(actor+k*36+8,p.distanceCm);put(actor+k*36+12,p.bearing);put(actor+k*36+28,uint32_t(p.attackEligible));put(actor+k*36+32,p.lastReactionTick);}
  put(actor+0x35c,idle);put(actor+0x1b0,context.physicalForward);put(actor+0x320,uint32_t(context.reverseStance));put(actor+0x8c0,context.reactionMask);put(actor+0x8d0,context.lookbackMask);
  for(auto&w:rng.words)w=gen();auto nativeRng=rng;semantic=-1;mask=~uint64_t(0);draws=0;
  R5900Context c{};c.pc=0x115d48;SET_GPR_U32((&c),4,actor);SET_GPR_U32((&c),28,0x4a30f0);SET_GPR_U32((&c),29,0x10000);SET_GPR_U32((&c),31,done);sub_00115D48_0x115d48(m.data(),&c,&rt);
  auto out=ssx::originalUpperReaction(idle,peers,context,[&](){return nativeRng.next();});
  bool okay=c.pc==done&&out.semantic==semantic&&out.mask==mask&&out.randomDraws==draws&&nativeRng.words==rng.words&&std::bit_cast<uint32_t>(idle)==u(actor+0x35c);
  for(unsigned k=0;k<6;++k)okay&=peers[k].lastReactionTick==u(actor+k*36+32);
  if(!okay){printf("Upper mismatch%u semantic%d/%d draws%u/%u timer%08x/%08x mask%llx/%llx\n",i,out.semantic,semantic,out.randomDraws,draws,std::bit_cast<uint32_t>(idle),u(actor+0x35c),(unsigned long long)out.mask,(unsigned long long)mask);return 4;}
  if(semantic>=316){unsigned index=semantic==316?0:semantic==317?1:semantic-317;requests[index]++;}
 }
 printf("30,000 full original115D48 cases exact: timer, RNG state/draw count, semantic/mask, all peer timestamps; requests316/317/319/320/321=%u/%u/%u/%u/%u\n",requests[0],requests[1],requests[2],requests[3],requests[4]);
}
