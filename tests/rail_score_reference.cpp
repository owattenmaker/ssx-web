#include "ps2_runtime_macros.h"
#include "../engine/rail_score.hpp"
#include <fstream>
#include <random>
#include <cstdio>
#include <cstring>
void sub_00117C28_0x117c28(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00119918_0x119918(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00119898_0x119898(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00119210_0x119210(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x420000,g_ps2RecompiledFunctionTableSlotCount=0xc8000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xc8000]={};
template<class T>T read(uint8_t*m,unsigned p){T value;std::memcpy(&value,m+p,sizeof value);return value;}
template<class T>void write(uint8_t*m,unsigned p,T value){std::memcpy(m+p,&value,sizeof value);}
static std::optional<ssx::OriginalRailScoreBonus> bonus;static std::vector<int> cleared;
int main(int argc,char**argv){if(argc!=2&&argc!=4)return 1;std::ofstream input,output;if(argc==4){input.open(argv[2],std::ios::binary);output.open(argv[3],std::ios::binary);}std::ifstream file(argv[1],std::ios::binary);std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(file)),{});if(bytes.size()!=32*1024*1024)return 2;auto*m=bytes.data();PS2Runtime rt;
 rt.registerFunction(0x119898,sub_00119898_0x119898);rt.registerFunction(0x119210,sub_00119210_0x119210);
 rt.registerFunction(0x1179e0,[](uint8_t*,R5900Context*c,PS2Runtime*){cleared.push_back(GPR_S32(c,5));c->pc=GPR_U32(c,31);});
 rt.registerFunction(0x117b88,[](uint8_t*,R5900Context*c,PS2Runtime*){if(bonus)throw std::runtime_error("Duplicate rail bonus");bonus=ssx::OriginalRailScoreBonus{GPR_S32(c,5),GPR_S32(c,6),GPR_S32(c,7),c->f[12]};c->pc=GPR_U32(c,31);});
 ssx::OriginalRailScoreProfile p;if(p.dt!=read<float>(m,0x49b5f0)||p.distanceReward!=read<float>(m,0x49b5f4)||p.invertedReward!=read<float>(m,0x49b5f8)||p.spinReward!=read<float>(m,0x49b6dc))return 3;
 for(unsigned i=0;i<12;i++)for(unsigned j=0;j<2;j++)if(p.thresholds[i][j]!=read<float>(m,0x459f68+i*8+j*4))return 4;
 auto context=[](unsigned pc){R5900Context c{};c.vu0_vf[0]=_mm_set_ps(1,0,0,0);c.pc=pc;SET_GPR_U32(&c,4,0x20000);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,29,0x10000);SET_GPR_U32(&c,31,0x12345678);return c;};
 std::mt19937 rng(0x117d74);std::uniform_real_distribution<float> value(-10,10),velocity(-5000,5000),distance(0,35000),scale(0,2);unsigned events=0;
 for(unsigned n=0;n<20000;n++){
  ssx::OriginalRailScoreState s;s.accumulated14=value(rng);s.inverted1C=value(rng);s.distance24=n%5?distance(rng):-1;s.spin34=value(rng);s.multiplier1C4=scale(rng);s.style20=n%5;s.bonusPoints84=int32_t(rng());s.threshold98=rng()%12;
  auto seed=[&](){for(auto [off,v]:std::array<std::pair<unsigned,float>,5>{{{0x14,s.accumulated14},{0x1c,s.inverted1C},{0x24,s.distance24},{0x34,s.spin34},{0x1c4,s.multiplier1C4}}})write(m,0x20000+off,v);write(m,0x20020,s.style20);write(m,0x20084,s.bonusPoints84);write(m,0x20098,s.threshold98);write(m,0x20030,-1.f);write(m,0x20040,-1.f);write(m,0x200a4,-1.f);write(m,0x201ac,0x30000u);};
  auto equal=[&](){return read<float>(m,0x20014)==s.accumulated14&&read<float>(m,0x2001c)==s.inverted1C&&read<float>(m,0x20024)==s.distance24&&read<float>(m,0x20034)==s.spin34&&read<int>(m,0x20020)==s.style20&&read<int>(m,0x20084)==s.bonusPoints84&&read<int>(m,0x20098)==s.threshold98;};
  const auto initial=s;seed();float spin=value(rng);int style=rng()%5;auto c=context(0x119918);SET_GPR_S32(&c,5,style);c.f[12]=spin;{ssx::OriginalRounding round;sub_00119918_0x119918(m,&c,&rt);}float award=ssx::originalRailRotationScore(s,p,style,spin);if(!equal()||c.f[0]!=award||c.pc!=0x12345678)return 5;
  ssx::terrain_original::Vector v{velocity(rng),velocity(rng),velocity(rng)};float up=value(rng),timeScale=scale(rng);write(m,0x301e0,std::array<float,4>{v[0],v[1],v[2],0});write(m,0x301c0,std::array<float,4>{0,0,up,0});write(m,0x30300,timeScale);bonus.reset();cleared.clear();c=context(0x117c28);{ssx::OriginalRounding round;sub_00117C28_0x117c28(m,&c,&rt);}auto actual=ssx::originalRailScoreTick(s,p,v,up,timeScale);
  if(!equal()||c.pc!=0x12345678||bool(actual)!=bool(bonus)){printf("Rail tick mismatch %u pc%x distance %.9g/%.9g reward %.9g/%.9g\n",n,c.pc,s.distance24,read<float>(m,0x20024),s.accumulated14,read<float>(m,0x20014));return 6;}
  if(argc==4){
   auto fw=[&](float value){input.write((char*)&value,4);};auto iw=[&](int32_t value){input.write((char*)&value,4);};
   for(float x:{initial.accumulated14,initial.inverted1C,initial.distance24,initial.spin34,initial.multiplier1C4})fw(x);for(int x:{initial.style20,initial.bonusPoints84,initial.threshold98})iw(x);for(float x:v)fw(x);fw(up);fw(timeScale);iw(style);fw(spin);
   for(unsigned off:{0x14u,0x1cu,0x24u,0x34u,0x1c4u,0x20u,0x84u,0x98u})output.write((char*)m+0x20000+off,4);
   uint32_t event[4]={uint32_t(bool(bonus)),bonus?uint32_t(bonus->points):0u,bonus?uint32_t(bonus->distanceCm):0u,bonus?std::bit_cast<uint32_t>(bonus->displaySeconds):0u};output.write((char*)event,sizeof event);
  }
  if(actual){++events;if(actual->eventType!=bonus->eventType||actual->points!=bonus->points||actual->distanceCm!=bonus->distanceCm||actual->displaySeconds!=bonus->displaySeconds||cleared!=std::vector<int>(actual->clearEvents.begin(),actual->clearEvents.end()))return 7;}else if(!cleared.empty())return 8;
 }
 printf("20000 original rail rotation and distance-scoring stages match, including inverted reward and %u ordered threshold events\n",events);
}
