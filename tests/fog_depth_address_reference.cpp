#include "runtime/gs/ps2_gs_memory.h"
#include "../engine/fog_depth_sprites.hpp"
#include "../engine/fog_composite.hpp"
#include <vector>
#include <cstdio>
#include <random>
int main(){using namespace GSMem;InitLookupTables();std::vector<u8> memory(4*1024*1024);std::mt19937 rng(0x36ae20);size_t pixels=0;
for(auto dimensions:{std::array<unsigned,2>{64,32},{320,224},{512,448},{640,448},{512,512}}){
 const auto width=dimensions[0],height=dimensions[1],bw=width/64;constexpr unsigned depthBase=8192,colorBase=0;std::vector<uint32_t> depths(width*height),colors(depths.size());
 for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x){auto index=y*width+x;depths[index]=rng();colors[index]=rng();WriteZ32(memory.data(),depthBase,bw,x,y,depths[index]);WriteCT32(memory.data(),colorBase,bw,x,y,colors[index]);}
 //Nearest texel addressing with the original half-texel UV origin; this models
 //the pass's pixel-address operation, not GS rasterization or depth tests.
 for(const auto& sprite:ssx::originalFogDepthSprites(width/8,height*2,0,0)){
  unsigned sx=(sprite[0]&65535)>>4,sy=((sprite[0]>>16)&65535)>>4;
  unsigned dx=(sprite[2]&65535)>>4,dy=((sprite[2]>>16)&65535)>>4;
  unsigned ex=(sprite[6]&65535)>>4,ey=((sprite[6]>>16)&65535)>>4;
  for(unsigned y=dy;y<ey;++y)for(unsigned x=dx;x<ex;++x){auto z=ReadZ16(memory.data(),depthBase,bw,sx+x-dx,sy+y-dy);auto old=ReadCT16(memory.data(),colorBase,bw,x,y);
   //FRAME mask00003FFF in RGBA channel layout maps to00FF in packed5551.
   WriteCT16(memory.data(),colorBase,bw,x,y,(old&0xff)|(z&0xff00));}
 }
 for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x){auto index=y*width+x;auto c=ReadCT32(memory.data(),colorBase,bw,x,y);
  if((c>>24)!=ssx::originalFogPaletteIndex(depths[index])||(c&0xffffff)!=(colors[index]&0xffffff)||ReadP8H(memory.data(),colorBase,bw,x,y)!=(c>>24))throw std::runtime_error("Fog memory mapping differs");++pixels;}
}
printf("GS memory-model check: %zu pixels across5 buffer sizes map Z bits8..15 to T8H and preserve RGB. Not a hardware rasterization comparison.\n",pixels);
}
