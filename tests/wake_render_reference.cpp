#include "ps2_runtime_macros.h"
#include "../engine/wake_render.hpp"
#include <fstream>
#include <random>
#include <cstdio>
#include <cstring>
#include <cfenv>
void sub_002DE058_0x2de058(uint8_t*,R5900Context*,PS2Runtime*);
void sub_002DDAB8_0x2ddab8(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x420000,g_ps2RecompiledFunctionTableSlotCount=0xc8000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xc8000]={};
template<class T>T get(uint8_t*m,unsigned p){T v;std::memcpy(&v,m+p,sizeof(v));return v;}
template<class T>void put(uint8_t*m,unsigned p,T v){std::memcpy(m+p,&v,sizeof(v));}
static unsigned drawCalls=0;
static std::array<uint32_t,6> drawArgs;
static std::array<uint32_t,5> drawMaterial;
static float drawFade=0,drawStep=0;
int main(int argc,char**argv){
 if(argc!=2)return 1;PS2Runtime rt;rt.registerFunction(0x33fff0,[](uint8_t*,R5900Context*c,PS2Runtime*){SET_GPR_U32(c,2,0);c->pc=GPR_U32(c,31);});
 std::ifstream file(argv[1],std::ios::binary);std::vector<uint8_t> memory((std::istreambuf_iterator<char>(file)),{});if(memory.size()!=32*1024*1024)return 2;auto*m=memory.data();
 constexpr unsigned owner=0x20000,actor=0x40000,rows=0x60000;put(m,owner,actor);put(m,actor+0x6c0,0x50000u);put(m,0x50038,int16_t(0));put(m,0x5003c,0x33fff0u);put(m,owner+0xc,rows);
 for(unsigned b=0;b<5;b++)put(m,owner+0x10+b*4,0x70000u+b*0x2000);
 rt.registerFunction(0x3883b8,[](uint8_t*m,R5900Context*c,PS2Runtime*){
  ++drawCalls;for(unsigned i=0;i<6;i++)drawArgs[i]=GPR_U32(c,4+i);
  const auto material=get<uint32_t>(m,drawArgs[0]+0xe84);
  for(unsigned i=0;i<5;i++)drawMaterial[i]=get<uint32_t>(m,material+i*4);
  drawFade=c->f[12];drawStep=c->f[13];c->pc=GPR_U32(c,31);
 });
 const auto graphics=get<uint32_t>(m,0x4a30f0-0x854),stack=get<uint32_t>(m,graphics+0xe84);
 std::fesetround(FE_TOWARDZERO);std::mt19937 random(0x2de110);std::uniform_real_distribution<float> colour(-.2f,1.2f),unit(0,1);
 auto context=[&](unsigned pc){R5900Context c{};c.pc=pc;SET_GPR_U32(&c,4,owner);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,29,0x10000);SET_GPR_U32(&c,31,0x12345678);return c;};
 for(unsigned n=0;n<20000;n++){
  std::array<float,4> env{colour(random),colour(random),colour(random),colour(random)};int counter=random()%1000000;ssx::OriginalWakeCursor cursor;cursor.capacity=n%2?32:22;cursor.head=random()%cursor.capacity;cursor.count=random()%(cursor.capacity+1);cursor.phase=unit(random)*4;
  put(m,0x4fa398,env);put(m,owner+4,cursor.count);put(m,owner+8,cursor.head);put(m,owner+0xb8,cursor.capacity);put(m,owner+0x38,cursor.phase);put(m,owner+0x40,counter);
  auto c=context(0x2de058);sub_002DE058_0x2de058(m,&c,&rt);auto visual=ssx::originalWakeVisual(env,counter);ssx::originalWakeAdvanceCursor(cursor);
  if(c.pc!=0x12345678||get<int>(m,owner+0x40)!=counter+1||get<int>(m,owner+8)!=cursor.head)return 3;
  for(unsigned k=0;k<4;k++)if(visual.environment[k]!=get<float>(m,owner+0xa0+k*4))return 4;
  for(unsigned b=0;b<5;b++){unsigned at=0x70000+b*0x2000+cursor.head*48;if(visual.u!=get<float>(m,at))return 5;for(unsigned k=0;k<3;k++)if(visual.rgb[k]!=get<uint32_t>(m,at+16+k*4))return 6;}
  cursor.count=1+random()%cursor.capacity;float lifetime=n%2?1.25f:.800000011920929f;std::array<ssx::OriginalWakeRow,32> values;
  for(unsigned i=0;i<values.size();i++){values[i].age64=unit(random)*lifetime;put(m,rows+i*0x70+0x64,values[i].age64);}
  put(m,owner+4,cursor.count);put(m,owner+0xbc,lifetime);c=context(0x2ddc68);SET_GPR_U32(&c,26,0x57414b45);SET_GPR_U32(&c,24,owner);SET_GPR_U32(&c,17,0x70);c.f[2]=.800000011920929f;c.f[3]=1;c.f[12]=1.25f;
  sub_002DDAB8_0x2ddab8(m,&c,&rt);auto window=ssx::originalWakeDrawWindow(cursor,values,lifetime);auto* cp=&c;
  if(c.pc!=0x12345678||window.start!=int(GPR_U32(cp,7))||window.count!=int(GPR_U32(cp,9))||window.fade!=c.f[12]||window.step!=c.f[13]){printf("wake fade mismatch %u %d/%d %.9g/%.9g\n",n,window.count,int(GPR_U32(cp,9)),window.step,c.f[13]);return 7;}
  // Execute the complete draw wrapper, intercepting only the GPU backend call.
  put(m,owner+0xb4,n%2?5u:4u);put(m,0x4a30f0+0x1438,n%7==0?1u:0u);
  drawCalls=0;c=context(0x2ddab8);sub_002DDAB8_0x2ddab8(m,&c,&rt);
  const bool expectedDraw=n%7!=0&&cursor.count>=2;
  if(c.pc!=0x12345678||drawCalls!=unsigned(expectedDraw)||get<uint32_t>(m,graphics+0xe84)!=stack)return 8;
  if(expectedDraw){
   if(drawArgs!=std::array<uint32_t,6>{graphics,owner+0x10,n%2?5u:4u,unsigned(window.start),unsigned(cursor.capacity),unsigned(window.count)}||drawFade!=window.fade||drawStep!=window.step)return 9;
   auto w0=get<uint32_t>(m,0x501420)&~12u;
   auto w1=get<uint32_t>(m,0x501424);
   w1=((w1&~0x400000u)|0x400000u)&~0x1800000u;w1&=~0x300000u;w1=(w1&0xfff00fffu)|0x14000u;w1=(w1&~3u)|2u;w1=(w1&~0x7cu)|0x14u;
   auto w2=(get<uint32_t>(m,0x501428)&0xe00003ffu&~0x3e0u)|0xe0u;
   if(drawMaterial[0]!=w0||drawMaterial[1]!=w1||drawMaterial[2]!=w2)return 10;
  }
 }
 puts("20000 complete wake draw wrappers: suppression, strip arguments, material state and stack restoration exact");
 puts("20000 original wake colour/UV preparations and draw windows exact, including wrap, GS colour quantization, capacity trimming and oldest-row fade");
}
