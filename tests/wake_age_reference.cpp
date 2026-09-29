#include "ps2_runtime_macros.h"
#include "../engine/wake_row.hpp"
#include <fstream>
#include <random>
#include <cstdio>
#include <cstring>
#include <cfenv>
void sub_002DD0B8_0x2dd0b8(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x420000,g_ps2RecompiledFunctionTableSlotCount=0xc8000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xc8000]={};
template<class T>T get(uint8_t*m,unsigned p){T v;std::memcpy(&v,m+p,sizeof(v));return v;}
template<class T>void put(uint8_t*m,unsigned p,T v){std::memcpy(m+p,&v,sizeof(v));}
int main(int argc,char**argv){
 if(argc!=2)return 1;PS2Runtime runtime;runtime.registerFunction(0x11fe98,[](uint8_t*,R5900Context*,PS2Runtime*){throw std::runtime_error("Wake aging oracle escaped its stage boundary");});std::ifstream file(argv[1],std::ios::binary);std::vector<uint8_t> memory((std::istreambuf_iterator<char>(file)),{});if(memory.size()!=32*1024*1024)return 2;auto*m=memory.data();
 std::fesetround(FE_TOWARDZERO);std::mt19937 random(0x2dd0b8);std::uniform_real_distribution<float> unit(0,1),value(-1,1);
 constexpr unsigned owner=0x20000,actor=0x40000,records=0x60000;
 put(m,owner,actor);put(m,owner+0xc,records);put(m,actor+0x438,0u);put(m,0x4a30f0-0x848,0x80000u);put(m,0x80084,0x81000u);put(m,0x81044,0x82000u);
 unsigned expired=0;
 for(unsigned trial=0;trial<20000;trial++){
  const int columns=trial%2?5:4;ssx::OriginalWakeCursor cursor;cursor.capacity=columns==5?32:22;cursor.head=random()%cursor.capacity;cursor.count=random()%(cursor.capacity+1);const int originalCount=cursor.count;float lifetime=.1f+unit(random)*1.4f;
  std::array<float,5> vertical;for(auto& x:vertical)x=value(random)*30;
  std::array<ssx::OriginalWakeRow,32> rows;
  put(m,owner+4,cursor.count);put(m,owner+8,cursor.head);put(m,owner+0xb4,columns);put(m,owner+0xb8,cursor.capacity);put(m,owner+0xbc,lifetime);put(m,owner+0x24,vertical);
  for(int band=0;band<columns;band++)put(m,owner+0x10+band*4,0x70000u+band*0x2000);
  for(int i=0;i<cursor.capacity;i++){
   auto& row=rows[i];row.age64=unit(random)*lifetime*(trial%2?1.3f:.7f);row.value60=value(random)*2;
   for(unsigned k=0;k<3;k++)row.drag50[k]=value(random)*1000;
   unsigned at=records+i*0x70;put(m,at+0x50,row.drag50);put(m,at+0x5c,0.f);put(m,at+0x60,row.value60);put(m,at+0x64,row.age64);
   for(int band=0;band<columns;band++){
    for(unsigned k=0;k<3;k++){row.velocity[band][k]=value(random)*2000;row.positions[band][k]=value(random)*100000;}
    put(m,at+band*16,row.velocity[band]);put(m,at+band*16+12,0.f);unsigned v=0x70000+band*0x2000+i*48;std::memset(m+v,0x44,32);put(m,v+32,row.positions[band]);put(m,v+44,1.f);
   }
  }
  std::array<uint8_t,0xc0> ownerBefore;std::memcpy(ownerBefore.data(),m+owner,ownerBefore.size());
  R5900Context c{};c.pc=0x2dd0b8;c.vu0_vf[0]=_mm_set_ps(1,0,0,0);SET_GPR_U32(&c,4,owner);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,29,0x10000);SET_GPR_U32(&c,31,0x12345678);sub_002DD0B8_0x2dd0b8(m,&c,&runtime);
  for(unsigned k=0;k<ownerBefore.size();k++)if((k<4||k>=8)&&ownerBefore[k]!=m[owner+k])return 9;
  ssx::originalWakeAgeRows(cursor,rows,columns,lifetime,vertical);expired+=cursor.count<originalCount;
  if(c.pc!=0x12345678||get<int>(m,owner+4)!=cursor.count||get<int>(m,owner+8)!=cursor.head)return 3;
  for(int i=0;i<cursor.capacity;i++){
   const auto& row=rows[i];unsigned at=records+i*0x70;if(row.age64!=get<float>(m,at+0x64)||row.value60!=get<float>(m,at+0x60))return 4;
   for(unsigned k=0;k<3;k++)if(row.drag50[k]!=get<float>(m,at+0x50+k*4))return 5;
   for(int band=0;band<columns;band++){
    unsigned v=0x70000+band*0x2000+i*48;
    for(unsigned k=0;k<3;k++)if(row.velocity[band][k]!=get<float>(m,at+band*16+k*4)||row.positions[band][k]!=get<float>(m,v+32+k*4)){printf("wake aging mismatch %u/%d/%d/%u %.9g/%.9g\n",trial,i,band,k,row.positions[band][k],get<float>(m,v+32+k*4));return 6;}
    for(unsigned k=0;k<32;k++)if(m[v+k]!=0x44)return 7;
    if(get<float>(m,v+44)!=1)return 8;
   }
  }
 }
 printf("20000 original wake aging passes exact: position integration, velocity increments, retained payloads and expiry cutoff (%u cutoffs)\n",expired);
}
