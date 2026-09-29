#include "ps2_runtime_macros.h"
#include "runtime/ps2_vu1.h"
#include "../engine/skin_vertex.hpp"
#include <fstream>
#include <random>
#include <cstring>
#include <cstdio>
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x100004,g_ps2RecompiledFunctionTableSlotCount=1;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[1]={};
int main(int argc,char**argv){
 if(argc!=3)return 2;std::array<uint8_t,16384> source{},code{},data{};
 std::ifstream file(argv[1],std::ios::binary);file.read(reinterpret_cast<char*>(source.data()),source.size());if(!file)return 2;
 const uint32_t lowerNop=0x8000033c,upperNop=0x2ff,halt=0x400002ff;
 for(unsigned at=0;at<code.size();at+=8){std::memcpy(code.data()+at,&lowerNop,4);std::memcpy(code.data()+at+4,&upperNop,4);}
 for(unsigned pc:{0x4c0,0x4c8,0x4d0,0x4d8})std::memcpy(code.data()+pc+4,source.data()+pc+4,4);
 //The original lower LQ at04C0 replaces vf14 with translation after its
 //first-column multiply. Keep this real data dependency, not a synthetic opcode.
 std::memcpy(code.data()+0x4c0,source.data()+0x4c0,4);std::memcpy(code.data()+0x4e4,&halt,4);
 PS2Runtime runtime;std::mt19937 rng(0x4c0);std::ofstream gpu(argv[2],std::ios::binary);
 for(unsigned n=0;n<12000;++n){
  std::array<float,4> point;std::array<float,16> matrix;for(auto& v:point)v=float(int(rng()%200001)-100000)/127.f;for(auto& v:matrix)v=float(int(rng()%200001)-100000)/137.f;
  if(n%3==0){matrix={1,0,0,0,0,1,0,0,0,0,1,0,point[0],point[1],point[2],1};point[3]=1;}
  VU1Interpreter vu;vu.state().vi[8]=100;std::memcpy(data.data()+100*16,matrix.data(),64);std::memcpy(vu.state().vf[20],point.data(),16);std::memcpy(vu.state().vf[14],matrix.data(),16);std::memcpy(vu.state().vf[18],matrix.data()+4,16);std::memcpy(vu.state().vf[19],matrix.data()+8,16);
  {ssx::terrain_original::Rounding rounding;vu.execute(code.data(),code.size(),data.data(),data.size(),runtime.gs(),nullptr,0x4c0,0,0,100);}
  auto result=ssx::originalSkinVertex(point,matrix);if(std::memcmp(result.data(),vu.state().vf[20],16)){printf("Original skin vertex mismatch%u\n",n);return 1;}
  std::array<float,24> record;std::memcpy(record.data(),point.data(),16);std::memcpy(record.data()+4,matrix.data(),64);std::memcpy(record.data()+20,vu.state().vf[20],16);gpu.write(reinterpret_cast<const char*>(record.data()),sizeof(record));
 }
 if(!gpu)return 2;puts("12000 original VU04C0..04D8 position transforms match all four float words, including the original translation load.");
}
