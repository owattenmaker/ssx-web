#include "ps2_runtime_macros.h"
#include "../engine/rider_trigger_contacts.hpp"
#include <vector>
#include <random>
#include <cstring>
#include <cstdio>
void sub_00108A48_0x108a48(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x420000,g_ps2RecompiledFunctionTableSlotCount=0xc8000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xc8000]={};
int main(int,char**){std::vector<uint8_t> memory(32*1024*1024);auto*m=memory.data();PS2Runtime rt;std::mt19937 rng(0x108c28);
for(unsigned i=0;i<20000;i++){
 std::array<uint32_t,64> ids;for(auto&v:ids)v=rng()%512;unsigned end=i%65;if(end<64)ids[end]=0xffffffffu;uint32_t resource=i%4==0?0xffffffffu:i%4==1?ids[rng()%64]:rng()%512;
 std::memcpy(m+0x205b8,ids.data(),sizeof(ids));std::memcpy(m+0x30078,&resource,4);R5900Context c{};c.pc=0x108c28;SET_GPR_U32(&c,4,0x20000);SET_GPR_U32(&c,5,0x30000);SET_GPR_U32(&c,29,0x10000);SET_GPR_U32(&c,31,0x12345678);sub_00108A48_0x108a48(m,&c,&rt);
 if(c.pc!=0x12345678||GPR_U32((&c),2)!=ssx::originalRiderTouchesInstance(ids,resource)){printf("Trigger membership mismatch %u\n",i);return 1;}
}
puts("20000 original rider trigger membership cases match, including terminators and64-entry bound");}
