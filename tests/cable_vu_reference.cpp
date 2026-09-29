// Development oracle for web/set-piece-cables.js: run the original cable line-strip VU1 microprogram (the fourth .vutext MPG,
// program 3, entry 0x3BF0 = MSCAL 0x77E from renderer slot +0x25C 0x381F10) on synthetic uploads shaped like 0x381F10's packet and
// dump the line strip it kicks. XGKICK (0x3DF0) is replaced by NOP and the packet is read from VU memory at the kick.
// Rows 0..3 the world -> guard-band clip matrix (columns), 4 viewport scale, 5 viewport offset (the live values at the PS2 world pass:
// (1024, 1024, -8388467.5) / (2047.5, 2047.5, 8388467.5)); TOP+0 A+D GIF tag, +1 RGBAQ, +2 LINESTRIP tag, +3..+6 the segment's cubic
// rows (t^3, t^2, t, 1), +7 (count, 0, 0, 0). Output (stdout): JSON {cases: [{rows, matrix, count, tag, verts: [[x16, y16, z, w]...]}]}.
#include "ps2_runtime_macros.h"
#include <cstdio>
#include <cstring>
#include <fstream>
#include <random>
#include <vector>
#include <array>
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x380000,g_ps2RecompiledFunctionTableSlotCount=0xa0000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xa0000]={};
static uint32_t bits(float f){uint32_t w;memcpy(&w,&f,4);return w;}
int main(int argc,char**argv){
 if(argc!=2)return 2;
 std::ifstream codeFile(argv[1],std::ios::binary);std::vector<uint8_t> code((std::istreambuf_iterator<char>(codeFile)),{});
 if(code.size()!=16384)return 3;
 const uint32_t kick=0x3df0;{uint32_t word;memcpy(&word,code.data()+kick,4);if(word!=0x800016fcu&&(word&0x7ff)!=0x6fc)return 6;uint32_t nop=0x8000033cu;memcpy(code.data()+kick,&nop,4);}
 PS2Runtime rt;std::mt19937 rng(0x381F10);auto uni=[&](float a,float b){return std::uniform_real_distribution<float>(a,b)(rng);};
 printf("{\"cases\":[");
 const unsigned N=400;
 for(unsigned n=0;n<N;n++){
  const uint32_t TOP=40;
  std::array<uint8_t,16384> data{};auto row=[&](uint32_t r,float x,float y,float z,float w){float v[4]={x,y,z,w};memcpy(data.data()+r*16,v,16);};
  auto rowu=[&](uint32_t r,uint32_t x,uint32_t y,uint32_t z,uint32_t w){uint32_t v[4]={x,y,z,w};memcpy(data.data()+r*16,v,16);};
  // clip = M p (column vectors in rows 0..3 as the VU reads them: vf1 = row 0 = column x ...)
  float m[16];for(int k=0;k<16;k++)m[k]=uni(-1.f,1.f);
  m[3]=uni(-.02f,.02f);m[7]=uni(-.02f,.02f);m[11]=uni(.2f,1.f);m[15]=uni(2.f,40.f); // w = dot(p, (m3, m7, m11)) + m15 (mostly positive)
  if(n%4==3){m[11]=uni(-1.f,1.f);m[15]=uni(-5.f,5.f);} // some points behind / outside
  for(int r=0;r<4;r++)row(r,m[r*4],m[r*4+1],m[r*4+2],m[r*4+3]);
  row(4,1024,1024,-8388467.5f,0);row(5,2047.5f,2047.5f,8388467.5f,0);
  const float scale=n%3==0?60.f:20.f;
  float A[3],B[3],C[3],D[3];for(int a=0;a<3;a++){A[a]=uni(-scale,scale);B[a]=uni(-scale,scale);C[a]=uni(-scale,scale);D[a]=uni(-scale,scale);}
  const uint32_t count=n%5==0?2+rng()%12:10;
  rowu(TOP+0,0x00000001u,0x10014000u,14u,0u);rowu(TOP+1,0x80000000u,0x3f800000u,1u,0u); // A+D tag, RGBAQ (0,0,0,128), reg 1
  rowu(TOP+2,count|0x8000u,0x10014000u,4u,0u); // written by the VU itself at the kick
  row(TOP+3,A[0],A[1],A[2],0);row(TOP+4,B[0],B[1],B[2],0);row(TOP+5,C[0],C[1],C[2],0);row(TOP+6,D[0],D[1],D[2],1);rowu(TOP+7,count,0,0,0);
  auto q=[&](uint32_t r,unsigned l){uint32_t v;memcpy(&v,data.data()+(r&1023)*16+l*4,4);return v;};
  rt.vu1().reset();rt.vu1().execute(code.data(),code.size(),data.data(),data.size(),rt.gs(),nullptr,0x3bf0,TOP,0,1);
  uint32_t lastPC=~0u;unsigned long steps=0;bool ended=false,kicked=false;std::vector<std::array<uint32_t,4>> verts;uint32_t tag[4]={};
  while(steps++<5000000ul){
   auto& s=rt.vu1().state();
   if(s.pc!=lastPC){lastPC=s.pc;
    if(s.pc==kick&&!kicked){kicked=true;const uint32_t base=uint32_t(s.vi[2])&1023;for(unsigned l=0;l<4;l++)tag[l]=q(base+2,l);
     const uint32_t nl=q(base+2,0)&0x7fff;for(uint32_t k=0;k<nl;k++)verts.push_back({q(base+3+k,0),q(base+3+k,1),q(base+3+k,2),q(base+3+k,3)});}
    if(ended)break;if(s.pc==0x3df8)ended=true;}
   rt.vu1().resume(code.data(),code.size(),data.data(),data.size(),rt.gs(),nullptr,TOP,0,1);
  }
  if(!ended||!kicked){fprintf(stderr,"case %u did not end (pc %x)\n",n,rt.vu1().state().pc);return 7;}
  printf("%s{\"count\":%u,\"matrix\":[",n?",":"",count);for(int k=0;k<16;k++)printf("%s%u",k?",":"",bits(m[k]));
  printf("],\"rows\":[");const float* R[4]={A,B,C,D};bool first=true;for(int r=0;r<4;r++)for(int a=0;a<3;a++){printf("%s%u",first?"":",",bits(R[r][a]));first=false;}
  printf("],\"tag\":[%u,%u,%u,%u],\"verts\":[",tag[0],tag[1],tag[2],tag[3]);
  for(size_t k=0;k<verts.size();k++)printf("%s[%u,%u,%u,%u]",k?",":"",verts[k][0],verts[k][1],verts[k][2],verts[k][3]);
  printf("]}");
 }
 printf("]}\n");return 0;
}
