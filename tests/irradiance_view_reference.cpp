#include "ps2_runtime_macros.h"
#include "../engine/irradiance_view.hpp"
#include "../engine/irradiance_rim.hpp"
#include <vector>
#include <random>
#include <cstring>
#include <cstdio>
void sub_00389CB8_0x389cb8(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x420000,g_ps2RecompiledFunctionTableSlotCount=0xc8000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xc8000]={};
int main(){std::vector<uint8_t> m(32*1024*1024);PS2Runtime rt;std::mt19937 rng(0x389d50);auto random=[&](){return float(int(rng()%20001)-10000)/37.f;};
for(unsigned n=0;n<20000;++n){std::array<float,16> matrix;for(auto&v:matrix)v=random()/270.f;matrix[3]=matrix[7]=matrix[11]=0;matrix[15]=1;for(unsigned i=12;i<15;++i)matrix[i]=random();if(n%5==0)matrix={1,0,0,0,0,1,0,0,0,0,1,0,matrix[12],matrix[13],matrix[14],1};std::array<float,4> point={random(),random(),random(),1};std::memcpy(m.data()+0x20000,&matrix,64);std::memcpy(m.data()+0x30000,&point,16);
R5900Context c{};c.pc=0x389d50;c.vu0_vf[0]=_mm_set_ps(1,0,0,0);SET_GPR_U32(&c,2,0x20000);SET_GPR_U32(&c,16,0x30000);SET_GPR_U32(&c,29,0x10000);{ssx::terrain_original::Rounding round;sub_00389CB8_0x389cb8(m.data(),&c,&rt);}auto r=ssx::originalIrradianceViewDirection(matrix,point);
//The source's direction scratch has Z cleared for horizontal length; F12 keeps Z.
auto flattened=r.direction;flattened[2]=0;if(c.pc!=0x389e98||std::memcmp(m.data()+0x100e0,&r.eye,16)||std::memcmp(m.data()+0x100f0,&flattened,16)||std::memcmp(&c.f[12],&r.direction[2],4)||std::memcmp(&c.f[20],&r.horizontalLength,4))throw std::runtime_error("Original irradiance view direction differs");}
for(unsigned n=0;n<2000;++n){std::array<float,4> values{random(),random(),random(),random()};if(n==0)values[1]=-0.f;
std::memcpy(m.data()+0x4a30f0-0x2700,&values[0],4);std::memcpy(m.data()+0x4a43c0,&values[1],4);std::memcpy(m.data()+0x4a30f0-0x26fc,&values[2],4);std::memcpy(m.data()+0x4a30f0-0x26f8,&values[3],4);
R5900Context c{};c.pc=0x389cb8;SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,29,0x102e0);sub_00389CB8_0x389cb8(m.data(),&c,&rt);auto shape=ssx::originalIrradianceRimShape(values[0],values[1],values[2],values[3]);if(c.pc!=0x389d38)throw std::runtime_error("Rim shape original incomplete");for(unsigned i=0;i<10;++i)if(std::memcmp(m.data()+0x1000c+i*16,&shape[i][3],4))throw std::runtime_error("Rim shape mismatch");}
puts("2000 original sparse rim-shape seeds match all10 fourth-lane words, including signed zero. Unused RGB scratch is not compared.");
puts("20000 original irradiance view-direction preparations match eye/direction/horizontal-length bits for nondegenerate affine-view fixtures. Angle and rim-matrix construction remain separate.");}
