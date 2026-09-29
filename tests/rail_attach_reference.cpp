#include "ps2_runtime_macros.h"
#include "../engine/rail_motion.hpp"
#include <fstream>
#include <random>
#include <cstring>
#include <iostream>
void sub_00108A48_0x108a48(uint8_t*,R5900Context*,PS2Runtime*);
void sub_001086B8_0x1086b8(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x420000,g_ps2RecompiledFunctionTableSlotCount=0xc8000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xc8000]={};
static int mode,animationClass;static unsigned flags;static ssx::OriginalRailQueryResult supplied;
static std::vector<ssx::RailVector> queries;
static void result(R5900Context*c,unsigned value){SET_GPR_U32(c,2,value);c->pc=GPR_U32(c,31);}
static void motion(uint8_t*,R5900Context*c,PS2Runtime*){result(c,mode);}
static void mainClass(uint8_t*,R5900Context*c,PS2Runtime*){if(GPR_U32(c,5)!=2)throw std::runtime_error("Wrong animation channel");result(c,animationClass);}
static void sequence(uint8_t*,R5900Context*c,PS2Runtime*){result(c,0x41000);}
static void flag(uint8_t*,R5900Context*c,PS2Runtime*){result(c,!!(flags&(1u<<GPR_U32(c,5))));}
static void query(uint8_t* ram,R5900Context*c,PS2Runtime*){
 ssx::RailVector p;std::memcpy(&p,ram+GPR_U32(c,5),12);queries.push_back(p);
 if(c->f[12]!=300||GPR_U32(c,7)!=1)throw std::runtime_error("Wrong rail query contract");
 if(supplied.found){std::memcpy(ram+GPR_U32(c,6),&supplied.point,12);float w=1;std::memcpy(ram+GPR_U32(c,6)+12,&w,4);std::memcpy(ram+GPR_U32(c,6)+16,&supplied.tangent,12);w=0;std::memcpy(ram+GPR_U32(c,6)+28,&w,4);}
 result(c,supplied.found);
}
int main(int argc,char**argv){
 if(argc!=2)return 2;
 std::ifstream f(argv[1],std::ios::binary);std::vector<uint8_t> elf((std::istreambuf_iterator<char>(f)),{}),ram(32*1024*1024);std::memcpy(ram.data()+0xff000,elf.data(),elf.size());
 auto put=[&](unsigned at,const auto& value){std::memcpy(ram.data()+at,&value,sizeof(value));};
 PS2Runtime runtime;runtime.registerFunction(0x11fe98,motion);runtime.registerFunction(0x311ae8,mainClass);runtime.registerFunction(0x311b20,sequence);runtime.registerFunction(0x1446a0,flag);runtime.registerFunction(0x334680,query);runtime.registerFunction(0x1086b8,sub_001086B8_0x1086b8);
 put(0x20780,0x30000u);put(0x3002c,0x31000u);put(0x208a0,0u);
 std::mt19937 rng(0x108a48);unsigned accepted=0,secondQueries=0;ssx::rail_original::Rounding rounding;
 for(unsigned test=0;test<20000;++test){
  auto random=[&](float scale){return float(int(rng()%20001)-10000)*scale;};
  ssx::OriginalRailRider rider;rider.motionMode=mode=test%4;animationClass=17+(test/4)%5;flags=(test/20)%8;
  rider.tolerance25C.current=float(rng()%1001)/1000;rider.flag330=rng()%2;
  for(unsigned k=0;k<3;++k){rider.bonePosition[k]=random(10);rider.offset9D0[k]=random(.001f);rider.velocity[k]=random(.1f);supplied.point[k]=rider.bonePosition[k]+rider.offset9D0[k]+random(.015f);supplied.tangent[k]=random(.001f);}
  if(test%11==0)rider.velocity={};
  float sum=0;for(auto& x:rider.boneQuaternion){x=random(.0001f);sum+=x*x;}for(auto& x:rider.boneQuaternion)x/=std::sqrt(sum);
  supplied.tangent=ssx::rail_original::normalizeRsqrt(supplied.tangent);supplied.found=test%13!=0;
  put(0x31000,rider.bonePosition);put(0x3100c,1.f);put(0x31010,rider.boneQuaternion);put(0x209d0,rider.offset9D0);put(0x209dc,0.f);put(0x201e0,rider.velocity);put(0x201ec,0.f);put(0x2025c,rider.tolerance25C.current);put(0x20330,rider.flag330);
  R5900Context c{};c.pc=0x108a48;c.vu0_vf[0]=_mm_set_ps(1,0,0,0);SET_GPR_U32(&c,4,0x20000);SET_GPR_U32(&c,5,0x50000);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,29,0x10000);SET_GPR_U32(&c,31,0x12345678);queries.clear();
  sub_00108A48_0x108a48(ram.data(),&c,&runtime);if(c.pc!=0x12345678)throw std::runtime_error("Incomplete original attach");
  unsigned queryIndex=0;ssx::OriginalRailAccess access;access.channel2Class=[](){return animationClass;};access.channel2SequenceFlag=[](unsigned bit){return bool(flags&(1u<<bit));};access.query=[&](auto p){if(queryIndex>=queries.size()||p!=queries[queryIndex++])throw std::runtime_error("Rail probe sequence differs");return supplied;};
  ssx::OriginalRailQueryResult hit;bool native=ssx::originalRailAttachTest(rider,access,hit);
  if(native!=bool(GPR_U32((&c),2))||queryIndex!=queries.size()){std::cerr<<"Attach mismatch "<<test<<" mode "<<mode<<" class "<<animationClass<<" flags "<<flags<<'\n';return 1;}
  accepted+=native;secondQueries+=queries.size()==2;
 }
 std::cout<<"20000 original attach tests match acceptance and probe sequence; "<<accepted<<" accepted, "<<secondQueries<<" secondary probes. World-query results and animation getters are controlled inputs.\n";
}
