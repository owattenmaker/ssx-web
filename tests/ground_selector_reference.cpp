#include "ps2_runtime_macros.h"
#include "../engine/ground_animation_control.hpp"
#include <fstream>
#include <cfenv>
#include <cstdio>
#include <random>
#include <bit>
void sub_00131620_0x131620(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x420000,g_ps2RecompiledFunctionTableSlotCount=0xc8000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xc8000]={};
static int currentSemantic,currentClass,requested,reverseCalls,draws;static bool reverseResult;static uint32_t randomWord;
int main(int argc,char**argv){
 setbuf(stdout,nullptr);PS2Runtime rt;std::ifstream file(argv[1],std::ios::binary);std::vector<uint8_t> m((std::istreambuf_iterator<char>(file)),{});if(m.size()!=32*1024*1024)return 2;
 auto u=[&](unsigned at){uint32_t x;std::memcpy(&x,m.data()+at,4);return x;};auto f=[&](unsigned at){return std::bit_cast<float>(u(at));};auto w=[&](unsigned at,auto x){std::memcpy(m.data()+at,&x,sizeof(x));};
 rt.registerFunction(0x312aa0,[](uint8_t*,R5900Context*c,PS2Runtime*){SET_GPR_U32(c,2,currentSemantic);c->pc=GPR_U32(c,31);});
 rt.registerFunction(0x311ae8,[](uint8_t*,R5900Context*c,PS2Runtime*){SET_GPR_U32(c,2,currentClass);c->pc=GPR_U32(c,31);});
 rt.registerFunction(0x114cc0,[](uint8_t*,R5900Context*c,PS2Runtime*){++reverseCalls;SET_GPR_U32(c,2,reverseResult);c->pc=GPR_U32(c,31);});
 rt.registerFunction(0x317810,[](uint8_t*,R5900Context*c,PS2Runtime*){++draws;SET_GPR_U32(c,2,randomWord);c->pc=GPR_U32(c,31);});
 rt.registerFunction(0x3128e8,[](uint8_t*,R5900Context*c,PS2Runtime*){if(requested!=-1||GPR_U32(c,6)||c->f[12]!=-1.f)throw std::runtime_error("Invalid original selector animation request");requested=GPR_S32(c,5);c->pc=GPR_U32(c,31);});
 w(0x20000,0x30000u);w(0x30784,0x40000u);std::mt19937 rng(0x131878);std::uniform_real_distribution<float> value(-1,1);std::fesetround(FE_TOWARDZERO);unsigned totalDraws=0;std::array<unsigned,439> outcomes{};
 for(unsigned k=0;k<40000;++k){
  ssx::OriginalGroundProfile profile;profile.surface.id=int(k%10)-1;ssx::OriginalGroundState s;s.controlState=0;s.velocity={value(rng)*2000,value(rng)*2000,value(rng)*2000};if(k%5==0)s.velocity={value(rng)*800,0,0};
  auto control=[&](){return ssx::GroundControlValue{value(rng),value(rng),value(rng)};};s.turn=control();s.crouch=control();s.brake=control();s.animationTurn=control();if(k%2)s.brake.current=0;if(k%4==0)s.brake.current=k%8?.9f:-.9f;
  s.lateral={value(rng),value(rng),value(rng)};if(k%3==0){s.animationTurn.current=k%2?.8f:-.8f;s.lateral[2]=.9f;}
  s.manualSpin=k%5==0?value(rng):0;s.boost=k%3==0?1.f:0;s.reverseStance=k%2;
  constexpr int choices[]={0,5,6,7,8,9,10,11,12,13,14,15,16,17,21,22,55,61,245,268,305};s.animationIndex=currentSemantic=choices[rng()%std::size(choices)];s.animationClass=currentClass=int(u(0x446990+s.animationIndex*28));
  ssx::RiderInput input;input.turn=k%7==0?0:value(rng);input.crouch=k%3==0?0:value(rng);reverseResult=k%13==0;randomWord=rng();
  w(0x301e0,s.velocity);w(0x301ec,0.f);w(0x30320,int32_t(s.reverseStance));w(0x303b8,s.lateral[2]);w(0x302dc,s.manualSpin);w(0x302fc,s.boost);w(0x30438,profile.surface.id);
  for(auto[off,c]:{std::pair{0x1f0,s.turn},std::pair{0x1fc,s.animationTurn},std::pair{0x214,s.brake},std::pair{0x220,s.crouch}}){w(0x30000+off,c.current);w(0x30004+off,c.rate);w(0x30008+off,c.target);}
  requested=-1;reverseCalls=draws=0;R5900Context c{};c.pc=0x131878;c.vu0_vf[0]=_mm_set_ps(1,0,0,0);c.f[21]=input.turn;c.f[22]=input.crouch;SET_GPR_U32(&c,17,0x20000);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,29,0x10000);SET_GPR_U32(&c,31,0x12345678);sub_00131620_0x131620(m.data(),&c,&rt);if(c.pc!=0x12345678){printf("Selector continuation%x\n",c.pc);return 3;}
  unsigned nativeDraws=0;bool supported=ssx::originalSelectGroundAnimation(profile,s,input,0,0,[&](){++nativeDraws;return randomWord;},reverseResult);int selected=requested<0?currentSemantic:requested;totalDraws+=nativeDraws;++outcomes[selected];
  if(!supported||s.animationIndex!=selected||s.animationClass!=int(u(0x446990+selected*28))||nativeDraws!=unsigned(draws)){printf("Selector mismatch%u old%d/%d selected%d/%d class%d/%d RNG%u/%d surface%d speed%.9g brake%.9g raw%.9g/%.9g\n",k,currentSemantic,currentClass,selected,s.animationIndex,int(u(0x446990+selected*28)),s.animationClass,nativeDraws,draws,profile.surface.id,std::sqrt(s.velocity[0]*s.velocity[0]+s.velocity[1]*s.velocity[1]+s.velocity[2]*s.velocity[2]),s.brake.current,input.turn,input.crouch);return 4;}
  for(auto[off,control]:{std::pair{0x1f0,s.turn},std::pair{0x1fc,s.animationTurn},std::pair{0x214,s.brake},std::pair{0x220,s.crouch}})for(auto[d,x]:{std::pair{0,control.current},std::pair{4,control.rate},std::pair{8,control.target}})if(u(0x30000+off+d)!=std::bit_cast<uint32_t>(x)){printf("Selector target mismatch%u old%d off%x expected%.9g actual%.9g\n",k,currentSemantic,off+d,f(0x30000+off+d),x);return 5;}
 }
 printf("40000 full original ground animation selectors match semantics/classes/control targets/RNG exactly (%u steep-bob draws)\n",totalDraws);for(unsigned n=0;n<outcomes.size();++n)if(outcomes[n])printf("semantic%u cases%u\n",n,outcomes[n]);
}
