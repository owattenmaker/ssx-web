#include "ps2_runtime_macros.h"
#include "../engine/passive_air_control.hpp"
#include <fstream>
#include <cfenv>
#include <cstdio>
#include <random>
#include <bit>
void sub_0012F620_0x12f620(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0012F730_0x12f730(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0012FB68_0x12fb68(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0031C228_0x31c228(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x420000,g_ps2RecompiledFunctionTableSlotCount=0xc8000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xc8000]={};
struct Event {int type=0,a=0,b=0;float value=0;bool operator==(const Event&)const=default;};
static std::vector<Event> events;static bool upperResult,handplantResult,railResult;static ssx::OriginalPassiveAirAnimation animation;static int upperClass;
int main(int argc,char**argv){
 setbuf(stdout,nullptr);PS2Runtime rt;std::ifstream file(argv[1],std::ios::binary);std::vector<uint8_t> m((std::istreambuf_iterator<char>(file)),{});if(m.size()!=32*1024*1024)return 2;
 auto u=[&](unsigned at){uint32_t x;std::memcpy(&x,m.data()+at,4);return x;};auto f=[&](unsigned at){return std::bit_cast<float>(u(at));};auto w=[&](unsigned at,auto x){std::memcpy(m.data()+at,&x,sizeof(x));};
 rt.registerFunction(0x31c228,sub_0031C228_0x31c228);
 rt.registerFunction(0x116120,[](uint8_t*,R5900Context*c,PS2Runtime*){events.push_back({1,int(GPR_U32(c,5)),int(GPR_U32(c,6))});SET_GPR_U32(c,2,GPR_U32(c,5)!=0);c->pc=GPR_U32(c,31);});
 rt.registerFunction(0x1163b0,[](uint8_t*,R5900Context*c,PS2Runtime*){events.push_back({2,int(GPR_U32(c,5)),int(GPR_U32(c,6))});SET_GPR_U32(c,2,upperResult);c->pc=GPR_U32(c,31);});
 rt.registerFunction(0x107578,[](uint8_t*,R5900Context*c,PS2Runtime*){events.push_back({3,int(GPR_U32(c,5)),0});SET_GPR_U32(c,2,handplantResult);c->pc=GPR_U32(c,31);});
 rt.registerFunction(0x106848,[](uint8_t*,R5900Context*c,PS2Runtime*){events.push_back({4});SET_GPR_U32(c,2,railResult);c->pc=GPR_U32(c,31);});
 rt.registerFunction(0x114130,[](uint8_t*,R5900Context*c,PS2Runtime*){events.push_back({5,int(GPR_U32(c,5)),int(GPR_U32(c,6))});c->pc=GPR_U32(c,31);});
 rt.registerFunction(0x312aa0,[](uint8_t*,R5900Context*c,PS2Runtime*){SET_GPR_U32(c,2,animation.semantic);c->pc=GPR_U32(c,31);});
 rt.registerFunction(0x311ae8,[](uint8_t*,R5900Context*c,PS2Runtime*){SET_GPR_U32(c,2,GPR_U32(c,5)==1?upperClass:animation.animationClass);c->pc=GPR_U32(c,31);});
 rt.registerFunction(0x3128e8,[](uint8_t*,R5900Context*c,PS2Runtime*){events.push_back({6,int(GPR_U32(c,5)),int(GPR_U32(c,6)),c->f[12]});c->pc=GPR_U32(c,31);});
 rt.registerFunction(0x11fec8,[](uint8_t*,R5900Context*c,PS2Runtime*){events.push_back({7,int(GPR_U32(c,5))});c->pc=GPR_U32(c,31);});
 rt.registerFunction(0x311e88,[](uint8_t*,R5900Context*c,PS2Runtime*){events.push_back({8,int(GPR_U32(c,5)),0,c->f[12]});c->pc=GPR_U32(c,31);});
 rt.registerFunction(0x311b20,[](uint8_t*,R5900Context*c,PS2Runtime*){events.push_back({9,int(GPR_U32(c,5))});SET_GPR_U32(c,2,0x70000);c->pc=GPR_U32(c,31);});
 auto context=[](){R5900Context c{};c.vu0_vf[0]=_mm_set_ps(1,0,0,0);SET_GPR_U32(&c,4,0x20000);SET_GPR_U32(&c,5,0x40000);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,29,0x10000);SET_GPR_U32(&c,31,0x12345678);return c;};
 w(0x20014,0x30000u);w(0x30784,0x60000u);std::mt19937 rng(0x12f730);std::uniform_real_distribution<float> value(-1,1);std::fesetround(FE_TOWARDZERO);unsigned transitions=0;
 for(unsigned k=0;k<20000;++k){
  ssx::OriginalPassiveAirState s;ssx::OriginalGroundState g;ssx::OriginalAirPrewindState p;
  auto control=[&](){return ssx::GroundControlValue{value(rng),value(rng),value(rng)};};g.turn=control();g.brake=control();g.crouch=control();g.animationTurn=control();p.spin=control();p.flip=control();
  if(k%5==0)g.turn.current=0;if(k%7==0)g.crouch.current=0;if(k%11==0)g.brake.current=0;
  auto store=[&](){for(auto[off,c]:{std::pair{0x1f0,g.turn},std::pair{0x1fc,g.animationTurn},std::pair{0x214,g.brake},std::pair{0x220,g.crouch},std::pair{0x2a4,p.spin},std::pair{0x2b0,p.flip}}){w(0x30000+off,c.current);w(0x30004+off,c.rate);w(0x30008+off,c.target);}w(0x20000,s.entryAngle);w(0x20004,s.entryMagnitude);w(0x20008,s.upperLatch);w(0x2000c,s.identityLatch);w(0x20010,s.lastIdentity);};
  auto equal=[&](){for(auto[off,c]:{std::pair{0x1f0,g.turn},std::pair{0x1fc,g.animationTurn},std::pair{0x214,g.brake},std::pair{0x220,g.crouch},std::pair{0x2a4,p.spin},std::pair{0x2b0,p.flip}}){for(auto[d,x]:{std::pair{0,c.current},std::pair{4,c.rate},std::pair{8,c.target}})if(u(0x30000+off+d)!=std::bit_cast<uint32_t>(x)){printf("Passive control mismatch%u off%x original%.9g native%.9g\n",k,off+d,f(0x30000+off+d),x);return false;}}
   if(u(0x20000)!=std::bit_cast<uint32_t>(s.entryAngle)||u(0x20004)!=std::bit_cast<uint32_t>(s.entryMagnitude)||u(0x20008)!=uint32_t(s.upperLatch)||u(0x2000c)!=uint32_t(s.identityLatch)||u(0x20010)!=uint32_t(s.lastIdentity)){printf("Passive latch mismatch%u angle%.9g/%.9g mag%.9g/%.9g latches%d,%d,%d/%d,%d,%d\n",k,f(0x20000),s.entryAngle,f(0x20004),s.entryMagnitude,int(u(0x20008)),int(u(0x2000c)),int(u(0x20010)),s.upperLatch,s.identityLatch,s.lastIdentity);return false;}return true;};
  store();auto c=context();sub_0012F620_0x12f620(m.data(),&c,&rt);ssx::originalPassiveAirBegin(s,g,p);if(c.pc!=0x12345678||!equal())return 3;
  if(k%3==0){s.entryAngle=value(rng)*6;s.entryMagnitude=value(rng);s.upperLatch=k%2;s.identityLatch=k%2;s.lastIdentity=int8_t(k%256);}if(k%13==0)g.turn.current=g.crouch.current=g.brake.current=0;
  uint32_t word0=(rng()&0x3ffff000u),word1=rng()&63u;if(k%3)word0&=~0x1000u;store();w(0x40000,word0);w(0x40004,word1);
  auto command=ssx::originalDecodePassiveAirCommand(word0,word1);int a=int((word0>>24)&63);if(a>=32)a-=64;int b=int(word1&63);if(b>=32)b-=64;float factor=std::bit_cast<float>(0x3d042108u);if(command.turn!=float(a)*factor||command.crouch!=float(b)*factor||command.identity!=int8_t(word0>>16)||command.upper14!=bool(word0&0x4000)||command.upper15!=bool(word0&0x8000)||command.handplant!=bool(word0&0x2000)||command.recover!=bool(word0&0x1000))return 4;
  constexpr ssx::OriginalPassiveAirAnimation animations[]={{5,1},{9,1},{10,1},{287,2},{268,9},{61,10}};animation=animations[k%6];upperResult=k%17==0;handplantResult=k%13==0;railResult=k%11==0;events.clear();c=context();sub_0012F730_0x12f730(m.data(),&c,&rt);if(c.pc!=0x12345678){printf("Passive continuation%x\n",c.pc);return 5;}auto expected=events;events.clear();
  ssx::OriginalPassiveAirAccess access;access.recover=[&](bool x){events.push_back({1,x,0});return x;};access.upper=[&](bool x,bool y){events.push_back({2,x,y});return upperResult;};access.handplant=[&](bool x){events.push_back({3,x});return handplantResult;};access.rail=[&](){events.push_back({4});return railResult;};access.stopBoost=[&](){events.push_back({5});};access.mainAnimation=[&](){return animation;};access.requestAnimation=[&](int semantic,float blend,uint32_t flags){events.push_back({6,semantic,int(flags),blend});};access.requestControl=[&](int n){events.push_back({7,n});};
  auto result=ssx::originalPassiveAirStep(s,g,command,access);transitions+=result.stop==ssx::OriginalPassiveAirResult::Stop::Control5;if(!equal()||events!=expected){printf("Passive update event mismatch%u expected%zu actual%zu\n",k,expected.size(),events.size());return 6;}
  bool active=k%3;upperClass=k%16;events.clear();w(0x70090,2.5f);c=context();SET_GPR_U32(&c,5,active);sub_0012FB68_0x12fb68(m.data(),&c,&rt);auto exit=ssx::originalPassiveAirLeave(active,upperClass);std::vector<Event> exitEvents;if(exit.fade)exitEvents.push_back({8,exit.fadeChannel,0,exit.fadeSeconds});if(exit.setUpperRate)exitEvents.push_back({9,exit.rateChannel});if(events!=exitEvents||f(0x70090)!=(exit.setUpperRate?exit.upperRate:2.5f)){printf("Passive exit mismatch%u\n",k);return 7;}
 }
 printf("20000 complete original passive4 entries,updates,exits match state/targets/ordered external calls/animation requests (%u transitions to5); signed command fields match\n",transitions);
}
