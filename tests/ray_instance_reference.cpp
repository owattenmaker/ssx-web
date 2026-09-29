#include "ps2_runtime_macros.h"
#include "../engine/ray_instance_collision.hpp"
#include <fstream>
#include <cstdio>
#include <random>
void sub_0032E100_0x32e100(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0032B6A8_0x32b6a8(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x340000,g_ps2RecompiledFunctionTableSlotCount=0x90000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0x90000]={};
int main(int argc,char**argv){
 using namespace ssx;using namespace terrain_original;Rounding round;
 PS2Runtime rt;rt.memory().initialize();std::vector<uint8_t> m(32*1024*1024);std::ifstream ee(argv[1],std::ios::binary);ee.read((char*)m.data(),m.size());std::ifstream vu(argv[2],std::ios::binary);vu.read((char*)rt.memory().getVU0Code(),4096);if(!ee||!vu)return 2;
 rt.registerFunction(0x32e4d0,sub_0032E100_0x32e100);rt.registerFunction(0x32b6a8,sub_0032B6A8_0x32b6a8);
 auto wr=[&](unsigned a,auto x){std::memcpy(m.data()+a,&x,sizeof(x));};auto rd=[&](unsigned a){float x;std::memcpy(&x,m.data()+a,4);return x;};auto vw=[&](unsigned a,Vector x,float w=0){wr(a,x);wr(a+12,w);};
 auto context=[&](unsigned pc){R5900Context c{};c.pc=pc;c.vu0_vf[0]=_mm_set_ps(1,0,0,0);SET_GPR_U32((&c),28,0x4a30f0);SET_GPR_U32((&c),29,0x10000);SET_GPR_U32((&c),31,0x12345678);SET_GPR_U32((&c),4,0x30000);SET_GPR_U32((&c),5,0x40000);SET_GPR_U32((&c),6,0x50000);SET_GPR_U32((&c),7,0x60000);return c;};
 auto check=[&](unsigned at,Vector value,unsigned k,const char* name){for(int i=0;i<3;i++)if(rd(at+i*4)!=value[i]){printf("%s mismatch %u/%d %.9g vs %.9g\n",name,k,i,rd(at+i*4),value[i]);exit(3);}};
 std::mt19937 rng(0x32e690);std::uniform_real_distribution<float> f(-10,10);auto vector=[&](){return Vector{f(rng),f(rng),f(rng)};};unsigned contacts=0,triangles=0;
 for(unsigned k=0;k<20000;k++){
  ray_instance::Ray ray{vector(),vector()};auto low=vector(),high=vector();for(int i=0;i<3;i++)if(low[i]>high[i])std::swap(low[i],high[i]);
  if(k%7==0)ray.delta[k%3]=0;if(k%13==0)ray.origin=low;if(k%17==0){ray.origin={0,0,0};low={-1,-1,-1};high={1,1,1};}
  vw(0x30060,ray.origin,1);vw(0x30070,ray.delta);vw(0x40000,low);vw(0x50000,high);
  auto c=context(0x32e288);sub_0032E100_0x32e100(m.data(),&c,&rt);if(bool(GPR_U32((&c),2))!=ray_instance::overlaps(ray,low,high)){printf("bounds %u\n",k);return 4;}
  c=context(0x32e690);sub_0032E100_0x32e100(m.data(),&c,&rt);auto box=ray_instance::box(ray,low,high);if(GPR_U32((&c),2)!=box.count){printf("box count %u %u vs %u\n",k,GPR_U32((&c),2),box.count);return 5;}
  for(unsigned i=0;i<box.count;i++){unsigned out=0x60000+i*128;check(out,box.hits[i].point,k,"box point");check(out+16,box.hits[i].normal,k,"box normal");if(rd(out+64)!=box.hits[i].fraction){puts("box fraction");return 6;}++contacts;}
  collision_transform::Matrix matrix;for(float& x:matrix)x=f(rng);matrix[3]=matrix[7]=matrix[11]=0;matrix[15]=1;float scale=std::abs(f(rng))+.1f;
  vw(0x30080,ray.origin,1);vw(0x30090,ray.delta);wr(0x40000,matrix);c=context(0x32e398);c.f[12]=scale;sub_0032E100_0x32e100(m.data(),&c,&rt);auto transformed=ray_instance::transform(ray,matrix,scale);check(0x30060,transformed.origin,k,"origin transform");check(0x30070,transformed.delta,k,"delta transform");
  c=context(0x32e688);sub_0032E100_0x32e100(m.data(),&c,&rt);if(GPR_U32((&c),2)!=ray_instance::sphereTreeContactCount())return 7;
  Vector a=vector(),b=vector(),d=vector(),normal=cross(difference(a,b),difference(b,d));float inverse=1.f/std::sqrt(dot(normal,normal));for(float& x:normal)x=x*inverse;
  if(k%2==0){Vector center;for(int i=0;i<3;i++)center[i]=((a[i]+b[i])+d[i])*(1.f/3);ray.origin=center;for(int i=0;i<3;i++){ray.origin[i]=ray.origin[i]+normal[i]*10;ray.delta[i]=normal[i]*-20;}}
  vw(0x30060,ray.origin,1);vw(0x30070,ray.delta);wr(0x30050,uint32_t(0x48e6a8));vw(0x40000,a,1);vw(0x50000,b,1);vw(0x60000,d,1);vw(0x70000,normal);
  c=context(k%3?0x32e4d0:0x32e5e8);SET_GPR_U32((&c),8,0x70000);SET_GPR_U32((&c),9,0x80000);SET_GPR_U32((&c),10,0x80010);SET_GPR_U32((&c),11,0x80020);sub_0032E100_0x32e100(m.data(),&c,&rt);
  auto triangle=ray_instance::triangle(ray,a,b,d,normal,k%3==0);if(bool(GPR_U32((&c),2))!=triangle.has_value()){printf("triangle bool %u\n",k);return 8;}
  if(triangle){check(0x80000,triangle->point,k,"triangle point");check(0x80010,triangle->normal,k,"triangle normal");if(rd(0x80020)!=triangle->fraction)return 9;++triangles;}
 }
 printf("20000 each original ray box/bounds/transform/triangle/sphere-stub cases match exactly (%u box contacts,%u triangle contacts)\n",contacts,triangles);
}
