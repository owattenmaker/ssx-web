#include "ps2_runtime_macros.h"
#include "../engine/instance_state.hpp"
#include <vector>
#include <random>
#include <cstring>
#include <cstdio>
void sub_002FC2C0_0x2fc2c0(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x420000,g_ps2RecompiledFunctionTableSlotCount=0xc8000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xc8000]={};
static std::vector<int> calls;static uint32_t destroyedFlags;static int16_t adjustment;
static void check(bool b){if(!b)throw std::runtime_error("Instance state source mismatch");}
int main(){std::vector<uint8_t> m(32*1024*1024);PS2Runtime rt;
auto write=[&](uint32_t at,uint32_t v){std::memcpy(m.data()+at,&v,4);};
rt.registerFunction(0x100100,[](uint8_t*m,R5900Context*c,PS2Runtime*){
 check(GPR_U32(c,4)==uint32_t(0x40000+adjustment)&&GPR_U32(c,5)==3);
 calls.push_back(3);std::memcpy(m+0x30008,&destroyedFlags,4);c->pc=GPR_U32(c,31);
});
rt.registerFunction(0x317d70,[](uint8_t*,R5900Context*c,PS2Runtime*){
 check(GPR_U32(c,4)==28&&GPR_U32(c,6)==0x20000000&&GPR_U32(c,7)==0);
 auto tag=GPR_U32(c,5);check(tag==0x4896b8||tag==0x4896c8);calls.push_back(tag==0x4896b8?106:119);
 SET_GPR_U32(c,2,0x60000);c->pc=GPR_U32(c,31);
});
for(auto address:{0x3506d8,0x350f60})
 rt.registerFunction(address,[](uint8_t*,R5900Context*c,PS2Runtime*){
 int type=c->pc==0x3506d8?6:19;
 check(GPR_U32(c,4)==0x60000&&GPR_U32(c,5)==0x30000);calls.push_back(type);c->pc=GPR_U32(c,31);
 });
std::mt19937 rng(0x2fc2c0);int count=0;
for(int32_t mode:{INT32_MIN,-1,0,1,2,3,4,INT32_MAX})for(int type:{-1,0,6,19,20,32767,-32768})for(int i=0;i<400;i++){
 uint32_t before=rng();destroyedFlags=rng();adjustment=int16_t(rng());
 write(0x20004,mode);write(0x30008,before);write(0x3000c,type==-1?0:0x40000);
 write(0x40010,uint16_t(type));write(0x4000c,0x50000);write(0x50008,uint16_t(adjustment));write(0x5000c,0x100100);
 R5900Context c{};c.pc=0x2fc2c0;SET_GPR_U32(&c,4,0x30000);SET_GPR_U32(&c,5,0x20000);SET_GPR_U32(&c,29,0x10000);SET_GPR_U32(&c,31,0x12345678);
 calls.clear();sub_002FC2C0_0x2fc2c0(m.data(),&c,&rt);check(c.pc==0x12345678);
 std::vector<int> expected;uint32_t flags=before;ssx::OriginalInstanceStateAccess access;
 access.destroyEntity=[&](int mode){expected.push_back(mode);flags=destroyedFlags;};
 access.constructEntity=[&](int type){expected.push_back(100+type);expected.push_back(type);};
 ssx::originalInstanceStateFallback(flags,type==-1?std::nullopt:std::optional<int16_t>(type),mode,access);
 uint32_t actual;std::memcpy(&actual,m.data()+0x30008,4);check(flags==actual&&calls==expected);++count;
}
std::printf("%d original instance fallback cases: dispatch order, guards, signed vtable adjustment, allocation contract and post-destruction flags match. Constructors are boundary stubs.\n",count);
}
