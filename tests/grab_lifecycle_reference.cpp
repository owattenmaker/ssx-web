#include "ps2_runtime_macros.h"
#include "../engine/grab_lifecycle.hpp"
#include "../engine/original_random.hpp"
#include <fstream>
#include <cstring>
#include <random>
#include <cstdio>
#include <cfenv>
#include <bit>
void sub_001352A8_0x1352a8(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x340000,g_ps2RecompiledFunctionTableSlotCount=0x90000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0x90000]={};

void sub_00120D90_0x120d90(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00120038_0x120038(uint8_t*,R5900Context*,PS2Runtime*);
static ssx::OriginalGrabProfile activeProfile;
static int classify(int semantic){if(semantic==287)return 2;for(auto&d:activeProfile.grabs)if(semantic==d.semantic)return 18;for(auto&d:activeProfile.tweak)if(semantic==d.semantic&&semantic!=438)return 19;for(auto&group:activeProfile.uber)for(auto&d:group)if(semantic==d.semantic&&semantic!=438)return 20;return -1;}
static int cls;static float stat;static std::vector<std::array<int,3>> calls;
constexpr unsigned mainSeq=0x50000,upperSeq=0x51000;
static void dependency(uint8_t*m,R5900Context*c,PS2Runtime*){
 switch(c->pc){
 case 0x149690:c->f[0]=stat;break;
 case 0x14dc80:case 0x14dd58:SET_GPR_U32(c,2,0x80000);break;
 case 0x311ae8:SET_GPR_U32(c,2,cls);break;
 case 0x311b20:SET_GPR_U32(c,2,GPR_U32(c,5)==2?mainSeq:upperSeq);break;
 case 0x1446a0:{uint64_t flags;memcpy(&flags,m+GPR_U32(c,4),8);SET_GPR_U32(c,2,(flags>>(GPR_U32(c,5)&63))&1);break;}
 case 0x1500d8:SET_GPR_U32(c,2,activeProfile.grabs.at(GPR_U32(c,6)).semantic);break;
 case 0x1500f8:SET_GPR_U32(c,2,activeProfile.grabs.at(GPR_U32(c,6)).upperSemantic);break;
 case 0x150118:{auto id=activeProfile.grabs.at(GPR_U32(c,6)).scoreId;SET_GPR_U32(c,2,id>=0?id:GPR_U32(c,6));break;}
 case 0x150138:SET_GPR_U32(c,2,activeProfile.tweak.at(GPR_U32(c,6)).semantic);break;
 case 0x150158:SET_GPR_U32(c,2,activeProfile.tweak.at(GPR_U32(c,6)).upperSemantic);break;
 case 0x150178:SET_GPR_U32(c,2,activeProfile.tweak.at(GPR_U32(c,6)).scoreId);break;
 case 0x150198:SET_GPR_U32(c,2,activeProfile.uber.at(GPR_U32(c,7)).at(GPR_U32(c,6)).semantic);break;
 case 0x1502c8:SET_GPR_U32(c,2,activeProfile.uber.at(GPR_U32(c,7)).at(GPR_U32(c,6)).upperSemantic);break;
 case 0x1503f8:SET_GPR_U32(c,2,activeProfile.uber.at(GPR_U32(c,7)).at(GPR_U32(c,6)).scoreId);break;
 case 0x28b180:SET_GPR_U32(c,2,0x80000);break;
 case 0x29a530:calls.push_back({3,0,0});break;
 case 0x119708:case 0x1197d8:calls.push_back({1,int(GPR_U32(c,5)),c->pc==0x119708});c->f[0]=0;break;
 case 0x10e098:break;
 case 0x311e88:calls.push_back({2,int(GPR_U32(c,5)),int(std::bit_cast<uint32_t>(c->f[12]))});break;
 case 0x3128e8:{int semantic=GPR_U32(c,5);calls.push_back({0,semantic,int(GPR_U32(c,6))});if(int next=classify(semantic);next>=0){cls=next;uint64_t flags=0;memcpy(m+mainSeq+0xb0,&flags,8);}break;}
 default:printf("unexpected%08x\n",c->pc);throw std::runtime_error("Unknown grab dependency");
 }c->pc=GPR_U32(c,31);
}
int main(int argc,char**argv){
 if(argc!=2)return 2;std::ifstream in(argv[1],std::ios::binary);std::vector<uint8_t>m((std::istreambuf_iterator<char>(in)),{});PS2Runtime rt;
 rt.registerFunction(0x120038,sub_00120038_0x120038);
 for(unsigned pc:{0x149690,0x14dc80,0x14dd58,0x311ae8,0x311b20,0x1446a0,0x1500d8,0x1500f8,0x150118,0x150138,0x150158,0x150178,0x150198,0x1502c8,0x1503f8,0x28b180,0x29a530,0x119708,0x1197d8,0x10e098,0x311e88,0x3128e8})rt.registerFunction(pc,dependency);
 auto put=[&](unsigned at,const auto&v){memcpy(m.data()+at,&v,sizeof(v));};auto get=[&](unsigned at){uint32_t v;memcpy(&v,m.data()+at,4);return v;};std::mt19937 gen(0x1352a8);
 constexpr unsigned control=0x30000,rider=0x40000,done=0x12345678;put(control+0x58,rider);
 std::array<unsigned,6> results{};unsigned advancedStarts=0,advancedScores=0;
 for(unsigned i=0;i<80000;++i){std::fesetround(FE_TOWARDZERO);ssx::OriginalGrabState state;state.state=i<20000?std::array{0,1,2,5}[i%4]:int(i%6);state.index=state.state==0?int(gen()%16)-1:int(gen()%15);int requested=int(gen()%16)-1;cls=std::array{2,9,18,19,20}[gen()%5];int nativeClass=cls;uint64_t flags=gen()%16,nativeFlags=flags;
  ssx::OriginalGrabProfile profile;stat=float(gen()%1000)*.001f;profile.playbackRate=ssx::originalGrabPlaybackRate(stat);for(unsigned n=0;n<15;++n)profile.grabs[n]={72+int(n),n%2?316:438};bool tweak=i>=20000&&gen()%2;ssx::OriginalGrabContext context;context.superTime=i%3?float(int(i%5)-2):0;context.boostTier=i%8;context.uberEnabled=i%2;profile.extendedDefinitions=i>=20000;
  if(i>=20000){for(unsigned n=0;n<15;++n){profile.grabs[n].scoreId=3+n;profile.tweak[n]={93+int(n),n%2?316:438,19+int(n)};for(unsigned tier=0;tier<2;++tier)profile.uber[tier][n]={n%3?113+int(n)+int(tier)*15:438,n%2?316:438,35+int(n)+int(tier)*15};}if(i%3==0)requested=state.index;}
  activeProfile=profile;put(rider+0x2f0,context.superTime);put(rider+0x2f4,context.boostTier);put(rider+0xb2c,uint32_t(context.uberEnabled));float mainRate=.75f,upperRate=.5f;
  put(control+4,state.state);put(control+8,state.index);put(mainSeq+0xb0,flags);put(mainSeq+0x90,mainRate);put(upperSeq+0x90,upperRate);calls.clear();R5900Context c{};c.pc=0x1352a8;SET_GPR_U32((&c),4,control);SET_GPR_U32((&c),5,requested);SET_GPR_U32((&c),6,tweak);SET_GPR_U32((&c),28,0x4a30f0);SET_GPR_U32((&c),29,0x10000);SET_GPR_U32((&c),31,done);sub_001352A8_0x1352a8(m.data(),&c,&rt);
  std::vector<std::array<int,3>> nativeCalls;ssx::OriginalGrabAnimationAccess access;access.mainClass=[&](){return nativeClass;};access.mainFlags=[&](){return nativeFlags;};access.play=[&](int semantic,bool force){nativeCalls.push_back({0,semantic,force});if(int next=classify(semantic);next>=0){nativeClass=next;nativeFlags=0;}};access.setRate=[&](unsigned channel,float rate){(channel==2?mainRate:upperRate)=rate;};access.fade=[&](unsigned channel,float time){nativeCalls.push_back({2,int(channel),int(std::bit_cast<uint32_t>(time))});};access.score=[&](int index,bool begin){nativeCalls.push_back({1,index,begin});};
  if(i>=20000)access.mappedScore=[&](int id,bool begin){nativeCalls.push_back({1,id,begin});advancedScores+=id>=19;};access.advancedStarted=[&](){nativeCalls.push_back({3,0,0});};
  auto result=ssx::originalGrabLifecycle(state,profile,requested,tweak,access,context);++results.at(state.state);advancedStarts+=result.advancedStarted;
  if(c.pc!=done||!result.supported||result.active!=bool(GPR_U32((&c),2))||state.state!=int(get(control+4))||state.index!=int(get(control+8))||nativeCalls!=calls||std::bit_cast<uint32_t>(mainRate)!=get(mainSeq+0x90)||std::bit_cast<uint32_t>(upperRate)!=get(upperSeq+0x90)){printf("Grab mismatch%u state%d/%d index%d/%d active%d/%u calls%zu/%zu\n",i,state.state,int(get(control+4)),state.index,int(get(control+8)),result.active,GPR_U32((&c),2),nativeCalls.size(),calls.size());printf("request%d tweak%d flags%llx nativeflags%llx cls%d nativeclass%d super%.9g tier%d\n",requested,tweak,(unsigned long long)flags,(unsigned long long)nativeFlags,cls,nativeClass,context.superTime,context.boostTier);for(auto c:calls)printf("originalcall%d,%d,%d\n",c[0],c[1],c[2]);for(auto c:nativeCalls)printf("nativecall%d,%d,%d\n",c[0],c[1],c[2]);return 4;}
 }
 for(unsigned i=0;i<20000;++i){cls=int(i%40)-5;float previous=float(int(gen()%50000)-10000)*.0001f;put(rider+0x318,previous);R5900Context c{};c.pc=0x120d90;SET_GPR_U32((&c),4,rider);SET_GPR_U32((&c),28,0x4a30f0);SET_GPR_U32((&c),29,0x10000);SET_GPR_U32((&c),31,done);sub_00120D90_0x120d90(m.data(),&c,&rt);float actual=ssx::originalGrabLegWeight(previous,cls);if(c.pc!=done||get(rider+0x318)!=std::bit_cast<uint32_t>(actual)){printf("Leg weight mismatch%u class%d input%.9g\n",i,cls,previous);return 5;}}
 puts("20,000 original120D90 leg-weight318 updates exact acrossall animation classes");
 printf("80,000 full original1352A8 grab FSM cases exact, including ordinary legacy API and tweak/Uber state3/4/chaining: %u advanced starts,%u advanced score requests\n",advancedStarts,advancedScores);for(unsigned n=0;n<6;++n)printf("state%u outcomes%u\n",n,results[n]);
}
