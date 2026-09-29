#include "ps2_runtime_macros.h"
#include "../engine/skin_palette.hpp"
#include "../engine/pose_matrix.hpp"
#include <fstream>
#include <random>
#include <cstring>
#include <cstdio>
void sub_00386128_0x386128(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x420000,g_ps2RecompiledFunctionTableSlotCount=0xc8000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xc8000]={};
void sub_00310640_0x310640(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00310120_0x310120(uint8_t*,R5900Context*,PS2Runtime*);
int main(int argc,char**argv){
 if(argc!=3)return 2;
 std::ofstream gpu(argv[2],std::ios::binary);unsigned gpuRecords=0;
 std::vector<uint8_t> m(32*1024*1024);PS2Runtime runtime;std::mt19937 rng(0x386bd0);
 auto put=[&](unsigned at,auto value){std::memcpy(m.data()+at,&value,sizeof(value));};
 for(unsigned n=0;n<20000;++n){
  std::array<ssx::OriginalSkinMatrix,32> bones;
  for(auto& matrix:bones)for(auto& x:matrix)x=float(int(rng()%200001)-100000)/137.f;
  if(n%4==0)for(auto& matrix:bones)matrix={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
  std::vector<ssx::OriginalSkinWeights> groups(n%7);
  for(unsigned i=0;i<groups.size();++i){auto& group=groups[i];unsigned count=1+rng()%4;
   for(unsigned j=0;j<count;++j)group.push_back({int16_t(n%4?int(rng()%201)-50:100/int(count)),uint8_t(rng()%32)});
   const unsigned at=0x50000+i*32;put(0x40000+i*12,uint32_t(count));put(0x40004+i*12,at);
   for(unsigned j=0;j<count;++j){put(at+j*4,group[j].weight);put(at+j*4+2,group[j].bone);put(at+j*4+3,uint8_t(0));}
  }
  put(0x30000,bones);put(0x213e8,uint32_t(0x60000));std::memset(m.data()+0x60000,0xa5,7*64);
  R5900Context c{};c.pc=0x386bd0;c.vu0_vf[0]=_mm_set_ps(1,0,0,0);SET_GPR_U32(&c,4,0x20000);SET_GPR_U32(&c,5,0x30000);SET_GPR_U32(&c,7,0x40000);SET_GPR_U32(&c,8,groups.size());SET_GPR_U32(&c,9,n%2?0x60000:0);SET_GPR_U32(&c,29,0x10000);SET_GPR_U32(&c,31,0x12345678);
  {ssx::terrain_original::Rounding rounding;sub_00386128_0x386128(m.data(),&c,&runtime);}
  auto result=ssx::originalSkinPalette(bones,groups);
  if(c.pc!=0x12345678||std::memcmp(result.data(),m.data()+0x60000,result.size()*64)){printf("Original skin palette mismatch%u\n",n);return 1;}
  for(unsigned group=0;group<groups.size()&&gpuRecords<12000;++group)for(unsigned col=0;col<4&&gpuRecords<12000;++col){
   std::array<float,28> record{};record[0]=groups[group].size();
   for(unsigned j=0;j<groups[group].size();++j){record[4+j]=groups[group][j].weight;for(unsigned lane=0;lane<4;++lane)record[8+j*4+lane]=bones[groups[group][j].bone][col*4+lane];}
   std::memcpy(record.data()+24,m.data()+0x60000+group*64+col*16,16);gpu.write(reinterpret_cast<const char*>(record.data()),sizeof(record));++gpuRecords;
  }
  for(unsigned i=groups.size()*64;i<7*64;++i)if(m[0x60000+i]!=0xa5)throw std::runtime_error("Original palette output overrun");
  std::array<float,4> position,rotation,scale;
  for(unsigned k=0;k<4;++k){position[k]=float(int(rng()%200001)-100000);rotation[k]=float(int(rng()%20001)-10000)/10000.f;scale[k]=float(rng()%20001)/10000.f;}
  put(0x20030,uint32_t(0x70000));put(0x20034,uint32_t(0x80000));put(0x20140,scale);put(0x90000,position);put(0x90010,rotation);
  const unsigned bone=n%29;c={};c.pc=0x310120;c.vu0_vf[0]=_mm_set_ps(1,0,0,0);SET_GPR_U32(&c,4,0x20000);SET_GPR_U32(&c,5,bone);SET_GPR_U32(&c,6,0x90000);SET_GPR_U32(&c,31,0x12345678);
  {ssx::terrain_original::Rounding rounding;sub_00310120_0x310120(m.data(),&c,&runtime);}
  auto pose=ssx::originalPoseMatrices(position,rotation,scale);
  if(c.pc!=0x12345678||std::memcmp(pose.unscaled.data(),m.data()+0x70000+bone*64,64)||std::memcmp(pose.scaled.data(),m.data()+0x80000+bone*64,64))throw std::runtime_error("Original pose matrix construction differs");

  const unsigned boneCount=n%30;std::array<ssx::OriginalSkinMatrix,32> binds;for(unsigned i=0;i<32;++i)binds[i]=bones[31-i];
  put(0x20010,boneCount);put(0x20034,uint32_t(0x30000));put(0x20038,uint32_t(0xa0000));put(0xa0000,binds);put(0x4a30f0+0x2858,uint32_t(1));
  c={};c.pc=0x310640;c.vu0_vf[0]=_mm_set_ps(1,0,0,0);SET_GPR_U32(&c,4,0x20000);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,29,0x10000);
  {ssx::terrain_original::Rounding rounding;sub_00310640_0x310640(m.data(),&c,&runtime);}
  if(c.pc!=0x310790)throw std::runtime_error("Original skin matrix stage did not return");
  for(unsigned i=0;i<boneCount;++i){auto combined=ssx::originalSkinPoseMatrix(bones[i],binds[i]);if(std::memcmp(combined.data(),m.data()+0x4fc420+i*64,64))throw std::runtime_error("Original pose/bind multiplication differs");}

 }
 if(gpuRecords!=12000||!gpu)throw std::runtime_error("Incomplete skin GPU reference");
 std::ifstream captured(argv[1],std::ios::binary);std::vector<char> records((std::istreambuf_iterator<char>(captured)),{});
 if(records.size()!=87*176)throw std::runtime_error("Expected87 captured pose matrices");
 for(unsigned n=0;n<87;++n){std::array<float,4> position,q,scale;auto p=records.data()+n*176;std::memcpy(position.data(),p,16);std::memcpy(q.data(),p+16,16);std::memcpy(scale.data(),p+32,16);auto result=ssx::originalPoseMatrices(position,q,scale);
  if(std::memcmp(result.unscaled.data(),p+48,64)||std::memcmp(result.scaled.data(),p+112,64)){printf("Captured pose matrix mismatch%u\n",n);return 1;}}
 puts("20000 original310120 pose matrix calls,20000 pose/bind palette loops and87 captured unscaled/scaled bone matrices match every word.");

 puts("20000 complete original386BD0 skin palette calls match every matrix word; zero/multiple groups,1..4 influences,integer weights and explicit/default output pointers.");
}
