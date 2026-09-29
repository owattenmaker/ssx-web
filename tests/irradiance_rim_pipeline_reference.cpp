#include "ps2_runtime_macros.h"
#include "../engine/irradiance_rim_pipeline.hpp"
#include <vector>
#include <random>
#include <cstring>
#include <cstdio>
#include <fstream>
void sub_00389CB8_0x389cb8(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0031BE50_0x31be50(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0031C128_0x31c128(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0031C228_0x31c228(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00389840_0x389840(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00389620_0x389620(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x420000,g_ps2RecompiledFunctionTableSlotCount=0xc8000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xc8000]={};
int main(int argc,char**argv){if(argc!=2)return 2;std::ifstream input(argv[1],std::ios::binary);std::vector<uint8_t> elf((std::istreambuf_iterator<char>(input)),{}),m(32*1024*1024);std::memcpy(m.data()+0xff000,elf.data(),elf.size());auto put=[&](unsigned at,auto value){std::memcpy(m.data()+at,&value,sizeof(value));};put(0x4ff130,std::array<float,4>{0,0,0,1});put(0x4ff140,std::array<float,4>{1,0,0,0});put(0x4ff150,std::array<float,4>{0,1,0,0});put(0x4ff160,std::array<float,4>{0,0,1,0});put(0x4ff1a0,std::array<float,16>{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1});put(0x4a30f0+0x2a90,0x60000u);put(0x610d8,0x62000u);put(0x62118,0u);put(0x6211c,0x100100u);
std::array<float,5> constants;unsigned j=0;for(auto at:{0x4a09f0,0x4a43c0,0x4a09f4,0x4a09f8,0x4a0a08})std::memcpy(&constants[j++],m.data()+at,4);
PS2Runtime rt;rt.registerFunction(0x100100,[](uint8_t*,R5900Context*c,PS2Runtime*){if(GPR_U32(c,4)!=0x60000)throw std::runtime_error("Wrong renderer getter");SET_GPR_U32(c,2,0x20000);c->pc=GPR_U32(c,31);});rt.registerFunction(0x31be50,sub_0031BE50_0x31be50);rt.registerFunction(0x31c128,sub_0031C128_0x31c128);rt.registerFunction(0x31c228,sub_0031C228_0x31c228);rt.registerFunction(0x389840,sub_00389840_0x389840);rt.registerFunction(0x389620,sub_00389620_0x389620);
std::mt19937 rng(0x389cb8);auto random=[&](){return float(int(rng()%20001)-10000)/37.f;};
for(unsigned n=0;n<10000;++n){ssx::OriginalIrradianceCoefficients out;for(auto&row:out)for(auto&v:row)v=random();float angle=random()/100.f,s=std::sin(angle),c=std::cos(angle);std::array<float,16> view{c,s,0,0,-s,c,0,0,0,0,1,0,random(),random(),random(),1};std::array<float,4> point{random(),random(),random(),1};float scale=n<3?float(n):random()/100.f;put(0x20000,view);put(0x30000,out);put(0x40000,point);
R5900Context ctx{};ctx.pc=0x389cb8;ctx.vu0_vf[0]=_mm_set_ps(1,0,0,0);ctx.f[12]=scale;SET_GPR_U32(&ctx,4,0x30000);SET_GPR_U32(&ctx,5,0x40000);SET_GPR_U32(&ctx,28,0x4a30f0);SET_GPR_U32(&ctx,29,0x10000);SET_GPR_U32(&ctx,31,0x12345678);{ssx::terrain_original::Rounding round;sub_00389CB8_0x389cb8(m.data(),&ctx,&rt);}ssx::originalIrradianceViewRim(out,view,point,scale,constants);if(ctx.pc!=0x12345678||std::memcmp(&out,m.data()+0x30000,160))throw std::runtime_error("Complete original rim pipeline mismatch");}
puts("10000 complete original389CB8 rim computations match all40 coefficient words; only the renderer view getter is a controlled boundary.");}
