#include "ps2_runtime_macros.h"
#include "../engine/soft_collision_control.hpp"
#include "../engine/collision_event.hpp"
#include <fstream>
#include <cstring>
#include <random>
#include <cstdio>
void sub_0012E778_0x12e778(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00116378_0x116378(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00116120_0x116120(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00113E80_0x113e80(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00113F88_0x113f88(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00108388_0x108388(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x340000,g_ps2RecompiledFunctionTableSlotCount=0x90000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0x90000]={};
static int motion,control,selected,nextControl,nextMotion;static bool ended,obstructed,boostCalled,bHeld,bPressed,balanceCalled,restore,rail,cancel,strong;
static float balance;static uint32_t randomValue;static unsigned draws;
static void dependency(uint8_t*,R5900Context*c,PS2Runtime*){
 switch(c->pc){
 case 0x11fe98:SET_GPR_U32(c,2,motion);break;
 case 0x11fee8:SET_GPR_U32(c,2,control);break;
 case 0x11fec8:nextControl=int(GPR_U32(c,5));break;
 case 0x11fe78:nextMotion=int(GPR_U32(c,5));break;
 case 0x106848:SET_GPR_U32(c,2,obstructed);break;
 case 0x114130:boostCalled=true;bHeld=GPR_U32(c,5);bPressed=GPR_U32(c,6);break;
 case 0x113f38:balanceCalled=true;balance=c->f[12];break;
 case 0x312ae8:SET_GPR_U32(c,2,ended);break;
 case 0x115640:restore=true;break;
 case 0x1326c8:rail=true;break;
 case 0x28b180:SET_GPR_U32(c,2,0x90000);break;
 case 0x29a220:case 0x270970:case 0x11a088:break;
 case 0x131348:cancel=true;break;
 case 0x3128e8:selected=int(GPR_U32(c,5));break;
 case 0x317810:SET_GPR_U32(c,2,randomValue+draws++*1234567u);break;
 case 0x2a0e70:strong=GPR_U32(c,6);break;
 default:throw std::runtime_error("Unknown soft-control oracle dependency");
 }c->pc=GPR_U32(c,31);
}
int main(int argc,char**argv){
 if(argc!=2)return 2;std::ifstream input(argv[1],std::ios::binary);std::vector<uint8_t> ram((std::istreambuf_iterator<char>(input)),{});if(ram.size()!=32*1024*1024)return 3;
 PS2Runtime runtime;for(uint32_t pc:{0x11fe98,0x11fee8,0x11fec8,0x11fe78,0x106848,0x114130,0x113f38,0x312ae8,0x115640,0x1326c8,0x28b180,0x29a220,0x270970,0x11a088,0x131348,0x3128e8,0x317810,0x2a0e70})runtime.registerFunction(pc,dependency);
 runtime.registerFunction(0x116378,sub_00116378_0x116378);runtime.registerFunction(0x116120,sub_00116120_0x116120);runtime.registerFunction(0x113e80,sub_00113E80_0x113e80);runtime.registerFunction(0x113f88,sub_00113F88_0x113f88);
 auto put=[&](unsigned at,const auto&v){memcpy(ram.data()+at,&v,sizeof(v));};auto getFloat=[&](unsigned at){float f;memcpy(&f,ram.data()+at,4);return f;};auto vec=[&](unsigned at,std::array<float,3> v){put(at,v);put(at+12,0.f);};
 constexpr uint32_t actor=0x30000,owner=0x40000,controller=0x50000,packet=0x60000,eventAt=0x61000,done=0x12345678;
 put(actor+0x77c,owner);put(controller,actor);put(actor+0x86c,0u);
 auto context=[&](uint32_t pc){R5900Context c{};c.pc=pc;c.vu0_vf[0]=_mm_set_ps(1,0,0,0);SET_GPR_U32((&c),28,0x4a30f0);SET_GPR_U32((&c),29,0x10000);SET_GPR_U32((&c),31,done);return c;};
 std::mt19937 random(0x108388);std::uniform_real_distribution<float> unit(-1,1);
 for(unsigned i=0;i<20000;++i){ssx::terrain_original::Rounding rounding;ssx::OriginalCollisionContext state;state.motionMode=motion=i%5;state.controlState=control=(i/5)%14;state.reverseStance=i&1;state.manualSpin=i%3?unit(random)*9:0;state.physical.right={1,0,0};state.physical.forward={0,1,0};state.velocityCmps={unit(random)*3000,unit(random)*3000,unit(random)*3000};ssx::OriginalCollisionEvent event;event.normal={unit(random),unit(random),unit(random)};event.closingSpeedCmps=std::abs(unit(random))*2000;
  vec(actor+0x1a0,state.physical.right);vec(actor+0x1b0,state.physical.forward);vec(actor+0x1e0,state.velocityCmps);put(actor+0x2dc,state.manualSpin);put(actor+0x320,uint32_t(state.reverseStance));vec(eventAt+32,event.normal);put(eventAt+48,event.closingSpeedCmps);
  selected=nextControl=-1;cancel=strong=false;draws=0;randomValue=random();auto c=context(0x108388);SET_GPR_U32((&c),4,actor);SET_GPR_U32((&c),5,eventAt);SET_GPR_U32((&c),6,1);sub_00108388_0x108388(ram.data(),&c,&runtime);
  unsigned count=0;auto result=ssx::originalSoftCollisionReaction({},state,event,[&](){return randomValue+count++*1234567u;});
  if(c.pc!=done||result.animation!=selected||result.nextControlState!=nextControl||result.cancelControlOne!=cancel||result.strongSoftImpact!=strong||result.randomDraws!=draws||result.manualSpin!=getFloat(actor+0x2dc)){printf("soft entry mismatch%u\n",i);return 4;}
 }
 puts("20,000 standalone original108388 selections/spin/control requests exact");
 for(unsigned i=0;i<20000;++i){ssx::terrain_original::Rounding rounding;ssx::OriginalGroundProfile profile;profile.surface.id=i%19;ssx::OriginalGroundState state;state.modeTiming=i%17?-1:1;state.velocity={unit(random)*2500,unit(random)*2500,unit(random)*2500};state.forward={unit(random),unit(random),unit(random)};state.turn={unit(random),std::abs(unit(random)),unit(random)};state.brake={unit(random),std::abs(unit(random)),unit(random)};state.crouch={std::abs(unit(random)),std::abs(unit(random)),unit(random)};state.animationTurn={unit(random),std::abs(unit(random)),unit(random)};
  motion=i%5;ended=i&1;obstructed=i%19==0;uint32_t word=random()&0xffffe000u;if(i%23==0)word|=0x1000;
  put(actor+0x470,state.modeTiming);vec(actor+0x1e0,state.velocity);vec(actor+0x3a0,state.forward);put(actor+0x438,profile.surface.id);put(actor+0x1f0,state.turn);put(actor+0x214,state.brake);put(actor+0x220,state.crouch);put(actor+0x1fc,state.animationTurn);put(packet,word);put(packet+4,0u);
  nextControl=nextMotion=-1;boostCalled=bHeld=bPressed=balanceCalled=restore=rail=false;balance=0;
  auto c=context(0x12e778);SET_GPR_U32((&c),4,controller);SET_GPR_U32((&c),5,packet);sub_0012E778_0x12e778(ram.data(),&c,&runtime);
  auto result=ssx::originalSoftControlStep(profile,state,{motion,ended,obstructed},ssx::originalSoftControlInput(word));
  bool targets=state.turn.rate==getFloat(actor+0x1f4)&&state.turn.target==getFloat(actor+0x1f8)&&state.crouch.rate==getFloat(actor+0x224)&&state.crouch.target==getFloat(actor+0x228)&&state.brake.rate==getFloat(actor+0x218)&&state.brake.target==getFloat(actor+0x21c)&&state.animationTurn.rate==getFloat(actor+0x200)&&state.animationTurn.target==getFloat(actor+0x204);
  if(c.pc!=done||!targets||result.nextControl!=nextControl||result.nextMotion!=nextMotion||result.requestBoost!=boostCalled||result.boostHeld!=bHeld||result.boostPressed!=bPressed||result.requestBalance!=balanceCalled||(balanceCalled&&result.balance!=balance)||result.restoreStance!=restore||result.resetRail!=rail){printf("soft update mismatch%u mode%d targets%d ctrl%d/%d boost%d/%d balance%.9g/%.9g\n",i,motion,targets,result.nextControl,nextControl,result.requestBoost,boostCalled,result.balance,balance);return 5;}
 }
 puts("20,000 original control3 target/recovery/continuation cases exact (external106848/stance/rail hooks explicit)");
}
