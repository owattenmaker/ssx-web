// Development oracle: run the original particle VU1 microprogram (the fifth .vutext
// MPG, program 4 of 0x439A40) on prepared VU1 data images and dump the GIF sprite
// packets it builds. Entry 0x000 = renderer slot 0x290 (380518, set-piece Particle
// kernels, builtin16); entry 0x510 = renderer slot 0x298 (3807A0, DynamicParticle
// birth rings, builtin26). XGKICK is replaced by NOP and the packet is read from VU
// memory at the kick instead (the tag template is not part of these images).
// Input: SPVU jobs file (tools/test_set_piece_particle_draw_native.py). Output on stdout:
//   J <job> <kicks> <sprites>
//   S <20 hex words>   (ST0, RGBAQ, XYZ2 v0, ST1, XYZ2 v1 as the program stores them)
#include "ps2_runtime_macros.h"
#include <cstdio>
#include <cstring>
#include <fstream>
#include <vector>
#include <array>
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x380000,g_ps2RecompiledFunctionTableSlotCount=0xa0000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xa0000]={};

int main(int argc,char**argv){
 if(argc!=3)return 2;
 std::ifstream codeFile(argv[1],std::ios::binary);std::vector<uint8_t> code((std::istreambuf_iterator<char>(codeFile)),{});
 if(code.size()!=16384)return 3;
 std::ifstream jobsFile(argv[2],std::ios::binary);std::vector<uint8_t> jobs((std::istreambuf_iterator<char>(jobsFile)),{});
 if(jobs.size()<8||memcmp(jobs.data(),"SPVU",4))return 4;
 uint32_t count;memcpy(&count,jobs.data()+4,4);
 const size_t jobSize=8+16384;if(jobs.size()!=8+count*jobSize)return 5;
 // XGKICK sites of entries 0x000 and 0x510 become NOPs; the packet is read at the kick.
 const uint32_t kicks[]={0x410,0x4e8,0x8f8,0x9c8};
 for(uint32_t at:kicks){uint32_t word;memcpy(&word,code.data()+at,4);if(word!=0x800026fcu)return 6;uint32_t nop=0x8000033cu;memcpy(code.data()+at,&nop,4);}
 PS2Runtime rt;
 for(uint32_t j=0;j<count;++j){
  const uint8_t* job=jobs.data()+8+j*jobSize;uint32_t start,top;memcpy(&start,job,4);memcpy(&top,job+4,4);
  std::array<uint8_t,16384> data;memcpy(data.data(),job+8,16384);
  auto q=[&](uint32_t row,unsigned lane){uint32_t v;memcpy(&v,data.data()+(row&1023)*16+lane*4,4);return v;};
  const uint32_t ends[]={start==0?0x4f0u:0x9d0u,start==0?0x500u:0x9e0u};
  std::vector<std::array<uint32_t,20>> sprites;unsigned kicked=0;
  rt.vu1().reset();rt.vu1().execute(code.data(),code.size(),data.data(),data.size(),rt.gs(),nullptr,start,top,0,1);
  uint32_t lastPC=~0u;unsigned long steps=0;bool ended=false;
  while(steps++<20000000ul){
   auto& s=rt.vu1().state();
   if(s.pc!=lastPC){
    lastPC=s.pc;
    for(uint32_t at:kicks)if(s.pc==at){
     // xgkick vi4: packet at vi4, NLOOP in the tag's low 15 bits (EOP set by the program).
     const uint32_t base=uint32_t(s.vi[4])&1023,n=q(base,0)&0x7fff;++kicked;
     for(uint32_t k=0;k<n;++k){std::array<uint32_t,20> w;for(unsigned r=0;r<5;++r)for(unsigned l=0;l<4;++l)w[r*4+l]=q(base+1+k*5+r,l);sprites.push_back(w);}
    }
    if(ended)break;
    if(s.pc==ends[0]||s.pc==ends[1])ended=true;
   }
   rt.vu1().resume(code.data(),code.size(),data.data(),data.size(),rt.gs(),nullptr,top,0,1);
  }
  if(!ended){fprintf(stderr,"job %u did not end (pc %x)\n",j,rt.vu1().state().pc);return 7;}
  printf("J %u %u %zu\n",j,kicked,sprites.size());
  for(auto& w:sprites){printf("S");for(uint32_t v:w)printf(" %08x",v);printf("\n");}
 }
 return 0;
}
