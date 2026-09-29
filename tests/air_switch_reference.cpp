// Development-only instruction oracle: original in-flight stance switch 0x135BE0 -> 0x114DB8
// (with the recompiled 0x11E098, 0x115168 and 0x31BE50) against engine/air_switch.hpp.
// 0x311B48 (sequence root turn), 0x1135B8 (new flight) and 0x3128E8 (play) are recording stubs.
// The original ELF is mapped at ram+0xFF000; every region below is private scratch memory.
#include "ps2_runtime_macros.h"
#include "../engine/air_switch.hpp"
#include <fstream>
#include <cstring>
#include <cstdio>
#include <cfenv>
#include <random>
#include <vector>
#include <bit>
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x420000,g_ps2RecompiledFunctionTableSlotCount=0xc8000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xc8000]={};
void sub_00135BE0_0x135be0(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00114DB8_0x114db8(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0011E098_0x11e098(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00115168_0x115168(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0031BE50_0x31be50(uint8_t*,R5900Context*,PS2Runtime*);
using Quad=ssx::AirSwitchQuad;
constexpr uint32_t RIDER=0x20000,AIR=0x30000,GEOMETRY=0x40000,LOCALS=0x41000,ANIM=0x50000,PRED=0x60000,WORLD=0x80000,TABLE=0x81000,SURFACES=0x82000,
    STACK=0x1F000,DONE=0x12345678,PIVOT_INDEX=3;
static uint8_t* M=nullptr;
template<class T> T rd(uint32_t a){T v;std::memcpy(&v,M+a,sizeof v);return v;}
template<class T> void wr(uint32_t a,const T& v){std::memcpy(M+a,&v,sizeof v);}
template<class T> void wrTo(std::vector<uint8_t>& img,uint32_t a,const T& v){std::memcpy(img.data()+a,&v,sizeof v);}
static uint32_t fb(float f){return std::bit_cast<uint32_t>(f);}
static void fail(const char* what){throw std::runtime_error(what);}
struct Call{uint32_t fn=0;int32_t arg=0;uint32_t value=0;bool operator==(const Call&)const=default;};
static std::vector<Call> originalCalls;static Quad originalFlightPosition{},originalFlightVelocity{};
static void ret(R5900Context* c){c->pc=GPR_U32(c,31);}
static void rotateStub(uint8_t*,R5900Context*c,PS2Runtime*){if(GPR_U32(c,4)!=ANIM)fail("0x311B48 animator");originalCalls.push_back({0x311b48,0,fb(c->f[12])});ret(c);}
static void flightStub(uint8_t*,R5900Context*c,PS2Runtime*){
    if(GPR_U32(c,4)!=PRED||GPR_U32(c,5)!=RIDER+0x110||GPR_U32(c,6)!=RIDER+0x1e0)fail("0x1135B8 arguments");
    originalCalls.push_back({0x1135b8,1,fb(c->f[12])});originalFlightPosition=rd<Quad>(RIDER+0x110);originalFlightVelocity=rd<Quad>(RIDER+0x1e0);ret(c);
}
static void playStub(uint8_t*,R5900Context*c,PS2Runtime*){
    if(GPR_U32(c,4)!=ANIM||GPR_U32(c,6)!=0)fail("0x3128E8 arguments");originalCalls.push_back({0x3128e8,int32_t(GPR_U32(c,5)),fb(c->f[12])});ret(c);
}
struct Rand{
    std::mt19937 g;explicit Rand(uint32_t seed):g(seed){}
    float uni(float lo,float hi){return std::uniform_real_distribution<float>(lo,hi)(g);}
    unsigned n(unsigned k){return g()%k;}
    bool coin(unsigned k=2){return g()%k==0;}
    std::array<float,3> unit(){for(;;){std::array<float,3> v{uni(-1,1),uni(-1,1),uni(-1,1)};float l=v[0]*v[0]+v[1]*v[1]+v[2]*v[2];if(l>1e-4f&&l<=1){l=std::sqrt(l);return {v[0]/l,v[1]/l,v[2]/l};}}}
    Quad quat(){for(;;){Quad q{uni(-1,1),uni(-1,1),uni(-1,1),uni(-1,1)};float l=q[0]*q[0]+q[1]*q[1]+q[2]*q[2]+q[3]*q[3];if(l>1e-3f&&l<=1){l=std::sqrt(l);for(auto& x:q)x/=l;return q;}}}
    float lane(){return coin(5)?uni(-1,1):coin(8)?-0.f:0.f;}
    Quad quad(std::array<float,3> v,float s=1){return {v[0]*s,v[1]*s,v[2]*s,lane()};}
};
static void storeValue(std::vector<uint8_t>* img,uint32_t a,const ssx::GroundControlValue& v){
    if(img){wrTo(*img,a,v.current);wrTo(*img,a+4,v.rate);wrTo(*img,a+8,v.target);}else{wr(a,v.current);wr(a+4,v.rate);wr(a+8,v.target);}
}
template<class T> static void put(std::vector<uint8_t>* img,uint32_t a,const T& v){if(img)wrTo(*img,a,v);else wr(a,v);}
static void storeRider(const ssx::OriginalAirSwitchRider& r,std::vector<uint8_t>* img,bool rebuilt){
    put(img,RIDER+0x110,r.position);put(img,RIDER+0x120,r.quaternion);put(img,RIDER+0x170,r.frameForward);put(img,RIDER+0x180,r.frameUp);
    if(rebuilt){put(img,RIDER+0x1a0,r.right);put(img,RIDER+0x1b0,r.forward);put(img,RIDER+0x1c0,r.up);put(img,RIDER+0x1d0,r.rebuiltPosition);}
    put(img,RIDER+0x1e0,r.velocity);put(img,RIDER+0x3a0,r.surfaceForward);put(img,RIDER+0x3b0,r.lateral);put(img,RIDER+0x320,r.reverseStance);
    storeValue(img,RIDER+0x1f0,r.turn);storeValue(img,RIDER+0x1fc,r.animationTurn);storeValue(img,RIDER+0x208,r.extraLean);storeValue(img,RIDER+0x214,r.brake);storeValue(img,RIDER+0x280,r.balance);
    put(img,LOCALS+PIVOT_INDEX*16,r.pivot);put(img,GEOMETRY+0x140,r.scale);
}
static void storeAir(const ssx::OriginalAirControlState& s,std::vector<uint8_t>* img){
    put(img,AIR+0x10,s.targetFlip);put(img,AIR+0x14,s.targetSpin);put(img,AIR+0x18,s.progressFlip);put(img,AIR+0x1c,s.progressSpin);
    put(img,AIR+0x20,s.totalSpin);put(img,AIR+0x24,s.totalFlip);put(img,AIR+0x28,s.adjustSpin);put(img,AIR+0x2c,s.adjustFlip);
}
struct Region{uint32_t begin,end;const char* name;};
static const Region regions[]={{RIDER,RIDER+0xa00,"rider"},{AIR,AIR+0x100,"air"},{ANIM,ANIM+0x100,"animator"},{PRED,PRED+0x100,"predictor"},{LOCALS,LOCALS+0x100,"locals"},{GEOMETRY,GEOMETRY+0x200,"geometry"}};
static std::vector<uint8_t>& snapshot(){static std::vector<uint8_t> image(0x100000);for(auto& g:regions)std::memcpy(image.data()+g.begin,M+g.begin,g.end-g.begin);return image;}
static bool compareImage(const std::vector<uint8_t>& expected,unsigned test){
    for(auto& g:regions)for(uint32_t a=g.begin;a<g.end;a+=4){uint32_t x,y;std::memcpy(&x,M+a,4);std::memcpy(&y,expected.data()+a,4);
        if(x!=y){std::printf("mismatch case %u: %s+0x%x original %08x (%.9g) native %08x (%.9g)\n",test,g.name,a-g.begin,x,std::bit_cast<float>(x),y,std::bit_cast<float>(y));return false;}}
    return true;
}
int main(int argc,char**argv){
    if(argc!=2)return 2;
    std::ifstream file(argv[1],std::ios::binary);std::vector<uint8_t> elf((std::istreambuf_iterator<char>(file)),{});if(elf.size()<0x100000)return 2;
    std::vector<uint8_t> ram(32*1024*1024);std::memcpy(ram.data()+0xff000,elf.data(),elf.size());M=ram.data();
    wr(0x4ff120,Quad{0,0,0,0});wr(0x4ff130,Quad{0,0,0,1});wr(0x4ff160,Quad{0,0,1,0});wr(0x4a0ea8,-0.f); // 0x4A0EA8 = -0.0 (boot 0x2F9838, glide snapshot)
    wr(0x4a30f0-0x848,WORLD);wr(WORLD+0x84,TABLE);wr(TABLE+0x44,SURFACES);
    wr(AIR+0x58,RIDER);wr(RIDER+0x780,GEOMETRY);wr(GEOMETRY+0x24,LOCALS);wr(RIDER+0x89c,PIVOT_INDEX);wr(RIDER+0x784,ANIM);wr(RIDER+0x788,PRED);
    PS2Runtime rt;
    rt.registerFunction(0x114db8,sub_00114DB8_0x114db8);rt.registerFunction(0x11e098,sub_0011E098_0x11e098);rt.registerFunction(0x115168,sub_00115168_0x115168);
    rt.registerFunction(0x31be50,sub_0031BE50_0x31be50);rt.registerFunction(0x311b48,rotateStub);rt.registerFunction(0x1135b8,flightStub);rt.registerFunction(0x3128e8,playStub);
    std::fesetround(FE_TOWARDZERO);Rand g(0x135be0);
    unsigned switched=0,reasons[6]{},riderFrame=0,reversed=0;
    for(unsigned test=0;test<60000;++test){
        ssx::OriginalAirSwitchRider r;ssx::OriginalAirSwitchPrediction p;ssx::OriginalAirControlState s;
        // Landing geometry: the normal is mostly upward, the board frame is a random yaw around it.
        auto n=g.unit();if(g.coin(3))n={g.uni(-.5f,.5f),g.uni(-.5f,.5f),1};float nl=std::sqrt(n[0]*n[0]+n[1]*n[1]+n[2]*n[2]);for(auto& x:n)x/=nl;
        p.normal=g.coin(6)?g.quad(g.unit()):g.quad(n);p.heading=g.quad(g.unit(),g.uni(100,3000));
        p.status=int(g.n(10))<8?(g.coin()?1:3):int(g.n(4));p.surface=g.coin(6)?18:int(g.n(18));p.surfaceProperty44=g.coin(8)?int(g.n(3)):0;
        auto up=g.coin(8)?g.unit():n;if(g.coin(10))for(auto& x:up)x=-x;r.frameUp=g.quad(up);
        auto side=g.unit();std::array<float,3> f{up[1]*side[2]-up[2]*side[1],up[2]*side[0]-up[0]*side[2],up[0]*side[1]-up[1]*side[0]};float fl=std::sqrt(f[0]*f[0]+f[1]*f[1]+f[2]*f[2]);
        if(fl<1e-3f||g.coin(12))f=g.unit();else for(auto& x:f)x/=fl;r.frameForward=g.quad(f);
        auto h=g.unit();float speed=g.uni(0,3000);if(g.coin(4)){for(unsigned k=0;k<3;++k)h[k]=f[k]*g.uni(-1,1)+h[k]*.3f;}
        r.velocity=g.quad(h,speed);if(g.coin(8)){float t=277.777771f/std::max(1e-3f,f[0]*h[0]+f[1]*h[1]+f[2]*h[2]);for(unsigned k=0;k<3;++k)r.velocity[k]=h[k]*t;}
        r.position={g.uni(-2e5f,2e5f),g.uni(-2e5f,2e5f),g.uni(-2e4f,2e4f),g.coin(4)?1.f:g.lane()};r.quaternion=g.quat();
        r.pivot={g.uni(-40,40),g.uni(-40,40),g.uni(-10,110),g.coin(3)?1.f:g.lane()};r.scale={.85f,.85f,.85f,g.coin(2)?1.f:g.uni(0,1)};if(g.coin(6))r.scale={g.uni(.5f,1.2f),g.uni(.5f,1.2f),g.uni(.5f,1.2f),g.uni(-1,1)};
        r.surfaceForward=g.quad(g.unit());r.lateral=g.quad(g.unit());r.reverseStance=int(g.n(2));
        for(auto* v:{&r.turn,&r.animationTurn,&r.extraLean,&r.brake,&r.balance}){v->current=g.coin(8)?0.f:g.uni(-2,2);v->rate=g.uni(-2,2);v->target=g.coin(8)?-0.f:g.uni(-2,2);}
        s.totalSpin=g.coin(4)?0.f:g.coin(3)?float(int(g.n(9))-4)*3.14159274f:g.uni(-40,40);s.totalFlip=g.uni(-20,20);s.adjustSpin=g.uni(-1.75f,1.75f);s.adjustFlip=g.coin(3)?0.f:g.uni(-1.75f,1.75f);
        s.progressFlip=g.coin(3)?0.f:g.uni(-7,7);s.targetFlip=g.coin(3)?0.f:g.uni(-7,7);s.progressSpin=g.uni(-7,7);s.targetSpin=g.uni(-7,7);
        // Memory: rider, predictor, surface table, air control state; 0x1A0..0x1DC get sentinels.
        std::memset(M+RIDER,0,0xa00);std::memset(M+ANIM,0x5a,0x100);std::memset(M+PRED,0,0x100);
        for(uint32_t a=RIDER+0x1a0;a<RIDER+0x1e0;a+=4)wr(a,0xdeadbeefu);
        wr(RIDER+0x780,GEOMETRY);wr(RIDER+0x89c,PIVOT_INDEX);wr(RIDER+0x784,ANIM);wr(RIDER+0x788,PRED);
        storeRider(r,nullptr,false);storeAir(s,nullptr);
        wr(PRED+0xac,p.status);wr(PRED+0x90,p.surface);wr(PRED+0x10,p.heading);wr(PRED+0x20,p.normal);
        for(unsigned k=0;k<19;++k)wr(SURFACES+k*0xb0+0x44,k==unsigned(p.surface)?p.surfaceProperty44:int(g.n(2)));
        auto& expected=snapshot();originalCalls.clear();
        R5900Context c{};c.pc=0x135be0;c.vu0_vf[0]=_mm_set_ps(1,0,0,0);SET_GPR_U32(&c,4,AIR);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,29,STACK);SET_GPR_U32(&c,31,DONE);
        sub_00135BE0_0x135be0(M,&c,&rt);if(c.pc!=DONE){std::printf("continuation %x\n",c.pc);return 3;}
        // Native.
        std::vector<Call> nativeCalls;Quad nativeFlightPosition{},nativeFlightVelocity{};
        ssx::OriginalAirSwitchAccess access;
        access.rotateAnimation=[&](float angle){nativeCalls.push_back({0x311b48,0,fb(angle)});};
        access.setAnimationRoot=[&](Quad t,Quad q){wrTo(expected,ANIM+0x30,t);wrTo(expected,ANIM+0x40,q);};
        access.setAnimationMirror=[&](int m){wrTo(expected,ANIM+0x18,m);};
        access.beginFlight=[&](Quad position,Quad velocity,float speed){nativeCalls.push_back({0x1135b8,1,fb(speed)});nativeFlightPosition=position;nativeFlightVelocity=velocity;};
        access.play=[&](int semantic){nativeCalls.push_back({0x3128e8,semantic,fb(-1.f)});};
        const bool required=ssx::originalAirSwitchRequired(r,p);
        const bool did=ssx::originalAirSwitch(r,p,access);if(did)ssx::originalAirSwitchControl(s,access);
        if(required!=did||unsigned(did)!=GPR_U32((&c),2)){std::printf("switch result mismatch case %u original %u native %d status %d surface %d prop %d\n",test,GPR_U32((&c),2),did,p.status,p.surface,p.surfaceProperty44);
            using namespace ssx::air_switch_original;ssx::OriginalRounding rr;bool rf=p.surface==18||p.surfaceProperty44!=0;auto nn=rf?r.frameUp:p.normal;auto hh=rf?r.velocity:p.heading;
            float a=dot4(r.frameUp,nn),b=dot4(r.frameForward,r.velocity),d=dot4(r.frameForward,nn);auto f2=projectNormalized(r.frameForward,nn,d),h2=projectNormalized(hh,nn,dot4(hh,nn));
            std::printf(" up.n %g f.v %g f.n %g f'.h' %g\n",a,b,d,dot4(f2,h2));
            uint32_t sp=STACK-0x20-0xd0;for(unsigned o=0;o<0x80;o+=16){auto q=rd<Quad>(sp+o);std::printf(" sp%02x %g %g %g %g\n",o,q[0],q[1],q[2],q[3]);}
            std::printf(" n %g %g %g %g h %g %g %g %g f %g %g %g %g\n",nn[0],nn[1],nn[2],nn[3],hh[0],hh[1],hh[2],hh[3],r.frameForward[0],r.frameForward[1],r.frameForward[2],r.frameForward[3]);return 4;}
        storeRider(r,&expected,did);storeAir(s,&expected);
        if(!compareImage(expected,test))return 5;
        if(originalCalls!=nativeCalls){std::printf("call sequence mismatch case %u (%zu/%zu)\n",test,originalCalls.size(),nativeCalls.size());return 6;}
        if(did&&(std::memcmp(&originalFlightPosition,&nativeFlightPosition,16)||std::memcmp(&originalFlightVelocity,&nativeFlightVelocity,16))){std::printf("0x1135B8 input mismatch case %u\n",test);return 7;}
        if(did){++switched;reversed+=r.reverseStance;riderFrame+=p.surface==18||p.surfaceProperty44!=0;}
        else ++reasons[p.status!=1&&p.status!=3?0:1];
    }
    std::printf("60000 complete original 0x135BE0/0x114DB8 (+0x11E098, 0x115168, 0x31BE50) cases exact: return, +0x110 pivot turn, 0x11E098 rows, 0x115168 flips, animator root/mirror, 0x1135B8 inputs, spin wrap/flip negations, 288 request (%u switched, %u into reverse stance, %u on the rider frame; %u status rejects, %u facing rejects)\n",
        switched,reversed,riderFrame,reasons[0],reasons[1]);
}
