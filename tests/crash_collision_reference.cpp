#include "ps2_runtime_macros.h"
#include "../engine/crash_collision.hpp"
#include <fstream>
#include <cstring>
#include <iostream>
#include <random>
void sub_00138640_0x138640(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00137860_0x137860(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00106538_0x106538(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00329B40_0x329b40(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000;
extern const uint32_t g_ps2RecompiledFunctionTableEnd=0x340000;
extern const uint32_t g_ps2RecompiledFunctionTableSlotCount=(0x340000-0x100000)/4;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[(0x340000-0x100000)/4]={};
using namespace ssx;using namespace ssx::terrain_original;
static OriginalCrashGroundHit terrainHit;static Vector bodyNormal,surfaceVelocity,entityForce;static float depth,impactSpeed;static bool began,impact,cleared,entityCalled;static unsigned interactions;
template<class T>static void write(uint8_t*m,uint32_t p,const T&v){std::memcpy(m+p,&v,sizeof(v));}
template<class T>static T read(uint8_t*m,uint32_t p){T v;std::memcpy(&v,m+p,sizeof(v));return v;}
static void vec(uint8_t*m,uint32_t p,Vector v){write(m,p,v);write(m,p+12,0.f);}
static void external(uint8_t*m,R5900Context*c,PS2Runtime*){
    if(c->pc==0x3342d0){uint32_t p=GPR_U32(c,6);std::memset(m+p,0,0x80);vec(m,p+16,bodyNormal);vec(m,p+32,surfaceVelocity);c->f[0]=depth;}
    if(c->pc==0x138960){uint32_t p=GPR_U32(c,5);std::memset(m+p,0,0x80);vec(m,p,terrainHit.point);vec(m,p+16,terrainHit.normal);vec(m,p+32,terrainHit.surfaceVelocity);write(m,p+0x4c,terrainHit.surface);write(m,p+0x50,terrainHit.hasLiveEntity?0x65000u:0u);c->f[0]=terrainHit.fraction;}
    if(c->pc==0x1135b8)began=true;if(c->pc==0x113998){cleared=true;write(m,GPR_U32(c,4)+0x98,0.f);}if(c->pc==0x111aa0){impact=true;impactSpeed=c->f[12];}
    if(c->pc==0x105398||c->pc==0x107888)++interactions;
    if(c->pc==0x33ffe8){entityCalled=true;entityForce=read<Vector>(m,GPR_U32(c,6));}
    if(c->pc==0x33fff0)SET_GPR_U32(c,2,1);
    c->pc=GPR_U32(c,31);
}
int main(int argc,char**argv){if(argc!=2)return 1;PS2Runtime runtime;std::vector<uint8_t> memory(32*1024*1024);auto*m=memory.data();std::ifstream ee(argv[1],std::ios::binary);ee.read((char*)m,memory.size());if(!ee)return 2;
    for(uint32_t pc:{0x3342d0,0x138960,0x1135b8,0x113998,0x111aa0,0x105398,0x107888,0x11e150,0x32f650,0x32f708,0x33ffe8,0x33fff0})runtime.registerFunction(pc,external);
    runtime.registerFunction(0x106538,sub_00106538_0x106538);runtime.registerFunction(0x329b40,sub_00329B40_0x329b40);
    constexpr uint32_t actor=0x10000,state=0x20000,stack=0x30000,body=0x40000,predictor=0x50000,done=0x12345678;
    write(m,state+0x40,actor);write(m,actor+0xaa0,body);write(m,body+44,0u);write(m,actor+0x788,predictor);write(m,actor+0x77c,0x70000u);write(m,actor+0x6c0,0x71000u);write(m,0x71040,int16_t(0));write(m,0x71044,0x33fff0u);
    write(m,0x4a30f0-0x848,0x60000u);write(m,0x60010,60);write(m,0x6500c,0x65100u);write(m,0x6510c,0x65200u);write(m,0x65358,int16_t(0));write(m,0x6535c,0x33ffe8u);
    std::mt19937 random(0x43525348);std::uniform_real_distribution<float> coord(-10000,10000),speed(-2500,2500),unit(-1,1),penetration(0,80);
    auto vector=[&](auto& d){return Vector{d(random),d(random),d(random)};};auto normal=[&](){auto v=vector(unit);float r=1/std::sqrt(dot(v,v));for(auto&x:v)x*=r;return v;};
    for(unsigned n=0;n<20000;++n){OriginalCrashActorState a;a.position=vector(coord);a.velocity=vector(speed);a.groundNormal=normal();OriginalCrashMotionState s;s.flag4=n%2;bodyNormal=normal();surfaceVelocity=vector(unit);depth=n%10?penetration(random):-1.f;if(n%100==0)bodyNormal={-a.groundNormal[0],-a.groundNormal[1],-a.groundNormal[2]};
        vec(m,actor+0x110,a.position);vec(m,actor+0x1e0,a.velocity);vec(m,actor+0x370,a.groundNormal);write(m,state+4,s.flag4);write(m,state,0);interactions=0;
        R5900Context c{};c.pc=0x138640;c.vu0_vf[0]=_mm_set_ps(1,0,0,0);SET_GPR_U32(&c,4,state);SET_GPR_U32(&c,29,stack);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,31,done);Rounding rounding;sub_00138640_0x138640(m,&c,&runtime);
        auto response=originalCrashSlidingBodyResponse(depth,a.groundNormal,bodyNormal,a.velocity,surfaceVelocity);if(response.moved)for(unsigned k=0;k<3;++k)a.position[k]=add(a.position[k],response.translationCm[k]);a.velocity=response.velocityCmps;originalCrashSlidingFinish(s,a);
        if(c.pc!=done||read<Vector>(m,actor+0x110)!=a.position||read<Vector>(m,actor+0x1e0)!=a.velocity||read<int>(m,state)!=s.submode||read<int>(m,state+4)!=s.flag4||interactions!=2)throw std::runtime_error("Sliding crash body response mismatch "+std::to_string(n));
    }
    std::cout<<"20000 complete original sliding crash body responses and post-interaction transitions match exactly\n";
    unsigned landings=0,pushes=0,starts=0;
    for(unsigned n=0;n<20000;++n){OriginalCrashActorState a;a.position=vector(coord);a.velocity=vector(speed);a.groundNormal={0,0,1};OriginalCrashMotionState s;s.submode=1;bodyNormal=normal();surfaceVelocity=vector(unit);depth=n%10?penetration(random):-1.f;terrainHit={};terrainHit.fraction=n%7?float(n%10)*.1f:-1.f;terrainHit.normal=normal();terrainHit.point=a.position;for(unsigned k=0;k<3;++k)terrainHit.point[k]=add(terrainHit.point[k],mul(terrainHit.normal[k],float(int(n%9)-4)*20));terrainHit.surfaceVelocity=vector(unit);terrainHit.surface=n%19;terrainHit.hasLiveEntity=n%3==0;
        vec(m,actor+0x110,a.position);vec(m,actor+0x1e0,a.velocity);vec(m,actor+0x370,a.groundNormal);write(m,state,1);write(m,0x70314,0);write(m,predictor+0x98,1.f);began=impact=cleared=entityCalled=false;interactions=0;
        R5900Context c{};c.pc=0x137860;c.vu0_vf[0]=_mm_set_ps(1,0,0,0);SET_GPR_U32(&c,4,state);SET_GPR_U32(&c,29,stack);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,31,done);Rounding rounding;sub_00137860_0x137860(m,&c,&runtime);
        auto result=originalCrashAirBodyResponse(s,a,depth,bodyNormal,terrainHit,60);
        if(c.pc!=done||read<Vector>(m,actor+0x110)!=a.position||read<Vector>(m,actor+0x1e0)!=a.velocity||read<int>(m,state)!=s.submode||began!=result.beginPredictor||impact!=result.impact||cleared!=result.landed||entityCalled!=result.dynamicCallbackRequired||interactions!=2||(impact&&(impactSpeed!=result.impactSpeed||read<int>(m,0x70314)!=1||read<Vector>(m,0x70320)!=result.impactVelocity))||(result.landed&&(read<Vector>(m,actor+0x370)!=a.groundNormal||read<Vector>(m,actor+0x3d0)!=a.surfaceVelocity||read<Vector>(m,actor+0x460)!=a.contactPoint||read<int>(m,actor+0x438)!=a.surface))||(entityCalled&&entityForce!=result.entityForce)){std::cerr<<"Air crash collision mismatch "<<n<<" position "<<(read<Vector>(m,actor+0x110)==a.position)<<" velocity "<<(read<Vector>(m,actor+0x1e0)==a.velocity)<<" landed "<<cleared<<'/'<<result.landed<<'\n';return 3;}
        landings+=result.landed;pushes+=result.moved;starts+=result.beginPredictor;
    }
    std::cout<<"20000 complete original airborne crash body responses match exactly; "<<landings<<" landings, "<<pushes<<" pushes, "<<starts<<" predictor restarts; collision flags, impact velocity and moving entity force exact\n";
}
