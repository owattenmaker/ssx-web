#include "ps2_runtime_macros.h"
#include "../engine/environment_properties.hpp"
#include "../engine/environment_regions.hpp"
#include "../engine/environment_transition.hpp"
#include <bit>
#include <cstring>
#include <fstream>
#include <iostream>
#include <random>
void sub_002BCBD0_0x2bcbd0(uint8_t*,R5900Context*,PS2Runtime*);
void sub_002C0408_0x2c0408(uint8_t*,R5900Context*,PS2Runtime*);
void sub_002C0A10_0x2c0a10(uint8_t*,R5900Context*,PS2Runtime*);
void sub_002BAF90_0x2baf90(uint8_t*,R5900Context*,PS2Runtime*);
void sub_002C1CD8_0x2c1cd8(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x380000,g_ps2RecompiledFunctionTableSlotCount=0xa0000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xa0000]={};
static int calls=0;
static void hook(uint8_t*,R5900Context*c,PS2Runtime*){++calls;c->pc=GPR_U32(c,31);}
static void blendCapture(uint8_t*m,R5900Context*c,PS2Runtime*r){
 std::cout<<"Original property blend weight "<<c->f[12]<<" target address "<<std::hex<<GPR_U32(c,5)<<std::dec<<'\n';
 sub_002BCBD0_0x2bcbd0(m,c,r);
}
static void selectionCapture(uint8_t*m,R5900Context*c,PS2Runtime*r){
 std::cout<<"Original property map descriptor "<<std::hex<<GPR_U32(c,5)<<std::dec<<'\n';
 sub_002C0A10_0x2c0a10(m,c,r);
}
int main(int argc,char**argv){
 if(argc!=2)return 1;std::ifstream f(argv[1],std::ios::binary);std::vector<uint8_t> m((std::istreambuf_iterator<char>(f)),{});if(m.size()!=32*1024*1024)return 2;
 PS2Runtime rt;rt.registerFunction(0x2bda90,hook);
 auto put=[&](unsigned at,const auto&v){std::memcpy(m.data()+at,&v,sizeof(v));};auto bits=[&](unsigned at){uint32_t v;std::memcpy(&v,m.data()+at,4);return v;};
 constexpr unsigned object=0x10000,incomingAddress=0x20000,done=0x12345678;
 put(object+4,uint32_t(0x484058));
 std::mt19937 random(0x2bd698);std::uniform_real_distribution<float> value(-10000,10000),factor(-2,2);
 for(unsigned n=0;n<20000;n++){
  ssx::OriginalEnvironmentProperties s;std::array<float,19> incoming;
  for(unsigned k=0;k<19;k++){s.current[k]=value(random);s.target[k]=value(random);incoming[k]=value(random);put(object+8+k*8,s.current[k]);put(object+12+k*8,s.target[k]);put(incomingAddress+4+k*4,incoming[k]);}
  float weight=n%5==0?0:n%5==1?1:factor(random);
  R5900Context c{};c.pc=0x2bd698;c.vu0_vf[0]=_mm_set_ps(1,0,0,0);SET_GPR_U32((&c),4,object);SET_GPR_U32((&c),5,incomingAddress);SET_GPR_U32((&c),28,0x4a30f0);SET_GPR_U32((&c),29,0x40000);SET_GPR_U32((&c),31,done);c.f[12]=weight;calls=0;
  ssx::OriginalRounding rounding;sub_002BCBD0_0x2bcbd0(m.data(),&c,&rt);ssx::originalEnvironmentPropertiesBlend(s,incoming,weight);
  if(c.pc!=done||calls!=2)return 3;
  for(unsigned k=0;k<19;k++)if(bits(object+8+k*8)!=std::bit_cast<uint32_t>(s.current[k])||bits(object+12+k*8)!=std::bit_cast<uint32_t>(s.target[k])){std::cerr<<"Environment blend mismatch "<<n<<":"<<k<<'\n';return 4;}
 }
 //Compare the actual captured quadtree across its domain and borders.
 const unsigned tree=bits(0x122bb04),nodesAddress=bits(tree+0x20),nodeCount=bits(tree+0xc);
 ssx::OriginalEnvironmentRegions regions;regions.scale=std::bit_cast<float>(bits(tree));regions.originX=std::bit_cast<float>(bits(tree+4));regions.originY=std::bit_cast<float>(bits(tree+8));regions.root=bits(tree+0x14)&0xffff;regions.outside=std::bit_cast<int32_t>(bits(tree+0x1c));regions.nodes.resize(nodeCount);
 std::memcpy(regions.nodes.data(),m.data()+nodesAddress,nodeCount*8);
 std::uniform_real_distribution<float> coord(-1000,34000);
 unsigned outsideCount=0;std::array<unsigned,4> leafCounts{};
 for(unsigned n=0;n<50000;n++){
  float gx=coord(random),gy=coord(random);
  if(n<12){constexpr float edges[]={-1,-.5f,0,.5f,32767,32768};gx=edges[n%6];gy=n<6?100:32767;}
  float x=regions.originX+gx/regions.scale,y=regions.originY+gy/regions.scale;
  R5900Context c{};c.pc=0x2c1cd8;SET_GPR_U32((&c),4,tree);SET_GPR_U32((&c),31,done);c.f[12]=x;c.f[13]=y;
  ssx::OriginalRounding rounding;sub_002C1CD8_0x2c1cd8(m.data(),&c,&rt);
  if(c.pc!=done||!GPR_U32((&c),2))return 6;
  int32_t actual=std::bit_cast<int32_t>(bits(GPR_U32((&c),2)+4));auto expected=ssx::originalEnvironmentRegionAt(regions,x,y);
  if(actual!=expected){std::cerr<<"Region mismatch "<<n<<" source "<<actual<<" native "<<expected<<'\n';return 7;}
  if(expected<0)++outsideCount;else if(expected<4)++leafCounts[expected];
 }
 std::cout<<"50,000 original spatial lookups match; outside "<<outsideCount<<" leaves "<<leafCounts[0]<<","<<leafCounts[1]<<","<<leafCounts[2]<<'\n';
 // Execute the real wrapper/region lookup on the untouched captured game state.
 rt.registerFunction(0x2c0a10,selectionCapture);rt.registerFunction(0x2baf90,sub_002BAF90_0x2baf90);rt.registerFunction(0x2c1cd8,sub_002C1CD8_0x2c1cd8);
 for(unsigned pc:{0x2bd698u,0x2bdee0u,0x2be258u})rt.registerFunction(pc,sub_002BCBD0_0x2bcbd0);
 rt.registerFunction(0x2bd698,blendCapture);
 auto scalar=[&](unsigned at){return std::bit_cast<float>(bits(at));};
 const unsigned rider=0x14701a0,wrapper=bits(0x4fa390),properties=bits(wrapper);
 R5900Context c{};c.pc=0x2c0778;c.vu0_vf[0]=_mm_set_ps(1,0,0,0);SET_GPR_U32((&c),4,wrapper);SET_GPR_U32((&c),28,0x4a30f0);SET_GPR_U32((&c),29,0x40000);SET_GPR_U32((&c),31,done);
 c.f[12]=scalar(rider+0x460);c.f[13]=scalar(rider+0x464);c.f[14]=scalar(0x4a30f0-0x7910);
 {ssx::OriginalRounding rounding;sub_002C0408_0x2c0408(m.data(),&c,&rt);}
 if(c.pc!=done){std::cerr<<"Environment wrapper stopped at "<<std::hex<<c.pc<<'\n';return 5;}
 std::cout<<"Original Snow Jam environment update: property "<<scalar(properties+0x30)<<" target "<<scalar(properties+0x34)<<" distance "<<scalar(properties)<<" coordinates "<<scalar(wrapper+8)<<","<<scalar(wrapper+12)<<'\n';
 //Compose lookup and transitions across all three real regions, including outside.
 rt.registerFunction(0x2c0a10,sub_002C0A10_0x2c0a10);rt.registerFunction(0x2bd698,sub_002BCBD0_0x2bcbd0);
 std::vector<ssx::OriginalEnvironmentPayload> payloads;
 const unsigned table=bits(0x122bb04+8);
 for(unsigned i=0;i<3;i++){unsigned pointer=bits(table+i*8+4);ssx::OriginalEnvironmentPayload p;p.transition=scalar(pointer);for(unsigned k=0;k<19;k++)p.values[k]=scalar(pointer+4+k*4);payloads.push_back(p);}
 std::array<float,19> defaults={0,6,0,0,0,10,1,1,0,0,0,1,1,1,1,scalar(0x4a30f0-0x4290),1,1,1};
 unsigned changed=0;
 for(unsigned n=0;n<10000;n++){
  ssx::OriginalEnvironmentTransition native;for(unsigned k=0;k<19;k++){native.properties.current[k]=value(random);native.properties.target[k]=value(random);put(properties+8+k*8,native.properties.current[k]);put(properties+12+k*8,native.properties.target[k]);}
  native.distance=n%11==0?-99999.f:std::abs(value(random));native.lastX=regions.originX+coord(random)/regions.scale;native.lastY=regions.originY+coord(random)/regions.scale;
  put(properties,native.distance);put(wrapper+8,native.lastX);put(wrapper+12,native.lastY);
  float x=regions.originX+coord(random)/regions.scale,y=regions.originY+coord(random)/regions.scale,weight=n%7==0?.25f:-99999.f;
  R5900Context c{};c.pc=0x2c0778;c.vu0_vf[0]=_mm_set_ps(1,0,0,0);SET_GPR_U32(&c,4,wrapper);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,29,0x40000);SET_GPR_U32(&c,31,done);c.f[12]=x;c.f[13]=y;c.f[14]=weight;
  ssx::OriginalRounding rounding;sub_002C0408_0x2c0408(m.data(),&c,&rt);ssx::originalEnvironmentTransitionStep(native,regions,payloads,defaults,x,y,weight);
  if(c.pc!=done||bits(properties)!=std::bit_cast<uint32_t>(native.distance)||bits(wrapper+8)!=std::bit_cast<uint32_t>(native.lastX)||bits(wrapper+12)!=std::bit_cast<uint32_t>(native.lastY)){std::cerr<<"Environment transition state mismatch "<<n<<'\n';return 8;}
  for(unsigned k=0;k<19;k++)if(bits(properties+8+k*8)!=std::bit_cast<uint32_t>(native.properties.current[k])||bits(properties+12+k*8)!=std::bit_cast<uint32_t>(native.properties.target[k])){std::cerr<<"Environment transition property mismatch "<<n<<":"<<k<<'\n';return 9;}
  ++changed;
 }
 std::cout<<changed<<" original2C0778 complete region/property transitions match\n";
 std::cout<<"20,000 original2BD698 environment blends match all19 current/target pairs and hook count\n";
}
