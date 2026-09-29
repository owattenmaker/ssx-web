#include "ps2_runtime_macros.h"
#include "../engine/npc_input.hpp"
#include <fstream>
#include <random>
#include <cfenv>
#include <cstdio>
#include <bit>
void sub_0010C0A8_0x10c0a8(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0010C140_0x10c140(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00100348_0x100348(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00100610_0x100610(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0031C228_0x31c228(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0010AD78_0x10ad78(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0010AED8_0x10aed8(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0010AA70_0x10aa70(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0026AA80_0x26aa80(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0026AB20_0x26ab20(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0010BFA8_0x10bfa8(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0010C1D0_0x10c1d0(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0010DEF0_0x10def0(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0010DEB0_0x10deb0(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0010D8F8_0x10d8f8(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00113128_0x113128(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00120090_0x120090(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0010BD10_0x10bd10(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0010D9E8_0x10d9e8(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0031C040_0x31c040(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00100680_0x100680(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0010BB18_0x10bb18(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0010B980_0x10b980(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0010BBF8_0x10bbf8(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00112A50_0x112a50(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0026AFB8_0x26afb8(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0026A9B0_0x26a9b0(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0026A428_0x26a428(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0026A638_0x26a638(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0026AC48_0x26ac48(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0010D1A0_0x10d1a0(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0010D870_0x10d870(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00112588_0x112588(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0010FC30_0x10fc30(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0011FE98_0x11fe98(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0011FEE8_0x11fee8(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0010DA10_0x10da10(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0010DBF0_0x10dbf0(uint8_t*,R5900Context*,PS2Runtime*);
static std::array<int,6> relationshipValues;
static int32_t sharedTick;
static std::array<uint32_t,3> drawWords;static unsigned drawIndex;
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x420000,g_ps2RecompiledFunctionTableSlotCount=0xc8000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xc8000]={};
int main(int argc,char**argv){
 PS2Runtime rt;rt.registerFunction(0x155a50,[](uint8_t*,R5900Context*c,PS2Runtime*){SET_GPR_U32(c,2,0xA0000);c->pc=GPR_U32(c,31);});rt.registerFunction(0x155b50,[](uint8_t*,R5900Context*c,PS2Runtime*){SET_GPR_S32(c,2,relationshipValues[GPR_U32(c,6)]);c->pc=GPR_U32(c,31);});rt.registerFunction(0x140b80,[](uint8_t*m,R5900Context*c,PS2Runtime*){uint32_t slot;std::memcpy(&slot,m+GPR_U32(c,4)+0x86c,4);SET_GPR_U32(c,2,slot);c->pc=GPR_U32(c,31);});rt.registerFunction(0x311ae8,[](uint8_t*m,R5900Context*c,PS2Runtime*){int value;std::memcpy(&value,m+GPR_U32(c,4),4);SET_GPR_S32(c,2,value);c->pc=GPR_U32(c,31);});rt.registerFunction(0x11fe98,sub_0011FE98_0x11fe98);rt.registerFunction(0x11fee8,sub_0011FEE8_0x11fee8);
 rt.registerFunction(0x112a50,sub_00112A50_0x112a50);rt.registerFunction(0x26afb8,sub_0026AFB8_0x26afb8);rt.registerFunction(0x26a9b0,sub_0026A9B0_0x26a9b0);rt.registerFunction(0x26a428,sub_0026A428_0x26a428);rt.registerFunction(0x26a638,sub_0026A638_0x26a638);rt.registerFunction(0x26ac48,sub_0026AC48_0x26ac48);rt.registerFunction(0x10d410,sub_0010D1A0_0x10d1a0);rt.registerFunction(0x10d870,sub_0010D870_0x10d870);rt.registerFunction(0x112588,sub_00112588_0x112588);rt.registerFunction(0x10fc30,sub_0010FC30_0x10fc30);
 rt.registerFunction(0x140bc8,[](uint8_t*m,R5900Context*c,PS2Runtime*){uint32_t flag;std::memcpy(&flag,m+GPR_U32(c,4)+0x874,4);SET_GPR_U32(c,2,flag^1);c->pc=GPR_U32(c,31);});rt.registerFunction(0x100680,sub_00100680_0x100680);rt.registerFunction(0x10bfa8,sub_0010BFA8_0x10bfa8);rt.registerFunction(0x100610,sub_00100610_0x100610);rt.registerFunction(0x10c0a8,sub_0010C0A8_0x10c0a8);rt.registerFunction(0x10c140,sub_0010C140_0x10c140);rt.registerFunction(0x10bd10,sub_0010BD10_0x10bd10);rt.registerFunction(0x10bb18,sub_0010BB18_0x10bb18);rt.registerFunction(0x10b980,sub_0010B980_0x10b980);rt.registerFunction(0x10bbf8,sub_0010BBF8_0x10bbf8);
 rt.registerFunction(0x140bc0,[](uint8_t*m,R5900Context*c,PS2Runtime*){uint32_t flag;std::memcpy(&flag,m+GPR_U32(c,4)+0x874,4);SET_GPR_U32(c,2,flag);c->pc=GPR_U32(c,31);});rt.registerFunction(0x10d9e8,sub_0010D9E8_0x10d9e8);rt.registerFunction(0x31c040,sub_0031C040_0x31c040);rt.registerFunction(0x1298c8,[](uint8_t*,R5900Context*c,PS2Runtime*){SET_GPR_S32(c,2,sharedTick);c->pc=GPR_U32(c,31);});rt.registerFunction(0x10deb0,sub_0010DEB0_0x10deb0);rt.registerFunction(0x10d8f8,sub_0010D8F8_0x10d8f8);rt.registerFunction(0x113128,sub_00113128_0x113128);
 for(unsigned a:{0x14dc80u,0x14dd58u})rt.registerFunction(a,[](uint8_t*,R5900Context*c,PS2Runtime*){SET_GPR_U32(c,2,0);c->pc=GPR_U32(c,31);});rt.registerFunction(0x10c1d0,sub_0010C1D0_0x10c1d0);rt.registerFunction(0x317810,[](uint8_t*,R5900Context*c,PS2Runtime*){if(drawIndex>=3)throw std::runtime_error("Unexpected original NPC RNG draw");SET_GPR_U32(c,2,drawWords[drawIndex++]);c->pc=GPR_U32(c,31);});rt.registerFunction(0x26aa80,sub_0026AA80_0x26aa80);rt.registerFunction(0x26ab20,sub_0026AB20_0x26ab20);rt.registerFunction(0x100348,sub_00100348_0x100348);rt.registerFunction(0x31c228,sub_0031C228_0x31c228);
 rt.registerFunction(0x140910,[](uint8_t*,R5900Context*c,PS2Runtime*){SET_GPR_U32(c,2,0x301e0);c->pc=GPR_U32(c,31);});
 rt.registerFunction(0x1408f0,[](uint8_t*,R5900Context*c,PS2Runtime*){SET_GPR_U32(c,2,0x30110);c->pc=GPR_U32(c,31);});std::ifstream file(argv[1],std::ios::binary);std::vector<uint8_t> m((std::istreambuf_iterator<char>(file)),{});if(m.size()!=32*1024*1024)return 2;
 auto write=[&](unsigned at,auto value){std::memcpy(m.data()+at,&value,sizeof(value));};auto read=[&](unsigned at){uint32_t value;std::memcpy(&value,m.data()+at,4);return value;};
 // Several original behavior entries share one generated compilation unit.
 // Its same-unit guest calls yield to the runtime; resume their real PC rather
 // than treating an intermediate return address as the final function return.
 auto runBehavior=[&](R5900Context& c){for(unsigned resume=0;resume<32;++resume){sub_00100680_0x100680(m.data(),&c,&rt);if(c.pc==0x12345678)return;if(c.pc<0x100680||c.pc>=0x101310)throw std::runtime_error("Unexpected NPC behavior continuation");}throw std::runtime_error("NPC behavior continuation limit");};
 std::mt19937 rng(0x10c0a8);std::uniform_real_distribution<float> value(-3000,3000);std::fesetround(FE_TOWARDZERO);
 for(unsigned k=0;k<20000;k++){
  std::array<float,3> velocity{value(rng),value(rng),value(rng)};if(k<30){velocity={833.3334350585938f,0,0};for(unsigned n=0;n<k/2;n++)velocity[0]=std::nextafter(velocity[0],k%2?INFINITY:-INFINITY);}
  float field=k%3?float(k%101):std::nextafter(20.f,k%2?INFINITY:-INFINITY);uint32_t original=rng();
  for(unsigned boost=0;boost<2;++boost){
   write(0x20018,0x30000u);write(0x301e0,velocity);write(0x301ec,0.f);write(0x20df8,field);write(0x40000,original);
   R5900Context c{};c.vu0_vf[0]=_mm_set_ps(1,0,0,0);SET_GPR_U32(&c,4,0x20000);SET_GPR_U32(&c,5,0x40000);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,31,0x12345678);
   if(boost)sub_0010C140_0x10c140(m.data(),&c,&rt);else sub_0010C0A8_0x10c0a8(m.data(),&c,&rt);
   uint32_t actual=boost?ssx::originalNpcBoostWord(original,velocity):ssx::originalNpcCrouchWord(original,velocity,field);
   if(c.pc!=0x12345678||actual!=read(0x40000)){printf("NPC command mismatch%u/%u expected%08x actual%08x\n",k,boost,read(0x40000),actual);return 3;}
  }
 }
 puts("20000 original NPC crouch words and20000 boost words match exactly, including preserved unrelated bits");
 for(unsigned k=0;k<20000;++k){
  using V=std::array<float,3>;V position{value(rng)*30,value(rng)*30,value(rng)*30},closest=position,previous=position,velocity{value(rng),value(rng),value(rng)},up{value(rng)/3000,value(rng)/3000,value(rng)/3000};
  for(unsigned i=0;i<3;++i){closest[i]+=value(rng);previous[i]+=value(rng);}if(k%17==0)closest=previous=position;if(k%19==0)velocity={};
  write(0x20018,0x30000u);write(0x30110,position);write(0x3011c,1.f);write(0x301e0,velocity);write(0x301ec,0.f);write(0x301c0,up);write(0x301cc,0.f);write(0x30490,closest);write(0x3049c,1.f);write(0x304b0,previous);write(0x304bc,1.f);
  R5900Context c{};c.vu0_vf[0]=_mm_set_ps(1,0,0,0);SET_GPR_U32(&c,4,0x40000);SET_GPR_U32(&c,5,0x20000);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,29,0x10000);SET_GPR_U32(&c,31,0x12345678);sub_00100610_0x100610(m.data(),&c,&rt);
  auto target=ssx::originalNpcSteeringTarget(position,closest,previous);for(unsigned i=0;i<3;++i)if(std::bit_cast<uint32_t>(target[i])!=read(0x40000+i*4)){printf("NPC target mismatch%u/%u\n",k,i);return 4;}
  write(0x306c0,0x50000u);write(0x50010,int16_t(0));write(0x50014,0x140910u);write(0x50028,int16_t(0));write(0x5002c,0x1408f0u);c={};c.vu0_vf[0]=_mm_set_ps(1,0,0,0);SET_GPR_U32(&c,4,0x20000);SET_GPR_U32(&c,5,0x40000);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,29,0x10000);SET_GPR_U32(&c,31,0x12345678);sub_00100348_0x100348(m.data(),&c,&rt);
  float actual=ssx::originalNpcSteering(position,velocity,up,target);if(c.pc!=0x12345678||actual!=c.f[0]){printf("NPC steering mismatch%u expected%.9g actual%.9g\n",k,c.f[0],actual);return 5;}
 }
 puts("20000 original NPC steering targets andsteering responses match exactly");

 for(unsigned k=0;k<20000;++k){
  ssx::OriginalNpcPath path;path.geometry.origin={value(rng),value(rng),value(rng)};path.geometry.segments={{1,0,0,3000},{0,1,-.1f,3000}};
  for(unsigned i=0;i<15;++i){float start=value(rng)+3000;path.geometry.events.push_back({rng()%5==0?16u:17u,0,start,start+std::abs(value(rng))});}
  ssx::OriginalNpcPrewindInput input;input.position=path.geometry.origin;input.closestRoutePoint=input.position;input.velocity={value(rng),value(rng),value(rng)};input.boardUp={0,0,1};input.previousDistance=value(rng);input.currentDistance=input.previousDistance+std::abs(value(rng));input.desiredSpeedDF0=value(rng)+3000;input.regionModeE1C=k%5==0;input.regionStartE50=path.geometry.origin;input.regionEndE60=input.regionStartE50;input.regionEndE60[0]+=1000;
  for(auto&v:input.position)v+=value(rng);input.plannedSpinE38=float(int(rng()%3)-1);input.plannedFlipE3C=float(int(rng()%3)-1);std::array<uint32_t,2> initial{rng(),rng()};
  write(0x20018,0x30000u);write(0x20df0,input.desiredSpeedDF0);write(0x20e1c,int(input.regionModeE1C));write(0x20e50,input.regionStartE50);write(0x20e5c,1.f);write(0x20e60,input.regionEndE60);write(0x20e6c,1.f);write(0x20e38,input.plannedSpinE38);write(0x20e3c,input.plannedFlipE3C);write(0x30110,input.position);write(0x3011c,1.f);write(0x301e0,input.velocity);write(0x301ec,0.f);write(0x301c0,input.boardUp);write(0x301cc,0.f);write(0x30490,input.closestRoutePoint);write(0x3049c,1.f);write(0x304c0,input.previousDistance);write(0x304c4,input.currentDistance);write(0x30ab8,0x60000u);
  write(0x60000,unsigned(path.geometry.events.size()));write(0x60004,0x61000u);write(0x60008,2u);write(0x6000c,path.geometry.origin);write(0x60018,0x62000u);write(0x60034,0x63000u);write(0x63008,int16_t(0));write(0x6300c,0x26aa80u);for(unsigned i=0;i<15;++i)write(0x61000+i*16,path.geometry.events[i]);for(unsigned i=0;i<2;++i)write(0x62000+i*16,path.geometry.segments[i]);write(0x40000,initial);
  R5900Context c{};c.vu0_vf[0]=_mm_set_ps(1,0,0,0);SET_GPR_U32(&c,4,0x20000);SET_GPR_U32(&c,5,0x40000);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,29,0x10000);SET_GPR_U32(&c,31,0x12345678);sub_0010AA70_0x10aa70(m.data(),&c,&rt);
  auto actual=ssx::originalNpcPrewindCommand(initial,path,input);if(c.pc!=0x12345678||actual[0]!=read(0x40000)||actual[1]!=read(0x40004)){printf("NPC prewind mismatch%u expected%08x,%08x actual%08x,%08x\n",k,read(0x40000),read(0x40004),actual[0],actual[1]);return 6;}
 }
 puts("20000 complete original NPC control2 producers match exactly (events/region/steering/spin/flip/boost)");

 for(unsigned k=0;k<20000;++k){
  ssx::OriginalNpcTrickPlan plan;plan.indexE0C=k;plan.enabledE10=k%4;plan.decisionE14=k%5;plan.phaseE18=k;plan.kindE1C=k;plan.fieldE20=k;plan.spinE38=value(rng);plan.flipE3C=value(rng);
  bool spin=k%2,flip=k%3;int32_t field=k%8,kind=k%4;float parameter=float(k%150)/100.f;
  write(0x20e0c,plan.indexE0C);write(0x20e10,plan.enabledE10);write(0x20e14,plan.decisionE14);write(0x20e18,plan.phaseE18);write(0x20e1c,plan.kindE1C);write(0x20e20,plan.fieldE20);write(0x20e38,plan.spinE38);write(0x20e3c,plan.flipE3C);write(0x20dfc,parameter);
  for(auto&v:drawWords)v=rng();drawIndex=0;
  R5900Context c{};SET_GPR_U32(&c,4,0x20000);SET_GPR_U32(&c,5,spin);SET_GPR_U32(&c,6,flip);SET_GPR_U32(&c,7,field);SET_GPR_U32(&c,8,kind);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,29,0x10000);SET_GPR_U32(&c,31,0x12345678);sub_0010BFA8_0x10bfa8(m.data(),&c,&rt);
  unsigned nativeIndex=0;ssx::originalNpcPrepareTrick(plan,spin,flip,field,kind,parameter,[&](){return drawWords[nativeIndex++];});
  if(c.pc!=0x12345678||nativeIndex!=drawIndex||read(0x20e0c)!=uint32_t(plan.indexE0C)||read(0x20e10)!=uint32_t(plan.enabledE10)||read(0x20e14)!=uint32_t(plan.decisionE14)||read(0x20e18)!=uint32_t(plan.phaseE18)||read(0x20e1c)!=uint32_t(plan.kindE1C)||read(0x20e20)!=uint32_t(plan.fieldE20)||read(0x20e38)!=std::bit_cast<uint32_t>(plan.spinE38)||read(0x20e3c)!=std::bit_cast<uint32_t>(plan.flipE3C)){printf("NPC trick plan mismatch%u RNG%u/%u\n",k,drawIndex,nativeIndex);return 7;}
 }
 puts("20000 original NPC trick plans andRNG draw sequences match bit-exactly");

 for(unsigned k=0;k<20000;++k){
  ssx::OriginalNpcPacingContext input;input.remaining=value(rng)*100+300000;if(k%4)input.referenceRemaining=value(rng)*100+300000;input.negativeThresholdDC=-100-std::abs(value(rng)*20);input.positiveThresholdE0=100+std::abs(value(rng)*20);input.modeE4=k%5;input.eventVariant=k%3;float current=.1f+std::abs(value(rng)/3000);
  for(unsigned i=0;i<6*0x24;i+=4)write(0x30000+i,0u);if(input.referenceRemaining){write(0x30000,1u);write(0x30004,1u);write(0x704d0,*input.referenceRemaining);}
  write(0x20018,0x30000u);write(0x304d0,input.remaining);write(0x300dc,input.negativeThresholdDC);write(0x300e0,input.positiveThresholdE0);write(0x300e4,input.modeE4);write(0x535c11,input.eventVariant);write(0x4a30f0-0x848,0x60000u);write(0x60084,0x61000u);write(0x6100c,0x62000u);write(0x62028,0x70000u);
  R5900Context c{};SET_GPR_U32(&c,4,0x20000);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,29,0x10000);SET_GPR_U32(&c,31,0x12345678);sub_0010DEF0_0x10def0(m.data(),&c,&rt);float target=ssx::originalNpcTimeScaleTarget(input);
  if(c.pc!=0x12345678||target!=c.f[0]){printf("NPC pacing mismatch%u expected%.9g actual%.9g\n",k,c.f[0],target);return 8;}
  write(0x30300,current);c={};c.f[12]=target;SET_GPR_U32(&c,4,0x30000);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,31,0x12345678);sub_00120090_0x120090(m.data(),&c,&rt);float actual=ssx::originalNpcTimeScaleApproach(current,target);if(read(0x30300)!=std::bit_cast<uint32_t>(actual)){printf("NPC timeScaleapproach mismatch%u\n",k);return 9;}
 }
 puts("20000 original NPC pacing targets andtimeScale approaches match exactly");

 for(unsigned k=0;k<20000;++k){
  std::array<ssx::OriginalPairRecord,6> records;for(auto&r:records){r.enabled=rng()%2;r.planarDistanceCm=std::abs(value(rng))*.5f;r.bearing=value(rng)*.001f;}
  int32_t target=int(rng()%7)-1,markedResult=int(rng()%3);float lateral=k%23==0?540.f:k%29==0?600.f:std::abs(value(rng))*.3f;std::array<float,3>forward{value(rng)/3000,value(rng)/3000,0};if(k%31==0)forward[0]=0;
  std::optional<int> marked;if(k%2)marked=rng()%6;sharedTick=k%3?int32_t(k):120;
  write(0x20018,0x30000u);write(0x20e70,target);write(0x40000,markedResult);write(0x304c8,lateral);write(0x301b0,forward);write(0x301bc,0.f);write(0x300f0,int(marked.has_value()));write(0x300f8,marked.value_or(0));
  for(unsigned i=0;i<6;++i){write(0x30000+i*0x24,uint32_t(records[i].enabled));write(0x30008+i*0x24,records[i].planarDistanceCm);write(0x3000c+i*0x24,records[i].bearing);}
  R5900Context c{};c.vu0_vf[0]=_mm_set_ps(1,0,0,0);SET_GPR_U32(&c,4,0x20000);SET_GPR_U32(&c,5,0x40000);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,29,0x10000);SET_GPR_U32(&c,31,0x12345678);sub_0010BD10_0x10bd10(m.data(),&c,&rt);
  bool result=ssx::originalNpcSelectPeer(target,markedResult,lateral,forward,records,marked,sharedTick);auto*cp=&c;
  if(c.pc!=0x12345678||read(0x20e70)!=uint32_t(target)||read(0x40000)!=uint32_t(markedResult)||GPR_U32(cp,2)!=unsigned(result)){printf("NPC peer selector mismatch%u target%d/%d marked%d/%d\n",k,int(read(0x20e70)),target,int(read(0x40000)),markedResult);return 10;}
 }
 puts("20000 complete original NPC peer selections match exactly, including retained target/output andcadence");

 const uint32_t behaviorAddresses[]={0x100680,0x1009e0,0x100b90,0x100f88};
 unsigned railCases=0,railLaunches=0;
 for(unsigned k=0;k<5000;++k){
  ssx::OriginalNpcPath path;path.geometry.origin={value(rng),value(rng),value(rng)};path.geometry.segments={{1,0,0,3000},{0,1,-.1f,3000}};
  for(unsigned i=0;i<15;++i){float start=value(rng)+3000;path.geometry.events.push_back({uint32_t(14+rng()%7),uint32_t(rng()%1900),start,start+std::abs(value(rng))});}
  ssx::OriginalNpcDrivingState state;state.desiredSpeedDF0=value(rng);state.parameterDF8=float(k%101);state.boardTimerE40=int(k%65)-2;state.targetPeerE70=int(rng()%7)-1;state.behaviorCounterF38=k%10;state.behavior=ssx::OriginalNpcBehavior(k%4);state.regionStartE50={1,2,3};state.regionEndE60={4,5,6};
  ssx::OriginalNpcDrivingContext input;input.position=path.geometry.origin;input.velocity={value(rng),value(rng),value(rng)};input.physicalForward={value(rng)/3000,value(rng)/3000,0};input.route.closestPoint=input.position;input.route.previousLookaheadPoint=input.position;for(unsigned i=0;i<3;++i){input.position[i]+=value(rng);input.route.previousLookaheadPoint[i]+=value(rng);}input.route.previousDistance=value(rng)+3000;input.route.currentDistance=input.route.previousDistance+std::abs(value(rng));input.route.lateralDistance=std::abs(value(rng)*.2f);input.boostMeter=float(k%110)/100;input.superTime=k%4?10.f:0.f;input.tick=sharedTick=k%3?int(k):120;
  if(k%2)input.designatedPeer=rng()%6;for(unsigned i=0;i<6;++i){input.peers[i].enabled=rng()%2;input.peers[i].planarDistanceCm=std::abs(value(rng)*.5f);input.peers[i].bearing=value(rng)*.001f;input.peerHuman[i]=rng()%2;write(0x30000+i*0x24,uint32_t(input.peers[i].enabled));write(0x30008+i*0x24,input.peers[i].planarDistanceCm);write(0x3000c+i*0x24,input.peers[i].bearing);write(0x92028+i*4,0x70000u+i*0x1000);write(0x706c0+i*0x1000,0x54000u);write(0x70874+i*0x1000,uint32_t(input.peerHuman[i]));}
  write(0x54040,int16_t(-0x6c0));write(0x54044,0x140bc0u);write(0x4a30f0-0x848,0x90000u);write(0x90084,0x91000u);write(0x9100c,0x92000u);
  std::array<uint32_t,2> initial{rng(),rng()};write(0x40000,initial);write(0x20018,0x30000u);write(0x20df0,state.desiredSpeedDF0);write(0x20df8,state.parameterDF8);write(0x20e40,state.boardTimerE40);write(0x20e70,state.targetPeerE70);write(0x20f38,state.behaviorCounterF38);write(0x20f44,0xffff0000u);write(0x20f48,behaviorAddresses[k%4]);write(0x20e50,state.regionStartE50);write(0x20e60,state.regionEndE60);
  write(0x30110,input.position);write(0x3011c,1.f);write(0x301e0,input.velocity);write(0x301ec,0.f);write(0x301c0,input.boardUp);write(0x301cc,0.f);write(0x301b0,input.physicalForward);write(0x301bc,0.f);write(0x30490,input.route.closestPoint);write(0x3049c,1.f);write(0x304b0,input.route.previousLookaheadPoint);write(0x304bc,1.f);write(0x304c0,input.route.previousDistance);write(0x304c4,input.route.currentDistance);write(0x304c8,input.route.lateralDistance);write(0x302f8,input.boostMeter);write(0x302f0,input.superTime);write(0x300f0,int(input.designatedPeer.has_value()));write(0x300f8,input.designatedPeer.value_or(0));write(0x30ab8,0x60000u);write(0x306c0,0x50000u);write(0x50010,int16_t(0));write(0x50014,0x140910u);write(0x50028,int16_t(0));write(0x5002c,0x1408f0u);
  write(0x60000,unsigned(path.geometry.events.size()));write(0x60004,0x61000u);write(0x60008,2u);write(0x6000c,path.geometry.origin);write(0x60018,0x62000u);write(0x60034,0x63000u);write(0x63008,int16_t(0));write(0x6300c,0x26aa80u);for(unsigned i=0;i<15;++i)write(0x61000+i*16,path.geometry.events[i]);for(unsigned i=0;i<2;++i)write(0x62000+i*16,path.geometry.segments[i]);
  R5900Context c{};c.vu0_vf[0]=_mm_set_ps(1,0,0,0);SET_GPR_U32(&c,4,0x20000);SET_GPR_U32(&c,5,0x40000);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,29,0x10000);SET_GPR_U32(&c,31,0x12345678);runBehavior(c);
  auto actual=ssx::originalNpcCruiseBehavior(initial,state,input,path);int16_t counter;std::memcpy(&counter,m.data()+0x20f38,2);
  if(c.pc!=0x12345678||actual[0]!=read(0x40000)||actual[1]!=read(0x40004)||read(0x20df0)!=std::bit_cast<uint32_t>(state.desiredSpeedDF0)||read(0x20e40)!=uint32_t(state.boardTimerE40)||read(0x20e70)!=uint32_t(state.targetPeerE70)||counter!=state.behaviorCounterF38||read(0x20f48)!=behaviorAddresses[int(state.behavior)]){printf("NPC cruise mismatch%u words%08x,%08x/%08x,%08x target%d/%d behavior%x/%x\n",k,read(0x40000),read(0x40004),actual[0],actual[1],int(read(0x20e70)),state.targetPeerE70,read(0x20f48),behaviorAddresses[int(state.behavior)]);return 11;}
  for(unsigned i=0;i<3;++i)if(read(0x20e50+i*4)!=std::bit_cast<uint32_t>(state.regionStartE50[i])||read(0x20e60+i*4)!=std::bit_cast<uint32_t>(state.regionEndE60[i]))return 12;
  state.behavior=ssx::OriginalNpcBehavior::Jump1009E0;state.parameterDFC=float(k%100)/100.f;write(0x20f48,0x1009e0u);write(0x20dfc,state.parameterDFC);
  write(0x20e0c,state.trick.indexE0C);write(0x20e10,state.trick.enabledE10);write(0x20e14,state.trick.decisionE14);write(0x20e18,state.trick.phaseE18);write(0x20e1c,state.trick.kindE1C);write(0x20e20,state.trick.fieldE20);write(0x20e38,state.trick.spinE38);write(0x20e3c,state.trick.flipE3C);write(0x40000,initial);
  for(auto&v:drawWords)v=rng();drawIndex=0;c={};c.pc=0x1009e0;c.vu0_vf[0]=_mm_set_ps(1,0,0,0);SET_GPR_U32(&c,4,0x20000);SET_GPR_U32(&c,5,0x40000);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,29,0x10000);SET_GPR_U32(&c,31,0x12345678);runBehavior(c);
  unsigned nativeIndex=0;actual=ssx::originalNpcJumpBehavior(initial,state,input,path,[&](){return drawWords[nativeIndex++];});std::memcpy(&counter,m.data()+0x20f38,2);
  if(c.pc!=0x12345678||actual[0]!=read(0x40000)||actual[1]!=read(0x40004)||drawIndex!=nativeIndex||read(0x20df0)!=std::bit_cast<uint32_t>(state.desiredSpeedDF0)||read(0x20e40)!=uint32_t(state.boardTimerE40)||read(0x20e70)!=uint32_t(state.targetPeerE70)||counter!=state.behaviorCounterF38||read(0x20f48)!=behaviorAddresses[int(state.behavior)]){printf("NPC jump behavior mismatch%u words%08x,%08x/%08x,%08x RNG%u/%u DF0%x/%x timer%d/%d peer%d/%d counter%d/%d behavior%x/%x pc%x\n",k,read(0x40000),read(0x40004),actual[0],actual[1],drawIndex,nativeIndex,read(0x20df0),std::bit_cast<uint32_t>(state.desiredSpeedDF0),int(read(0x20e40)),state.boardTimerE40,int(read(0x20e70)),state.targetPeerE70,counter,state.behaviorCounterF38,read(0x20f48),behaviorAddresses[int(state.behavior)],c.pc);return 13;}
  const auto&p=state.trick;
  if(read(0x20e0c)!=uint32_t(p.indexE0C)||read(0x20e10)!=uint32_t(p.enabledE10)||read(0x20e14)!=uint32_t(p.decisionE14)||read(0x20e18)!=uint32_t(p.phaseE18)||read(0x20e1c)!=uint32_t(p.kindE1C)||read(0x20e20)!=uint32_t(p.fieldE20)||read(0x20e38)!=std::bit_cast<uint32_t>(p.spinE38)||read(0x20e3c)!=std::bit_cast<uint32_t>(p.flipE3C)){printf("NPC jump plan mismatch%u\n",k);return 14;}

  if(k%3==0){path.geometry.events[0]={19,uint32_t(k%2),input.route.currentDistance-10,input.route.currentDistance+50};write(0x61000,path.geometry.events[0]);}
  input.physicalRightZ=value(rng)/3000;write(0x301a8,input.physicalRightZ);write(0x40000,initial);c={};c.pc=0x10ad78;c.vu0_vf[0]=_mm_set_ps(1,0,0,0);SET_GPR_U32(&c,4,0x20000);SET_GPR_U32(&c,5,0x40000);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,29,0x10000);SET_GPR_U32(&c,31,0x12345678);sub_0010AD78_0x10ad78(m.data(),&c,&rt);actual=ssx::originalNpcManualCommand(initial,input,path);if(actual[0]!=read(0x40000)||actual[1]!=read(0x40004)){printf("NPC manualcommand mismatch%u\n",k);return 20;}
  { // 10AED8 control7 (rail): jump-zone launch/trick plan, balance, RailSpin, speed-zone boost.
   const int style=int(rng()%5),cls=rng()%3?7:14;write(0x30328,style);write(0x30784,0x7f000u);write(0x7f000,cls);
   input.prewindStyle=style;input.mainAnimationClass=cls;
   if(k%4==0)state.trick.spinE38=float(int(rng()%3)-1);
   write(0x20df0,state.desiredSpeedDF0);write(0x20dfc,state.parameterDFC);write(0x20e50,state.regionStartE50);write(0x20e60,state.regionEndE60);
   write(0x20e0c,state.trick.indexE0C);write(0x20e10,state.trick.enabledE10);write(0x20e14,state.trick.decisionE14);write(0x20e18,state.trick.phaseE18);write(0x20e1c,state.trick.kindE1C);write(0x20e20,state.trick.fieldE20);write(0x20e38,state.trick.spinE38);write(0x20e3c,state.trick.flipE3C);
   write(0x10000-0x70+0x18,0u); // unwritten 10B980 output on a miss: 0 in every captured rail sequence
   write(0x40000,initial);for(auto&v:drawWords)v=rng();drawIndex=0;
   c={};c.pc=0x10aed8;c.vu0_vf[0]=_mm_set_ps(1,0,0,0);SET_GPR_U32(&c,4,0x20000);SET_GPR_U32(&c,5,0x40000);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,29,0x10000);SET_GPR_U32(&c,31,0x12345678);
   for(unsigned resume=0;;++resume){sub_0010AED8_0x10aed8(m.data(),&c,&rt);if(c.pc==0x12345678)break;if(resume>16||c.pc<0x10aed8||c.pc>=0x10b0e8)throw std::runtime_error("Unexpected NPC rail producer continuation");}
   unsigned nativeIndex=0;actual=ssx::originalNpcRailCommand(initial,state,input,path,[&](){return drawWords[nativeIndex++];});
   const auto&t=state.trick;bool same=actual[0]==read(0x40000)&&actual[1]==read(0x40004)&&drawIndex==nativeIndex&&read(0x20df0)==std::bit_cast<uint32_t>(state.desiredSpeedDF0);
   for(unsigned i=0;i<3;++i)same&=read(0x20e50+i*4)==std::bit_cast<uint32_t>(state.regionStartE50[i])&&read(0x20e60+i*4)==std::bit_cast<uint32_t>(state.regionEndE60[i]);
   same&=read(0x20e0c)==uint32_t(t.indexE0C)&&read(0x20e10)==uint32_t(t.enabledE10)&&read(0x20e14)==uint32_t(t.decisionE14)&&read(0x20e18)==uint32_t(t.phaseE18)&&read(0x20e1c)==uint32_t(t.kindE1C)&&read(0x20e20)==uint32_t(t.fieldE20)&&read(0x20e38)==std::bit_cast<uint32_t>(t.spinE38)&&read(0x20e3c)==std::bit_cast<uint32_t>(t.flipE3C);
   if(!same){printf("NPC rail producer mismatch%u words%08x,%08x/%08x,%08x RNG%u/%u E20 %x/%x DF0 %x/%x\n",k,read(0x40000),read(0x40004),actual[0],actual[1],drawIndex,nativeIndex,read(0x20e20),uint32_t(t.fieldE20),read(0x20df0),std::bit_cast<uint32_t>(state.desiredSpeedDF0));return 21;}
   ++railCases;railLaunches+=(actual[0]&0x6000u)==0x6000u;
  }

 }
 puts("5000 complete original NPC manual commands,cruise AND jump-zone behaviors match commands/state/RNG exactly");
 printf("%u complete original NPC control7 (rail, 10AED8) producers match commands/state/RNG exactly (%u jump-zone launches)\n",railCases,railLaunches);

 rt.registerFunction(0x140910,[](uint8_t*,R5900Context*c,PS2Runtime*){SET_GPR_U32(c,2,GPR_U32(c,4)+0x1e0);c->pc=GPR_U32(c,31);});rt.registerFunction(0x1408f0,[](uint8_t*,R5900Context*c,PS2Runtime*){SET_GPR_U32(c,2,GPR_U32(c,4)+0x110);c->pc=GPR_U32(c,31);});
 unsigned routeSwitches=0;
 for(unsigned k=0;k<1000;++k){
  constexpr unsigned actor=0x30000,owner=actor-0xf50;
  std::array<ssx::OriginalNpcPath,2> paths;for(unsigned i=0;i<2;++i){auto&p=paths[i];p.geometry.origin={0.f,float(i)*300,0.f};p.geometry.low={-10000,-10000,-10000};p.geometry.high={10000,10000,10000};p.geometry.segments={{1,0,0,3000},{0,1,-.1f,3000}};p.flags38=0x2e;if(k%3==0)p.geometry.events={{16,930,600,1200}};
   unsigned at=0x60000+i*64;write(at,unsigned(p.geometry.events.size()));write(at+4,0x65000u+i*32);write(at+8,2u);write(at+12,p.geometry.origin);write(at+0x1c,p.geometry.low);write(at+0x28,p.geometry.high);write(at+0x18,0x64000u+i*32);write(at+0x34,0x63000u);write(at+0x38,p.flags38);for(unsigned j=0;j<2;++j)write(0x64000+i*32+j*16,p.geometry.segments[j]);if(!p.geometry.events.empty())write(0x65000+i*32,p.geometry.events[0]);
  }
  write(0x63008,int16_t(0));write(0x6300c,0x26aa80u);write(0x4d33a8,2u);write(0x4d33ac,0x60000u);
  ssx::OriginalNpcDrivingState state;state.behavior=ssx::OriginalNpcBehavior::Peer100F88;state.targetPeerE70=1;state.behaviorCounterF38=2;state.parameterDF8=70;state.desiredSpeedDF0=1600;
  ssx::OriginalNpcDrivingContext input;input.position={500,0,0};input.velocity={float(500+k%7*500),0,-10};input.physicalForward={1,0,0};input.route.pathIndex=0;input.route.closestPoint={500,0,0};input.route.previousLookaheadPoint={1296,0,0};input.route.lookaheadPoint=input.route.previousLookaheadPoint;input.route.previousDistance=490;input.route.currentDistance=500;input.tick=sharedTick=k%2?int(k):120;
  std::vector<int> occupancy;for(unsigned i=0;i<6;++i){input.peers[i].enabled=i!=0;input.peers[i].planarDistanceCm=i==1?float(100+(k%12)*100):1400.f;input.peers[i].bearing=i==1?(k%3==0?2.f:k%3==1?.1f:-.2f):1.f;input.peerHuman[i]=i!=0&&k%4==0;input.peerVelocities[i]={i==0?input.velocity[0]:float(500+(k%5)*300),0,-10};input.peerPathIndices[i]=i==0?0:int(k%2);if(!input.peerHuman[i])occupancy.push_back(input.peerPathIndices[i]);
   unsigned r=i==0?actor:0x70000+i*0x1000;write(0x92028+i*4,r);write(r+0x6c0,0x54000u);write(r+0x874,uint32_t(input.peerHuman[i]));write(r+0x1e0,input.peerVelocities[i]);write(r+0x1ec,0.f);write(r+0xab8,0x60000u+input.peerPathIndices[i]*64);write(actor+i*0x24,uint32_t(input.peers[i].enabled));write(actor+i*0x24+8,input.peers[i].planarDistanceCm);write(actor+i*0x24+12,input.peers[i].bearing);
  }
  ssx::OriginalNpcPathScoreContext score;score.position=input.position;score.velocity=input.velocity;score.roleE00=2;score.randomizeE08=k%2;score.currentPathIndex=0;score.npcPathIndices=occupancy;
  write(0x4a30f0-0x848,0x90000u);write(0x90084,0x91000u);write(0x9100c,0x92000u);write(0x92078,6u);for(unsigned off:{0x10u,0x28u,0x40u,0x48u})write(0x54000+off,int16_t(-0x6c0));write(0x54014,0x140910u);write(0x5402c,0x1408f0u);write(0x54044,0x140bc0u);write(0x5404c,0x140bc8u);write(0x540a0,int16_t(-0xf50));write(0x540a4,0x10d410u);
  write(owner+0x18,actor);write(owner+0xdf0,state.desiredSpeedDF0);write(owner+0xdf8,state.parameterDF8);write(owner+0xe70,state.targetPeerE70);write(owner+0xf38,state.behaviorCounterF38);write(owner+0xf44,0xffff0000u);write(owner+0xf48,0x100f88u);write(owner+0xe00,int16_t(2));write(owner+0xe04,0u);write(owner+0xe08,int(score.randomizeE08));write(actor+0x77c,owner);write(actor+0xf0,0u);write(actor+0x110,input.position);write(actor+0x11c,1.f);write(actor+0x1c0,input.boardUp);write(actor+0x1cc,0.f);write(actor+0x1b0,input.physicalForward);write(actor+0x1bc,0.f);write(actor+0x490,input.route.closestPoint);write(actor+0x49c,1.f);write(actor+0x4a0,input.route.lookaheadPoint);write(actor+0x4ac,1.f);write(actor+0x4b0,input.route.previousLookaheadPoint);write(actor+0x4bc,1.f);write(actor+0x4c0,input.route.previousDistance);write(actor+0x4c4,input.route.currentDistance);write(actor+0x4c8,input.route.lateralDistance);write(actor+0x4cc,input.route.heading);write(actor+0xabc,0x55000u);write(0x55000,input.route.cache.origin);write(0x55010,input.route.cache.distance);write(0x55014,input.route.cache.segment);
  std::array<uint32_t,2> words{};write(0x40000,words);for(auto&w:drawWords)w=rng();drawIndex=0;R5900Context c{};c.pc=0x100f88;c.vu0_vf[0]=_mm_set_ps(1,0,0,0);SET_GPR_U32(&c,4,owner);SET_GPR_U32(&c,5,0x40000);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,29,0x10000);SET_GPR_U32(&c,31,0x12345678);runBehavior(c);
  unsigned nativeDraw=0;auto actual=ssx::originalNpcPeerBehavior(words,state,input,paths,score,[&](){return drawWords[nativeDraw++];});routeSwitches+=input.route.pathIndex!=0;int16_t counter;std::memcpy(&counter,m.data()+owner+0xf38,2);
  if(c.pc!=0x12345678||actual[0]!=read(0x40000)||actual[1]!=read(0x40004)||nativeDraw!=drawIndex||read(owner+0xe70)!=uint32_t(state.targetPeerE70)||read(owner+0xf48)!=behaviorAddresses[int(state.behavior)]||counter!=state.behaviorCounterF38||read(actor+0xab8)!=0x60000+input.route.pathIndex*64){printf("NPC peer behavior mismatch%u word%08x/%08x path%d/%d RNG%u/%u\n",k,read(0x40000),actual[0],int(read(actor+0xab8)-0x60000)/64,input.route.pathIndex,drawIndex,nativeDraw);return 15;}
 state.behavior=ssx::OriginalNpcBehavior::Designated100B90;state.targetPeerE70=1;write(owner+0xf48,0x100b90u);write(owner+0xe70,1);input.peerPositions[1]={float(int(k%25)-10)*80,float(int(k%31)-15)*60,150};write(0x71000+0x110,input.peerPositions[1]);write(0x71000+0x11c,1.f);words={rng(),rng()};write(0x40000,words);c={};c.pc=0x100b90;c.vu0_vf[0]=_mm_set_ps(1,0,0,0);SET_GPR_U32(&c,4,owner);SET_GPR_U32(&c,5,0x40000);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,29,0x10000);SET_GPR_U32(&c,31,0x12345678);runBehavior(c);actual=ssx::originalNpcDesignatedBehavior(words,state,input,paths[input.route.pathIndex]);std::memcpy(&counter,m.data()+owner+0xf38,2);
 if(actual[0]!=read(0x40000)||actual[1]!=read(0x40004)||read(owner+0xe70)!=uint32_t(state.targetPeerE70)||read(owner+0xf48)!=behaviorAddresses[int(state.behavior)]||counter!=state.behaviorCounterF38){printf("NPC designated mismatch%u word%08x,%08x/%08x,%08x\n",k,read(0x40000),read(0x40004),actual[0],actual[1]);return 19;}

 }
 printf("1000 complete original NPC peer AND designated behaviors match, including %u in-provider route switches\n",routeSwitches);

 for(unsigned k=0;k<20000;++k){
  ssx::OriginalNpcDrivingState state;state.targetPeerE70=k%3? -1:int(k%6);state.offRouteTicksE74=k%49==0?INT32_MAX:int(k%250)-5;state.oppositeHeadingTicksE78=k%53==0?INT32_MAX:int(k%80)-5;
  ssx::OriginalNpcDrivingContext context;context.physicalForward={value(rng)/3000,value(rng)/3000,0};if(k%31==0)context.physicalForward[0]=0;context.route.heading=value(rng)*.001f;context.route.lateralDistance=float(k%1500);context.motionMode=k%6;context.controlState=k%14;context.trajectoryElapsed=float(k%400)/10;
  write(0x20018,0x30000u);write(0x20e70,state.targetPeerE70);write(0x20e74,state.offRouteTicksE74);write(0x20e78,state.oppositeHeadingTicksE78);write(0x20de0,context.motionMode);write(0x20de4,context.controlState);write(0x3077c,0x20000u);write(0x301b0,context.physicalForward);write(0x301bc,0.f);write(0x304cc,context.route.heading);write(0x304c8,context.route.lateralDistance);write(0x30788,0x50000u);write(0x500a0,context.trajectoryElapsed);
  R5900Context c{};c.vu0_vf[0]=_mm_set_ps(1,0,0,0);SET_GPR_U32(&c,4,0x20000);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,29,0x10000);SET_GPR_U32(&c,31,0x12345678);sub_0010D1A0_0x10d1a0(m.data(),&c,&rt);auto*cp=&c;
  bool result=ssx::originalNpcRecovery(state,context);if(c.pc!=0x12345678||GPR_U32(cp,2)!=unsigned(result)||read(0x20e74)!=uint32_t(state.offRouteTicksE74)||read(0x20e78)!=uint32_t(state.oppositeHeadingTicksE78)){printf("NPC recovery mismatch%u mode%d ctrl%d result%u/%u counters%d,%d/%d,%d\n",k,context.motionMode,context.controlState,GPR_U32(cp,2),unsigned(result),int(read(0x20e74)),int(read(0x20e78)),state.offRouteTicksE74,state.oppositeHeadingTicksE78);return 16;}
 }
 puts("20000 complete original NPC recovery requests/counters match exactly");

 for(unsigned k=0;k<20000;++k){
  ssx::OriginalNpcDrivingState state;state.lastAttackTickF4=k%51==0?INT32_MAX:int(k%600);state.defensiveTimerF30=k%3?float(int(k%30)-1)/60.f:0;state.defensiveDecisionF34=k%2;ssx::OriginalNpcDrivingContext input;input.tick=sharedTick=k%3?int(k):600;input.selfSlot=k%6;input.physicalForward={value(rng)/3000,value(rng)/3000,0};
  write(0x20018,0x30000u);write(0x300f4,state.lastAttackTickF4);write(0x3086c,input.selfSlot);write(0x301b0,input.physicalForward);write(0x301bc,0.f);write(0x20f30,state.defensiveTimerF30);write(0x20f34,state.defensiveDecisionF34);write(0x4a30f0-0x848,0x90000u);write(0x90084,0x91000u);write(0x9100c,0x92000u);write(0x92078,6u);write(0x306c0,0x54000u);write(0x54038,int16_t(-0x6c0));write(0x5403c,0x140b80u);write(0x54040,int16_t(-0x6c0));write(0x54044,0x140bc0u);
  for(unsigned i=0;i<6;++i){input.peers[i].planarDistanceCm=float(rng()%400);input.peers[i].bearing=value(rng)*.001f;input.peerHuman[i]=rng()%2;input.peerUpperAnimationClass[i]=rng()%2?13:7;relationshipValues[i]=int(rng()%6);write(0x30008+i*0x24,input.peers[i].planarDistanceCm);write(0x3000c+i*0x24,input.peers[i].bearing);unsigned actor=0x70000+i*0x1000;write(0x92028+i*4,actor);write(actor+0x6c0,0x54000u);write(actor+0x874,uint32_t(input.peerHuman[i]));write(actor+0x784,0x80000u+i*16);write(0x80000+i*16,input.peerUpperAnimationClass[i]);}
  R5900Context c{};c.vu0_vf[0]=_mm_set_ps(1,0,0,0);SET_GPR_U32(&c,4,0x20000);SET_GPR_U32(&c,5,0x40000);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,29,0x10000);SET_GPR_U32(&c,31,0x12345678);sub_0010DBF0_0x10dbf0(m.data(),&c,&rt);auto*cp=&c;auto attack=ssx::originalNpcAttackRequest(state,input,[&](unsigned i){return relationshipValues[i];});
  if(c.pc!=0x12345678||GPR_U32(cp,2)!=unsigned(attack.has_value())||read(0x300f4)!=uint32_t(state.lastAttackTickF4)||(attack&&read(0x40000)!=unsigned(*attack))){printf("NPC attack mismatch%u\n",k);return 17;}
  for(auto&w:drawWords)w=rng();drawIndex=0;c={};SET_GPR_U32(&c,4,0x20000);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,29,0x10000);SET_GPR_U32(&c,31,0x12345678);sub_0010DA10_0x10da10(m.data(),&c,&rt);unsigned nativeDraw=0;int32_t defense=ssx::originalNpcDefensiveRequest(state,input,[&](){return drawWords[nativeDraw++];});
  if(c.pc!=0x12345678||GPR_U32(cp,2)!=uint32_t(defense)||nativeDraw!=drawIndex||read(0x20f30)!=std::bit_cast<uint32_t>(state.defensiveTimerF30)||read(0x20f34)!=uint32_t(state.defensiveDecisionF34)){printf("NPC defense mismatch%u\n",k);return 18;}
 }
 puts("20000 original NPC attack anddefense decisions match exactly withrelationship inputs andRNG");

}
