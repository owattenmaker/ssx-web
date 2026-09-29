// Development oracle for web/weather-renderer.js: run the original snowfall VU1 microprogram (the sixth .vutext MPG,
// program 5: flakes at 0x000 = renderer slot 0x2A8 0x381310, fluff at 0x408 = slot 0x2B0 0x3816F0) on synthetic layer
// uploads and dump the sprites it builds. XGKICK is replaced by NOP and the packet is read from VU memory at the kick.
// Rows: 0 viewport scale, 1 viewport offset, 2 size factor (the 364CD0 program-5 case: context +0x160, +0x170, (+0xE0, +0xF4));
// TOP+0..3 the clip matrix, +4/+5 ST rows, +6 offset - 1.5, +7 colour*128, +8 (count, seeds), +9 (lower, size), +10 (upper, extent).
// A diagonal clip matrix with w = 16 (every flake inside) and unit viewport rows make the sprite centre (x, y, z) * 16 / 16 *
// 16 (FTOI4) = the flake's normalized position * 16: the test compares it with the renderer's flakePosition.
// Output (stdout): JSON {cases: [{kind, count, seeds, offsetN, lower, upper, extent, size, sprites: [[cx16, cy16, z16, hw16, a]...]}]}
#include "ps2_runtime_macros.h"
#include <cstdio>
#include <cstring>
#include <fstream>
#include <random>
#include <vector>
#include <array>
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x380000,g_ps2RecompiledFunctionTableSlotCount=0xa0000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xa0000]={};
static float f32(uint32_t w){float f;memcpy(&f,&w,4);return f;}
static uint32_t bits(float f){uint32_t w;memcpy(&w,&f,4);return w;}
int main(int argc,char**argv){
 if(argc!=2)return 2;
 std::ifstream codeFile(argv[1],std::ios::binary);std::vector<uint8_t> code((std::istreambuf_iterator<char>(codeFile)),{});
 if(code.size()!=16384)return 3;
 const uint32_t kicks[]={0x3a8,0x3f0,0x838,0x880};
 for(uint32_t at:kicks){uint32_t word;memcpy(&word,code.data()+at,4);if(word!=0x800036fcu)return 6;uint32_t nop=0x8000033cu;memcpy(code.data()+at,&nop,4);}
 PS2Runtime rt;std::mt19937 rng(0x2E55D8);auto uni=[&](float a,float b){return std::uniform_real_distribution<float>(a,b)(rng);};
 printf("{\"cases\":[");
 for(unsigned n=0;n<40;n++){
  const int kind=n%2;const uint32_t TOP=32,count=kind?1+rng()%40:1+rng()%600;
  std::array<uint8_t,16384> data{};auto row=[&](uint32_t r,float x,float y,float z,float w){float v[4]={x,y,z,w};memcpy(data.data()+r*16,v,16);};
  auto rowu=[&](uint32_t r,uint32_t x,uint32_t y,uint32_t z,uint32_t w){uint32_t v[4]={x,y,z,w};memcpy(data.data()+r*16,v,16);};
  row(0,4096,4096,4096,4096);row(1,0,0,0,0);row(2,1,1,0,0); // screen = ndc * 4096 (FTOI4: 1/65536 of the box)
  row(TOP+0,1,0,0,0);row(TOP+1,0,1,0,0);row(TOP+2,0,0,1,0);row(TOP+3,0,0,0,16); // clip = (x, y, z, 16)
  row(TOP+4,0,0,0,0);row(TOP+5,1,1,0,0);
  const float extent=float(std::array<int,3>{1500,2000,3000}[rng()%3]);
  float lower[3],off[3];for(int a=0;a<3;a++){lower[a]=uni(-1.1f,.1f);off[a]=uni(-.5f,.5f);}
  row(TOP+6,off[0]-1.5f,off[1]-1.5f,off[2]-1.5f,0);row(TOP+7,uni(0,128),uni(0,128),uni(0,128),uni(0,128));
  const uint32_t s0=rng(),s1=rng(),s2=rng();rowu(TOP+8,count,s0,s1,s2);
  const float size=kind?600.f:uni(0,8);row(TOP+9,lower[0],lower[1],lower[2],size);row(TOP+10,lower[0]+1,lower[1]+1,lower[2]+1,extent);
  const uint32_t start=kind?0x408:0x000;std::vector<std::array<uint32_t,20>> sprites;
  auto q=[&](uint32_t r,unsigned l){uint32_t v;memcpy(&v,data.data()+(r&1023)*16+l*4,4);return v;};
  rt.vu1().reset();rt.vu1().execute(code.data(),code.size(),data.data(),data.size(),rt.gs(),nullptr,start,TOP,0,1);
  uint32_t lastPC=~0u;unsigned long steps=0;bool ended=false;const uint32_t endPC=kind?0x888:0x3f8;
  while(steps++<50000000ul){
   auto& s=rt.vu1().state();
   if(s.pc!=lastPC){lastPC=s.pc;
    for(uint32_t at:kicks)if(s.pc==at){const uint32_t base=uint32_t(s.vi[6])&1023,m=q(base,0)&0x7fff;
     for(uint32_t k=0;k<m;++k){std::array<uint32_t,20> w;for(unsigned r=0;r<5;++r)for(unsigned l=0;l<4;++l)w[r*4+l]=q(base+1+k*5+r,l);sprites.push_back(w);}}
    if(ended)break;if(s.pc==endPC)ended=true;}
   rt.vu1().resume(code.data(),code.size(),data.data(),data.size(),rt.gs(),nullptr,TOP,0,1);
  }
  if(!ended){fprintf(stderr,"case %u did not end (pc %x)\n",n,rt.vu1().state().pc);return 7;}
  printf("%s{\"kind\":%d,\"count\":%u,\"seeds\":[%u,%u,%u],\"offsetN\":[%u,%u,%u],\"lower\":[%u,%u,%u],\"extent\":%u,\"size\":%u,\"sprites\":[",n?",":"",kind,count,s0,s1,s2,
   bits(off[0]-1.5f),bits(off[1]-1.5f),bits(off[2]-1.5f),bits(lower[0]),bits(lower[1]),bits(lower[2]),bits(extent),bits(size));
  for(size_t k=0;k<sprites.size();k++){const auto& w=sprites[k];
   // flakes: ST0, RGBA, XYZ(v1 - h), ST1, XYZ(v1 + h); fluff: ST0, RGBA, XYZ(+h), ST1, XYZ(-h)
   const int32_t x0=int32_t(w[8]),y0=int32_t(w[9]),z0=int32_t(w[10]),x1=int32_t(w[16]),y1=int32_t(w[17]),z1=int32_t(w[18]);
   printf("%s[%d,%d,%d,%d,%d,%d,%u]",k?",":"",x0,y0,z0,x1,y1,z1,w[7]);}
  printf("]}");
 }
 printf("]}\n");return 0;
}
