#include "ps2_runtime_macros.h"
#include <fstream>
#include <vector>
#include <array>
#include <cstring>
#include <iostream>
void sub_00312490_0x312490(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x420000,g_ps2RecompiledFunctionTableSlotCount=0xc8000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xc8000]={};
static std::vector<unsigned> dispatched;static unsigned later,inserted;
static unsigned read(uint8_t*m,unsigned a){unsigned v;std::memcpy(&v,m+a,4);return v;}
static void write(uint8_t*m,unsigned a,unsigned v){std::memcpy(m+a,&v,4);}
static void first(uint8_t*m,R5900Context*c,PS2Runtime*){SET_GPR_U32(c,2,read(m,GPR_U32(c,4)+4));c->pc=GPR_U32(c,31);}
static void flag(uint8_t*m,R5900Context*c,PS2Runtime*){if(GPR_U32(c,5)!=63)throw std::runtime_error("Unexpected completion flag");SET_GPR_U32(c,2,read(m,GPR_U32(c,4)+4)>>31);c->pc=GPR_U32(c,31);}
static void callback(uint8_t*m,R5900Context*c,PS2Runtime*){
 dispatched.push_back(GPR_U32(c,6));
 if(dispatched.size()==1){
  // A callback cancels a later pending flag and adds a new completed node.
  // Original312490 must keep dispatching the cohort collected before this.
  write(m,later+0xb4,0);write(m,inserted+0xb4,0x80000000);
  write(m,inserted+0xc8,read(m,0x30004));write(m,0x30004,inserted);
 }
 c->pc=GPR_U32(c,31);
}
int main(int argc,char**argv){
 if(argc!=2)return 2;std::ifstream file(argv[1],std::ios::binary);std::vector<uint8_t> elf((std::istreambuf_iterator<char>(file)),{}),m(32*1024*1024);std::memcpy(m.data()+0xff000,elf.data(),elf.size());
 PS2Runtime runtime;runtime.registerFunction(0x314760,first);runtime.registerFunction(0x1446a0,flag);runtime.registerFunction(0x110000,callback);
 unsigned cases=0;
 for(unsigned mask=1;mask<64;++mask){
  write(m.data(),0x20050,0x30000);write(m.data(),0x20058,0x31000);write(m.data(),0x31018,0);write(m.data(),0x3101c,0x110000);
  std::vector<unsigned> expected;
  for(unsigned channel=0;channel<6;++channel){unsigned node=0x40000+channel*0x400;write(m.data(),0x30004+channel*8,node);write(m.data(),node+0xb4,mask&(1u<<channel)?0x80000000:0);write(m.data(),node+0xc8,node+0x100);write(m.data(),node+0x1b4,0x80000000);write(m.data(),node+0x1c8,0);if(mask&(1u<<channel))expected.push_back(node);expected.push_back(node+0x100);}
  later=expected.back();inserted=0x50000;dispatched.clear();R5900Context c{};c.pc=0x312490;SET_GPR_U32(&c,4,0x20000);SET_GPR_U32(&c,29,0x10000);SET_GPR_U32(&c,31,0x12345678);
  sub_00312490_0x312490(m.data(),&c,&runtime);
  if(c.pc!=0x12345678||dispatched!=expected)throw std::runtime_error("Original completion cohort differs");++cases;
 }
 std::cout<<cases<<" original completion dispatch cohorts retain channel/list order, include already-collected callbacks after flag cancellation, and exclude nodes added during dispatch. Getter and callback bodies are controlled.\n";
}
