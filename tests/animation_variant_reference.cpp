#include "ps2_runtime_macros.h"
#include "../engine/animation_variant.hpp"
#include "../engine/original_random.hpp"
#include <fstream>
#include <cstring>
#include <random>
#include <cstdio>
#include <cfenv>
#include <bit>
void sub_00311710_0x311710(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x340000,g_ps2RecompiledFunctionTableSlotCount=0x90000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0x90000]={};

static uint32_t draw;static unsigned calls;
static void randomCall(uint8_t*,R5900Context*c,PS2Runtime*){++calls;SET_GPR_U32(c,2,draw);c->pc=GPR_U32(c,31);}
int main(int argc,char**argv){
 if(argc!=2)return 2;std::ifstream in(argv[1],std::ios::binary);std::vector<uint8_t>m((std::istreambuf_iterator<char>(in)),{});PS2Runtime rt;rt.registerFunction(0x317810,randomCall);
 auto put=[&](unsigned at,const auto&v){memcpy(m.data()+at,&v,sizeof(v));};std::mt19937 gen(0x311710);unsigned done=0x12345678;
 for(unsigned i=0;i<30000;++i){unsigned count=1+gen()%16;uint32_t required=gen()&15;std::vector<ssx::OriginalAnimationVariant> variants;
  for(unsigned n=0;n<count;++n){uint32_t flags=gen(),weight=gen()%500;if(n==count-1){flags|=required;weight+=1;}variants.push_back({gen()%519,weight,flags});put(0x449960+n*12,variants.back());}
  put(0x30000,uint16_t(0));put(0x30002,uint16_t(count));draw=i%7?gen():0;calls=0;
  R5900Context c{};c.pc=0x311710;SET_GPR_U32((&c),4,0x30000);SET_GPR_U32((&c),5,0);SET_GPR_U32((&c),6,required);SET_GPR_U32((&c),28,0x4a30f0);SET_GPR_U32((&c),29,0x10000);SET_GPR_U32((&c),31,done);sub_00311710_0x311710(m.data(),&c,&rt);
  unsigned nativeCalls=0;uint32_t leaf=ssx::originalAnimationVariant(variants,required,[&](){++nativeCalls;return draw;});
  if(c.pc!=done||leaf!=GPR_U32((&c),2)||calls!=nativeCalls){printf("Variant mismatch%u leaf%u/%u calls%u/%u\n",i,leaf,GPR_U32((&c),2),nativeCalls,calls);return 4;}
 }
 // Authored boundary behavior: first100 weight receives draw100 too, and a
 // singleton ignores both mask and zero weight without drawing.
 std::array<ssx::OriginalAnimationVariant,2> edge{{{1,100,~0u},{2,100,~0u}}};
 if(ssx::originalAnimationVariant(edge,0,[](){return 100;})!=1)return 5;
 edge[0].weight=0;if(ssx::originalAnimationVariant(edge,0,[](){return 0;})!=1)return 6;
 puts("30,000 original311710 weighted variant selections and exact draw counts passed; inclusive boundaries and singleton bypass preserved");
}
