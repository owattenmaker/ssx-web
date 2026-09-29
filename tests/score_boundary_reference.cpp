#include "ps2_runtime_macros.h"
#include "../engine/score_boundary.hpp"
#include <fstream>
#include <random>
#include <cstdio>
#include <cstring>
void sub_00119E38_0x119e38(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00119D40_0x119d40(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00117838_0x117838(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00119938_0x119938(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00119958_0x119958(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x420000,g_ps2RecompiledFunctionTableSlotCount=0xc8000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xc8000]={};
template<class T>T read(uint8_t*m,unsigned p){T value;std::memcpy(&value,m+p,sizeof value);return value;}
template<class T>void write(uint8_t*m,unsigned p,T value){std::memcpy(m+p,&value,sizeof value);}
static unsigned calls;static std::array<int32_t,5> arguments;static float seenAmount,seenMultiplier,returned;static int seenActive,seenUbers,seenTotal;
int main(int argc,char**argv){if(argc!=2)return 1;std::ifstream file(argv[1],std::ios::binary);std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(file)),{});if(bytes.size()!=32*1024*1024)return 2;auto*m=bytes.data();PS2Runtime rt;
 rt.registerFunction(0x117838,sub_00117838_0x117838);rt.registerFunction(0x3e6448,[](uint8_t*m,R5900Context*c,PS2Runtime*){std::memset(m+GPR_U32(c,4),GPR_U32(c,5),GPR_U32(c,6));c->pc=GPR_U32(c,31);});
 rt.registerFunction(0x11a228,[](uint8_t*m,R5900Context*c,PS2Runtime*){++calls;for(unsigned i=0;i<5;i++)arguments[i]=GPR_S32(c,5+i);auto base=GPR_U32(c,4);seenAmount=read<float>(m,base+0x14);seenMultiplier=read<float>(m,base+0x18);seenActive=read<int>(m,base+0x70);seenUbers=read<int>(m,base+0x54);seenTotal=read<int>(m,base+0x74);write(m,base+0x70,0);write(m,base+0xa4,4.5f);write(m,base+0x1c4,2.25f);c->f[0]=returned;c->pc=GPR_U32(c,31);});
 if(read<uint32_t>(m,0x49b6f0)!=0x3e051eb8)return 3;
 std::mt19937 rng(0x119d40);std::uniform_real_distribution<float> amount(-2,5);
 for(unsigned n=0;n<20000;n++){
  ssx::OriginalScoreBoundaryState s;s.score.accumulated14=n%7?amount(rng):0;s.multiplier18=amount(rng);s.identity.active70=n%4?1+rng()%6:0;
  std::memset(m+0x20000,0xa5,0x1c8);write(m,0x20014,s.score.accumulated14);write(m,0x20018,s.multiplier18);write(m,0x20070,s.identity.active70);
  int stance=rng()%2,field=rng()%4,style=rng()%5,flag=rng()%3;returned=amount(rng);calls=0;R5900Context c{};c.pc=0x119d40;SET_GPR_U32(&c,4,0x20000);SET_GPR_S32(&c,5,stance);SET_GPR_S32(&c,6,field);SET_GPR_S32(&c,7,style);SET_GPR_S32(&c,8,flag);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,29,0x10000);SET_GPR_U32(&c,31,0x12345678);
  {ssx::OriginalRounding round;sub_00119D40_0x119d40(m,&c,&rt);}if(c.pc!=0x12345678||calls!=1)return 4;
  bool invoked=false;float result=ssx::originalScoreBoundary(s,stance,field,style,flag,[&](auto& state,int a,int b,int d,int e,int zero){
   invoked=true;if(arguments!=std::array<int32_t,5>{a,b,d,e,zero}||state.score.accumulated14!=seenAmount||state.multiplier18!=seenMultiplier||state.identity.active70!=seenActive)throw std::runtime_error("Commit ordering/input mismatch");
   state.identity.active70=0;state.score.comboTimeoutA4=4.5f;state.score.multiplier1C4=2.25f;return returned;
  });if(!invoked||result!=c.f[0])return 5;
  const std::pair<unsigned,float> floats[]={{0x14,s.score.accumulated14},{0x18,s.multiplier18},{0x1c,s.inverted1C},{0x24,s.identity.time24},{0x2c,s.identity.time2C},{0x30,s.airSeconds30},{0x34,s.identity.spin34},{0x38,s.identity.flip38},{0x3c,s.score.holdIncrement3C},{0x40,s.score.holdSeconds40},{0x44,s.score.totalSeconds44},{0x48,s.score.longestSeconds48},{0x6c,s.activeSeconds6C},{0x78,s.timer78},{0xa4,s.score.comboTimeoutA4},{0x1c4,s.score.multiplier1C4}};
  for(auto [off,v]:floats)if(read<uint32_t>(m,0x20000+off)!=std::bit_cast<uint32_t>(v)){printf("Reset float %x\n",off);return 6;}
  const std::pair<unsigned,int> ints[]={{0,s.identity.stance00},{4,s.identity.field04},{8,s.identity.field08},{0xc,s.identity.style0C},{0x10,s.identity.flag10},{0x20,s.identity.style20},{0x28,s.identity.flag28},{0x4c,s.score.normalCount4C},{0x50,s.score.tweakCount50},{0x54,s.score.uberCount54},{0x58,s.score.superUberCount58},{0x5c,s.score.activeUber5C},{0x70,s.identity.active70},{0x74,s.field74},{0x7c,s.identity.field7C},{0x80,s.field80},{0x84,s.score.bonusPoints84},{0x88,s.threshold88},{0x8c,s.score.holdThresholdIndex8C},{0x90,s.threshold90},{0x94,s.threshold94},{0x98,s.threshold98}};
  for(auto [off,v]:ints)if(read<int>(m,0x20000+off)!=v){printf("Reset integer %x\n",off);return 7;}
  for(unsigned i=0;i<3;i++)if(read<int>(m,0x20060+i*4)!=s.score.history60[i]||s.identity.grabs60[i]!=s.score.history60[i])return 8;
  if(read<uint32_t>(m,0x2009c)!=0xa5a5a5a5||read<uint32_t>(m,0x201ac)!=0xa5a5a5a5)return 9;
  s.score.accumulated14=returned;s.identity.spin34=returned;s.identity.time24=1250;s.inverted1C=.75f;write(m,0x20014,s.score.accumulated14);write(m,0x20034,s.identity.spin34);write(m,0x20024,s.identity.time24);write(m,0x2001c,s.inverted1C);
  const bool active=s.identity.active70!=0;calls=0;c.pc=0x119e38;SET_GPR_U32(&c,4,0x20000);SET_GPR_S32(&c,5,stance);SET_GPR_S32(&c,6,field);SET_GPR_U32(&c,31,0x12345678);{ssx::OriginalRounding round;sub_00119E38_0x119e38(m,&c,&rt);}
  unsigned invokedExit=0;auto exit=ssx::originalScoreTakeoffBoundary(s,stance,field,[&](auto& state,int a,int b,int d,int e,int flag){++invokedExit;if(arguments!=std::array<int32_t,5>{a,b,d,e,flag}||seenAmount!=state.score.accumulated14)throw std::runtime_error("Takeoff commit mismatch");state.score.comboTimeoutA4=4.5f;state.score.multiplier1C4=2.25f;return returned;});
  if(c.pc!=0x12345678||calls!=unsigned(!active)||invokedExit!=calls||exit!=c.f[0])return 10;
  for(auto [off,v]:std::array<std::pair<unsigned,float>,8>{{{0x14,s.score.accumulated14},{0x1c,s.inverted1C},{0x24,s.identity.time24},{0x2c,s.identity.time2C},{0x30,s.airSeconds30},{0x34,s.identity.spin34},{0xa4,s.score.comboTimeoutA4},{0x1c4,s.score.multiplier1C4}}})if(read<float>(m,0x20000+off)!=v)return 11;
  for(auto [off,v]:std::array<std::pair<unsigned,int>,7>{{{0,s.identity.stance00},{4,s.identity.field04},{0xc,s.identity.style0C},{0x10,s.identity.flag10},{0x20,s.identity.style20},{0x28,s.identity.flag28},{0x70,s.identity.active70}}})if(read<int>(m,0x20000+off)!=v)return 12;

 }
 for(unsigned n=0;n<20000;n++){
  ssx::OriginalScoreBoundaryState s;s.score.accumulated14=amount(rng);s.multiplier18=amount(rng);s.identity.time24=amount(rng)*1000;s.airSeconds30=amount(rng);s.score.uberCount54=int(rng()%15);s.field74=int(rng()%100);int tier=1+rng()%4,style=rng()%5;
  std::memset(m+0x20000,0,0x1c8);write(m,0x20014,s.score.accumulated14);write(m,0x20018,s.multiplier18);write(m,0x20024,s.identity.time24);write(m,0x20030,s.airSeconds30);write(m,0x20054,s.score.uberCount54);write(m,0x20074,s.field74);
  R5900Context c{};c.pc=0x119938;SET_GPR_U32(&c,4,0x20000);SET_GPR_U32(&c,5,tier);SET_GPR_U32(&c,6,style);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,29,0x10000);SET_GPR_U32(&c,31,0x12345678);
  sub_00119938_0x119938(m,&c,&rt);float zero=ssx::originalRailUberScoreBegin(s,tier,style);
  if(c.pc!=0x12345678||c.f[0]!=zero||read<float>(m,0x2006c)!=s.activeSeconds6C||read<int>(m,0x20070)!=s.identity.active70||read<int>(m,0x2005c)!=s.score.activeUber5C||read<int>(m,0x20020)!=s.identity.style20||read<int>(m,0x2000c)!=s.identity.style0C)return 13;
  calls=0;returned=amount(rng);c.pc=0x119958;SET_GPR_U32(&c,4,0x20000);SET_GPR_U32(&c,5,style);SET_GPR_U32(&c,31,0x12345678);sub_00119958_0x119958(m,&c,&rt);
  auto result=ssx::originalRailUberScoreEnd(s,style,[&](auto& state,int a,int b,int d,int e,int flag){if(arguments!=std::array<int32_t,5>{a,b,d,e,flag}||seenUbers!=state.score.uberCount54||seenTotal!=state.field74||seenActive!=state.identity.active70||seenAmount!=state.score.accumulated14)throw std::runtime_error("Rail Uber commit ordering mismatch");state.identity.active70=0;state.score.comboTimeoutA4=4.5f;state.score.multiplier1C4=2.25f;return returned;});
  if(c.pc!=0x12345678||calls!=1||c.f[0]!=result)return 14;
  for(auto [off,v]:std::array<std::pair<unsigned,float>,7>{{{0x14,s.score.accumulated14},{0x24,s.identity.time24},{0x30,s.airSeconds30},{0x6c,s.activeSeconds6C},{0x78,s.timer78},{0xa4,s.score.comboTimeoutA4},{0x1c4,s.score.multiplier1C4}}})if(read<float>(m,0x20000+off)!=v)return 15;
  for(auto [off,v]:std::array<std::pair<unsigned,int>,6>{{{0xc,s.identity.style0C},{0x20,s.identity.style20},{0x54,s.score.uberCount54},{0x5c,s.score.activeUber5C},{0x70,s.identity.active70},{0x74,s.field74}}})if(read<int>(m,0x20000+off)!=v)return 16;
 }
 puts("20,000 original119938/119958 rail Uber score entry/exit boundaries match");
 puts("20000 original119D40 and119E38 boundaries match commit inputs/order, entry reward, reset fields, persistent fields and active-Uber restoration;11A228 callback isolated");
}
