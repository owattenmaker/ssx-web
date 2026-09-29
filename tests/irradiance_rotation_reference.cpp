#include "ps2_runtime_macros.h"
#include "../engine/irradiance_rotation.hpp"
#include <vector>
#include <random>
#include <cstring>
#include <cstdio>
#include <fstream>
void sub_00389CB8_0x389cb8(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0031BE50_0x31be50(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x420000,g_ps2RecompiledFunctionTableSlotCount=0xc8000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xc8000]={};
int main(int argc,char**argv){if(argc!=2)return 2;std::ifstream input(argv[1],std::ios::binary);std::vector<uint8_t> elf((std::istreambuf_iterator<char>(input)),{}),m(32*1024*1024);std::memcpy(m.data()+0xff000,elf.data(),elf.size());auto put=[&](unsigned at,auto value){std::memcpy(m.data()+at,&value,sizeof(value));};put(0x4ff130,std::array<float,4>{0,0,0,1});put(0x4ff150,std::array<float,4>{0,1,0,0});put(0x4ff160,std::array<float,4>{0,0,1,0});put(0x4ff1a0,std::array<float,16>{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1});PS2Runtime rt;rt.registerFunction(0x31be50,sub_0031BE50_0x31be50);std::mt19937 rng(0x389f9c);
for(unsigned n=0;n<20000;++n){float pitch=float(int(rng()%30001)-15000)/10000.f,yaw=float(rng()%62832)/10000.f;if(n<4){pitch=n%2?0.f:-0.f;yaw=n<2?0.f:3.1415927410125732f;}std::array<float,4> eye{float(int(rng()%200001)-100000),float(int(rng()%200001)-100000),float(rng()%100000),1};put(0x100e0,eye);if(n%2==0)put(0x4a30f0+0x28b4,0u);
R5900Context c{};c.pc=0x389f9c;c.f[23]=pitch;c.f[21]=yaw;c.vu0_vf[0]=_mm_set_ps(1,0,0,0);SET_GPR_U32(&c,2,0x500000);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,29,0x10000);{ssx::terrain_original::Rounding round;sub_00389CB8_0x389cb8(m.data(),&c,&rt);}auto result=ssx::originalIrradianceRimRotation(pitch,yaw,eye);if(c.pc!=0x38a4d4||std::memcmp(result.data(),m.data()+0x101a0,36)){printf("Rim rotation mismatch %u\n",n);return 1;}}
puts("20000 original rim rotation matrices match all9 float words, with real sin/cos, lazy axis-conversion initialization and signed-zero angles.");}
