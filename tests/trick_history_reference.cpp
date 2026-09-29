#include "ps2_runtime_macros.h"
#include "../engine/trick_history.hpp"
#include <fstream>
#include <random>
#include <cstdio>
void sub_001190F0_0x1190f0(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x410000,g_ps2RecompiledFunctionTableSlotCount=0xc4000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xc4000]={nullptr};
int main(int argc,char**argv){
 if(argc!=2)return 1;std::ifstream file(argv[1],std::ios::binary);std::vector<uint8_t> m((std::istreambuf_iterator<char>(file)),{});if(m.size()!=32*1024*1024)return 2;
 PS2Runtime rt;std::mt19937 rng(0x1190f0);
 auto w=[&](unsigned at,auto value){std::memcpy(m.data()+at,&value,sizeof(value));};
 constexpr unsigned score=0x20000,identity=0x30000;
 for(unsigned n=0;n<30000;n++){
  ssx::OriginalTrickHistory h;h.next=n%10;
  ssx::OriginalTrickIdentity id{uint32_t(rng()),uint32_t(rng())};
  if(n%4==0)id[1]=(id[1]&~0x3f800u)|0x800u;
  if(n%4==1)id[0]=(id[0]&~0xfc00000u)|0x400000u;
  if(n%7==0)id={0,0};
  for(auto& e:h.entries)e=rng()%3?id:ssx::OriginalTrickIdentity{uint32_t(rng()),uint32_t(rng())};
  ssx::OriginalTrickRepeatContext context;
  auto blocked=int(n%8);if(blocked<5){int* flags[]={&context.field08,&context.style20,&context.active70,&context.field7C,&context.flag28};*flags[blocked]=n%2?1:-1;}
  w(score+8,context.field08);w(score+0x20,context.style20);w(score+0x70,context.active70);w(score+0x7c,context.field7C);w(score+0x28,context.flag28);w(score+0xf8,h.next);
  for(unsigned k=0;k<10;k++)w(score+0xa8+k*8,h.entries[k]);w(identity,id);
  R5900Context c{};c.pc=0x1190f0;SET_GPR_U32(&c,4,score);SET_GPR_U32(&c,5,identity);SET_GPR_U32(&c,31,0x12345678);
  sub_001190F0_0x1190f0(m.data(),&c,&rt);
  int repeats=ssx::originalTrickRepeatCount(h,id,context);auto* cp=&c;
  if(c.pc!=0x12345678||GPR_S32(cp,2)!=repeats||std::memcmp(m.data()+score+0xa8,h.entries.data(),80)||std::memcmp(m.data()+score+0xf8,&h.next,4)){printf("Trick history mismatch %u\n",n);return 3;}
 }
 // A full ring of the same ordinary trick saturates at ten repeats; a new
 // identity replaces the oldest entry, so stale identities eventually expire.
 ssx::OriginalTrickHistory h;ssx::OriginalTrickIdentity a{1,0},b{2,0};
 for(int i=0;i<25;i++)if(ssx::originalTrickRepeatCount(h,a,{})!=std::min(i,10))return 4;
 for(int i=0;i<10;i++)ssx::originalTrickRepeatCount(h,b,{});
 if(ssx::originalTrickRepeatCount(h,a,{})!=0)return 5;
 puts("30000 original1190F0 cases match return count, all80 history bytes and cursor; repeat saturation and eviction also pass.");
}
