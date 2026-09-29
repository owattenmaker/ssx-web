#include "ps2_runtime_macros.h"
#include "../engine/trick_name.hpp"
#include <fstream>
#include <cstring>
#include <random>
#include <cstdio>
void sub_00116950_0x116950(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x420000,g_ps2RecompiledFunctionTableSlotCount=0xc8000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xc8000]={};
int main(int argc,char**argv){if(argc!=2)return 1;std::ifstream f(argv[1],std::ios::binary);std::vector<uint8_t>m((std::istreambuf_iterator<char>(f)),{});if(m.size()!=32*1024*1024)return 2;PS2Runtime rt;
 const unsigned addresses[]={0x43d118,0x43d130,0x43cf40,0x43cf60,0x43cf70,0x43cfb0,0x43cfc8,0x43cfe0,0x43d0c0,0x43d160,0x4a0ff0,0x43d160,0x43d2b8,0x43d2d0,0x43d2e0,0x43d100,0x43d320};
 const unsigned count[]={6,12,8,4,15,6,5,56,16,86,2,86,6,4,16,6,26};
 const unsigned word[]={0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1},shift[]={0,3,7,10,12,16,19,22,28,3,10,11,0,18,20,24,27};
 ssx::OriginalTrickNameTables tables;std::array<std::vector<unsigned>,17> valid;
 for(unsigned i=0;i<17;i++)for(unsigned j=0;j<count[i];j++){uint32_t p;std::memcpy(&p,m.data()+addresses[i]+4*j,4);if(p){tables[i].push_back(std::string((char*)m.data()+p));valid[i].push_back(j);}else tables[i].push_back(std::nullopt);}
 std::mt19937 rng(0x116950);
 for(unsigned n=0;n<20000;n++){
  ssx::OriginalTrickIdentity id{};for(unsigned i=0;i<17;i++)id[word[i]]|=valid[i][rng()%valid[i].size()]<<shift[i];
  std::memcpy(m.data()+0x10000,id.data(),8);std::memset(m.data()+0x20000,0,4096);
  R5900Context c{};c.pc=0x116950;SET_GPR_U32(&c,4,0x20000);SET_GPR_U32(&c,5,0x10000);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,31,0x12345678);sub_00116950_0x116950(m.data(),&c,&rt);
  auto expected=ssx::originalTrickName(id,tables);if(c.pc!=0x12345678||expected!=(char*)m.data()+0x20000){printf("Name mismatch %u\nexpected %s\noriginal %s\n",n,expected.c_str(),m.data()+0x20000);return 3;}
 }
 puts("20,000 original116950 trick names match byte-for-byte");
}
