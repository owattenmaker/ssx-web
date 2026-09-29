#include "ps2_runtime_macros.h"
#include "../engine/rider_light_world.hpp"
#include <fstream>
#include <random>
#include <vector>
#include <cstring>
#include <cstdio>
#define DECLARE(name) void name(uint8_t*,R5900Context*,PS2Runtime*)
DECLARE(sub_001220D8_0x1220d8);
DECLARE(sub_00389260_0x389260);DECLARE(sub_00389CB8_0x389cb8);
DECLARE(sub_0031BE50_0x31be50);DECLARE(sub_0031C128_0x31c128);DECLARE(sub_0031C228_0x31c228);
DECLARE(sub_00389840_0x389840);DECLARE(sub_00389620_0x389620);
DECLARE(sub_0038A6A8_0x38a6a8);DECLARE(sub_0038A530_0x38a530);DECLARE(sub_0038A618_0x38a618);
DECLARE(sub_00389520_0x389520);DECLARE(sub_00389308_0x389308);DECLARE(sub_00389558_0x389558);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x420000,g_ps2RecompiledFunctionTableSlotCount=0xc8000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xc8000]={};
static unsigned slot;static float rimScale;
int main(int argc,char**argv){
 if(argc!=2)return 2;std::ifstream file(argv[1],std::ios::binary);std::vector<uint8_t> elf((std::istreambuf_iterator<char>(file)),{}),m(32*1024*1024);
 std::memcpy(m.data()+0xff000,elf.data(),elf.size());auto put=[&](unsigned at,auto value){std::memcpy(m.data()+at,&value,sizeof(value));};
 put(0x4ff130,std::array<float,4>{0,0,0,1});put(0x4ff140,std::array<float,4>{1,0,0,0});put(0x4ff150,std::array<float,4>{0,1,0,0});put(0x4ff160,std::array<float,4>{0,0,1,0});put(0x4ff1a0,std::array<float,16>{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1});
 put(0x4a30f0+0x2a90,0x60000u);put(0x610d8,0x62000u);put(0x62118,0u);put(0x6211c,0x100100u);
 put(0x806c0,0x90000u);put(0x90038,int16_t(-0x6c0));put(0x9003c,0x100104u);
 std::array<float,5> constants;unsigned index=0;for(auto at:{0x4a09f0,0x4a43c0,0x4a09f4,0x4a09f8,0x4a0a08})std::memcpy(&constants[index++],m.data()+at,4);
 PS2Runtime runtime;
#define REGISTER(address,name) runtime.registerFunction(address,name)
 REGISTER(0x389260,sub_00389260_0x389260);REGISTER(0x389cb8,sub_00389CB8_0x389cb8);
 REGISTER(0x31be50,sub_0031BE50_0x31be50);REGISTER(0x31c128,sub_0031C128_0x31c128);REGISTER(0x31c228,sub_0031C228_0x31c228);
 REGISTER(0x389840,sub_00389840_0x389840);REGISTER(0x389620,sub_00389620_0x389620);
 REGISTER(0x38a6a8,sub_0038A6A8_0x38a6a8);REGISTER(0x38a530,sub_0038A530_0x38a530);REGISTER(0x38a618,sub_0038A618_0x38a618);
 REGISTER(0x389520,sub_00389520_0x389520);REGISTER(0x389308,sub_00389308_0x389308);REGISTER(0x389558,sub_00389558_0x389558);
 REGISTER(0x100100,[](uint8_t*,R5900Context*c,PS2Runtime*){SET_GPR_U32(c,2,0x20000);c->pc=GPR_U32(c,31);});
 REGISTER(0x100104,[](uint8_t*,R5900Context*c,PS2Runtime*){if(GPR_U32(c,4)!=0x80000)throw std::runtime_error("Wrong rider index getter");SET_GPR_U32(c,2,slot);c->pc=GPR_U32(c,31);});
 REGISTER(0x2edfb8,[](uint8_t*,R5900Context*c,PS2Runtime*){if(GPR_U32(c,4)!=slot)throw std::runtime_error("Wrong environment slot");SET_GPR_U32(c,2,0x40000);c->pc=GPR_U32(c,31);});
 REGISTER(0x2eefa8,[](uint8_t*,R5900Context*c,PS2Runtime*){if(GPR_U32(c,4)!=slot)throw std::runtime_error("Wrong rim slot");c->f[0]=rimScale;c->pc=GPR_U32(c,31);});
 REGISTER(0x11ff48,[](uint8_t* memory,R5900Context*c,PS2Runtime*){if(GPR_U32(c,5)!=0x80000)throw std::runtime_error("Wrong pose target");std::memcpy(memory+GPR_U32(c,4),memory+0x30000,16);c->pc=GPR_U32(c,31);});
 std::mt19937 rng(0x1220d8);auto random=[&](){return float(int(rng()%20001)-10000)/37.f;};
 for(unsigned n=0;n<2000;++n){
  slot=n%6;rimScale=random()/200;
  ssx::OriginalIrradianceCoefficients environment;for(auto& row:environment)for(float& v:row)v=random()/300;
  const float angle=random()/100,s=std::sin(angle),c=std::cos(angle);
  std::array<float,16> view{c,s,0,0,-s,c,0,0,0,0,1,0,random(),random(),random(),1};
  std::array<float,4> point{random(),random(),random(),1};put(0x20000,view);put(0x30000,point);put(0x40000,environment);
  std::array<ssx::OriginalRiderLocalLight,8> storage;std::array<const ssx::OriginalRiderLocalLight*,8> lights{};
  for(unsigned i=0;i<8;++i){
   const unsigned address=0xa0000+i*128;
   if((n+i)%5==0){put(0x80794+i*4,0u);continue;}
   auto& entry=storage[i];auto& light=entry.light;entry.kind=(n+i)%4;lights[i]=&entry;
   light.geometry.radius=1000;light.geometry.position={random(),random(),random()};light.geometry.axis={random()/300,random()/300,random()/300};light.color={random()/300,random()/300,random()/300};light.intensity=random()/50;light.outerCosine=-.8f;light.innerCosine=.6f;light.distanceMode=int8_t(n%5);light.angularMode=int8_t(n%8-2);
   if(n%9==0)light.geometry.position={point[0],point[1],point[2]};
   put(address+16,entry.kind);put(address+20,light.intensity);put(address+28,light.geometry.radius);put(address+32,light.color);put(address+44,light.geometry.axis);put(address+56,light.geometry.position);put(address+92,light.innerCosine);put(address+96,light.outerCosine);put(address+100,light.distanceMode);put(address+101,light.angularMode);put(0x80794+i*4,address);
  }
  const bool controller=n%3!=0;put(0x8077c,controller?0xb0000u:0u);
  std::array<ssx::OriginalRiderDirectionalLight,5> directional;const unsigned count=n%6;
  for(auto& light:directional){light.direction={random()/300,random()/300,random()/300};light.color={random()/300,random()/300,random()/300};}
  ssx::OriginalRiderExtraLighting extra{{random()/300,random()/300,random()/300},{directional.data(),count}};
  put(0xb0d30,extra.ambient);put(0xb0d3c,count);
  for(unsigned i=0;i<count;++i){put(0xb0d40+i*12,directional[i].color);put(0xb0d80+i*16,directional[i].direction);}
  R5900Context ctx{};ctx.pc=0x1220d8;ctx.vu0_vf[0]=_mm_set_ps(1,0,0,0);SET_GPR_U32(&ctx,4,0x80000);SET_GPR_U32(&ctx,28,0x4a30f0);SET_GPR_U32(&ctx,29,0x10000);SET_GPR_U32(&ctx,31,0x12345678);
  {ssx::terrain_original::Rounding rounding;sub_001220D8_0x1220d8(m.data(),&ctx,&runtime);}
  const auto result=ssx::originalRiderIrradiance(environment,view,point,rimScale,constants,lights,controller?&extra:nullptr);
  std::vector<ssx::OriginalWorldLight> worldLights;ssx::OriginalRiderLightWorld::Selection retained;
  for(unsigned i=0;i<8;++i)if(lights[i]){retained.ids[i]=i+1;worldLights.push_back({i+1,{lights[i]->kind,lights[i]->light,1.f}});}
  ssx::OriginalRiderLightWorld world({},std::array<ssx::OriginalSpatialLightRoot,8>{},worldLights);
  auto retainedBefore=retained.ids;
  auto worldResult=world.shade(retained,environment,view,point,rimScale,constants,controller?&extra:nullptr);
  if(retained.ids!=retainedBefore||std::memcmp(&worldResult,m.data()+0x807c0,160))throw std::runtime_error("Retained world light shading differs from original assembly");
  if(ctx.pc!=0x12345678||std::memcmp(&result,m.data()+0x807c0,160)){printf("Rider irradiance mismatch case%u\n",n);return 1;}
 }
 puts("2000 complete original rider lighting assemblies match all40 coefficient words, including retained asset-ID world shading; environment/view/pose/rim getters are controlled boundaries, all numeric callees are original.");
}
