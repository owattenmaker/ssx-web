#include "ps2_runtime_macros.h"
#include "runtime/ps2_vu1.h"
#include "../engine/terrain_contact_math.hpp"
#include <fstream>
#include <random>
#include <cstring>
#include <cstdio>
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x100004,g_ps2RecompiledFunctionTableSlotCount=1;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[1]={};
int main(int argc,char**argv){
 if(argc!=2)return 2;std::array<uint8_t,16384> code{},data{};std::ifstream file(argv[1],std::ios::binary);file.read(reinterpret_cast<char*>(code.data()),code.size());if(!file)return 2;
 PS2Runtime runtime;std::mt19937 rng(0x80);unsigned checked=0;
 for(unsigned n=0;n<2000;++n){
  data.fill(0);const unsigned count=1+n%25;std::vector<std::array<float,16>> matrices(count);
  std::array<float,16> projection;for(auto& v:projection)v=float(int(rng()%20001)-10000)/300.f;std::memcpy(data.data(),projection.data(),64);
  for(auto& matrix:matrices)for(auto& v:matrix)v=float(int(rng()%200001)-100000)/127.f;
  if(n%2==0)for(auto& matrix:matrices)matrix={.85f,0,0,0,0,.85f,0,0,0,0,.85f,0,10000,-20000,30000,1};
  std::memcpy(data.data()+17*16,matrices.data(),count*64);std::memcpy(data.data()+1023*16,&count,4);
  std::array<float,12> extra;for(auto& v:extra)v=float(int(rng()%20001)-10000)/500.f;std::memcpy(data.data()+915*16,extra.data(),48);
  VU1Interpreter vu;{ssx::terrain_original::Rounding rounding;vu.execute(code.data(),code.size(),data.data(),data.size(),runtime.gs(),nullptr,0x80,0,0,5000);}
  for(unsigned i=0;i<count;++i){if(std::memcmp(data.data()+(117+i*3)*16,matrices[i].data(),48)){printf("Normal palette mismatch case%u group%u\n",n,i);return 1;}checked+=12;}
 }
 printf("2000 complete original program2 entry0080 executions preserve%u raw normal-palette words at VU117+, independently of projection/extra matrices;1..25 groups.\n",checked);
}
