// Development oracle: original 0x1163B0 (attacks with no opponent) against
// engine/attack_control.cpp. Animator callees are stubbed with one shared model.
#include "ps2_runtime_macros.h"
#include "../engine/attack_control.hpp"
#include <fstream>
#include <cstring>
#include <random>
#include <cstdio>
#include <cfenv>
#include <bit>
#include <vector>
#include <algorithm>
void sub_001163B0_0x1163b0(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x340000,g_ps2RecompiledFunctionTableSlotCount=0x90000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0x90000]={};
constexpr unsigned rider=0x40000,animator=0x50000,done=0x12345678;
constexpr std::array<unsigned,2> sequenceAt{0x60000,0x61000};
static int classOf(int semantic){
 switch(semantic){case 322:case 325:return 3;case 323:case 324:case 326:case 327:return 13;case 320:case 321:return 17;case 5:return 7;case 61:return 10;case 20:return 15;default:return 0;}
}
static int channelOf(int semantic){return semantic>=325&&semantic<=327?0:1;}
struct Sequence{float rate=1,weight=1,target=1,fade=0;uint32_t stop=0;uint64_t latched=0,raised=0;};
struct Model{std::array<int,2> requested{438,438};std::array<Sequence,2> seq;};
using Call=std::array<uint64_t,4>;
static std::vector<Call> calls;static uint8_t* ram;
static uint32_t get(unsigned at){uint32_t v;memcpy(&v,ram+at,4);return v;}
static void put32(unsigned at,uint32_t v){memcpy(ram+at,&v,4);}
static float getf(unsigned at){return std::bit_cast<float>(get(at));}
static void putf(unsigned at,float v){put32(at,std::bit_cast<uint32_t>(v));}
static void store(const Model& m){
 for(unsigned c=0;c<2;++c){put32(animator+4*c,uint32_t(m.requested[c]));auto&s=m.seq[c];unsigned at=sequenceAt[c];putf(at+0x90,s.rate);putf(at+0x94,s.weight);putf(at+0x98,s.target);putf(at+0x9c,s.fade);put32(at+0xa0,s.stop);memcpy(ram+at+0xb0,&s.latched,8);memcpy(ram+at+0xb8,&s.raised,8);}
}
static Model load(){
 Model m;for(unsigned c=0;c<2;++c){m.requested[c]=int(get(animator+4*c));auto&s=m.seq[c];unsigned at=sequenceAt[c];s.rate=getf(at+0x90);s.weight=getf(at+0x94);s.target=getf(at+0x98);s.fade=getf(at+0x9c);s.stop=get(at+0xa0);memcpy(&s.latched,ram+at+0xb0,8);memcpy(&s.raised,ram+at+0xb8,8);}return m;
}
static void playModel(Model& m,int semantic){unsigned c=channelOf(semantic);m.requested[c]=semantic;m.seq[c]=Sequence{1,0,1,.23f,0,0,0};}
static void dependency(uint8_t*mem,R5900Context*c,PS2Runtime*){
 switch(c->pc){
 case 0x311ae8:{int s=int(get(animator+4*GPR_U32(c,5)));SET_GPR_U32(c,2,s==438?0:classOf(s));break;}
 case 0x312aa0:SET_GPR_U32(c,2,get(animator+4*GPR_U32(c,5)));break;
 case 0x311b20:SET_GPR_U32(c,2,sequenceAt.at(GPR_U32(c,5)));break;
 case 0x1446a0:case 0x1446b8:{uint64_t flags;memcpy(&flags,mem+GPR_U32(c,4)+(c->pc==0x1446b8?8:0),8);SET_GPR_U32(c,2,(flags>>(GPR_U32(c,5)&63))&1);break;}
 case 0x3128e8:{uint64_t mask;memcpy(&mask,mem+animator+0x20,8);int semantic=int(GPR_U32(c,5));calls.push_back({0,uint64_t(semantic),GPR_U32(c,6),mask^std::bit_cast<uint32_t>(c->f[12])});Model m=load();playModel(m,semantic);store(m);break;}
 case 0x311e88:{unsigned ch=GPR_U32(c,5);calls.push_back({1,ch,std::bit_cast<uint32_t>(c->f[12]),0});put32(animator+4*ch,438);break;}
 case 0x313a10:{unsigned at=GPR_U32(c,4);calls.push_back({2,at==sequenceAt[0]?0u:1u,std::bit_cast<uint32_t>(c->f[12]),std::bit_cast<uint32_t>(c->f[13])});putf(at+0x98,c->f[12]);putf(at+0x9c,c->f[13]);put32(at+0xa0,0);break;}
 default:printf("unexpected %08x\n",c->pc);throw std::runtime_error("Unknown attack dependency");
 }c->pc=GPR_U32(c,31);
}
int main(int argc,char**argv){
 if(argc!=2)return 2;std::ifstream in(argv[1],std::ios::binary);std::vector<uint8_t>m((std::istreambuf_iterator<char>(in)),{});ram=m.data();PS2Runtime rt;
 for(unsigned pc:{0x311ae8,0x312aa0,0x311b20,0x1446a0,0x1446b8,0x3128e8,0x311e88,0x313a10})rt.registerFunction(pc,dependency);
 std::mt19937 gen(0x1163b0);auto unit=[&](){return float(gen()%2000001)*1e-6f;};
 const std::array<int,8> upperSemantics{438,320,322,323,324,5,61,322};const std::array<int,6> lowerSemantics{438,325,326,327,20,326};
 const float step=std::bit_cast<float>(0x3c360b62u);
 unsigned plays=0,fades=0,blendCopies=0,charges=0,clamps=0;std::array<unsigned,2> returns{};std::array<unsigned,7> semanticsPlayed{};
 for(unsigned i=0;i<200000;++i){
  std::fesetround(FE_TOWARDZERO);
  Model model;model.requested={lowerSemantics[gen()%lowerSemantics.size()],upperSemantics[gen()%upperSemantics.size()]};
  for(auto&s:model.seq){s.rate=gen()%3?1.f:unit();s.weight=unit()*.5f;s.target=gen()%2?1.f:unit()*.5f;s.fade=gen()%3?0.f:unit()*.2f;s.stop=gen()%2;s.latched=gen()%16;s.raised=gen()%2?s.latched&gen():gen()%16;}
  ssx::OriginalAttackRider r;r.switchStance=gen()%2;uint32_t switchWord=r.switchStance?(gen()%2?1u:0x80000000u|gen()):0u;
  r.timeScale=std::array{1.f,1.f,unit(),0.f,.5f}[gen()%5];
  switch(gen()%6){case 0:r.strength=0;break;case 1:r.strength=1;break;case 2:r.strength=unit()-.5f;break;case 3:r.strength=std::bit_cast<float>(std::bit_cast<uint32_t>(1.f)+int(gen()%9)-4);break;case 4:r.strength=1+r.timeScale*step*(gen()%2?1:-1);break;default:r.strength=unit()*.75f;}
  for(auto&v:r.right)v=gen()%9==0?(gen()%2?0.f:std::bit_cast<float>(uint32_t(gen()%0x7fffff))):unit()-1;
  for(auto&v:r.facing)v=unit()-1;
  const bool left=gen()%2,right=gen()%2;const uint64_t mask=(uint64_t(gen())<<32)|gen(),priorMask=(uint64_t(gen())<<32)|gen();
  // Original run.
  calls.clear();store(model);put32(rider+0x784,animator);put32(rider+0x320,switchWord);putf(rider+0x300,r.timeScale);putf(rider+0x350,r.strength);
  for(unsigned k=0;k<4;++k){putf(rider+0x1a0+4*k,r.right[k]);putf(rider+0x340+4*k,r.facing[k]);}
  memcpy(m.data()+rider+0x8c0,&mask,8);memcpy(m.data()+animator+0x20,&priorMask,8);
  R5900Context c{};c.pc=0x1163b0;SET_GPR_U32((&c),4,rider);SET_GPR_U32((&c),5,left);SET_GPR_U32((&c),6,right);SET_GPR_U32((&c),28,0x4a30f0);SET_GPR_U32((&c),29,0x10000);SET_GPR_U32((&c),31,done);
  sub_001163B0_0x1163b0(m.data(),&c,&rt);
  const Model original=load();const auto originalCalls=calls;
  // Native run on the same model.
  Model n=model;std::vector<Call> nativeCalls;ssx::OriginalAttackAccess a;
  a.channelClass=[&](unsigned ch){return n.requested.at(ch)==438?0:classOf(n.requested.at(ch));};
  a.requestedSemantic=[&](unsigned ch){return n.requested.at(ch);};
  a.latched=[&](unsigned ch,unsigned bit){return bool((n.seq.at(ch).latched>>bit)&1);};
  a.raised=[&](unsigned ch,unsigned bit){return bool((n.seq.at(ch).raised>>bit)&1);};
  a.setRate=[&](unsigned ch,float rate){n.seq.at(ch).rate=rate;};
  a.weights=[&](unsigned ch){auto&s=n.seq.at(ch);return ssx::OriginalAttackSequenceWeights{s.weight,s.target,s.fade};};
  a.setWeights=[&](unsigned ch,float w,float t){n.seq.at(ch).weight=w;n.seq.at(ch).target=t;};
  a.fadeTo=[&](unsigned ch,float t,float f){nativeCalls.push_back({2,ch,std::bit_cast<uint32_t>(t),std::bit_cast<uint32_t>(f)});n.seq.at(ch).target=t;n.seq.at(ch).fade=f;n.seq.at(ch).stop=0;};
  a.play=[&](int semantic,bool force){nativeCalls.push_back({0,uint64_t(semantic),uint64_t(force),mask^std::bit_cast<uint32_t>(-1.f)});playModel(n,semantic);};
  a.fade=[&](unsigned ch,float seconds){nativeCalls.push_back({1,ch,std::bit_cast<uint32_t>(seconds),0});n.requested.at(ch)=438;};
  ssx::OriginalAttackRider native=r;const bool result=ssx::originalAttackStep(native,left,right,a);
  uint64_t finalMask;memcpy(&finalMask,m.data()+animator+0x20,8);const bool anyPlay=std::any_of(originalCalls.begin(),originalCalls.end(),[](auto&x){return x[0]==0;});
  bool same=c.pc==done&&result==bool(GPR_U32((&c),2))&&GPR_U32((&c),2)<=1&&nativeCalls==originalCalls&&std::bit_cast<uint32_t>(native.strength)==get(rider+0x350)&&finalMask==(anyPlay?~uint64_t(0):priorMask);
  for(unsigned k=0;k<4;++k)same=same&&std::bit_cast<uint32_t>(native.facing[k])==get(rider+0x340+4*k);
  for(unsigned ch=0;ch<2;++ch){auto&x=n.seq[ch];auto&y=original.seq[ch];same=same&&n.requested[ch]==original.requested[ch]&&std::bit_cast<uint32_t>(x.rate)==std::bit_cast<uint32_t>(y.rate)&&std::bit_cast<uint32_t>(x.weight)==std::bit_cast<uint32_t>(y.weight)&&std::bit_cast<uint32_t>(x.target)==std::bit_cast<uint32_t>(y.target)&&std::bit_cast<uint32_t>(x.fade)==std::bit_cast<uint32_t>(y.fade)&&x.stop==y.stop;}
  if(!same){printf("Attack mismatch %u: left%d right%d switch%d upper%d lower%d result %d/%u calls %zu/%zu strength %.9g/%.9g mask %llx\n",i,left,right,r.switchStance,model.requested[1],model.requested[0],result,GPR_U32((&c),2),nativeCalls.size(),originalCalls.size(),native.strength,getf(rider+0x350),(unsigned long long)finalMask);
   for(auto&x:originalCalls)printf(" original %llu %llu %llx %llx\n",(unsigned long long)x[0],(unsigned long long)x[1],(unsigned long long)x[2],(unsigned long long)x[3]);
   for(auto&x:nativeCalls)printf(" native   %llu %llu %llx %llx\n",(unsigned long long)x[0],(unsigned long long)x[1],(unsigned long long)x[2],(unsigned long long)x[3]);
   for(unsigned k=0;k<4;++k)printf(" facing %08x/%08x\n",std::bit_cast<uint32_t>(native.facing[k]),get(rider+0x340+4*k));return 4;}
  ++returns[result];for(auto&x:originalCalls){if(x[0]==0){++plays;if(x[1]>=322&&x[1]<=327)++semanticsPlayed[x[1]-322];}if(x[0]==1)++fades;if(x[0]==2)++blendCopies;}
  if(get(rider+0x350)!=std::bit_cast<uint32_t>(r.strength)&&native.strength!=0)++charges;
 }
 // Cruise clamp 0x131804..0x13182C over every six-bit command value and specials.
 for(int v=-32;v<32;++v){float turn=float(v)*std::bit_cast<float>(0x3d042108u);float expected=-.5f<=turn?(turn<.5f?turn:.5f):-.5f;if(std::bit_cast<uint32_t>(ssx::originalAttackClampTurn(turn))!=std::bit_cast<uint32_t>(expected))return 5;++clamps;}
 printf("200,000 full original 0x1163B0 cases exact (return, ordered play/fade/313A10 calls incl. bone mask and force, strength +0x350, facing +0x340, channel 0/1 rate/weight/target/fade/stop, animator+0x20 restore): %u returned 1, %u returned 0\n",returns[1],returns[0]);
 printf("  %u plays (322..327: %u %u %u %u %u %u), %u 0.1 s fades, %u 313A10 block-cycle fade copies, %u charge-strength updates; %u turn clamps\n",plays,semanticsPlayed[0],semanticsPlayed[1],semanticsPlayed[2],semanticsPlayed[3],semanticsPlayed[4],semanticsPlayed[5],fades,blendCopies,charges,clamps);
}
