#include "ps2_runtime_macros.h"
#include "../engine/camera_transform.hpp"
#include <fstream>
#include <random>
#include <vector>
#include <cstring>
#include <cstdio>
void sub_00166F90_0x166f90(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0031B748_0x31b748(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0031B7A8_0x31b7a8(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0031BE50_0x31be50(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x420000,g_ps2RecompiledFunctionTableSlotCount=0xc8000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xc8000]={};
void sub_003956B0_0x3956b0(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0015E668_0x15e668(uint8_t*,R5900Context*,PS2Runtime*);
static std::array<float,16> observed;
int main(int argc,char**argv){
 if(argc!=3)return 2;std::ifstream input(argv[1],std::ios::binary);std::vector<uint8_t> elf((std::istreambuf_iterator<char>(input)),{}),m(32*1024*1024);std::memcpy(m.data()+0xff000,elf.data(),elf.size());
 auto put=[&](unsigned at,auto value){std::memcpy(m.data()+at,&value,sizeof(value));};
 put(0x4ff130,std::array<float,4>{0,0,0,1});put(0x4ff150,std::array<float,4>{0,1,0,0});put(0x4ff160,std::array<float,4>{0,0,1,0});put(0x4ff1a0,std::array<float,16>{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1});
 PS2Runtime rt;rt.registerFunction(0x31be50,sub_0031BE50_0x31be50);rt.registerFunction(0x31b7a8,sub_0031B7A8_0x31b7a8);
 rt.registerFunction(0x31b748,[](uint8_t*m,R5900Context*c,PS2Runtime*r){std::memcpy(observed.data(),m+GPR_U32(c,5),64);sub_0031B748_0x31b748(m,c,r);});
 std::mt19937 rng(0x166f90);
 for(unsigned n=0;n<20000;++n){
  float pitch=float(int(rng()%62833)-31416)/10000.f,yaw=float(int(rng()%125665)-62832)/10000.f;
  if(n<8){pitch=n%2?0.f:-0.f;yaw=(n/2)*1.5707963705062866f;}
  std::array<float,4> eye{float(int(rng()%200001)-100000),float(int(rng()%200001)-100000),float(rng()%100000),float(n%3)};
  put(0x20040,eye);put(0x20050,yaw);put(0x20054,pitch);
  R5900Context c{};c.pc=0x166f90;c.vu0_vf[0]=_mm_set_ps(1,0,0,0);SET_GPR_U32(&c,4,0x20000);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,29,0x10000);SET_GPR_U32(&c,31,0x12345678);
  {ssx::terrain_original::Rounding rounding;sub_00166F90_0x166f90(m.data(),&c,&rt);}
  auto result=ssx::originalCameraTransform(eye,yaw,pitch);
  if(c.pc!=0x12345678||std::memcmp(result.matrix.data(),observed.data(),64)||std::memcmp(result.position.data(),m.data()+0x20060,16)||std::memcmp(result.quaternion.data(),m.data()+0x20070,16)){
   printf("Camera transform mismatch%u yaw%g pitch%g\n",n,yaw,pitch);for(unsigned i=0;i<4;++i){float expected;std::memcpy(&expected,m.data()+0x20070+i*4,4);printf("q%u %.9g %.9g\n",i,result.quaternion[i],expected);}return 1;
  }
  const float third=float(int(rng()%62833)-31416)/10000.f;
  put(0x313e4,0x40000u);put(0x50000,eye);put(0x4ff140,std::array<float,4>{1,0,0,0});
  c={};c.pc=0x395750;c.f[12]=pitch;c.f[13]=yaw;c.f[14]=third;c.vu0_vf[0]=_mm_set_ps(1,0,0,0);SET_GPR_U32(&c,4,0x30000);SET_GPR_U32(&c,5,0x50000);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,29,0x10000);SET_GPR_U32(&c,31,0x12345678);
  {ssx::terrain_original::Rounding rounding;sub_003956B0_0x3956b0(m.data(),&c,&rt);}
  auto view=ssx::originalRendererEulerView(eye,pitch,yaw,third);
  if(c.pc!=0x12345678||std::memcmp(view.data(),m.data()+0x40000,64)){printf("Renderer view mismatch%u\n",n);return 1;}

  put(0x60020,result.position);put(0x60030,result.quaternion);
  put(0x4c53a0,std::array<float,16>{0,0,1,0,-1,0,0,0,0,1,0,0,0,0,0,1});
  c={};c.pc=0x15e968;c.vu0_vf[0]=_mm_set_ps(1,0,0,0);SET_GPR_U32(&c,17,0x60000);SET_GPR_U32(&c,18,0x10020);SET_GPR_U32(&c,20,0x60040);SET_GPR_U32(&c,21,0x60030);SET_GPR_U32(&c,22,0x4c0000);SET_GPR_U32(&c,29,0x10000);
  {ssx::terrain_original::Rounding rounding;sub_0015E668_0x15e668(m.data(),&c,&rt);}
  auto renderView=ssx::originalCameraRenderView(result.position,result.quaternion);
  if(c.pc!=0x15eaec||std::memcmp(renderView.data(),m.data()+0x60040,64)){printf("Camera render view mismatch%u\n",n);for(unsigned k=0;k<16;++k){float expected;std::memcpy(&expected,m.data()+0x60040+k*4,4);if(std::memcmp(&expected,&renderView[k],4))printf("word%u %.9g %.9g\n",k,renderView[k],expected);}return 1;}

 }
 std::ifstream captured(argv[2],std::ios::binary);std::vector<char> bytes((std::istreambuf_iterator<char>(captured)),{});
 if(bytes.size()!=4*96)throw std::runtime_error("Expected four captured camera view fixtures");
 for(unsigned n=0;n<4;++n){std::array<float,4> eye,q;std::memcpy(eye.data(),bytes.data()+n*96,16);std::memcpy(q.data(),bytes.data()+n*96+16,16);auto view=ssx::originalCameraRenderView(eye,q);if(std::memcmp(view.data(),bytes.data()+n*96+32,64)){printf("Captured view mismatch%u\n",n);return 1;}}
 puts("Four captured PS2 camera views also match all16 words, including the directly observed rider-lighting view.");

 puts("20000 original camera transforms, Euler views and final quaternion render views match every compared float word; real sincos/quaternion callees, final view stage15E968..15EAE8.");
}
