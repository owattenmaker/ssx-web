#include "ps2_runtime_macros.h"
#include <fstream>
#include <cstdio>
#include "grab-definition-registry.inc"
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x340000,g_ps2RecompiledFunctionTableSlotCount=0x90000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0x90000]={};
int main(int argc,char**argv){PS2Runtime rt;registerDefinitions(rt);std::ifstream in(argv[1],std::ios::binary);std::vector<uint8_t>m((std::istreambuf_iterator<char>(in)),{});if(m.size()!=32*1024*1024)return 2;
 auto call=[&](unsigned pc,unsigned a1,unsigned a2,unsigned a3=0){R5900Context c{};c.pc=pc;SET_GPR_U32((&c),4,0);SET_GPR_U32((&c),5,a1);SET_GPR_U32((&c),6,a2);SET_GPR_U32((&c),7,a3);SET_GPR_U32((&c),28,0x4a30f0);SET_GPR_U32((&c),29,0x10000);SET_GPR_U32((&c),31,0x12345678);auto fn=rt.lookupFunction(pc);fn(m.data(),&c,&rt);if(c.pc!=0x12345678)throw std::runtime_error("Definition continuation");return GPR_S32((&c),2);};
 constexpr unsigned functions[4][3]={{0x1500d8,0x1500f8,0x150118},{0x150138,0x150158,0x150178},{0x150198,0x1502c8,0x1503f8},{0x150198,0x1502c8,0x1503f8}};
 for(unsigned slot=0;slot<6;++slot)for(unsigned group=0;group<4;++group)for(unsigned index=0;index<15;++index){int semantic=call(functions[group][0],slot,index,group==3),upper=call(functions[group][1],slot,index,group==3),score=call(functions[group][2],slot,index,group==3);int base=call(0x150528,score,0),hold=call(0x150540,score,0);printf("%u %u %u %d %d %d %d %d\n",slot,group,index,semantic,upper,score,base,hold);}}
