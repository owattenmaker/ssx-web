#include "ps2_runtime_macros.h"
#include "../engine/crash_control.hpp"
#include <fstream>
#include <cstring>
#include <iostream>
#include <random>
void sub_0012CA30_0x12ca30(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0012CD20_0x12cd20(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0012DA88_0x12da88(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00136D40_0x136d40(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00136DE0_0x136de0(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0012CB68_0x12cb68(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0012E528_0x12e528(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0012D848_0x12d848(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0012D9D8_0x12d9d8(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000;
extern const uint32_t g_ps2RecompiledFunctionTableEnd=0x340000;
extern const uint32_t g_ps2RecompiledFunctionTableSlotCount=(0x340000-0x100000)/4;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[(0x340000-0x100000)/4]={};
using namespace ssx;using namespace ssx::terrain_original;
struct RecoveryCall{int kind=0,value=0;float scalar=0;bool operator==(const RecoveryCall&)const=default;};
static bool recoveryActive=false;static std::vector<RecoveryCall> actualRecovery,expectedRecovery;
static OriginalCrashClipState clip;static bool began,continuation,observer,rateUpdate;static int played;static bool meterChanged,meterReached;static float meterValue;
template<class T>static void write(uint8_t*m,uint32_t p,T v){std::memcpy(m+p,&v,sizeof(v));}
template<class T>static T read(uint8_t*m,uint32_t p){T v;std::memcpy(&v,m+p,sizeof(v));return v;}
static void vec(uint8_t*m,uint32_t p,Vector v){write(m,p,v);write(m,p+12,0.f);}
static void pose(uint8_t*m,uint32_t p,AnimationTransform t){vec(m,p,t.position);write(m,p+16,t.rotation);}
static AnimationTransform poseRead(uint8_t*m,uint32_t p){return {read<Vector>(m,p),read<AnimationQuaternion>(m,p+16)};}
static void external(uint8_t*,R5900Context*c,PS2Runtime*){
    if(recoveryActive){int kind=0,value=0;float scalar=0;switch(c->pc){case 0x33fff0:kind=1;scalar=c->f[12];break;case 0x2948d0:kind=2;break;case 0x33fff8:kind=3;scalar=c->f[12];break;case 0x3128e8:kind=4;value=GPR_U32(c,5);break;case 0x11fec8:kind=5;value=GPR_U32(c,5);break;case 0x11fe78:kind=6;value=GPR_U32(c,5);break;case 0x119e38:kind=7;value=GPR_U32(c,5);break;case 0x10f280:kind=8;value=GPR_U32(c,5);break;case 0x116120:kind=9;value=GPR_U32(c,6);break;}if(kind)actualRecovery.push_back({kind,value,scalar});}
    switch(c->pc){case 0x311ae8:SET_GPR_U32(c,2,clip.animationClass);break;case 0x312aa0:SET_GPR_U32(c,2,clip.semantic);break;
    case 0x312ab0:c->f[0]=clip.progress;break;case 0x312ae8:SET_GPR_U32(c,2,clip.complete);break;case 0x311b20:SET_GPR_U32(c,2,0x60000);break;
    case 0x1135b8:began=true;break;case 0x12da88:continuation=true;break;case 0x296868:case 0x296e80:observer=true;break;
    case 0x29e970:meterChanged=meterReached=true;break;case 0x29e590:meterChanged=true;meterValue=c->f[12];break;
    case 0x3128e8:played=GPR_U32(c,5);break;case 0x12e528:rateUpdate=true;break;default:SET_GPR_U32(c,2,0);break;}c->pc=GPR_U32(c,31);
}
int main(int argc,char**argv){if(argc!=2)return 1;PS2Runtime runtime;std::vector<uint8_t> memory(32*1024*1024);auto*m=memory.data();std::ifstream ee(argv[1],std::ios::binary);ee.read((char*)m,memory.size());if(!ee)return 2;
    for(uint32_t pc:{0x311ae8,0x312aa0,0x312ab0,0x312ae8,0x311b20,0x1135b8,0x12da88,0x296868,0x296e80,0x3128e8,0x12e528,0x28b180,0x29e578,0x2708f0,0x309990})runtime.registerFunction(pc,external);
    runtime.registerFunction(0x136d40,sub_00136D40_0x136d40);runtime.registerFunction(0x136de0,sub_00136DE0_0x136de0);
    constexpr uint32_t actor=0x10000,control=0x20000,motion=0x30030,geometry=0x40000,bones=0x41000,animation=0x50000,stack=0x70000,done=0x12345678;
    write(m,control+0x80,actor);write(m,motion+0x40,actor);write(m,actor+0x780,geometry);write(m,geometry+0x2c,bones);write(m,actor+0x89c,0u);write(m,actor+0x8a4,1u);write(m,actor+0x77c,motion-0x30);write(m,actor+0x784,animation);
    std::mt19937 random(0x434f4e54);std::uniform_real_distribution<float> coord(-10000,10000),vel(-1500,1500),unit(-1,1),positive(.1f,1);
    auto vector=[&](auto&d){return Vector{d(random),d(random),d(random)};};auto transform=[&](){AnimationTransform p;p.position=vector(coord);p.rotation={unit(random),unit(random),unit(random),unit(random)};float l=0;for(auto x:p.rotation)l+=x*x;l=std::sqrt(l);for(auto&x:p.rotation)x/=l;return p;};
    auto context=[&](uint32_t pc){R5900Context c{};c.pc=pc;c.vu0_vf[0]=_mm_set_ps(1,0,0,0);SET_GPR_U32(&c,4,control);SET_GPR_U32(&c,29,stack);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,31,done);return c;};
    for(unsigned n=0;n<20000;++n){OriginalCrashControlState s;s.previousProgress=.123f;s.impactPending54=4;s.recovery70=.4f;OriginalCrashMotionState motionState;motionState.submode=n%2;OriginalCrashActorState a;a.velocity=vector(vel);a.groundNormal=vector(unit);auto first=transform(),second=transform();auto offset=vector(unit);clip.animationClass=n%3?27:22;
        write(m,control+0x50,s.previousProgress);write(m,control+0x54,s.impactPending54);write(m,control+0x70,s.recovery70);pose(m,bones,first);pose(m,bones+32,second);vec(m,actor+0x9d0,offset);vec(m,actor+0x1e0,a.velocity);vec(m,actor+0x370,a.groundNormal);write(m,actor+0x150,0);write(m,actor+0x2e8,1.f);write(m,actor+0x2ec,2.f);write(m,actor+0x318,1.f);write(m,motion,motionState.submode);auto c=context(0x12ca30);Rounding rounding;sub_0012CA30_0x12ca30(m,&c,&runtime);auto result=originalCrashControlBegin(s,motionState,a,first,second,offset,clip.animationClass);
        auto p=poseRead(m,control+16),q=poseRead(m,control+48);if(c.pc!=done||read<int>(m,control)!=s.phase||p.position!=s.previousPrimary.position||p.rotation!=s.previousPrimary.rotation||q.position!=s.previousSecondary.position||q.rotation!=s.previousSecondary.rotation||read<float>(m,control+0x50)!=s.previousProgress||read<float>(m,control+0x54)!=0||read<float>(m,control+0x70)!=0||read<int>(m,actor+0x2e8)||read<int>(m,actor+0x2ec)||bool(read<int>(m,actor+0x150))!=a.detached||(a.detached&&(read<Vector>(m,actor+0x130)!=a.detachedPosition||read<AnimationQuaternion>(m,actor+0x140)!=a.detachedQuaternion||read<Vector>(m,motion+16)!=motionState.detachedVelocity||read<Vector>(m,motion+32)!=motionState.detachedAngularVelocity||read<float>(m,actor+0x318)!=0)))throw std::runtime_error("Crash control entry mismatch");
    }
    std::cout<<"20000 original control8 entry cases match posed anchors, boost-window resets and class22 detachment exactly\n";
    unsigned completions=0;
    for(unsigned n=0;n<20000;++n){OriginalCrashControlState s;s.previousPrimary=transform();s.previousSecondary=transform();s.previousProgress=positive(random)*.9f;OriginalCrashMotionState motionState;motionState.submode=n%2;motionState.flag4=1;OriginalCrashActorState a;a.velocity=vector(vel);a.groundNormal=vector(unit);
        clip={};clip.progress=positive(random);clip.complete=n%2;clip.duration10=positive(random)*4;clip.speed90=positive(random)*2;clip.controllerScale1C=positive(random);clip.primary=transform();clip.secondary=transform();clip.animationClass=n%3==0?23:(n%3==1?27:29);clip.semantic=328+n%34;if(clip.complete)clip.progress=1;
        write(m,control,0);pose(m,control+16,s.previousPrimary);pose(m,control+48,s.previousSecondary);write(m,control+0x50,s.previousProgress);pose(m,bones,clip.primary);pose(m,bones+32,clip.secondary);write(m,0x60010,clip.duration10);write(m,0x60090,clip.speed90);write(m,animation+0x1c,clip.controllerScale1C);vec(m,actor+0x1e0,a.velocity);vec(m,actor+0x370,a.groundNormal);write(m,actor+0x150,0);write(m,motion,motionState.submode);write(m,motion+4,1);began=continuation=observer=false;auto c=context(0x12cd20);Rounding rounding;sub_0012CD20_0x12cd20(m,&c,&runtime);auto result=originalCrashInitialControlStep(s,motionState,a,clip);
        auto p=poseRead(m,control+16),q=poseRead(m,control+48);if(c.pc!=done||read<int>(m,control)!=s.phase||p.position!=s.previousPrimary.position||p.rotation!=s.previousPrimary.rotation||q.position!=s.previousSecondary.position||q.rotation!=s.previousSecondary.rotation||read<float>(m,control+0x50)!=s.previousProgress||read<Vector>(m,actor+0x1e0)!=a.velocity||bool(read<int>(m,actor+0x150))!=a.detached||began!=result.beginPredictor||continuation!=clip.complete||observer!=result.groundContinuationObserverRequired||(clip.complete&&(read<Vector>(m,motion+48)!=motionState.angularVelocity||read<int>(m,motion+4)!=0))||(a.detached&&(read<Vector>(m,motion+16)!=motionState.detachedVelocity||read<Vector>(m,motion+32)!=motionState.detachedAngularVelocity||read<Vector>(m,actor+0x130)!=a.detachedPosition||read<AnimationQuaternion>(m,actor+0x140)!=a.detachedQuaternion))){std::cerr<<"Crash initial control mismatch "<<n<<" phase "<<read<int>(m,control)<<'/'<<s.phase<<" angular "<<(read<Vector>(m,motion+48)==motionState.angularVelocity)<<" detached "<<a.detached<<" detachedVelocity "<<(read<Vector>(m,motion+16)==motionState.detachedVelocity)<<" detachedAngular "<<(read<Vector>(m,motion+32)==motionState.detachedAngularVelocity)<<" detachedPosition "<<(read<Vector>(m,actor+0x130)==a.detachedPosition)<<" detachedQ "<<(read<AnimationQuaternion>(m,actor+0x140)==a.detachedQuaternion)<<'\n';return 3;}completions+=clip.complete;
    }
    std::cout<<"20000 original initial crash-control steps match; "<<completions<<" clip completions with pose-derived angular release and continuation requests\n";
    for(int semantic=300;semantic<400;++semantic)for(int phase:{1,2}){clip.semantic=semantic;write(m,control,phase);played=-1;rateUpdate=false;auto c=context(0x12da88);Rounding rounding;sub_0012DA88_0x12da88(m,&c,&runtime);int expected=originalCrashContinuationSemantic(semantic,phase==1);if(c.pc!=done||rateUpdate!=(expected!=semantic)||played!=(expected==semantic?-1:expected))throw std::runtime_error("Crash continuation table mismatch");}
    std::cout<<"200 original crash continuation semantic mappings match both air/ground choices and playback-rate callback ordering\n";
    for(uint32_t pc:{0x114130,0x116120,0x12cd20,0x12d4e8,0x12d160,0x12d848,0x12d9d8,0x29e970,0x29e590})runtime.registerFunction(pc,external);
    for(unsigned n=0;n<20000;++n){OriginalCrashControlState s;s.phase=random()%6;s.recovery70=unit(random)*2;uint32_t command=random()%2?0x2000:0;int category=int(random()%5)-1;write(m,control,s.phase);write(m,control+0x70,s.recovery70);write(m,actor+0xb20,category);write(m,0x65000,command);meterChanged=meterReached=false;auto c=context(0x12cb68);SET_GPR_U32(&c,5,0x65000);Rounding rounding;sub_0012CB68_0x12cb68(m,&c,&runtime);auto result=originalCrashRecoveryMeter(s,command,category);
        if(c.pc!=done||read<float>(m,control+0x70)!=s.recovery70||meterChanged!=result.changedByInput||meterReached!=result.reachedRecovery||(meterChanged&&!meterReached&&meterValue!=result.value))throw std::runtime_error("Crash recovery meter mismatch");}
    std::cout<<"20000 original control8 recovery meter updates and feedback gates match exactly\n";
    for(unsigned n=0;n<20000;++n){Vector angular=vector(unit);for(auto& x:angular)x*=20;float duration=positive(random)*4,recovery=unit(random)*2,permission=unit(random);clip.animationClass=n%3==0?25:(n%3==1?29:27);vec(m,motion+48,angular);write(m,0x60010,duration);write(m,control+0x70,recovery);write(m,actor+0x470,permission);auto c=context(0x12e528);Rounding rounding;sub_0012E528_0x12e528(m,&c,&runtime);auto result=originalCrashPlaybackRate(angular,duration,clip.animationClass,recovery,permission);
        if(c.pc!=done||c.f[0]!=result.base||read<float>(m,0x60090)!=result.applied)throw std::runtime_error("Crash playback rate mismatch");}
    std::cout<<"20000 original crash continuation playback rates match both returned base and applied sequence speed\n";

    recoveryActive=true;for(uint32_t pc:{0x33fff0,0x33fff8,0x2948d0,0x11fec8,0x11fe78,0x119e38,0x10f280})runtime.registerFunction(pc,external);
    write(m,actor+0x6c0,0x72000u);write(m,0x72088,int16_t(0));write(m,0x7208c,0x33fff0u);write(m,0x72090,int16_t(0));write(m,0x72094,0x33fff8u);
    OriginalCrashRecoveryCallbacks cb;cb.reportImpact=[&](float x){expectedRecovery.push_back({1,0,x});};cb.stopCrashEffect=[&](){expectedRecovery.push_back({2});};cb.setRecoveryPresentation=[&](float x){expectedRecovery.push_back({3,0,x});};cb.playAnimation=[&](int n){expectedRecovery.push_back({4,n});};cb.enterControl=[&](int n){expectedRecovery.push_back({5,n});};cb.enterMotion=[&](int n){expectedRecovery.push_back({6,n});};cb.setAirScoringStance=[&](bool x){expectedRecovery.push_back({7,int(x)});};cb.refundCrashBoost=[&](bool x){expectedRecovery.push_back({8,int(x)});};cb.requestReset=[&](int n){expectedRecovery.push_back({9,n});};
    for(unsigned n=0;n<20000;++n){OriginalCrashControlState s;s.phase=3;s.impactPending54=random()%2;s.impactVelocity60=vector(vel);int submode=random()%2;bool stance=random()%2;clip.progress=positive(random);clip.complete=random()%2;write(m,control,3);write(m,control+0x54,s.impactPending54);vec(m,control+0x60,s.impactVelocity60);write(m,motion,submode);write(m,actor+0x320,int(stance));write(m,actor+0x324,0);actualRecovery.clear();expectedRecovery.clear();auto c=context(0x12d848);Rounding rounding;sub_0012D848_0x12d848(m,&c,&runtime);originalCrashGetUpStep(s,submode,clip.progress,clip.complete,stance,cb);if(c.pc!=done||read<int>(m,control+0x54)!=s.impactPending54||actualRecovery!=expectedRecovery)throw std::runtime_error("Crash get-up callback ordering mismatch");}
    std::cout<<"20000 original crash get-up phase3 updates match impact observers, presentation gain and ordered ground/air reentry requests\n";
    for(unsigned n=0;n<20000;++n){OriginalCrashControlState s;s.phase=4;s.recovery70=unit(random)*2;float permission=unit(random);clip.complete=random()%2;write(m,control,4);write(m,control+0x70,s.recovery70);write(m,actor+0x470,permission);actualRecovery.clear();expectedRecovery.clear();auto c=context(0x12d9d8);Rounding rounding;sub_0012D9D8_0x12d9d8(m,&c,&runtime);originalCrashResetClipStep(s,permission,clip.complete,cb);if(c.pc!=done||actualRecovery!=expectedRecovery)throw std::runtime_error("Crash reset-clip callback ordering mismatch");}
    std::cout<<"20000 original crash reset-clip phase4 updates match boost refund and forced-reset ordering\n";

}
