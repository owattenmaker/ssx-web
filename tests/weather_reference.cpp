// Instruction oracle for engine/wind_push.hpp and engine/weather.hpp (docs/weather.md): the recompiled originals on synthetic
// memory against the port.
//   0x125970 rider wind push (+ 0x2EF0A0 / 0x2EE570 through the Weather painter vtable 0x484058; 0x1250A8 stubbed: records dv)
//   0x2E4F50 snowfall layer update, 0x2E5430 camera shift
//   0x2F4330 snowfall input, 0x2F4260 impact input, 0x2F39E0 camera splash update with 0x2F3810 / 0x2F37A8 / 0x2F2598 /
//   0x2F3640 / 0x2F2D70 / 0x2F2810 / 0x2F3030 and the visual RNG 0x3177F0 (0x4FF018)
#include "ps2_runtime_macros.h"
#include "../engine/wind_push.hpp"
#include "../engine/weather.hpp"
#include "../engine/original_random.hpp"
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <random>
#include <vector>
#define ORIGINAL(name) void name(uint8_t*,R5900Context*,PS2Runtime*);
ORIGINAL(sub_00125970_0x125970) ORIGINAL(sub_002EF0A0_0x2ef0a0) ORIGINAL(sub_002EE570_0x2ee570)
ORIGINAL(sub_002E4F50_0x2e4f50) ORIGINAL(sub_002E5430_0x2e5430) ORIGINAL(sub_002F39C8_0x2f39c8) ORIGINAL(sub_002F3810_0x2f3810)
ORIGINAL(sub_002F37A8_0x2f37a8) ORIGINAL(sub_002F2598_0x2f2598) ORIGINAL(sub_002F3640_0x2f3640) ORIGINAL(sub_002F2D70_0x2f2d70)
ORIGINAL(sub_002F2810_0x2f2810) ORIGINAL(sub_002F3030_0x2f3030) ORIGINAL(sub_003177F0_0x3177f0) ORIGINAL(sub_002F4260_0x2f4260)
ORIGINAL(sub_002F4330_0x2f4330) ORIGINAL(sub_00317A08_0x317a08)
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x440000,g_ps2RecompiledFunctionTableSlotCount=(0x440000-0x100000)/4;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[(0x440000-0x100000)/4]={};
using namespace ssx;
static uint8_t* mem;
static constexpr uint32_t gp=0x4a30f0,done=0x12345678,stackTop=0x90000;
static constexpr uint32_t RIDER=0x1000000,IFACE=0x1001000,WRAP=0x1002000,PAINTER=0x1002100,LAYER=0x1003000,FPS=0x1004000,SPLASH=0x1010000,
    GAME=0x1020000,GAME84=0x1020100,VIEWS=0x1020200,CAMERA=0x1020400,RECORD=0x1021000,POS=0x1021100;
static std::mt19937 rng(0x125970);
static float uni(float a,float b){return std::uniform_real_distribution<float>(a,b)(rng);}
template<class T> static void put(uint32_t a,const T& v){std::memcpy(mem+a,&v,sizeof(v));}
template<class T> static T get(uint32_t a){T v;std::memcpy(&v,mem+a,sizeof(v));return v;}
static uint32_t bits(float f){return std::bit_cast<uint32_t>(f);}
static PS2Runtime* runtime;
using Original=void(*)(uint8_t*,R5900Context*,PS2Runtime*);
static void run(Original f,uint32_t pc,uint32_t a0,uint32_t a1=0,float f12=0){
    R5900Context c{};c.pc=pc;c.vu0_vf[0]=_mm_set_ps(1,0,0,0);for(unsigned i=0;i<32;i++)c.f[i]=uni(-1e3,1e3);c.f[12]=f12;
    for(unsigned r:{1u,2u,3u,8u,9u,10u,11u,12u,13u,14u,15u,24u,25u})SET_GPR_U64(&c,r,(uint64_t(rng())<<32)|rng());
    SET_GPR_U32(&c,4,a0);SET_GPR_U32(&c,5,a1);SET_GPR_U32(&c,28,gp);SET_GPR_U32(&c,29,stackTop);SET_GPR_U32(&c,31,done);
    OriginalRounding rounding;for(unsigned guard=0;c.pc!=done;guard++){if(guard>100000)throw std::runtime_error("original incomplete");f(mem,&c,runtime);}
}
static bool pushed=false;static weather::Quad pushedDv{};
static float pickKmh(){switch(rng()%6){case 0:return 0;case 1:return uni(9.9f,10.1f);case 2:return uni(49.9f,50.1f);case 3:return float(rng()%80);default:return uni(0,70);}}
static float pickDeg(){switch(rng()%5){case 0:return float(int(rng()%8)*45);case 1:return uni(-720,720);case 2:return float(rng()%400);default:return uni(0,360);}}
static std::array<float,19> randomWeather(){
    std::array<float,19> w{};for(auto& x:w)x=uni(0,2);
    w[0]=rng()%3?uni(0,7):float(rng()%3)*1.5f;w[1]=uni(0,8);w[2]=uni(0,9);w[3]=pickDeg();w[4]=pickKmh();w[6]=rng()%4?uni(0,1):float(rng()%2);
    w[7]=rng()%3?uni(-2,2):1.f;w[8]=uni(0,2);w[9]=uni(0,20);return w;
}
static void storeLayer(const OriginalSnowfallLayer& L){
    std::memset(mem+LAYER,0,0xC0);put(LAYER,L.camera);put(LAYER+4,L.kind);put(LAYER+8,L.count);put(LAYER+0xC,L.extent);put(LAYER+0x10,L.speed);put(LAYER+0x14,L.size);
    put(LAYER+0x18,L.alpha);put(LAYER+0x1C,L.gravity);put(LAYER+0x20,L.offset);put(LAYER+0x30,L.gustNew);put(LAYER+0x40,L.gustOld);put(LAYER+0x90,L.timer);put(LAYER+0x94,L.period);
    put(LAYER+0x98,L.seeds);put(LAYER+0xA4,L.colour);put(LAYER+0xB0,L.first);
}
static bool sameLayer(const OriginalSnowfallLayer& L){
    auto W=[&](uint32_t o){return get<uint32_t>(LAYER+o);};
    bool ok=W(0)==uint32_t(L.camera)&&W(4)==uint32_t(L.kind)&&W(8)==uint32_t(L.count)&&W(0xC)==bits(L.extent)&&W(0x10)==bits(L.speed)&&W(0x14)==bits(L.size)&&W(0x18)==bits(L.alpha)&&W(0x1C)==bits(L.gravity)
        &&W(0x90)==bits(L.timer)&&W(0x94)==bits(L.period)&&W(0xB0)==uint32_t(L.first);
    for(unsigned k=0;k<4;k++)ok=ok&&W(0x20+4*k)==bits(L.offset[k])&&W(0x30+4*k)==bits(L.gustNew[k])&&W(0x40+4*k)==bits(L.gustOld[k]);
    for(unsigned k=0;k<3;k++)ok=ok&&W(0xA4+4*k)==bits(L.colour[k]);
    return ok;
}
static OriginalSnowfallLayer randomLayer(){
    OriginalSnowfallLayer L;L.kind=int32_t(rng()%2);L.count=int32_t(rng()%3000);L.extent=float(std::array<int,4>{1500,2000,3000,2000}[rng()%4]);
    L.speed=uni(0,4000);L.size=uni(0,8);L.alpha=uni(0,1);L.gravity=rng()%2?-200.f:-100.f;
    for(unsigned k=0;k<3;k++){L.offset[k]=rng()%5?uni(-.5f,.5f)*L.extent:uni(-2,2)*L.extent;L.gustNew[k]=uni(-3000,3000);L.gustOld[k]=uni(-3000,3000);L.colour[k]=uni(0,1);}
    L.offset[3]=1;L.gustNew[3]=rng()%2?0.f:uni(-1,1);L.gustOld[3]=rng()%2?0.f:uni(-1,1);
    L.period=rng()%4?uni(1,5):uni(.01f,.1f);L.timer=rng()%3?uni(-0.01f,.05f):uni(0,L.period);L.first=int32_t(rng()%4==0);
    L.seeds={uint32_t(rng()),uint32_t(rng()),uint32_t(rng())};return L;
}
// camera splash memory image
static void storeSplash(const OriginalCameraSplash& s){
    std::memset(mem+SPLASH,0,0x1030);put(SPLASH+0xC,uint32_t(0x488230));put(SPLASH+0x10,s.camera);put(SPLASH+0x14,int32_t(s.drops.size()));put(SPLASH+0x18,int32_t(s.crystals.size()));
    for(size_t i=0;i<s.drops.size();i++){const auto& d=s.drops[i];const uint32_t o=SPLASH+0x1C+0x3C*uint32_t(i);put(o,d.alive);put(o+4,d.x);put(o+8,d.y);put(o+0xC,d.w);put(o+0x10,d.h);put(o+0x14,d.vx);put(o+0x18,d.vy);put(o+0x1C,d.age);put(o+0x20,d.life);put(o+0x24,d.colour);put(o+0x34,d.angle);put(o+0x38,d.spin);}
    for(size_t i=0;i<s.crystals.size();i++){const auto& c=s.crystals[i];const uint32_t o=SPLASH+0x724+0x4C*uint32_t(i);put(o,c.spawned);put(o+4,c.alive);put(o+8,c.x);put(o+0xC,c.y);put(o+0x10,c.w);put(o+0x14,c.h);put(o+0x18,c.vx);put(o+0x1C,c.vy);put(o+0x20,c.age);put(o+0x24,c.life);put(o+0x28,c.colour);put(o+0x38,c.angle);put(o+0x3C,c.spin);put(o+0x40,c.timer);put(o+0x44,c.ax);put(o+0x48,c.ay);}
    put(SPLASH+0x100C,s.hasPrevious);put(SPLASH+0x1010,s.previous);put(SPLASH+0x1020,s.speed);put(SPLASH+0x1024,s.pending);put(SPLASH+0x1028,s.snowfall);
}
static std::string diffSplash(const OriginalCameraSplash& s){
    auto W=[&](uint32_t a){return get<uint32_t>(a);};char buf[160];
    if(W(SPLASH+0x14)!=s.drops.size()){snprintf(buf,sizeof buf,"drops %u vs %zu",W(SPLASH+0x14),s.drops.size());return buf;}
    if(W(SPLASH+0x18)!=s.crystals.size()){snprintf(buf,sizeof buf,"crystals %u vs %zu",W(SPLASH+0x18),s.crystals.size());return buf;}
    for(size_t i=0;i<s.drops.size();i++){const auto& d=s.drops[i];const uint32_t o=SPLASH+0x1C+0x3C*uint32_t(i);
        const std::array<float,14> v{d.x,d.y,d.w,d.h,d.vx,d.vy,d.age,d.life,d.colour[0],d.colour[1],d.colour[2],d.colour[3],d.angle,d.spin};
        if(W(o)!=uint32_t(d.alive))return "drop alive";for(unsigned k=0;k<14;k++)if(W(o+4+4*k)!=bits(v[k])){snprintf(buf,sizeof buf,"drop %zu field %u %08x vs %08x",i,k,W(o+4+4*k),bits(v[k]));return buf;}}
    for(size_t i=0;i<s.crystals.size();i++){const auto& c=s.crystals[i];const uint32_t o=SPLASH+0x724+0x4C*uint32_t(i);
        const std::array<float,17> v{c.x,c.y,c.w,c.h,c.vx,c.vy,c.age,c.life,c.colour[0],c.colour[1],c.colour[2],c.colour[3],c.angle,c.spin,c.timer,c.ax,c.ay};
        if(W(o)!=uint32_t(c.spawned)||W(o+4)!=uint32_t(c.alive))return "crystal flags";for(unsigned k=0;k<17;k++)if(W(o+8+4*k)!=bits(v[k])){snprintf(buf,sizeof buf,"crystal %zu field %u %08x vs %08x",i,k,W(o+8+4*k),bits(v[k]));return buf;}}
    if(W(SPLASH+0x100C)!=uint32_t(s.hasPrevious))return "hasPrevious";for(unsigned k=0;k<4;k++)if(W(SPLASH+0x1010+4*k)!=bits(s.previous[k]))return "previous";
    if(W(SPLASH+0x1020)!=bits(s.speed))return "speed";if(W(SPLASH+0x1024)!=bits(s.pending)){snprintf(buf,sizeof buf,"pending %08x vs %08x",W(SPLASH+0x1024),bits(s.pending));return buf;}if(W(SPLASH+0x1028)!=bits(s.snowfall))return "snowfall";
    return "";
}
static OriginalCameraSplash randomSplash(const OriginalSplashTweaks& k){
    OriginalCameraSplash s;s.hasPrevious=int32_t(rng()%5!=0);s.previous={uni(-3e5f,3e5f),uni(-3e5f,3e5f),uni(0,3e5f),1};s.speed=rng()%3?uni(0,150):uni(0,1200);
    s.pending=rng()%3?uni(0,3):uni(0,12);s.snowfall=uni(0,7);
    weather::Lcg lcg{*new uint32_t(uint32_t(rng()))};
    const int nd=int(rng()%(k.maxDrops+1)),nc=int(rng()%(k.maxCrystals+1));
    for(int i=0;i<nd;i++){OriginalSplashDrop d;splash_detail::spawnDrop(d,uni(-20,660),uni(-20,500),{uni(4,8),uni(4,8)},k,lcg);d.age=rng()%4?uni(0,d.life):uni(d.life-.05f,d.life);d.vx=uni(-8,8);d.vy=uni(-8,8);if(rng()%8==0)d.alive=0;s.drops.push_back(d);}
    for(int i=0;i<nc;i++){OriginalSplashCrystal c;splash_detail::spawnCrystal(c,uni(-20,660),uni(-20,500),int32_t(rng()%3),k,lcg);c.age=rng()%4?uni(0,c.life):uni(c.life-.05f,c.life);c.vx=uni(-6,6);c.vy=uni(-6,6);c.timer=uni(-.02f,3);c.spawned=int32_t(rng()%3);c.ax=uni(-.01f,.01f);c.ay=uni(0,.025f);s.crystals.push_back(c);}
    return s;
}
int main(int argc,char** argv){
    if(argc<2)return 2;std::vector<uint8_t> memory(32*1024*1024);mem=memory.data();
    {std::ifstream f(argv[1],std::ios::binary);std::vector<uint8_t> elf((std::istreambuf_iterator<char>(f)),{});if(elf.size()<0x3b0000)return 3;std::memcpy(mem+0xFF000,elf.data(),elf.size());}
    PS2Runtime rt;runtime=&rt;rt.setMissingFunctionPolicy(PS2Runtime::MissingFunctionPolicy::Stop);
    rt.registerFunction(0x125970,sub_00125970_0x125970);rt.registerFunction(0x2ef0a0,sub_002EF0A0_0x2ef0a0);rt.registerFunction(0x2ee570,sub_002EE570_0x2ee570);
    // 0x2C15F8 / 0x2C1600 (Weather properties 3 / 4: jr ra; addiu v0, a0, 0x20 / 0x28): no entry label in the recompiled 0x2C0B70
    rt.registerFunction(0x2c15f8,[](uint8_t*,R5900Context* c,PS2Runtime*){SET_GPR_U32(c,2,GPR_U32(c,4)+0x20);c->pc=GPR_U32(c,31);});
    rt.registerFunction(0x2c1600,[](uint8_t*,R5900Context* c,PS2Runtime*){SET_GPR_U32(c,2,GPR_U32(c,4)+0x28);c->pc=GPR_U32(c,31);});
    rt.registerFunction(0x1250a8,[](uint8_t* m,R5900Context* c,PS2Runtime*){pushed=true;std::memcpy(pushedDv.data(),m+GPR_U32(c,5),16);c->pc=GPR_U32(c,31);});
    rt.registerFunction(0x2e4f50,sub_002E4F50_0x2e4f50);rt.registerFunction(0x2e5430,sub_002E5430_0x2e5430);
    for(uint32_t pc:{0x2f39c8u,0x2f39e0u})rt.registerFunction(pc,sub_002F39C8_0x2f39c8);
    rt.registerFunction(0x2f3810,sub_002F3810_0x2f3810);rt.registerFunction(0x2f37a8,sub_002F37A8_0x2f37a8);rt.registerFunction(0x2f2598,sub_002F2598_0x2f2598);
    rt.registerFunction(0x2f3640,sub_002F3640_0x2f3640);rt.registerFunction(0x2f2d70,sub_002F2D70_0x2f2d70);rt.registerFunction(0x2f2810,sub_002F2810_0x2f2810);
    rt.registerFunction(0x2f3030,sub_002F3030_0x2f3030);rt.registerFunction(0x3177f0,sub_003177F0_0x3177f0);rt.registerFunction(0x317a08,sub_00317A08_0x317a08);rt.registerFunction(0x2f4260,sub_002F4260_0x2f4260);rt.registerFunction(0x2f4330,sub_002F4330_0x2f4330);
    for(unsigned i=0;i<640;i++)put(0x504FB8+4*i,originalFlagSineTableBits[i]); // BSS sine table (0x392DF0)
    {const float x[4]={1,0,0,0},y[4]={0,1,0,0},z[4]={0,0,1,0},w[4]={0,0,0,1};put(0x4FF140,x);put(0x4FF150,y);put(0x4FF160,z);put(0x4FF130,w);} // BSS unit quads (savestates)
    unsigned total=0,bad=0;
    // ---- 0x125970 ----
    put(RIDER+0x6C0,IFACE);put(IFACE+0x20,uint32_t(0));put(IFACE+0x24,uint32_t(0x1250a8));
    put(0x4FA370+0x20,WRAP);put(WRAP,PAINTER);put(PAINTER+4,uint32_t(0x484058));
    unsigned pushes=0;
    for(unsigned n=0;n<200000;n++){
        const float kmh=pickKmh(),deg=pickDeg();const bool human=rng()%8!=0;
        const weather::Quad v{rng()%6?uni(-4000,4000):uni(-300,300),rng()%6?uni(-4000,4000):uni(-300,300),uni(-3000,3000),rng()%4?0.f:uni(-1,1)};
        put(RIDER+0x874,uint32_t(human));put(RIDER+0x86C,uint32_t(0));put(RIDER+0x1E0,v);put(PAINTER+0x20,deg);put(PAINTER+0x28,kmh);
        pushed=false;run(sub_00125970_0x125970,0x125970,RIDER);
        const auto port=human?originalWindPush(kmh,deg,v):std::nullopt;
        if(pushed!=bool(port)||(port&&std::memcmp(port->data(),pushedDv.data(),16))){if(bad++<5)printf("125970 mismatch %u kmh %g deg %g pushed %d/%d\n",n,kmh,deg,int(pushed),int(bool(port)));}
        pushes+=pushed;
    }
    printf("0x125970 wind push: 200000 cases (%u pushes), %u mismatches\n",pushes,bad);total+=bad;bad=0;
    // ---- 0x2E4F50 / 0x2E5430 ----
    put(gp+0x2A74,FPS);unsigned gusts=0;
    for(unsigned n=0;n<200000;n++){
        auto L=randomLayer();const auto w=randomWeather();auto rec=originalWeatherRecord(w,rng()%6?0.f:uni(0,1));for(auto& x:rec)if(rng()%10==0)x=uni(-2,2);
        const int32_t fps=rng()%5?60:int32_t(1+rng()%120);put(FPS+0x10,fps);put(RECORD,rec);uint32_t lcg=uint32_t(rng());put(gp+0xA0C,lcg);
        storeLayer(L);run(sub_002E4F50_0x2e4f50,0x2e4f50,LAYER,RECORD);
        const uint32_t d=originalSnowfallLayerUpdate(L,rec,lcg,fps);gusts+=d>0;
        if(!sameLayer(L)||get<uint32_t>(gp+0xA0C)!=lcg){if(bad++<5){printf("2E4F50 mismatch %u kind %d lcg %d:",n,L.kind,int(get<uint32_t>(gp+0xA0C)!=lcg));
            const std::array<float,20> v{float(L.count),L.extent,L.speed,L.size,L.alpha,L.gravity,L.offset[0],L.offset[1],L.offset[2],L.offset[3],L.gustNew[0],L.gustNew[1],L.gustNew[2],L.gustNew[3],L.gustOld[0],L.gustOld[1],L.gustOld[2],L.gustOld[3],L.timer,L.period};
            const std::array<uint32_t,20> o{8,0xC,0x10,0x14,0x18,0x1C,0x20,0x24,0x28,0x2C,0x30,0x34,0x38,0x3C,0x40,0x44,0x48,0x4C,0x90,0x94};
            for(unsigned q=0;q<20;q++){uint32_t w=get<uint32_t>(LAYER+o[q]);uint32_t p=q==0?uint32_t(L.count):bits(v[q]);if(w!=p)printf(" +0x%X %08x/%08x",o[q],w,p);}printf("\n");}}
        if(rng()%2){weather::Quad delta{uni(-500,500),uni(-500,500),uni(-500,500),rng()%4?0.f:uni(-1,1)};put(POS,delta);storeLayer(L);run(sub_002E5430_0x2e5430,0x2e5430,LAYER,POS);
            originalSnowfallCameraShift(L,delta);if(!sameLayer(L)){if(bad++<5)printf("2E5430 mismatch %u\n",n);}}
    }
    printf("0x2E4F50 layer update + 0x2E5430 camera shift: 200000 cases (%u gusts), %u mismatches\n",gusts,bad);total+=bad;bad=0;
    // ---- camera splash ----
    OriginalSplashTweaks k;
    {auto check=[&](uint32_t off,uint32_t want,const char* name){if(get<uint32_t>(gp+off)!=want){printf("tweak %s gp+0x%X %08x (port %08x)\n",name,off,get<uint32_t>(gp+off),want);++bad;}};
     check(0x115C,1,"enable");check(0x1164,uint32_t(k.maxDrops),"maxDrops");check(0x1168,uint32_t(k.maxCrystals),"maxCrystals");check(0x116C,bits(k.crystalShare),"share");check(0x1170,bits(k.crystalSpawn),"spawn");
     check(0x1174,uint32_t(k.groupMin),"groupMin");check(0x1178,uint32_t(k.groupMax),"groupMax");check(0x117C,uint32_t(k.spawnPerCrystal),"perCrystal");check(0x1180,bits(k.impactDistance),"impactDistance");
     check(0x1184,bits(k.impactIntensity),"impactIntensity");check(0x1188,bits(k.impactSnowfall),"impactSnowfall");check(0x118C,bits(k.impactMultiplier),"impactMultiplier");check(0x1190,bits(k.snowfallMin),"snowfallMin");
     check(0x1194,bits(k.snowfallMultiplier),"snowfallMultiplier");check(0x1198,bits(k.pushoffSpeed),"pushoff");check(0x119C,bits(k.dropLifeMin),"dropLifeMin");check(0x11A0,bits(k.dropLifeMax),"dropLifeMax");
     check(0x11A4,bits(k.dropSizeMin),"dropSizeMin");check(0x11A8,bits(k.dropSpawnSize),"dropSpawnSize");check(0x11AC,bits(k.dropSizeDiff),"dropSizeDiff");check(0x11B0,bits(k.dropJiggleXMin),"jx0");check(0x11B4,bits(k.dropJiggleXMax),"jx1");
     check(0x11B8,bits(k.dropJiggleYMin),"jy0");check(0x11BC,bits(k.dropJiggleYMax),"jy1");check(0x11C0,bits(k.dropGrowMin),"g0");check(0x11C4,bits(k.dropGrowMax),"g1");check(0x11C8,bits(k.dropPushMin),"p0");check(0x11CC,bits(k.dropPushMax),"p1");
     check(0x11D0,bits(k.dropMaxSpeed),"dropMaxSpeed");check(0x11D4,bits(k.dropStepMin),"s0");check(0x11D8,bits(k.dropStepMax),"s1");check(0x11DC,bits(k.dropSpin),"dropSpin");check(0x11E0,bits(k.groupSpreadX),"gx");check(0x11E4,bits(k.groupSpreadY),"gy");
     check(0x11E8,bits(k.crystalLifeMin),"cl0");check(0x11EC,bits(k.crystalLifeMax),"cl1");check(0x11F0,bits(k.crystalSizeMin),"cs0");check(0x11F4,bits(k.crystalSizeMax),"cs1");check(0x11F8,bits(k.crystalSizeDiff),"csd");
     check(0x11FC,bits(k.crystalAccelXMin),"ax0");check(0x1200,bits(k.crystalAccelXMax),"ax1");check(0x1204,bits(k.crystalAccelYMin),"ay0");check(0x1208,bits(k.crystalAccelYMax),"ay1");check(0x120C,bits(k.crystalGustMin),"cg0");
     check(0x1210,bits(k.crystalGustMax),"cg1");check(0x1214,bits(k.crystalGrowMin),"cgr0");check(0x1218,bits(k.crystalGrowMax),"cgr1");check(0x121C,bits(k.crystalPushMin),"cp0");check(0x1220,bits(k.crystalPushMax),"cp1");
     check(0x1224,bits(k.crystalMaxSpeed),"cms");check(0x1228,bits(k.crystalSpin),"cspin");
     printf("camera splash tweakables (ELF .sdata vs port): %u mismatches\n",bad);total+=bad;bad=0;}
    put(gp+0x24C8,k.centreX);put(gp+0x24CC,k.centreY);put(0x5046E0,k.dropColourA);put(0x5046F0,k.dropColourB);put(0x504700,k.crystalColourA);put(0x504710,k.crystalColourB);
    put(gp-0x848,GAME);put(GAME+0x84,GAME84);put(GAME84+0x84,VIEWS);put(VIEWS+0x10,uint32_t(1));put(VIEWS+4,CAMERA);
    unsigned spawns=0,visuals=0;
    for(unsigned n=0;n<60000;n++){
        auto s=randomSplash(k);uint32_t lcg=uint32_t(rng());OriginalRandomState vis;for(auto& x:vis.words)x=uint32_t(rng());
        // inputs first (0x2F4330 snowfall, 0x2F4260 impact), then the update
        const float snowfall=rng()%3?uni(0,7):uni(1.4f,1.6f);storeSplash(s);run(sub_002F4330_0x2f4330,0x2f4330,SPLASH,0,snowfall);originalSplashSnowfall(s,snowfall,k);
        if(rng()%3==0){weather::Quad at{s.previous[0]+uni(-500,500),s.previous[1]+uni(-500,500),s.previous[2]+uni(-500,500),1};const float strength=uni(0,6000);put(POS,at);
            run(sub_002F4260_0x2f4260,0x2f4260,SPLASH,POS,strength);originalSplashImpact(s,at,strength,k);}
        const std::string pre=diffSplash(s);if(!pre.empty()){if(bad++<5)printf("2F4330/2F4260 mismatch %u: %s\n",n,pre.c_str());continue;}
        const weather::Quad cam{s.previous[0]+(rng()%6?uni(-100,100):uni(-6000,6000)),s.previous[1]+uni(-100,100),s.previous[2]+uni(-100,100),1};put(CAMERA+0x20,cam);
        put(gp+0xA0C,lcg);put(0x4FF018,vis.words);
        run(sub_002F39C8_0x2f39c8,0x2f39e0,SPLASH);
        const auto d=originalSplashUpdate(s,cam,lcg,[&]{return vis.next();},k);spawns+=d.visual>1;visuals+=d.visual;
        const std::string diff=diffSplash(s);std::array<uint32_t,6> words;std::memcpy(words.data(),mem+0x4FF018,24);
        if(!diff.empty()||get<uint32_t>(gp+0xA0C)!=lcg||words!=vis.words){if(bad++<5)printf("2F39E0 mismatch %u: %s%s%s\n",n,diff.c_str(),get<uint32_t>(gp+0xA0C)!=lcg?" lcg":"",words!=vis.words?" visual":"");}
    }
    printf("0x2F39E0 camera splash (+0x2F4330 / 0x2F4260): 60000 cases (%u with crystal draws, %u visual draws), %u mismatches\n",spawns,visuals,bad);total+=bad;
    if(total){printf("FAILED %u\n",total);return 1;}puts("weather oracle: all cases match");return 0;
}
