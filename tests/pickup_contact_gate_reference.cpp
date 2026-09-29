#include "ps2_runtime_macros.h"
#include "../engine/pickup_contact_gate.hpp"
#include <vector>
#include <cstring>
#include <cstdio>
void sub_00355748_0x355748(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x420000,g_ps2RecompiledFunctionTableSlotCount=0xc8000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xc8000]={};
static bool accepts;static std::vector<int> calls;
int main(int,char**){std::vector<uint8_t> m(32*1024*1024);PS2Runtime rt;auto put=[&](unsigned at,uint32_t v){std::memcpy(m.data()+at,&v,4);};
put(0x40000,0x50000);put(0x50054,0x123400);put(0x5004c,0x123404);put(0x4a30f0+0x2a74,0x70000);
rt.registerFunction(0x123400,[](uint8_t*,R5900Context*c,PS2Runtime*){if(GPR_U32(c,4)!=0x40000)throw std::runtime_error("Wrong component refresh");calls.push_back(0);c->pc=GPR_U32(c,31);});
rt.registerFunction(0x2d1b30,[](uint8_t*,R5900Context*c,PS2Runtime*){if(GPR_U32(c,4)!=0x63000)throw std::runtime_error("Wrong rider identity");calls.push_back(1);SET_GPR_U32(c,2,17);c->pc=GPR_U32(c,31);});
rt.registerFunction(0x123404,[](uint8_t*,R5900Context*c,PS2Runtime*){if(GPR_U32(c,4)!=0x40000||GPR_U32(c,5)!=17)throw std::runtime_error("Wrong component predicate");calls.push_back(2);SET_GPR_U32(c,2,accepts);c->pc=GPR_U32(c,31);});
rt.registerFunction(0x34fe00,[](uint8_t*,R5900Context*c,PS2Runtime*){if(GPR_U32(c,4)!=0x20000||GPR_U32(c,5)!=0x61000||GPR_U32(c,6)!=0x62000||GPR_U32(c,7)!=0x63000)throw std::runtime_error("Wrong contact forwarding");calls.push_back(3);c->pc=GPR_U32(c,31);});
for(int i=0;i<20000;i++){const int presence=i%3,rate=(i%7)*15;int32_t before=(i%83)-2,expected=before;accepts=(i/3)%2;put(0x2001c,presence?0x30000:0);put(0x30000,presence==2?0x40000:0);put(0x20020,uint32_t(before));put(0x70010,rate);calls.clear();R5900Context c{};c.pc=0x355770;SET_GPR_U32(&c,4,0x20000);SET_GPR_U32(&c,5,0x61000);SET_GPR_U32(&c,6,0x62000);SET_GPR_U32(&c,7,0x63000);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,29,0x10000);SET_GPR_U32(&c,31,0x12345678);sub_00355748_0x355748(m.data(),&c,&rt);bool forward=ssx::originalPickupContactGate(expected,rate,presence==2,accepts);std::vector<int> events;if(presence==2)events={0,1,2};if(forward)events.push_back(3);int32_t actual;std::memcpy(&actual,m.data()+0x20020,4);if(c.pc!=0x12345678||actual!=expected||calls!=events){printf("Gate mismatch %d\n",i);return 1;}}
puts("20000 original contact-gate cases match predicate order, cooldown and forwarding arguments");}
