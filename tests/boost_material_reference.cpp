#include "ps2_runtime_macros.h"
#include <fstream>
#include <cstring>
#include <iostream>
void sub_00386DD0_0x386dd0(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00368970_0x368970(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00362478_0x362478(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x400000,g_ps2RecompiledFunctionTableSlotCount=0xc0000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xc0000]={};
template<class T>T rd(uint8_t*m,uint32_t p){T v;std::memcpy(&v,m+(p&0x1ffffff),sizeof(v));return v;}template<class T>void wr(uint8_t*m,uint32_t p,T v){std::memcpy(m+(p&0x1ffffff),&v,sizeof(v));}
int main(int argc,char**argv){std::vector<uint8_t>memory(32*1024*1024);auto*m=memory.data();std::ifstream file(argv[1],std::ios::binary);file.read((char*)m,memory.size());PS2Runtime rt;R5900Context c{};
 uint32_t renderer=rd<uint32_t>(m,0x4a30f0-0x854);
 for(unsigned textureId:{57u,58u,59u,60u,61u}){
 uint32_t tm=rd<uint32_t>(m,renderer+0x18f4),texture=rd<uint32_t>(m,renderer+0xf50+textureId*4);
 uint32_t descriptor=rd<uint32_t>(m,tm+8+texture*4);
 std::cout<<"TEXTURE_ID "<<std::dec<<textureId<<" HANDLE "<<std::hex<<texture<<" DESCRIPTOR "<<descriptor<<" TEMPLATE "<<rd<uint64_t>(m,descriptor+0x38)<<'\n';
 // The glide capture has not uploaded wake yet. Supply only a scratch VRAM
 // allocation, leaving its actual texture format/function/template untouched.
 if(textureId>=57){
  if(rd<uint32_t>(m,descriptor+0x28)!=0xffffffffu)throw std::runtime_error("Wake fixture residency changed");
  const auto track=rd<uint32_t>(m,tm+8+rd<uint32_t>(m,renderer+0xf50+55*4)*4);
  wr(m,descriptor+0x28,rd<uint32_t>(m,track+0x28));wr(m,descriptor+0x30,rd<uint32_t>(m,track+0x30));
  std::cout<<"BOOST_SYNTHETIC_VRAM_ALLOCATION (texture template unchanged)\n";
 }wr(m,tm+0x1f4c,uint32_t(-1));wr(m,0xa1000,0x900000u);
 c={};c.pc=0x368970;SET_GPR_U32(&c,4,tm);SET_GPR_U32(&c,5,texture);SET_GPR_U32(&c,6,0);SET_GPR_U32(&c,7,0xa1000);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,29,0x50000);SET_GPR_U32(&c,31,0x12345678);sub_00368970_0x368970(m,&c,&rt);
 bool found=false;for(uint32_t p=0x900000;p<rd<uint32_t>(m,0xa1000);p+=16)if(rd<uint64_t>(m,p+8)==6){uint64_t value=rd<uint64_t>(m,p);std::cout<<"BOUND_TEX0 "<<std::hex<<value<<" TFX "<<((value>>35)&3)<<" TCC "<<((value>>34)&1)<<'\n';if(((value>>35)&3)!=0||((value>>34)&1)!=1)throw std::runtime_error("Unexpected original track/wake texture function");found=true;}
 if(!found)throw std::runtime_error("Original binding did not emit TEX0");
 }
 wr(m,0xa1000,0u);wr(m,0xa1004,0x910000u);c={};c.pc=0x362478;SET_GPR_U32(&c,4,0xa1000);SET_GPR_U32(&c,5,7);SET_GPR_U32(&c,6,0);SET_GPR_U32(&c,7,0);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,29,0x50000);SET_GPR_U32(&c,31,0x12345678);sub_00362478_0x362478(m,&c,&rt);uint64_t alpha=rd<uint64_t>(m,0x910000);std::cout<<"BOUND_ALPHA "<<std::hex<<alpha<<'\n';if(alpha!=0x48||rd<uint64_t>(m,0x910008)!=0x42)throw std::runtime_error("Unexpected original boost additive blend");

}
