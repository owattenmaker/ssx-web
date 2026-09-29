#include "ps2_runtime_macros.h"
#include "../engine/instance_state.hpp"
#include <vector>
#include <cstring>
#include <cstdio>
void sub_00333EF8_0x333ef8(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x420000,g_ps2RecompiledFunctionTableSlotCount=0xc8000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xc8000]={};
int main(){std::vector<uint8_t> m(32*1024*1024);PS2Runtime rt;unsigned count=0;
for(uint32_t low=0;low<65536;++low)for(bool entity:{false,true}){
 uint32_t flags=0xa5310000u|low,ptr=entity?0x40000:0;
 std::memcpy(m.data()+0x30008,&flags,4);std::memcpy(m.data()+0x3000c,&ptr,4);
 R5900Context c{};c.pc=0x3340f4;SET_GPR_U32(&c,16,0x30000);SET_GPR_U32(&c,20,0x20000);
 sub_00333EF8_0x333ef8(m.data(),&c,&rt);
 using R=ssx::OriginalInstanceBodyRoute;auto expected=ssx::originalInstanceBodyRoute(flags,entity);
 uint32_t end=expected==R::Static?0x334104:expected==R::Entity?0x3341a8:0x334288;
 if(c.pc!=end)throw std::runtime_error("Original body route differs");++count;
}
std::printf("%u original body collector flag routes match; original execution stops before bounds/virtual dispatch.\n",count);
}
