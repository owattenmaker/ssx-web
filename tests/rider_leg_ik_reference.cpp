#include "ps2_runtime_macros.h"
#include "../engine/rider_pose_motion.hpp"
#include "../engine/terrain_contact_math.hpp"
#include <fstream>
#include <cstring>
#include <iostream>
#include <random>
#define FN(name) void sub_##name(uint8_t*,R5900Context*,PS2Runtime*);
FN(0011F3D8_0x11f3d8) FN(0031BCB0_0x31bcb0) FN(0031BF60_0x31bf60) FN(0031BE50_0x31be50) FN(0031C128_0x31c128)
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x320000,g_ps2RecompiledFunctionTableSlotCount=0x88000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0x88000]={};
using namespace ssx;using V=AnimationVector;using Q=AnimationQuaternion;
template<class T>void write(uint8_t*m,uint32_t p,const T&v){std::memcpy(m+p,&v,sizeof(v));}
template<class T>T read(uint8_t*m,uint32_t p){T v;std::memcpy(&v,m+p,sizeof(v));return v;}
int main(int argc,char**argv){if(argc!=2)return 1;PS2Runtime runtime;std::vector<uint8_t> memory(32*1024*1024);auto*m=memory.data();std::ifstream f(argv[1],std::ios::binary);f.read((char*)m,memory.size());if(!f)return 2;
#define REG(pc,name) runtime.registerFunction(pc,sub_##name);
 REG(0x31BCB0,0031BCB0_0x31bcb0) REG(0x31BF60,0031BF60_0x31bf60) REG(0x31BE50,0031BE50_0x31be50) REG(0x31C128,0031C128_0x31c128)
 constexpr uint32_t actor=0x20000,geometry=0x30000,worldAt=0x40000,localAt=0x45000,legAt=0x50000,targetAt=0x60000,stack=0x10000;
 write(m,actor+0x780,geometry);write(m,geometry+0x2c,worldAt);write(m,geometry+0x24,localAt);
 RiderLegBinding leg;leg.thigh=0;leg.shin=1;leg.foot=2;write(m,legAt,0);write(m,legAt+4,1);write(m,legAt+8,2);
 auto poseWrite=[&](uint32_t p,AnimationTransform t){write(m,p,t.position);write(m,p+12,1.f);write(m,p+16,t.rotation);};
 std::mt19937 rng(0x4c454749);std::uniform_real_distribution<float> unit(-1,1),range(0,1);
 auto quaternion=[&](){Q q;float squared=0;for(auto&x:q){x=unit(rng);squared+=x*x;}float inv=1/std::sqrt(squared);for(auto&x:q)x*=inv;return q;};
 for(int n=0;n<30000;++n){terrain_original::Rounding rounding;std::vector<AnimationTransform> world(3),local(3);V scale{.8f+range(rng)*.4f,.8f+range(rng)*.4f,.8f+range(rng)*.4f};float weight=n%10==0?0:n%10==1?1:range(rng);
  world[0].position={unit(rng)*10000,unit(rng)*10000,unit(rng)*10000};world[0].rotation=quaternion();for(int i=1;i<3;++i){world[i].position=world[0].position;world[i].position[0]+=20+20*i;world[i].position[1]+=unit(rng)*20;world[i].rotation=quaternion();local[i].position={30+range(rng)*30,0,0};}
  AnimationTransform target;target.position=world[0].position;for(auto&x:target.position)x+=unit(rng)*60;target.rotation=n%4?quaternion():world[2].rotation;if(n%8==0)for(auto&x:target.rotation)x=-x;
  for(int i=0;i<3;++i){poseWrite(worldAt+i*32,world[i]);write(m,localAt+i*16,local[i].position);write(m,localAt+i*16+12,1.f);}write(m,geometry+0x140,scale);write(m,geometry+0x14c,1.f);poseWrite(targetAt,target);
  R5900Context c{};c.pc=0x11f3d8;c.vu0_vf[0]=_mm_set_ps(1,0,0,0);SET_GPR_U32(&c,4,actor);SET_GPR_U32(&c,5,legAt);SET_GPR_U32(&c,6,targetAt);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,29,stack);SET_GPR_U32(&c,31,0x12345678);c.f[12]=weight;sub_0011F3D8_0x11f3d8(m,&c,&runtime);originalRiderLegContact(world,local,scale,leg,target,weight);
  if(c.pc!=0x12345678)throw std::runtime_error("Original leg routine did not return");
  for(int i=0;i<3;++i){auto p=read<V>(m,worldAt+i*32);auto q=read<Q>(m,worldAt+i*32+16);if(p!=world[i].position||q!=world[i].rotation){std::cerr<<"case "<<n<<" weight "<<weight<<" bone "<<i<<"\n";for(int k=0;k<3;++k)std::cerr<<std::hexfloat<<"p "<<p[k]<<'/'<<world[i].position[k]<<"\n";for(int k=0;k<4;++k)std::cerr<<std::hexfloat<<"q "<<q[k]<<'/'<<world[i].rotation[k]<<"\n";return 3;}}
 }
 std::cout<<"30000 complete original11F3D8 leg solves match all thigh/shin/foot positions and quaternions for weights0..1, both quaternion hemispheres and near-parallel interpolation\n";
}
