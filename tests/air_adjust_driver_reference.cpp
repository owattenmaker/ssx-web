#include "ps2_runtime_macros.h"
#include "../engine/animation_sequence.hpp"
#include <fstream>
#include <cfenv>
#include <cstdio>
#include <random>
#include <bit>
void sub_001043F8_0x1043f8(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00313C50_0x313c50(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00313CF0_0x313cf0(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00313D28_0x313d28(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00313D40_0x313d40(uint8_t*,R5900Context*,PS2Runtime*);
void sub_003135B0_0x3135b0(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00313800_0x313800(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x420000,g_ps2RecompiledFunctionTableSlotCount=0xc8000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xc8000]={};
static bool removed;
int main(int argc,char**argv){
 setbuf(stdout,nullptr);PS2Runtime rt;std::ifstream file(argv[1],std::ios::binary);std::vector<uint8_t> m((std::istreambuf_iterator<char>(file)),{});if(m.size()!=32*1024*1024)return 2;
 auto u=[&](unsigned at){uint32_t x;std::memcpy(&x,m.data()+at,4);return x;};auto f=[&](unsigned at){return std::bit_cast<float>(u(at));};auto w=[&](unsigned at,auto x){std::memcpy(m.data()+at,&x,sizeof(x));};auto half=[&](unsigned at){uint16_t x;std::memcpy(&x,m.data()+at,2);return x;};
 rt.registerFunction(0x313c50,sub_00313C50_0x313c50);rt.registerFunction(0x313cf0,sub_00313CF0_0x313cf0);rt.registerFunction(0x313d28,sub_00313D28_0x313d28);rt.registerFunction(0x313d40,sub_00313D40_0x313d40);rt.registerFunction(0x3135b0,sub_003135B0_0x3135b0);rt.registerFunction(0x313800,sub_00313800_0x313800);
 rt.registerFunction(0x3145f8,[](uint8_t*,R5900Context*c,PS2Runtime*){if(removed||GPR_U32(c,4)!=0x80000||GPR_U32(c,5)!=0x60000)throw std::runtime_error("Invalid driver removal request");removed=true;c->pc=GPR_U32(c,31);});
 std::fesetround(FE_TOWARDZERO);auto packed=[&](unsigned leaf){return u(u(0x4a30f0+0xd8c)+0x1030+leaf*4);};auto duration=[&](uint32_t id){unsigned bank=u(u(0x4a30f0+0xd08)+(id&255)*4);unsigned desc=u(bank+8)+(id>>8)*20;return float(int(half(desc+12))-1)*std::bit_cast<float>(0x3d088889u);};
 constexpr std::array<unsigned,8> primaryLeaves={157,163,160,164,158,162,159,161};constexpr std::array<unsigned,8> secondaryLeaves={165,171,168,172,166,170,167,169};
 std::mt19937 rng(0x1043f8);std::uniform_real_distribution<float> value(-1,1);unsigned removals=0,initializations=0;
 for(unsigned k=0;k<30000;++k){
  ssx::OriginalAnimationSequence s;s.semantic=k%10==9?-1:297+int(k%8);unsigned choice=k%10==9?0:k%8;
  s.rate=value(rng)*2;s.weight=(value(rng)+1)*.5f;s.targetWeight=k%2?1:0;s.fadeRemaining=k%5?std::abs(value(rng))*.2f:0;s.stopWhenFaded=k%3==0;s.completed=k%2;s.flags=rng();s.raisedFlags=rng();s.seekPending=k%2;
  unsigned primary=packed(primaryLeaves[choice]),secondary=packed(secondaryLeaves[choice]);float secondaryDuration=duration(secondary);
  s.slots.push_back({primary,value(rng)*3,duration(primary),value(rng)*2,value(rng),bool(k%2),true});
  bool fresh=k%3==0;if(!fresh)s.slots.push_back({packed(165+unsigned(rng()%8)),value(rng)*3,1,value(rng)*2,value(rng),false,true});else ++initializations;
  float adjustFlip=value(rng)*2,adjustSpin=value(rng)*2,timeScale=.25f+std::abs(value(rng))*2;
  if(k%11==0)adjustFlip=adjustSpin=0;if(k%17==0)adjustSpin=1;
  memset(m.data()+0x60000,0,0xd0);w(0x50060,0x40000u);w(0x4028c,adjustFlip);w(0x40298,adjustSpin);w(0x60000,s.semantic);
  for(unsigned n=0;n<s.slots.size();++n){unsigned at=0x60000+n*28;const auto&slot=s.slots[n];w(at+4,slot.clip);w(at+8,slot.time);w(at+12,slot.rate);w(at+16,slot.duration);w(at+20,slot.weight);w(at+24,uint32_t(slot.enabled));w(at+28,uint32_t(slot.loop));}
  w(0x60090,s.rate);w(0x60094,s.weight);w(0x60098,s.targetWeight);w(0x6009c,s.fadeRemaining);w(0x600a0,uint32_t(s.stopWhenFaded));w(0x600b0,s.flags);w(0x600b8,s.raisedFlags);w(0x600c0,uint32_t(s.completed));w(0x600c4,uint32_t(s.seekPending));
  R5900Context c{};c.pc=0x1043f8;c.f[12]=timeScale;SET_GPR_U32(&c,4,0x50000);SET_GPR_U32(&c,5,0x80000);SET_GPR_U32(&c,6,0x60000);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,29,0x10000);SET_GPR_U32(&c,31,0x12345678);removed=false;sub_001043F8_0x1043f8(m.data(),&c,&rt);
  unsigned leaf=ssx::originalAirAdjustSecondaryLeaf(s.semantic);if(leaf!=secondaryLeaves[choice])return 3;bool actual=ssx::originalAnimationAirAdjustStep(s,packed(leaf),secondaryDuration,adjustFlip,adjustSpin,timeScale);removals+=actual;
  if(c.pc!=0x12345678||actual!=removed||actual!=bool(GPR_U32((&c),2))||s.slots.size()!=2){printf("Driver return mismatch%u\n",k);return 4;}
  for(unsigned n=0;n<2;++n){unsigned at=0x60000+n*28;const auto&slot=s.slots[n];if(slot.clip!=u(at+4)||uint32_t(slot.enabled)!=u(at+24)||uint32_t(slot.loop)!=u(at+28)){printf("Driver slot identity/flags mismatch%u/%u\n",k,n);return 5;}for(auto[off,x]:{std::pair{8,slot.time},std::pair{12,slot.rate},std::pair{16,slot.duration},std::pair{20,slot.weight}})if(u(at+off)!=std::bit_cast<uint32_t>(x)){printf("Driver slot mismatch%u/%u off%x original%.9g native%.9g\n",k,n,off,f(at+off),x);return 6;}}
  for(auto[off,x]:{std::pair{0x90,s.rate},std::pair{0x94,s.weight},std::pair{0x98,s.targetWeight},std::pair{0x9c,s.fadeRemaining}})if(u(0x60000+off)!=std::bit_cast<uint32_t>(x)){printf("Driver fade mismatch%u off%x\n",k,off);return 7;}
  uint64_t flags,raised;memcpy(&flags,m.data()+0x600b0,8);memcpy(&raised,m.data()+0x600b8,8);if(flags!=s.flags||raised!=s.raisedFlags||u(0x600c0)!=uint32_t(s.completed)||u(0x600c4)!=uint32_t(s.seekPending)){printf("Driver event/completion mismatch%u\n",k);return 8;}
 }
 printf("30000 complete original1043F8 kind11 updates exact:clip mappings,primary seeks,two-slot weights,secondary loops,flags/completion preservation andfade/removal (%u slot initializations,%u removals)\n",initializations,removals);
}
