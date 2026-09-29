#include "ps2_runtime_macros.h"
#include "../engine/start_control.hpp"
#include <fstream>
#include <random>
#include <cstring>
#include <cstdio>
void sub_0012BE20_0x12be20(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x420000,g_ps2RecompiledFunctionTableSlotCount=0xc8000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xc8000]={};static std::vector<int> calls;static float angle;static int semantic;
int main(int argc,char**argv){if(argc!=2)return 1;std::ifstream f(argv[1],std::ios::binary);std::vector<uint8_t>m((std::istreambuf_iterator<char>(f)),{});if(m.size()!=32*1024*1024)return 2;PS2Runtime rt;std::mt19937 rng(0x12be20);
 auto put=[&](unsigned a,const auto&v){std::memcpy(m.data()+a,&v,sizeof(v));};auto word=[&](unsigned a){uint32_t x;std::memcpy(&x,m.data()+a,4);return x;};
 rt.registerFunction(0x311a50,[](uint8_t*,R5900Context*c,PS2Runtime*){calls.push_back(0);c->pc=GPR_U32(c,31);});
 rt.registerFunction(0x31be50,[](uint8_t*m,R5900Context*c,PS2Runtime*){calls.push_back(1);angle=c->f[12];float sine=.25f,cosine=.75f;std::memcpy(m+GPR_U32(c,4),&sine,4);std::memcpy(m+GPR_U32(c,5),&cosine,4);c->pc=GPR_U32(c,31);});
 rt.registerFunction(0x3128e8,[](uint8_t*,R5900Context*c,PS2Runtime*){calls.push_back(2);semantic=GPR_U32(c,5);if(c->f[12]!=-1||GPR_U32(c,6))throw std::runtime_error("Wrong start-entry play arguments");c->pc=GPR_U32(c,31);});
 put(0x10014,0x20000u);put(0x20784,0x30000u);
 for(unsigned n=0;n<20000;n++){
  ssx::OriginalStartControlState s{int(rng()%8)-1,float(rng()%100),float(rng()%100),float(rng()%100),float(rng()%100)};bool stance=n%2;put(0x10000,s);put(0x20324,uint32_t(stance));calls.clear();
  R5900Context c{};c.pc=0x12be20;SET_GPR_U32(&c,4,0x10000);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,29,0x90000);SET_GPR_U32(&c,31,0x12345678);
  ssx::OriginalRounding rounding;sub_0012BE20_0x12be20(m.data(),&c,&rt);const auto entry=ssx::originalStartControlEnter(s,stance);
  std::array<float,4> root{0,0,.25f,.75f},position{0,0,0,1};
  if(c.pc!=0x12345678||calls!=std::vector<int>({0,1,2})||entry.semantic!=semantic||std::bit_cast<uint32_t>(entry.rootHalfAngle)!=std::bit_cast<uint32_t>(angle)||word(0x20320)!=unsigned(entry.mirror)||word(0x30018)!=unsigned(entry.mirror)||std::memcmp(m.data()+0x10000,&s,sizeof(s))||std::memcmp(m.data()+0x30030,position.data(),16)||std::memcmp(m.data()+0x30040,root.data(),16)){printf("Start entry mismatch %u\n",n);return 3;}
 }
 puts("20,000 original start entries match phase reset, stance, root-angle and semantic; trigonometry is a controlled boundary");
}
