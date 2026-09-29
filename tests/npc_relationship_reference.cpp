#include "ps2_runtime_macros.h"
#include <fstream>
#include <cstdio>
void sub_00155B50_0x155b50(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0014A080_0x14a080(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0014A0E0_0x14a0e0(uint8_t*,R5900Context*,PS2Runtime*);
void sub_001448D8_0x1448d8(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00145750_0x145750(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00145970_0x145970(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00146E98_0x146e98(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00147398_0x147398(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00147798_0x147798(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00144C78_0x144c78(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x420000,g_ps2RecompiledFunctionTableSlotCount=0xc8000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xc8000]={};
int main(int argc,char**argv){
 PS2Runtime rt;std::ifstream file(argv[1],std::ios::binary);std::vector<uint8_t> memory((std::istreambuf_iterator<char>(file)),{});if(memory.size()!=32*1024*1024)return 2;
 rt.registerFunction(0x14a080,sub_0014A080_0x14a080);rt.registerFunction(0x14a0e0,sub_0014A0E0_0x14a0e0);rt.registerFunction(0x1448d8,sub_001448D8_0x1448d8);rt.registerFunction(0x145750,sub_00145750_0x145750);rt.registerFunction(0x145970,sub_00145970_0x145970);rt.registerFunction(0x146e98,sub_00146E98_0x146e98);rt.registerFunction(0x147398,sub_00147398_0x147398);rt.registerFunction(0x147798,sub_00147798_0x147798);rt.registerFunction(0x144c78,sub_00144C78_0x144c78);
 for(unsigned actor=0;actor<6;++actor){for(unsigned peer=0;peer<6;++peer){R5900Context c{};SET_GPR_U32(&c,4,0);SET_GPR_U32(&c,5,actor);SET_GPR_U32(&c,6,peer);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,29,0x10000);SET_GPR_U32(&c,31,0x12345678);sub_00155B50_0x155b50(memory.data(),&c,&rt);auto*cp=&c;if(c.pc!=0x12345678)return 3;printf("%d%c",int(GPR_S32(cp,2)),peer==5?'\n':' ');}}
}
