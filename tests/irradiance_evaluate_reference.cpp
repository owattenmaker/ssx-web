#include "ps2_runtime_macros.h"
#include "runtime/ps2_vu1.h"
#include "../engine/irradiance_evaluate.hpp"
#include <fstream>
#include <random>
#include <cstdio>
#include <cstring>
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x100004,g_ps2RecompiledFunctionTableSlotCount=1;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[1]={};
int main(int argc,char**argv){
 if(argc<3||argc>5)return 2;
 std::ofstream gpu,normals;if(argc>=4)gpu.open(argv[3],std::ios::binary);if(argc==5)normals.open(argv[4],std::ios::binary);
 std::array<uint8_t,16384> original{},data{},evaluation{};
 std::ifstream micro(argv[1],std::ios::binary);micro.read(reinterpret_cast<char*>(original.data()),original.size());if(!micro)return 2;
 std::ifstream bankFile(argv[2],std::ios::binary);std::vector<ssx::OriginalIrradianceCoefficients> banks(45);
 bankFile.read(reinterpret_cast<char*>(banks.data()),banks.size()*160);if(!bankFile)return 2;
 const uint32_t lowerNop=0x8000033c,upperNop=0x2ff,halt=0x400002ff;
 for(unsigned offset=0;offset<evaluation.size();offset+=8){std::memcpy(evaluation.data()+offset,&lowerNop,4);std::memcpy(evaluation.data()+offset+4,&upperNop,4);}
 // Isolate the original upper lighting instructions. Packed normal, supplied
 // matrix columns and coefficient registers are boundary inputs; unrelated vertex,
 // clipping and next-vertex loads are deliberately not executed in this oracle.
 for(unsigned pc:{0x4b0,0x4e0,0x4e8,0x4f0,0x4f8,0x500,0x510,0x518,0x520,0x530,0x538,0x540,0x548,0x550,0x558,0x560,0x568,0x590,0x5b0,0x668})
  std::memcpy(evaluation.data()+pc+4,original.data()+pc+4,4);
 std::memcpy(evaluation.data()+0x684,&halt,4);
 PS2Runtime runtime;std::mt19937 rng(0x1150);
 for(unsigned n=0;n<12000;++n){
  auto bank=banks[n%banks.size()];
  if(n%2)for(auto& row:bank)for(float& v:row)v=float(int(rng()%20001)-10000)/10000;
  std::array<int32_t,3> packed;for(auto&v:packed)v=int(rng()%65536)-32768;
  std::array<std::array<float,3>,3> columns{{{1,0,0},{0,1,0},{0,0,1}}};
  if(n%2)for(auto& column:columns)for(float&v:column)v=float(int(rng()%20001)-10000)/10000;
  if(n<6){packed={};packed[n/2]=n%2?32767:-32768;}
  const auto normal=ssx::originalIrradianceTransformNormal(packed,columns);
  data.fill(0);std::memcpy(data.data()+7*16,&bank,160);
  VU1Interpreter vu;
  {ssx::terrain_original::Rounding rounding;
   vu.execute(original.data(),original.size(),data.data(),data.size(),runtime.gs(),nullptr,0x1150,0,0,1000);
  }
  const auto scaled=ssx::originalIrradianceScale(bank,255.f);
  if(std::memcmp(data.data()+7*16,&scaled,160))throw std::runtime_error("Original lighting upload scaling differs");
  for(unsigned i=0;i<10;++i)std::memcpy(vu.state().vf[i+1],data.data()+(7+i)*16,16);
  for(unsigned i=0;i<3;++i)std::memcpy(&vu.state().vf[16][i],&packed[i],4);
  for(unsigned i=0;i<3;++i){vu.state().vf[15][i]=columns[0][i];vu.state().vf[14][i]=columns[1][i];vu.state().vf[18][i]=columns[2][i];}
  vu.state().vf[16][3]=0;vu.state().i=255.f;
  {ssx::terrain_original::Rounding rounding;vu.execute(evaluation.data(),evaluation.size(),data.data(),data.size(),runtime.gs(),nullptr,0x4b0,0,0,1000);}
  if(std::memcmp(normal.data(),vu.state().vf[16],12))throw std::runtime_error("Original packed normal transform differs");
  if(argc==5){std::array<float,20> record{};for(unsigned i=0;i<3;++i){record[i]=float(packed[i]);for(unsigned j=0;j<3;++j)record[4+i*4+j]=columns[i][j];}std::memcpy(record.data()+16,vu.state().vf[16],12);normals.write(reinterpret_cast<const char*>(record.data()),sizeof(record));if(!normals)return 2;}
  const auto result=ssx::originalIrradianceEvaluate(bank,normal);
  const auto color=ssx::originalIrradianceVertexColor(bank,normal);
  if(argc>=4){
   std::array<float,48> record{};std::memcpy(record.data(),vu.state().vf[16],12);std::memcpy(record.data()+4,data.data()+7*16,160);
   for(unsigned lane=0;lane<4;++lane){uint32_t byte;std::memcpy(&byte,&vu.state().vf[22][lane],4);record[44+lane]=float(byte);}
   gpu.write(reinterpret_cast<const char*>(record.data()),sizeof(record));if(!gpu)return 2;
  }

  for(unsigned lane=0;lane<4;++lane){uint32_t packed;std::memcpy(&packed,&vu.state().vf[22][lane],4);if(packed!=color[lane])throw std::runtime_error("Original vertex color quantization differs");}

  if(std::memcmp(result.data(),vu.state().vf[15],16)){
   std::printf("Irradiance evaluation mismatch case%u\n",n);
   for(unsigned i=0;i<4;++i)std::printf("lane%u native%.9g original%.9g\n",i,result[i],vu.state().vf[15][i]);return 1;
  }
 }
 std::puts("12000 original VU lighting evaluations match all four float lanes, including original ITOF15 normal decoding/transform, actual255 coefficient preprocessing, original FTOI0 vertex-color quantization,45 shipped banks, signed coefficients and normal axes.");
}
