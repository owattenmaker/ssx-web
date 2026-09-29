#include "ps2_runtime_macros.h"
#include "../engine/crash_recovery.hpp"
#include <fstream>
#include <cstring>
#include <iostream>
#include <random>
void sub_0012D160_0x12d160(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0012D4E8_0x12d4e8(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000;
extern const uint32_t g_ps2RecompiledFunctionTableEnd=0x340000;
extern const uint32_t g_ps2RecompiledFunctionTableSlotCount=(0x340000-0x100000)/4;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[(0x340000-0x100000)/4]={};
using namespace ssx;using namespace ssx::terrain_original;
struct Call{int kind=0,value=0;float scalar=0;bool operator==(const Call&)const=default;};
static std::vector<Call> actual,expected;static OriginalCrashClipState actualClip,nativeClip;static float playback;static int nextClass;
template<class T>static void write(uint8_t*m,uint32_t p,T v){std::memcpy(m+p,&v,sizeof(v));}
template<class T>static T read(uint8_t*m,uint32_t p){T v;std::memcpy(&v,m+p,sizeof(v));return v;}
static void vec(uint8_t*m,uint32_t p,Vector v){write(m,p,v);write(m,p+12,0.f);}
static void changed(OriginalCrashClipState& clip){clip.duration10=mul(clip.duration10,1.25f);clip.animationClass=nextClass;}
static void external(uint8_t*m,R5900Context*c,PS2Runtime*){
    int kind=0,value=0;float scalar=0;uint32_t pc=c->pc;
    switch(pc){case 0x12e528:kind=1;c->f[0]=playback;break;case 0x33fff8:kind=2;scalar=c->f[12];break;case 0x33fff0:kind=3;scalar=c->f[12];break;
    case 0x12dd98:kind=4;break;case 0x12dcb0:kind=5;break;case 0x12df48:kind=6;break;case 0x12de80:kind=7;break;case 0x12e468:kind=8;break;case 0x12e010:kind=9;break;
    case 0x313cf0:kind=10;scalar=c->f[12];break;
    case 0x296310:kind=11;value=0;break;case 0x296868:kind=11;value=1;break;case 0x296e80:kind=11;value=2;break;case 0x296e20:kind=11;value=3;break;case 0x297438:kind=11;value=4;break;case 0x29f660:kind=11;value=5;break;
    case 0x10f280:kind=12;value=GPR_U32(c,5);break;case 0x116120:kind=13;value=GPR_U32(c,6);break;case 0x15e360:if(GPR_U32(c,5)!=4||c->f[13]!=0)throw std::runtime_error("Unexpected crash camera shake contract");kind=14;scalar=c->f[12];break;
    case 0x311ae8:SET_GPR_U32(c,2,actualClip.animationClass);break;case 0x312ae8:SET_GPR_U32(c,2,actualClip.complete);break;case 0x312ab0:c->f[0]=actualClip.progress;break;case 0x311b20:SET_GPR_U32(c,2,0x50000);break;
    default:SET_GPR_U32(c,2,0);break;}
    if(kind){actual.push_back({kind,value,scalar});if(kind>=4&&kind<=9){changed(actualClip);write(m,0x50010,actualClip.duration10);}}
    c->pc=GPR_U32(c,31);
}
int main(int argc,char**argv){if(argc!=2)return 1;PS2Runtime runtime;std::vector<uint8_t> memory(32*1024*1024);auto*m=memory.data();std::ifstream ee(argv[1],std::ios::binary);ee.read((char*)m,memory.size());if(!ee)return 2;
    for(uint32_t pc:{0x12e528,0x33fff8,0x33fff0,0x12dd98,0x12dcb0,0x12df48,0x12de80,0x12e468,0x12e010,0x313cf0,0x296310,0x296868,0x296e80,0x296e20,0x297438,0x29f660,0x10f280,0x116120,0x15e360,0x311ae8,0x312ae8,0x312ab0,0x311b20,0x28b180})runtime.registerFunction(pc,external);
    constexpr uint32_t actor=0x10000,control=0x20000,owner=0x30000,stack=0x70000,done=0x12345678;
    write(m,control+0x80,actor);write(m,actor+0x77c,owner);write(m,actor+0x6c0,0x40000u);write(m,0x40088,int16_t(0));write(m,0x4008c,0x33fff0u);write(m,0x40090,int16_t(0));write(m,0x40094,0x33fff8u);write(m,0x4a30f0-0x848,0x60000u);write(m,0x60084,0x61000u);write(m,0x61084,0x62000u);for(unsigned n=0;n<4;++n)write(m,0x62004+n*4,0x63000u);
    std::mt19937 random(0x5245434f);std::uniform_real_distribution<float> speed(-2000,2000),unit(-1,1),positive(.1f,2);
    OriginalCrashContinuationCallbacks cb;cb.currentClip=[&](){return nativeClip;};cb.updatePlaybackRate=[&](){expected.push_back({1});return playback;};cb.setRecoveryPresentation=[&](float x){expected.push_back({2,0,x});};cb.reportImpact=[&](float x){expected.push_back({3,0,x});};auto change=[&](int kind){expected.push_back({kind});changed(nativeClip);};cb.playGroundContinuation=[&](){change(4);};cb.playAirContinuation=[&](){change(5);};cb.playGroundGetUp=[&](){change(6);};cb.playAirGetUp=[&](){change(7);};cb.playSpecialLanding=[&](){change(8);};cb.rebakeResetClip=[&](){change(9);};cb.seekClipSeconds=[&](float x){expected.push_back({10,0,x});};cb.notify=[&](OriginalCrashObserver n){expected.push_back({11,int(n)});};cb.refundCrashBoost=[&](bool x){expected.push_back({12,int(x)});};cb.requestReset=[&](int x){expected.push_back({13,x});};cb.cameraShake=[&](float x){expected.push_back({14,0,x});};
    for(unsigned phase:{1u,2u}) {
        unsigned transitions=0;
        for(unsigned n=0;n<20000;++n){OriginalCrashControlState s;s.phase=phase;s.impactPending54=n%3==0;s.impactVelocity60={speed(random),speed(random),speed(random)};s.recovery70=unit(random)*2;OriginalCrashMotionState motion;motion.submode=random()%2;OriginalCrashActorState a;a.velocity={speed(random),speed(random),speed(random)};a.groundNormal={0,0,1};a.detached=random()%2;a.surface=n%5?0:18;if(n%10==0){a.velocity={0,10,0};s.impactVelocity60={0,0,-1000};}
            OriginalCrashRecoveryInputs input;input.deviceIndex870=int(random()%4)-1;input.deviceEnabled87C=random()%2;input.resetPermission470=unit(random);actualClip={};actualClip.animationClass=25+random()%5;actualClip.progress=positive(random)*.5f;actualClip.duration10=positive(random);actualClip.complete=random()%2;nextClass=25+random()%5;nativeClip=actualClip;playback=positive(random);
            write(m,control,int(phase));write(m,control+0x54,s.impactPending54);vec(m,control+0x60,s.impactVelocity60);write(m,control+0x70,s.recovery70);write(m,owner+0x30,motion.submode);vec(m,actor+0x1e0,a.velocity);vec(m,actor+0x370,a.groundNormal);write(m,actor+0x150,int(a.detached));write(m,actor+0x438,a.surface);write(m,actor+0x470,input.resetPermission470);write(m,actor+0x870,input.deviceIndex870);write(m,actor+0x87c,int(input.deviceEnabled87C));write(m,0x50010,actualClip.duration10);actual.clear();expected.clear();
            R5900Context c{};c.pc=phase==1?0x12d4e8:0x12d160;c.vu0_vf[0]=_mm_set_ps(1,0,0,0);SET_GPR_U32(&c,4,control);SET_GPR_U32(&c,29,stack);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,31,done);Rounding rounding;
            if(phase==1){sub_0012D4E8_0x12d4e8(m,&c,&runtime);originalCrashAirRecoveryStep(s,motion,a,input,cb);}else{sub_0012D160_0x12d160(m,&c,&runtime);originalCrashGroundRecoveryStep(s,motion,a,input,cb);}
            if(c.pc!=done||read<int>(m,control)!=s.phase||read<int>(m,control+0x54)!=s.impactPending54||read<Vector>(m,actor+0x1e0)!=a.velocity||actual!=expected){std::cerr<<"Crash recovery mismatch phase "<<phase<<" case "<<n<<" states "<<read<int>(m,control)<<'/'<<s.phase<<" velocity "<<(read<Vector>(m,actor+0x1e0)==a.velocity)<<" events "<<actual.size()<<'/'<<expected.size()<<'\n';for(auto x:actual)std::cerr<<"source "<<x.kind<<':'<<x.value<<':'<<x.scalar<<'\n';for(auto x:expected)std::cerr<<"native "<<x.kind<<':'<<x.value<<':'<<x.scalar<<'\n';return 3;}transitions+=s.phase!=int(phase);
        }
        std::cout<<"20000 original crash recovery phase"<<phase<<" calls match exact physical state and callback order; "<<transitions<<" phase transitions\n";
    }
}
