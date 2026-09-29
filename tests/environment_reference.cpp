#include "ps2_runtime_macros.h"
#include "../engine/environment_lighting.hpp"
#include <fstream>
#include <random>
#include <cstring>
#include <cstdio>
#include <cfenv>
void sub_002EDB20_0x2edb20(uint8_t*,R5900Context*,PS2Runtime*);
void sub_002ED1D0_0x2ed1d0(uint8_t*,R5900Context*,PS2Runtime*);
void sub_002ED338_0x2ed338(uint8_t*,R5900Context*,PS2Runtime*);
void sub_003885E0_0x3885e0(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x390000,g_ps2RecompiledFunctionTableSlotCount=0xa4000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xa4000]={};
static ssx::EnvironmentColour sample(int id,float u,float v){return {.4f+u*.1f,.6f+v*.07f,.7f+u*.03f,.8f+float(id)*.001f};}
static void callback(uint8_t*m,R5900Context*c,PS2Runtime*){auto col=sample(GPR_U32(c,6),c->f[12],c->f[13]);memcpy(m+GPR_U32(c,4),col.data(),16);c->pc=GPR_U32(c,31);}
int main(int argc,char**argv){if(argc!=2)return 2;std::ifstream in(argv[1],std::ios::binary);std::vector<uint8_t>m((std::istreambuf_iterator<char>(in)),{});PS2Runtime rt;rt.registerFunction(0x3889f0,callback);
auto put=[&](unsigned at,const auto&v){memcpy(m.data()+at,&v,sizeof(v));};auto u=[&](unsigned at){uint32_t v;memcpy(&v,m.data()+at,4);return v;};constexpr unsigned patch=0x30000,output=0x40000,renderer=0x50000,table=0x60000,desc=0x70000,data=0x80000,record=0x90000,done=0x12345678;
auto context=[&](){R5900Context c{};c.vu0_vf[0]=_mm_set_ps(1,0,0,0);SET_GPR_U32((&c),28,0x4a30f0);SET_GPR_U32((&c),29,0x10000);SET_GPR_U32((&c),31,done);return c;};
std::mt19937 gen(0x2edb20);std::uniform_real_distribution<float>d(-2,2);std::fesetround(FE_TOWARDZERO);
put(renderer+0x18f4,table);put(table+8,desc);put(desc+0xc,0u);put(desc+0x1c,data);put(desc+0x38,(uint64_t(5)<<26)|(uint64_t(5)<<30));ssx::OriginalEnvironmentTexture tex;tex.width=tex.height=32;for(unsigned n=0;n<1100*4;++n)m[data+n]=gen();for(unsigned y=0;y<=32;++y)for(unsigned x=0;x<=32;++x){std::array<uint8_t,4>p;memcpy(p.data(),m.data()+data+4*(y*32+x),4);tex.rgba.push_back(p);}
for(unsigned n=0;n<20000;++n){float x=d(gen),y=d(gen);auto c=context();c.pc=0x3889f0;SET_GPR_U32((&c),4,output);SET_GPR_U32((&c),5,renderer);SET_GPR_U32((&c),6,0);SET_GPR_U32((&c),7,1);c.f[12]=x;c.f[13]=y;sub_003885E0_0x3885e0(m.data(),&c,&rt);auto native=ssx::originalEnvironmentTextureSample(tex,x,y);if(c.pc!=done||memcmp(native.data(),m.data()+output,16)){printf("texture mismatch %u uv %.9g %.9g\n",n,x,y);for(int k=0;k<4;++k)printf("%08x/%08x ",std::bit_cast<unsigned>(native[k]),u(output+k*4));puts("");return 3;}}
puts("20,000 original3889F0 texture samples bit exact");
unsigned world=u(u(0x4a30f0+0x16c8));put(world+0x24,6u);put(world+0x3f0,3u);m[patch+0x155]=0;put(patch+0x156,int16_t(0));put(0x4a30f0-0x854,renderer);put(renderer+0x10d8,0xa0000u);put(0xa0000+0x1d0,int16_t(0));put(0xa0000+0x1d4,0x3889f0u);ssx::OriginalEnvironmentGlobals globals;
for(unsigned n=0;n<20000;++n){ssx::OriginalEnvironmentPatch p;p.flags=(n&1)?(1|(5<<3)):(5|(1<<3));p.textures={0,1,-1};for(auto&v:p.lightUV)v=d(gen);for(auto&q:p.baseUV)for(auto&v:q)v=d(gen);put(patch+12,p.flags);put(patch+16,p.lightUV);put(patch+32,p.baseUV);for(int k=0;k<3;++k)put(patch+0x1a0+2*k,int16_t(p.textures[k]));float x=d(gen),y=d(gen);auto c=context();SET_GPR_U32((&c),4,patch);SET_GPR_U32((&c),5,output);SET_GPR_U32((&c),6,output+16);c.f[12]=x;c.f[13]=y;sub_002EDB20_0x2edb20(m.data(),&c,&rt);auto native=*ssx::originalEnvironmentPatchSample(p,x,y,globals.multiplier,sample);if(c.pc!=done||memcmp(&native,m.data()+output,32)){printf("patch mismatch %u\n",n);for(int k=0;k<8;++k)printf("%08x/%08x ",reinterpret_cast<unsigned*>(&native)[k],u(output+k*4));puts("");return 4;}}
puts("20,000 original2EDB20 patch colour cases bit exact");
for(unsigned n=0;n<20000;++n){ssx::OriginalEnvironmentState state,target;for(auto*p:{&state.ambient,&state.ratio,&target.ambient,&target.ratio})for(auto&v:*p)v=d(gen);put(record+0x28,state);put(output,target);float w=d(gen);auto c=context();SET_GPR_U32((&c),4,record);SET_GPR_U32((&c),5,output);SET_GPR_U32((&c),6,output+16);c.f[12]=w;sub_002ED338_0x2ed338(m.data(),&c,&rt);ssx::originalEnvironmentBlend(state,target,w);if(c.pc!=done||memcmp(&state,m.data()+record+0x28,32)){printf("blend mismatch %u\n",n);return 5;}}
puts("20,000 original2ED338 filter cases bit exact");}
