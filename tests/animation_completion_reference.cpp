// Instruction oracle for engine/animation_completion.hpp: the recompiled completion
// dispatch 0x103918 (inside sub_00103578) with every completion handler of table
// 0x456990 (0x104A40, 0x104A60 + table 0x4569E0, 0x104B48, 0x104B78, 0x104B98,
// 0x104BB8, 0x104BD8, 0x104C18, 0x104C38, 0x104C80, 0x104CA0), the real 0x312B18,
// 0x312BD0 and 0x144670, against originalAnimationCompletion. Recording stubs:
// 0x3115C8 (state table: +8 = completion kind), 0x3145F8 (remove), 0x3128E8 (play),
// 0x314760 (channel head), 0x313A10 (fade). Compared: callee order and arguments,
// the head's +0x94/+0x98 weight copy, the finished sequence's +0xB0/+0xB8 flag
// words and the value 0x103918 stores into the animator's requested slot
// (0x312B18 returns its semantic, 0x312BD0 the 0x3128E8 result, 0x104A60/0x104CA0 438).
#include "ps2_runtime_macros.h"
#include "../engine/animation_completion.hpp"
#include <bit>
#include <cstring>
#include <cstdio>
#include <random>
#include <vector>
void sub_00103578_0x103578(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00104A40_0x104a40(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00104A60_0x104a60(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00104B48_0x104b48(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00104B78_0x104b78(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00104B98_0x104b98(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00104BB8_0x104bb8(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00104BD8_0x104bd8(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00104C18_0x104c18(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00104C38_0x104c38(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00104C80_0x104c80(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00104CA0_0x104ca0(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00312B18_0x312b18(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00312BD0_0x312bd0(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00144670_0x144670(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x420000,g_ps2RecompiledFunctionTableSlotCount=0xc8000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xc8000]={};
using namespace ssx;
struct Event {uint32_t id;std::vector<uint32_t> w;bool operator==(const Event&)const=default;};
static std::vector<Event> events;
static uint8_t* mem;
static constexpr uint32_t anim=0x200000,table=0x201000,seq=0x210000,head=0x220000,rider=0x230000,def=0x240000,done=0x12345678,gp=0x4a30f0;
static uint32_t bits(float f){return std::bit_cast<uint32_t>(f);}
static void put(uint32_t a,const auto&v){std::memcpy(mem+a,&v,sizeof(v));}
static uint32_t word(uint32_t a){uint32_t v;std::memcpy(&v,mem+a,4);return v;}
static uint64_t dword(uint32_t a){uint64_t v;std::memcpy(&v,mem+a,8);return v;}
static uint32_t playResult(int semantic){return 0xabc00000u|uint32_t(semantic);}
static void stub(uint8_t*,R5900Context*c,PS2Runtime*){
 const uint32_t pc=c->pc,a0=GPR_U32(c,4),a1=GPR_U32(c,5),a2=GPR_U32(c,6);
 switch(pc){
 case 0x3115c8:SET_GPR_U32(c,2,def);break; // semantic -> state descriptor
 case 0x3145f8:events.push_back({0x3145f8,{a0,a1}});break;
 case 0x3128e8:events.push_back({0x3128e8,{a0,a1,a2,bits(c->f[12])}});SET_GPR_U32(c,2,playResult(int(a1)));break;
 case 0x314760:events.push_back({0x314760,{a0,a1}});SET_GPR_U32(c,2,head);break;
 case 0x313a10:events.push_back({0x313a10,{a0,bits(c->f[12]),bits(c->f[13])}});break;
 default:throw std::runtime_error("Unexpected callee");
 }
 c->pc=GPR_U32(c,31);
}
static R5900Context context(uint32_t pc){R5900Context c{};c.pc=pc;c.vu0_vf[0]=_mm_set_ps(1,0,0,0);SET_GPR_U32(&c,28,gp);SET_GPR_U32(&c,29,0x90000);SET_GPR_U32(&c,31,done);return c;}
int main(){
 std::vector<uint8_t> memory(32*1024*1024);mem=memory.data();PS2Runtime rt;rt.setMissingFunctionPolicy(PS2Runtime::MissingFunctionPolicy::Stop);
 for(uint32_t pc:{0x3115c8u,0x3145f8u,0x3128e8u,0x314760u,0x313a10u})rt.registerFunction(pc,stub);
 rt.registerFunction(0x104a40,sub_00104A40_0x104a40);rt.registerFunction(0x104a60,sub_00104A60_0x104a60);rt.registerFunction(0x104b48,sub_00104B48_0x104b48);
 rt.registerFunction(0x104b78,sub_00104B78_0x104b78);rt.registerFunction(0x104b98,sub_00104B98_0x104b98);rt.registerFunction(0x104bb8,sub_00104BB8_0x104bb8);
 rt.registerFunction(0x104bd8,sub_00104BD8_0x104bd8);rt.registerFunction(0x104c18,sub_00104C18_0x104c18);rt.registerFunction(0x104c38,sub_00104C38_0x104c38);
 rt.registerFunction(0x104c80,sub_00104C80_0x104c80);rt.registerFunction(0x104ca0,sub_00104CA0_0x104ca0);
 rt.registerFunction(0x312b18,sub_00312B18_0x312b18);rt.registerFunction(0x312bd0,sub_00312BD0_0x312bd0);rt.registerFunction(0x144670,sub_00144670_0x144670);
 // ELF jump tables 0x456990 (completion kind) and 0x4569E0 (kind 2 by semantic - 269), read by the original.
 const uint32_t kinds[11]={0x103978,0x103990,0x1039a8,0x1039c0,0x1039d8,0x1039f0,0x103a08,0x103a20,0x103a38,0x103a50,0x103a68};
 for(unsigned i=0;i<11;i++)put(0x456990+4*i,kinds[i]);
 const uint32_t kind2[18]={0x104ac0,0x104ad4,0x104a98,0x104aac,0x104ae8,0x104afc,0x104b10,0x104b24,0x104b38,0x104ac0,0x104ad4,0x104aac,0x104aac,0x104b38,0x104ac0,0x104ad4,0x104a98,0x104aac};
 for(unsigned i=0;i<18;i++)put(0x4569e0+4*i,kind2[i]);
 std::mt19937 rng(0x103918);auto uniform=[&](float a,float b){return std::uniform_real_distribution<float>(a,b)(rng);};
 unsigned counts[4]={},cases=0;
 for(unsigned n=0;n<60000;n++){
  events.clear();std::memset(mem+anim,0,0x100);std::memset(mem+seq,0,0xd0);std::memset(mem+head,0,0xd0);std::memset(mem+def,0,0x20);
  const uint32_t kind=n%11;const unsigned channel=rng()%6;
  int semantic;switch(rng()%4){case 0:semantic=int(rng()%460);break;case 1:semantic=269+int(rng()%18);break;case 2:semantic=68+int(rng()%3);break;default:semantic=49+int(rng()%6);}
  const uint32_t word64=rng()%3==0?0u:rng();const int32_t press=int32_t(rng()%3);
  put(anim+0x50,table);put(anim+0x60,rider);put(anim+0x64,word64);put(rider+0x330,press);
  const uint32_t list=table+8*channel;put(list,1u);put(list+4,seq);
  for(unsigned c=0;c<6;c++)put(anim+4*c,0x5eed0000u+c);
  put(seq,semantic);put(def+8,kind);
  const float weight=uniform(0,1),target=rng()%2?0.f:uniform(0,1),fade=rng()%3==0?0.f:uniform(0,.3f);put(seq+0x94,weight);put(seq+0x98,target);put(seq+0x9c,fade);
  const uint64_t latched=(uint64_t(rng())<<32|rng())|(rng()%5?(uint64_t(1)<<63):0),raised=uint64_t(rng())<<32|rng();put(seq+0xb0,latched);put(seq+0xb8,raised);
  auto c=context(0x103918);SET_GPR_U32(&c,4,anim);SET_GPR_U32(&c,5,list);SET_GPR_U32(&c,6,seq);
  sub_00103578_0x103578(mem,&c,&rt);
  if(c.pc!=done)throw std::runtime_error("0x103918 did not return");
  // Native expectation.
  const auto model=originalAnimationCompletion(kind,semantic,word64,press);using A=OriginalAnimationCompletion::Action;
  std::vector<Event> expected;uint32_t slot=438;uint64_t latchedAfter=latched,raisedAfter=raised;
  const uint32_t minusOne=bits(-1.f);
  switch(model.action){
  case A::Remove:expected.push_back({0x3145f8,{list,seq}});break;
  case A::Keep:break;
  case A::Replace:
   expected.push_back({0x3145f8,{list,seq}});expected.push_back({0x3128e8,{anim,uint32_t(model.semantic),0,minusOne}});expected.push_back({0x314760,{list,0}});
   if(fade!=0)expected.push_back({0x313a10,{head,bits(target),bits(fade)}});slot=uint32_t(model.semantic);break; // 0x312B18 returns its semantic argument
  case A::Play:{const uint64_t bit=uint64_t(1)<<63;raisedAfter|=latched&bit;latchedAfter&=~bit;expected.push_back({0x3128e8,{anim,uint32_t(model.semantic),0,minusOne}});slot=playResult(model.semantic);break;}
  }
  if(events!=expected){fprintf(stderr,"case %u kind %u semantic %d: callee sequence differs (%zu vs %zu events)\n",n,kind,semantic,events.size(),expected.size());return 1;}
  if(word(anim+4*channel)!=slot){fprintf(stderr,"case %u kind %u semantic %d: requested slot %x, native %x\n",n,kind,semantic,word(anim+4*channel),slot);return 1;}
  for(unsigned o=0;o<6;o++)if(o!=channel&&word(anim+4*o)!=0x5eed0000u+o){fprintf(stderr,"case %u: another channel slot written\n",n);return 1;}
  if(dword(seq+0xb0)!=latchedAfter||dword(seq+0xb8)!=raisedAfter){fprintf(stderr,"case %u kind %u: flag words differ\n",n,kind);return 1;}
  if(model.action==A::Replace&&(word(head+0x94)!=bits(weight)||word(head+0x98)!=bits(weight))){fprintf(stderr,"case %u kind %u: weight copy differs\n",n,kind);return 1;}
  ++counts[int(model.action)];++cases;
 }
 printf("animation completion 0x103918: %u cases match (remove %u, keep %u, replace %u, play %u)\n",cases,counts[0],counts[1],counts[2],counts[3]);
}
