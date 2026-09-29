#include "ps2_runtime_macros.h"
#include "../engine/instance_state.hpp"
#include <vector>
#include <cstring>
#include <cstdio>
void sub_00335B90_0x335b90(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00336D40_0x336d40(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x420000,g_ps2RecompiledFunctionTableSlotCount=0xc8000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xc8000]={};
int main(){std::vector<uint8_t> m(32*1024*1024);PS2Runtime rt;unsigned count=0;
for(int mode:{0,2})for(uint32_t low=0;low<65536;++low)for(bool entity:{false,true}){
 uint32_t flags=0xa5310000u|low,ptr=entity?0x40000:0;
 std::memcpy(m.data()+0x30008,&flags,4);std::memcpy(m.data()+0x3000c,&ptr,4);
 R5900Context c{};c.pc=mode==0?0x335bb0:0x336d64;SET_GPR_U32(&c,16,0x30000);SET_GPR_U32(&c,17,0x20000);
 if(mode==0)sub_00335B90_0x335b90(m.data(),&c,&rt);else sub_00336D40_0x336d40(m.data(),&c,&rt);
 using R=ssx::OriginalInstanceBodyRoute;auto expected=ssx::originalInstanceRayRoute(flags,entity,mode);
 uint32_t end=mode==0?(expected==R::Static?0x335bc0:expected==R::Entity?0x335c68:0x335d68):(expected==R::Static?0x336d78:expected==R::Entity?0x336e18:0x336f1c);
 if(c.pc!=end)throw std::runtime_error("Original ray route differs");++count;
}
std::printf("%u original mode0/2 ray flag routes match; original execution stops before bounds/virtual dispatch.\n",count);
}
