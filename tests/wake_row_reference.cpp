#include "ps2_runtime_macros.h"
#include "../engine/wake_row.hpp"
#include <fstream>
#include <random>
#include <cstdio>
#include <cstring>
#include <cfenv>
void sub_002DCF28_0x2dcf28(uint8_t*,R5900Context*,PS2Runtime*);
void sub_002DE368_0x2de368(uint8_t*,R5900Context*,PS2Runtime*);
void sub_002DDD30_0x2ddd30(uint8_t*,R5900Context*,PS2Runtime*);
void sub_002DE058_0x2de058(uint8_t*,R5900Context*,PS2Runtime*);
void sub_002D1928_0x2d1928(uint8_t*,R5900Context*,PS2Runtime*);
void sub_002D18B0_0x2d18b0(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x420000,g_ps2RecompiledFunctionTableSlotCount=0xc8000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xc8000]={};
template<class T>T get(uint8_t*m,unsigned p){T v;std::memcpy(&v,m+p,sizeof(v));return v;}
template<class T>void put(uint8_t*m,unsigned p,T v){std::memcpy(m+p,&v,sizeof(v));}
int main(int argc,char**argv){
 if(argc!=2)return 1;PS2Runtime runtime;runtime.registerFunction(0x2de368,sub_002DE368_0x2de368);runtime.registerFunction(0x2d1928,sub_002D1928_0x2d1928);runtime.registerFunction(0x2d18b0,sub_002D18B0_0x2d18b0);
 std::ifstream file(argv[1],std::ios::binary);std::vector<uint8_t> memory((std::istreambuf_iterator<char>(file)),{});if(memory.size()!=32*1024*1024)return 2;auto*m=memory.data();
 const auto table=get<ssx::OriginalWakeNoiseTable>(m,0x445ab0);std::fesetround(FE_TOWARDZERO);
 std::mt19937 random(0x2ddd30);std::uniform_real_distribution<float> signedValue(-1,1),unit(0,1);
 constexpr unsigned owner=0x20000,actor=0x40000,point=0x50000,rows=0x60000;
 auto context=[&](unsigned pc){R5900Context c{};c.pc=pc;c.vu0_vf[0]=_mm_set_ps(1,0,0,0);SET_GPR_U32(&c,4,owner);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,29,0x10000);SET_GPR_U32(&c,31,0x12345678);return c;};
 for(bool device:{false,true}){
  put(m,owner,actor);put(m,owner+0xb4,0);put(m,actor+0x870,device?0:-1);
  for(unsigned band=0;band<5;band++)put(m,owner+0x10+band*4,0x70000u+band*0x2000);
  auto c=context(0x2dcf28);sub_002DCF28_0x2dcf28(m,&c,&runtime);auto profile=ssx::originalWakeProfile(device);
  if(c.pc!=0x12345678||get<int>(m,owner+0xb4)!=profile.columns||get<int>(m,owner+0xb8)!=profile.capacity||get<float>(m,owner+0xbc)!=profile.lifetime)return 11;
  for(unsigned band=0;band<5;band++){
   if(get<float>(m,owner+0x24+band*4)!=profile.verticalIncrement[band])return 12;
   for(unsigned row=0;row<32;row++){unsigned at=0x70000+band*0x2000+row*48;if(get<float>(m,at)!=0||get<float>(m,at+4)!=profile.textureV[band]||get<float>(m,at+8)!=1||get<float>(m,at+12)!=0)return 13;}
  }
 }
 puts("Original device-bound/non-device wake dimensions, lifetimes, vertical increments and texture coordinates exact");
 for(unsigned trial=0;trial<20000;trial++){
  ssx::OriginalWakeCursor cursor;cursor.capacity=trial%2?32:22;cursor.count=random()%(cursor.capacity+1);cursor.head=random()%cursor.capacity;cursor.phase=unit(random)*4;
  put(m,owner,actor);put(m,owner+4,cursor.count);put(m,owner+8,cursor.head);put(m,owner+0xc,rows);put(m,owner+0x38,cursor.phase);put(m,owner+0xb8,cursor.capacity);
  auto c=context(0x2de058);sub_002DE058_0x2de058(m,&c,&runtime);ssx::originalWakeAdvanceCursor(cursor);
  if(c.pc!=0x12345678||cursor.count!=get<int>(m,owner+4)||cursor.head!=get<int>(m,owner+8)||cursor.phase!=get<float>(m,owner+0x38))return 3;
  ssx::OriginalWakeRowInput input;input.columns=trial%2?5:4;input.phase=cursor.phase;input.amplitude90=unit(random)*1500;input.alpha94=unit(random);input.value60=unit(random);
  for(unsigned k=0;k<3;k++){input.coefficient60[k]=signedValue(random);input.coefficient70[k]=signedValue(random);input.direction80[k]=signedValue(random);input.velocity[k]=signedValue(random)*3000;input.point[k]=signedValue(random)*100000;input.normal[k]=signedValue(random);}
  put(m,owner+0x60,input.coefficient60);put(m,owner+0x6c,0.f);put(m,owner+0x70,input.coefficient70);put(m,owner+0x7c,0.f);put(m,owner+0x80,input.direction80);put(m,owner+0x8c,0.f);put(m,owner+0x90,input.amplitude90);put(m,owner+0x94,input.alpha94);put(m,owner+0xb4,input.columns);
  put(m,actor+0x1e0,input.velocity);put(m,actor+0x1ec,0.f);put(m,actor+0x370,input.normal);put(m,actor+0x37c,0.f);put(m,point,input.point);put(m,point+12,1.f);
  for(int band=0;band<input.columns;band++){unsigned base=0x70000+band*0x2000;put(m,owner+0x10+band*4,base);std::memset(m+base+cursor.head*48,0x44,48);}
  c=context(0x2ddd30);SET_GPR_U32(&c,5,point);c.f[12]=input.value60;sub_002DDD30_0x2ddd30(m,&c,&runtime);
  const auto row=ssx::originalWakeRow(input,table);const unsigned at=rows+cursor.head*0x70;
  if(c.pc!=0x12345678||row.value60!=get<float>(m,at+0x60)||row.age64!=get<float>(m,at+0x64))return 4;
  for(int band=0;band<input.columns;band++)for(unsigned k=0;k<3;k++)if(row.velocity[band][k]!=get<float>(m,at+band*16+k*4)){printf("wake fan mismatch %u/%d/%u %.9g/%.9g\n",trial,band,k,row.velocity[band][k],get<float>(m,at+band*16+k*4));return 5;}
  for(unsigned k=0;k<3;k++)if(row.drag50[k]!=get<float>(m,at+0x50+k*4))return 6;
  for(int band=0;band<input.columns;band++){
   const unsigned v=0x70000+band*0x2000+cursor.head*48;
   if(row.alpha!=get<int32_t>(m,v+28)||get<float>(m,v+44)!=1)return 7;
   for(unsigned k=0;k<3;k++)if(row.anchor[k]!=get<float>(m,v+32+k*4))return 8;
   for(unsigned k=0;k<28;k++)if(m[v+k]!=0x44)return 9;
  }
 }
 for(int octaves=0;octaves<=5;octaves++)for(float phase:{0.f,.25f,.5f,std::nextafter(1.f,0.f),1.f,std::nextafter(1.f,2.f),3.9999f,4.f}){
  auto c=context(0x2d1928);SET_GPR_U32(&c,4,octaves);c.f[12]=phase;sub_002D1928_0x2d1928(m,&c,&runtime);float result=ssx::originalWakeOctaves(table,phase,octaves);
  if(c.pc!=0x12345678||result!=c.f[0])return 10;
 }
 puts("48 original wake-noise boundary/octave cases exact, including period boundaries and the final repeated weighted sample");
 puts("20000 original wake cursor advances and full row births exact: noise-driven fan, drag, payload, repeated anchors/alpha and unchanged UV/RGB");
}
