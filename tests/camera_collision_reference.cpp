#include "ps2_runtime_macros.h"
#include "../engine/original_camera.hpp"
#include <fstream>
#include <random>
#include <vector>
#include <cstring>
#include <cstdio>
void sub_0015EE00_0x15ee00(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x420000,g_ps2RecompiledFunctionTableSlotCount=0xc8000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xc8000]={};
static ssx::OriginalCameraQuad queryA,queryB,normal;
static float fraction;
static unsigned builds,queries;
static void queryBuild(uint8_t* memory,R5900Context* c,PS2Runtime*){
 ++builds;std::memcpy(queryA.data(),memory+GPR_U32(c,5),16);std::memcpy(queryB.data(),memory+GPR_U32(c,6),16);
 if(c->f[12]!=ssx::original_camera::originalCameraTerrainPreferredFraction||GPR_U32(c,7)!=2)throw std::runtime_error("Original camera query contract differs");
 c->pc=GPR_U32(c,31);
}
static void query(uint8_t* memory,R5900Context* c,PS2Runtime*){
 ++queries;std::memcpy(memory+GPR_U32(c,6)+16,normal.data(),16);c->f[0]=fraction;c->pc=GPR_U32(c,31);
}
static void overrideMode(uint8_t*,R5900Context* c,PS2Runtime*){SET_GPR_U32(c,2,0);c->pc=GPR_U32(c,31);}
int main(int argc,char** argv){
 if(argc!=2)return 2;
 std::ifstream file(argv[1],std::ios::binary);std::vector<uint8_t> elf((std::istreambuf_iterator<char>(file)),{}),memory(32*1024*1024);
 std::memcpy(memory.data()+0xff000,elf.data(),elf.size());
 auto put=[&](unsigned at,auto value){std::memcpy(memory.data()+at,&value,sizeof(value));};
 PS2Runtime runtime;runtime.registerFunction(0x32e100,queryBuild);runtime.registerFunction(0x336850,query);runtime.registerFunction(0x15d020,overrideMode);
 put(0x4a30f0-0x848,0x30000u);put(0x30084,0x31000u);put(0x31020,0x32000u);
 put(0x4ff160,ssx::OriginalCameraQuad{0,0,1,0});
 std::mt19937 rng(0x15ee00);unsigned mismatch=0,changed=0;
 for(unsigned n=0;n<20000;++n){
  auto random=[&](float scale){return float(int(rng()%20001)-10000)*scale;};
  ssx::OriginalCameraCompositorState state;state.eye={random(20),random(20),random(20),1};
  state.lookAt={state.eye[0]+300+random(.02f),state.eye[1]+random(.02f),state.eye[2]+random(.08f),1};
  state.lift=random(.01f);state.lastProbeNormal={random(.0001f),random(.0001f),1,0};
  normal={random(.0001f),random(.0001f),n%2?1.f:-1.f,0};fraction=n%7?float(rng()%1001)/1000:-1.f;
  put(0x200e0,state.lookAt);put(0x20100,state.eye);put(0x20460,state.lift);put(0x20470,state.lastProbeNormal);
  R5900Context c{};c.pc=0x15ee00;c.vu0_vf[0]=_mm_set_ps(1,0,0,0);
  SET_GPR_U32(&c,4,0x20000);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,29,0x10000);SET_GPR_U32(&c,31,0x12345678);
  builds=queries=0;unsigned nativeQueries=0;
  ssx::OriginalCameraInput input;input.terrainProbe=[&](const auto& a,const auto& b)->std::optional<ssx::OriginalCameraProbeHit>{
   ++nativeQueries;if(a!=queryA||b!=queryB)throw std::runtime_error("Original camera probe endpoints differ");
   if(fraction<0)return {};return ssx::OriginalCameraProbeHit{fraction,normal};
  };
  {ssx::original_camera::Rounding rounding;sub_0015EE00_0x15ee00(memory.data(),&c,&runtime);ssx::original_camera::cameraCollision(state,input);}
  if(c.pc!=0x12345678||builds!=1||queries!=1||nativeQueries!=1)throw std::runtime_error("Camera collision control flow differs");
  auto compare=[&](unsigned offset,const auto& value){if(std::memcmp(memory.data()+0x20000+offset,&value,sizeof(value))){if(mismatch++<5)std::printf("Collision mismatch case %u offset %x fraction %g normalZ %g\n",n,offset,fraction,normal[2]);}};
  compare(0x100,state.eye);compare(0x460,state.lift);compare(0x470,state.lastProbeNormal);compare(0x4a0,state.occludedLastFrame);compare(0x4a4,state.firstFrameAfterReset);changed+=fraction>0;
 }
 if(mismatch)throw std::runtime_error("Original camera collision fields differ: "+std::to_string(mismatch));
 std::printf("20000 original camera clearance calls match eye, lift, normal and flags; %u positive-hit cases. Probe endpoints and midpoint query contract match. Terrain result and override-disabled getter are controlled boundaries.\n",changed);
}
