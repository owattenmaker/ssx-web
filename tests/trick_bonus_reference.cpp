#include "ps2_runtime_macros.h"
#include "../engine/trick_bonus.hpp"
#include <fstream>
#include <random>
#include <cstdio>
void sub_0011B1A8_0x11b1a8(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x410000,g_ps2RecompiledFunctionTableSlotCount=0xc4000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xc4000]={nullptr};
int main(int argc,char**argv){
 if(argc!=2)return 1;std::ifstream file(argv[1],std::ios::binary);std::vector<uint8_t> m((std::istreambuf_iterator<char>(file)),{});if(m.size()!=32*1024*1024)return 2;
 PS2Runtime rt;rt.registerFunction(0x3e6448,[](uint8_t*m,R5900Context*c,PS2Runtime*){std::memset(m+GPR_U32(c,4),GPR_U32(c,5),GPR_U32(c,6));c->pc=GPR_U32(c,31);});
 ssx::OriginalNamedTrickTable table;for(unsigned i=0;i<24;i++){auto p=m.data()+0x43d608+i*16;std::memcpy(&table[i].id,p,4);std::memcpy(&table[i].points,p+4,4);std::memcpy(table[i].identityFields.data(),p+8,7);}
 std::mt19937 rng(0x11b1a8);unsigned matches=0;
 for(unsigned n=0;n<30000;n++){
  ssx::OriginalTrickIdentity id{uint32_t(rng()),uint32_t(rng())};
  if(n%2==0){auto f=table[(n/2)%24].identityFields;id[0]=(id[0]&~0xf03ffc00u)|(uint32_t(f[0])<<10)|(uint32_t(f[1])<<12)|(uint32_t(f[2])<<16)|(uint32_t(f[3])<<19)|(uint32_t(f[4])<<28);id[1]=(id[1]&~0x3fbf8u)|(uint32_t(f[5])<<3)|(uint32_t(f[6])<<11);}
  std::memcpy(m.data()+0x30000,id.data(),8);R5900Context c{};c.pc=0x11b1a8;SET_GPR_U32(&c,4,0x20000);SET_GPR_U32(&c,5,0x30000);SET_GPR_U32(&c,29,0x10000);SET_GPR_U32(&c,31,0x12345678);
  sub_0011B1A8_0x11b1a8(m.data(),&c,&rt);auto value=ssx::originalNamedTrickBonus(id,table);auto*cp=&c;
  if(c.pc!=0x12345678||GPR_S32(cp,2)!=value||std::memcmp(m.data()+0x30000,id.data(),8)){printf("Named trick mismatch %u\n",n);return 3;}matches+=value!=0;
 }
 if(matches<15000)return 4;printf("30000 original11B1A8 cases match bonus and rewritten identity (%u matches, all24 authored combinations exercised).\n",matches);
}
