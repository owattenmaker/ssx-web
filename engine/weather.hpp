#pragma once
// Original SSX3 (PS2 USA, SLUS_207.72) weather simulation (docs/weather.md):
//  - the Weather painter record the snowfall object reads per camera (0x2E5DA0 getters 0x2EE448..0x2EE978 on the
//    camera's environment block 6/7, world painter type 12 tWPIGD_Weather, class 0x484058);
//  - the snowfall layers 0x2E4F50 (4 flake layers + 2 fluff layers per camera, built by 0x2E5920 / 0x2E4D88): wind
//    gusts from the visual LCG gp+0xA0C, the scrolling offset the VU1 program 5 wraps the flakes with;
//  - the camera splash 0x2F39E0 (snow drops and ice crystals on the lens, vtable 0x488230): pending units from the
//    snowfall (0x2F4330) and big impacts (0x2F4260), spawns 0x2F3810 / 0x2F3640 / 0x2F37A8 / 0x2F2598 / 0x2F2D70,
//    updates 0x2F2810 (drops) / 0x2F3030 (crystals), the crystal pick with the shared visual RNG 0x4FF018.
// Float policy (as engine/one_way_volume.hpp): EE add.s / sub.s originalScalarAdd/Subtract (guard bit), EE mul.s
// terrain_original::mul, EE div.s / sqrt.s originalScalarDivide / originalScalarSqrt, cvt.w.s truncation; VU lanes
// terrain_original add/sub/mul/sqrt under OriginalRounding (chop).
#include "flag_cloth.hpp"
#include "original_float.hpp"
#include "terrain_contact_math.hpp"
#include <array>
#include <bit>
#include <cstdint>
#include <functional>
#include <vector>

namespace ssx {
namespace weather {
using Quad=std::array<float,4>;
// EE cvt.w.s: truncation toward zero, saturating.
inline int32_t cvtWS(float x){if(x!=x)return 0x7FFFFFFF;if(x>=2147483648.f)return 0x7FFFFFFF;if(x<=-2147483648.f)return int32_t(0x80000000u);return int32_t(x);}
// gp+0xA0C: w = ((w * 0x18FCD + 0xE9507C) & 0x7FFFFF) | 0x3F800000, read back as a float in [1, 2).
struct Lcg {uint32_t& word;uint32_t draws=0;float next(){word=((word*0x18FCDu+0xE9507Cu)&0x7FFFFFu)|0x3F800000u;++draws;return std::bit_cast<float>(word);}};
inline float unit(float r){return originalScalarSubtract(r,1.f);}
inline Quad vmul(const Quad& a,float s){Quad r;for(unsigned k=0;k<4;k++)r[k]=terrain_original::mul(a[k],s);return r;}
inline Quad vadd(const Quad& a,const Quad& b){Quad r;for(unsigned k=0;k<4;k++)r[k]=terrain_original::add(a[k],b[k]);return r;}
inline Quad vsub(const Quad& a,const Quad& b){Quad r;for(unsigned k=0;k<4;k++)r[k]=terrain_original::sub(a[k],b[k]);return r;}
inline float vdot(const Quad& a,const Quad& b){ // vmul, vadday.x, vmaddaz.x (1*z), vmaddw.x (1*w)
    using namespace terrain_original;const Quad p{mul(a[0],b[0]),mul(a[1],b[1]),mul(a[2],b[2]),mul(a[3],b[3])};
    return add(add(add(p[0],p[1]),mul(1.f,p[2])),mul(1.f,p[3]));
}
inline const float kRadians=std::bit_cast<float>(0x3c8efa36u); // gp-0x3A18 0.017453294
inline const float kTable=std::bit_cast<float>(0x42a2f983u);   // gp-0x3A14 81.48733
}

// ---- Weather painter record (0x2E5DA0: 16 floats per camera) --------------------------------------------------------
// current = the camera block's Weather properties (+0x08 + 8 i); shelter = block +0x24 (0 for the camera blocks);
// disabled = gp+0xA70 (0 in the race). Test values (gp+0x164C "Enable Test Values") are not modelled.
using OriginalWeatherRecord=std::array<float,16>;
inline OriginalWeatherRecord originalWeatherRecord(const std::array<float,19>& c,float shelter=0.f,bool disabled=false){
    using namespace terrain_original;OriginalRounding rounding;OriginalWeatherRecord r{};
    const float open=originalScalarSubtract(1.f,shelter);
    r[0]=disabled?0.f:mul(c[0],open); // 0x2EE448 snowfall intensity (0x158)
    r[1]=mul(c[2],open);               // 0x2EE508 snowfall wind (0x168)
    r[2]=disabled?0.f:c[8];            // 0x2EE600 fluff intensity (0x198)
    r[3]=c[9];                         // 0x2EE660 fluff wind (0x1A0)
    r[4]=c[1];                         // 0x2EE4C0 flake size (0x160)
    r[5]=c[3];                         // 0x2EE570 wind direction, degrees (0x170)
    r[6]=c[6];                         // 0x2EE5B8 flurries (0x188)
    r[7]=c[7];                         // 0x2EE6A8 gravity multiplier (0x190)
    for(unsigned i=0;i<8;i++)r[8+i]=c[11+i]; // 0x2EE780..0x2EE978: flake A R G B, fluff A R G B (0x1B0..0x1E8)
    return r;
}

// ---- Snowfall layer (0x2E4D88 object, 0xC0 bytes) -------------------------------------------------------------------
struct OriginalSnowfallLayer {
    int32_t camera=0,kind=0,count=0;               // +0x00, +0x04 (0 flakes, 1 fluff), +0x08
    float extent=0,speed=0,size=0,alpha=0,gravity=0; // +0x0C, +0x10, +0x14, +0x18, +0x1C
    weather::Quad offset{0,0,0,1},gustNew{},gustOld{}; // +0x20, +0x30, +0x40
    float timer=0,period=0;                         // +0x90, +0x94
    std::array<uint32_t,3> seeds{};                 // +0x98..+0xA0 (visual RNG words drawn at the load)
    std::array<float,3> colour{1,1,1};              // +0xA4..+0xAC
    int32_t first=1;                                // +0xB0
};
// 0x2E4F50 (one game update; fps = [gp+0x2A74]+0x10 = 60). Returns the LCG draws (4 per gust).
inline uint32_t originalSnowfallLayerUpdate(OriginalSnowfallLayer& L,const OriginalWeatherRecord& rec,uint32_t& lcgWord,int32_t fps=60){
    using namespace terrain_original;using namespace weather;OriginalRounding rounding;Lcg lcg{lcgWord};
    if(L.kind==0){L.alpha=rec[8];L.colour={rec[9],rec[10],rec[11]};L.count=cvtWS(mul(rec[0],500.f));L.speed=mul(rec[1],500.f);L.size=rec[4];}
    else if(L.kind==1){L.alpha=rec[12];L.colour={rec[13],rec[14],rec[15]};L.count=cvtWS(mul(rec[2],6.f));L.speed=mul(rec[3],250.f);}
    const float dt=originalScalarDivide(1.f,float(fps));
    L.timer=originalScalarSubtract(L.timer,dt);
    if(L.timer<0.f){
        const float r1=lcg.next();
        L.gustOld=L.gustNew;
        const float S=L.speed,negS=-S;
        float angle=mul(rec[5],kRadians);
        const float flurries=rec[6];
        const float r2=lcg.next();
        angle=mul(angle,kTable);
        const float span=originalScalarSubtract(S,negS);
        const float r3=lcg.next();
        const float calm=originalScalarSubtract(1.f,flurries);
        float rx=mul(span,unit(r1));
        const int32_t index=cvtWS(angle)&0x1FF;
        rx=originalScalarAdd(negS,rx);
        float ry=mul(span,unit(r2));
        const float sine=originalFlagSine(unsigned(index)),cosine=originalFlagSine(unsigned(index+0x80));
        const float r4=lcg.next();
        float rz=mul(span,unit(r3));
        float wy=mul(S,sine),wx=mul(S,cosine);
        ry=originalScalarAdd(negS,ry);
        rx=mul(flurries,rx);ry=mul(flurries,ry);
        wy=mul(wy,calm);
        rz=originalScalarAdd(negS,rz);
        wx=mul(wx,calm);
        wy=originalScalarAdd(wy,ry);
        const float wz=mul(flurries,rz);
        wx=originalScalarAdd(wx,rx);
        L.gustNew={wx,wy,wz,0.f};
        const float T=originalScalarAdd(mul(unit(r4),4.f),1.f);
        L.timer=T;L.period=T;
        if(L.first){L.gustOld=L.gustNew;L.first=0;}
    }
    const float t=originalScalarDivide(L.timer,L.period);
    const float w=originalScalarSubtract(1.f,t);
    Quad gust=vadd(vmul(L.gustNew,w),vmul(L.gustOld,t));
    L.offset=vadd(L.offset,vmul(gust,dt));
    const float fall=mul(mul(L.gravity,rec[7]),dt);
    L.offset=vadd(L.offset,vmul(Quad{0,0,1,0},fall)); // 0x4FF160
    const float C=L.extent,half=mul(C,.5f);
    auto wrap=[&](float v){
        if(half<v){float q=originalScalarDivide(v,C);q=originalScalarSubtract(q,float(cvtWS(q)));return originalScalarSubtract(mul(q,C),C);}
        if(v<-half){float q=originalScalarDivide(v,C);q=originalScalarSubtract(q,float(cvtWS(q)));return originalScalarAdd(mul(q,C),C);}
        return v;
    };
    L.offset={wrap(L.offset[0]),wrap(L.offset[1]),wrap(L.offset[2]),1.f};
    return lcg.draws;
}
// 0x2E5430 (the render, once per drawn frame): offset -= the camera's movement since the last draw, wrapped as above.
inline void originalSnowfallCameraShift(OriginalSnowfallLayer& L,const weather::Quad& delta){
    using namespace terrain_original;using namespace weather;OriginalRounding rounding;
    L.offset=vsub(L.offset,delta);
    const float C=L.extent,half=mul(C,.5f);
    auto wrap=[&](float v){
        if(half<v){float q=originalScalarDivide(v,C);q=originalScalarSubtract(q,float(cvtWS(q)));return originalScalarSubtract(mul(q,C),C);}
        if(v<-half){float q=originalScalarDivide(v,C);q=originalScalarSubtract(q,float(cvtWS(q)));return originalScalarAdd(mul(q,C),C);}
        return v;
    };
    L.offset={wrap(L.offset[0]),wrap(L.offset[1]),wrap(L.offset[2]),1.f};
}

// ---- Camera splash (0x2F3550 object, vtable 0x488230) ---------------------------------------------------------------
// Tweakables (debug menu "Camera Splash Menu" 0x24BF60, gp+0x115C..; values as set in the race).
struct OriginalSplashTweaks {
    bool enable=true,render=true;                               // 0x115C, 0x1160
    int32_t maxDrops=30,maxCrystals=24;                         // 0x1164, 0x1168
    float crystalShare=.6f,crystalSpawn=.01f;                   // 0x116C, 0x1170
    int32_t groupMin=1,groupMax=3,spawnPerCrystal=2;            // 0x1174, 0x1178, 0x117C
    float impactDistance=450.f,impactIntensity=105.f,impactSnowfall=1.f,impactMultiplier=std::bit_cast<float>(0x3f8ccccdu); // 0x1180..0x118C
    float snowfallMin=1.5f,snowfallMultiplier=std::bit_cast<float>(0x3c75c28fu); // 0x1190, 0x1194
    float pushoffSpeed=50.f;                                    // 0x1198 Min Velocity For Pushoff (km/h)
    float dropLifeMin=6.f,dropLifeMax=9.f,dropSizeMin=4.f,dropSpawnSize=6.f,dropSizeDiff=10.f; // 0x119C..0x11AC
    float dropJiggleXMin=std::bit_cast<float>(0xbcf5c28fu),dropJiggleXMax=std::bit_cast<float>(0x3cf5c28fu); // 0x11B0, 0x11B4
    float dropJiggleYMin=std::bit_cast<float>(0xbc23d70au),dropJiggleYMax=std::bit_cast<float>(0x3d23d70au); // 0x11B8, 0x11BC
    float dropGrowMin=std::bit_cast<float>(0xbcf5c28fu),dropGrowMax=std::bit_cast<float>(0x3d8f5c29u);       // 0x11C0, 0x11C4
    float dropPushMin=std::bit_cast<float>(0x3ca3d70au),dropPushMax=std::bit_cast<float>(0x3d8f5c29u);       // 0x11C8, 0x11CC
    float dropMaxSpeed=6.f,dropStepMin=0.f,dropStepMax=1.f,dropSpin=360.f;                                  // 0x11D0..0x11DC
    float groupSpreadX=18.f,groupSpreadY=14.f;                  // 0x11E0, 0x11E4
    float crystalLifeMin=8.f,crystalLifeMax=12.f,crystalSizeMin=6.f,crystalSizeMax=10.f,crystalSizeDiff=15.f; // 0x11E8..0x11F8
    float crystalAccelXMin=std::bit_cast<float>(0xbc23d70au),crystalAccelXMax=std::bit_cast<float>(0x3c23d70au); // 0x11FC, 0x1200
    float crystalAccelYMin=0.f,crystalAccelYMax=std::bit_cast<float>(0x3ccccccdu);                         // 0x1204, 0x1208
    float crystalGustMin=1.5f,crystalGustMax=4.f;               // 0x120C, 0x1210
    float crystalGrowMin=std::bit_cast<float>(0xbd4ccccdu),crystalGrowMax=std::bit_cast<float>(0x3dcccccdu); // 0x1214, 0x1218
    float crystalPushMin=std::bit_cast<float>(0x3ca3d70au),crystalPushMax=std::bit_cast<float>(0x3ca3d70au); // 0x121C, 0x1220
    float crystalMaxSpeed=5.f,crystalSpin=360.f;                // 0x1224, 0x1228
    float centreX=320.f,centreY=240.f;                          // gp+0x24C8 / 0x24CC
    std::array<float,4> dropColourA{.6f,.8f,.85f,1.f},dropColourB{.8f,.8f,.85f,1.f};       // 0x5046E0 / 0x5046F0 (A R G B)
    std::array<float,4> crystalColourA{.6f,.8f,.85f,1.f},crystalColourB{.8f,.8f,.85f,1.f}; // 0x504700 / 0x504710
};
struct OriginalSplashDrop {   // 0x3C bytes at +0x1C
    int32_t alive=0;float x=0,y=0,w=0,h=0,vx=0,vy=0,age=0,life=0;std::array<float,4> colour{};float angle=0,spin=0;
};
struct OriginalSplashCrystal { // 0x4C bytes at +0x724
    int32_t spawned=0,alive=0;float x=0,y=0,w=0,h=0,vx=0,vy=0,age=0,life=0;std::array<float,4> colour{};float angle=0,spin=0,timer=0,ax=0,ay=0;
};
struct OriginalCameraSplash {
    int32_t camera=0;                                      // +0x10
    std::vector<OriginalSplashDrop> drops;                 // +0x14 count, +0x1C
    std::vector<OriginalSplashCrystal> crystals;           // +0x18 count, +0x724
    int32_t hasPrevious=0;weather::Quad previous{0,0,0,1}; // +0x100C, +0x1010
    float speed=0,pending=0,snowfall=0;                    // +0x1020 (km/h), +0x1024, +0x1028
};
struct OriginalSplashDraws {uint32_t lcg=0,visual=0;};
namespace splash_detail {
inline float lerp4(float a,float b,float u,float v){return originalScalarAdd(terrain_original::mul(a,v),terrain_original::mul(b,u));} // a*(1-u) + b*u
// 0x2F2598: a drop at pos with the size bounds (a2 = [nominal, max]).
inline void spawnDrop(OriginalSplashDrop& d,float x,float y,const std::array<float,2>& size,const OriginalSplashTweaks& k,weather::Lcg& lcg){
    using namespace terrain_original;using weather::unit;
    const float r1=lcg.next();const float diff=k.dropSizeDiff,negDiff=-diff,span=originalScalarSubtract(diff,negDiff);
    const float r2=lcg.next();
    float w=mul(originalScalarSubtract(size[0],k.dropSizeMin),unit(r1));w=originalScalarAdd(k.dropSizeMin,w);
    const float delta=originalScalarAdd(negDiff,mul(span,unit(r2)));const float h0=originalScalarAdd(w,delta);
    float h=k.dropSizeMin;if(k.dropSizeMin<=h0)h=std::min(h0,size[1]);
    d.age=0;
    const float r3=lcg.next();
    d.alive=1;d.life=originalScalarAdd(k.dropLifeMin,mul(originalScalarSubtract(k.dropLifeMax,k.dropLifeMin),unit(r3)));
    d.w=w;d.h=h;d.x=x;d.y=y;d.vx=0;d.vy=0; // 0x4A5960 = (0, 0)
    const float r4=lcg.next();d.angle=mul(unit(r4),360.f);
    const float r5=lcg.next();d.spin=originalScalarAdd(-k.dropSpin,mul(originalScalarSubtract(k.dropSpin,-k.dropSpin),unit(r5)));
    const float r6=lcg.next();const float u=unit(r6),v=originalScalarSubtract(1.f,u);
    for(unsigned c=0;c<4;c++)d.colour[c]=lerp4(k.dropColourA[c],k.dropColourB[c],u,v);
}
// 0x2F2D70: a crystal (index-th of its group: 0.7^index size).
inline void spawnCrystal(OriginalSplashCrystal& c,float x,float y,int32_t index,const OriginalSplashTweaks& k,weather::Lcg& lcg){
    using namespace terrain_original;using weather::unit;
    const float r1=lcg.next();const float diff=k.crystalSizeDiff,negDiff=-diff,span=originalScalarSubtract(diff,negDiff);
    const float r2=lcg.next();
    float w=originalScalarAdd(k.crystalSizeMin,mul(originalScalarSubtract(k.crystalSizeMax,k.crystalSizeMin),unit(r1)));
    const float h0=originalScalarAdd(w,originalScalarAdd(negDiff,mul(span,unit(r2))));
    float h=k.crystalSizeMin;if(k.crystalSizeMin<=h0)h=std::min(h0,k.crystalSizeMax);
    const float shrink=std::bit_cast<float>(0x3f333333u); // gp-0x3910 0.7
    for(int32_t i=index;i>0;i--){w=mul(w,shrink);h=mul(h,shrink);}
    const float r3=lcg.next();
    c.age=0;c.alive=1;c.life=originalScalarAdd(k.crystalLifeMin,mul(originalScalarSubtract(k.crystalLifeMax,k.crystalLifeMin),unit(r3)));
    c.w=w;c.h=h;c.x=x;c.y=y;c.vx=0;c.vy=0;
    const float r4=lcg.next();const float u=unit(r4),v=originalScalarSubtract(1.f,u);
    for(unsigned q=0;q<4;q++)c.colour[q]=lerp4(k.crystalColourA[q],k.crystalColourB[q],u,v);
    const float r5=lcg.next();c.angle=mul(unit(r5),360.f);
    const float r6=lcg.next();
    c.spawned=0;c.timer=0;c.ax=0;c.ay=0;
    c.spin=originalScalarAdd(-k.crystalSpin,mul(originalScalarSubtract(k.crystalSpin,-k.crystalSpin),unit(r6)));
}
// The push away from the screen centre, scaled by the camera speed (0x2F2810 / 0x2F3030).
inline std::array<float,2> pushDirection(float x,float y,const OriginalSplashTweaks& k){
    using namespace terrain_original;
    const float dx=originalScalarSubtract(x,k.centreX),dy=originalScalarSubtract(y,k.centreY);
    const float len=originalScalarSqrt(originalScalarAdd(mul(dx,dx),mul(dy,dy)));
    if(len==0.f)return {dx,dy};
    const float inv=originalScalarDivide(1.f,len);return {mul(dx,inv),mul(dy,inv)};
}
inline float pushAmount(float speed,const OriginalSplashTweaks& k){
    const float f=originalScalarDivide(originalScalarSubtract(speed,k.pushoffSpeed),originalScalarSubtract(120.f,k.pushoffSpeed));
    return 0.f<=f?std::min(f,1.f):0.f;
}
inline void clampSpeed(float& vx,float& vy,float speed2,float limit){
    using namespace terrain_original;
    const float s=originalScalarSqrt(speed2);
    if(!(limit<s))return;
    float nx=vx,ny=vy;
    if(s!=0.f){const float inv=originalScalarDivide(1.f,s);ny=mul(vy,inv);nx=mul(vx,inv);}
    vx=mul(nx,limit);vy=mul(ny,limit);
}
inline bool offScreen(float x,float y,float w,float h){
    return originalScalarAdd(x,w)<=0.f||640.f<=originalScalarSubtract(x,w)||originalScalarAdd(y,h)<=0.f||480.f<=originalScalarSubtract(y,h);
}
// 0x2F2810
inline void updateDrop(OriginalSplashDrop& d,float speed,const OriginalSplashTweaks& k,weather::Lcg& lcg){
    using namespace terrain_original;using weather::unit;
    const float r1=lcg.next();
    d.age=originalScalarAdd(d.age,std::bit_cast<float>(0x3c888889u)); // gp-0x3918 1/60
    d.vx=originalScalarAdd(d.vx,originalScalarAdd(k.dropJiggleXMin,mul(originalScalarSubtract(k.dropJiggleXMax,k.dropJiggleXMin),unit(r1))));
    const float r2=lcg.next();
    d.vy=originalScalarAdd(d.vy,originalScalarAdd(k.dropJiggleYMin,mul(originalScalarSubtract(k.dropJiggleYMax,k.dropJiggleYMin),unit(r2))));
    const auto dir=pushDirection(d.x,d.y,k);const float amount=pushAmount(speed,k);
    const float r3=lcg.next();
    const float px=mul(dir[0],amount),py=mul(dir[1],amount);
    const float push=originalScalarAdd(k.dropPushMin,mul(originalScalarSubtract(k.dropPushMax,k.dropPushMin),unit(r3)));
    d.vy=originalScalarAdd(d.vy,mul(py,push));d.vx=originalScalarAdd(d.vx,mul(px,push));
    const float speed2=originalScalarAdd(mul(d.vx,d.vx),mul(d.vy,d.vy));
    const float r4=lcg.next();
    d.w=originalScalarAdd(d.w,originalScalarAdd(k.dropGrowMin,mul(originalScalarSubtract(k.dropGrowMax,k.dropGrowMin),unit(r4))));
    const float r5=lcg.next();
    d.h=originalScalarAdd(d.h,originalScalarAdd(k.dropGrowMin,mul(originalScalarSubtract(k.dropGrowMax,k.dropGrowMin),unit(r5))));
    clampSpeed(d.vx,d.vy,speed2,k.dropMaxSpeed);
    const float r6=lcg.next();
    const float lx=mul(d.vx,k.dropStepMin),hx=mul(d.vx,k.dropStepMax);
    const float r7=lcg.next();
    const float ly=mul(d.vy,k.dropStepMin),hy=mul(d.vy,k.dropStepMax);
    const float stepX=originalScalarAdd(lx,mul(originalScalarSubtract(hx,lx),unit(r6)));
    const float stepY=originalScalarAdd(ly,mul(originalScalarSubtract(hy,ly),unit(r7)));
    d.x=originalScalarAdd(d.x,stepX);d.y=originalScalarAdd(d.y,stepY);
    if(d.life<=d.age||offScreen(d.x,d.y,d.w,d.h))d.alive=0;
}
// 0x2F3030
inline void updateCrystal(OriginalSplashCrystal& c,float speed,const OriginalSplashTweaks& k,weather::Lcg& lcg){
    using namespace terrain_original;using weather::unit;
    const float dt=std::bit_cast<float>(0x3c888889u); // gp-0x390C
    c.timer=originalScalarSubtract(c.timer,dt);c.age=originalScalarAdd(c.age,dt);
    if(c.timer<0.f){
        const float r1=lcg.next();const float r2=lcg.next();const float r3=lcg.next();
        c.timer=originalScalarAdd(k.crystalGustMin,mul(originalScalarSubtract(k.crystalGustMax,k.crystalGustMin),unit(r1)));
        c.ax=originalScalarAdd(k.crystalAccelXMin,mul(originalScalarSubtract(k.crystalAccelXMax,k.crystalAccelXMin),unit(r2)));
        c.ay=originalScalarAdd(k.crystalAccelYMin,mul(originalScalarSubtract(k.crystalAccelYMax,k.crystalAccelYMin),unit(r3)));
    }
    c.vx=originalScalarAdd(c.vx,c.ax);c.vy=originalScalarAdd(c.vy,c.ay);
    const auto dir=pushDirection(c.x,c.y,k);const float amount=pushAmount(speed,k);
    const float r4=lcg.next();
    const float px=mul(dir[0],amount),py=mul(dir[1],amount);
    const float push=originalScalarAdd(k.crystalPushMin,mul(originalScalarSubtract(k.crystalPushMax,k.crystalPushMin),unit(r4)));
    c.vy=originalScalarAdd(c.vy,mul(py,push));c.vx=originalScalarAdd(c.vx,mul(px,push));
    const float speed2=originalScalarAdd(mul(c.vx,c.vx),mul(c.vy,c.vy));
    const float r5=lcg.next();
    c.w=originalScalarAdd(c.w,originalScalarAdd(k.crystalGrowMin,mul(originalScalarSubtract(k.crystalGrowMax,k.crystalGrowMin),unit(r5))));
    const float r6=lcg.next();
    c.h=originalScalarAdd(c.h,originalScalarAdd(k.crystalGrowMin,mul(originalScalarSubtract(k.crystalGrowMax,k.crystalGrowMin),unit(r6))));
    clampSpeed(c.vx,c.vy,speed2,k.crystalMaxSpeed);
    c.x=originalScalarAdd(c.x,c.vx);c.y=originalScalarAdd(c.y,c.vy);
    if(c.life<=c.age||offScreen(c.x,c.y,c.w,c.h))c.alive=0;
}
// 0x2F37A8
inline void addDrop(OriginalCameraSplash& s,float x,float y,const std::array<float,2>& size,const OriginalSplashTweaks& k,weather::Lcg& lcg){
    if(!(int32_t(s.drops.size())<k.maxDrops))return;
    OriginalSplashDrop d;spawnDrop(d,x,y,size,k,lcg);s.drops.push_back(d);
}
// 0x2F3640
inline void addCrystalGroup(OriginalCameraSplash& s,float x,float y,int32_t count,const OriginalSplashTweaks& k,weather::Lcg& lcg){
    using namespace terrain_original;using weather::unit;
    for(int32_t i=0;i<count;i++){
        if(!(int32_t(s.crystals.size())<k.maxCrystals))continue;
        const float r1=lcg.next();const float r2=lcg.next();
        const float dx=originalScalarAdd(-k.groupSpreadX,mul(originalScalarSubtract(k.groupSpreadX,-k.groupSpreadX),unit(r1)));
        const float dy=originalScalarAdd(-k.groupSpreadY,mul(originalScalarSubtract(k.groupSpreadY,-k.groupSpreadY),unit(r2)));
        OriginalSplashCrystal c;spawnCrystal(c,originalScalarAdd(x,dx),originalScalarAdd(y,dy),i,k,lcg);s.crystals.push_back(c);
    }
}
}
// 0x2F4330: the snowfall object's per-camera input (group 2, before the splash update).
inline void originalSplashSnowfall(OriginalCameraSplash& s,float snowfall,const OriginalSplashTweaks& k={}){
    using namespace terrain_original;OriginalRounding rounding;
    s.snowfall=snowfall;
    if(!(k.snowfallMin<snowfall))return;
    const float scale=mul(k.snowfallMultiplier,std::bit_cast<float>(0x3bfc0fc1u)); // gp-0x38F4 1/130
    s.pending=originalScalarAdd(s.pending,mul(mul(snowfall,originalScalarAdd(s.speed,10.f)),scale));
}
// 0x2F4260: a large snow impact (0x2E1598 -> 0x2F4118 for each camera) at position with the impact strength (cm/s).
inline void originalSplashImpact(OriginalCameraSplash& s,const weather::Quad& position,float strength,const OriginalSplashTweaks& k={}){
    using namespace terrain_original;using namespace weather;OriginalRounding rounding;
    const Quad d=vsub(s.previous,position);const float len=terrain_original::sqrt(vdot(d,d));
    const float near=originalScalarSubtract(1.f,originalScalarDivide(len,k.impactDistance));
    if(!(0.f<near))return;if(!(k.impactSnowfall<s.snowfall))return;
    const float f=originalScalarDivide(originalScalarSubtract(mul(strength,std::bit_cast<float>(0x3d1374bcu)),k.impactIntensity),originalScalarSubtract(120.f,k.impactIntensity)); // gp-0x38F8 0.036
    const float g=0.f<=f?std::min(f,1.f):0.f;
    s.pending=originalScalarAdd(s.pending,mul(mul(near,g),k.impactMultiplier));
}
// 0x2F39C8 (vtable +0x70).
inline void originalSplashReset(OriginalCameraSplash& s){s.snowfall=0;s.hasPrevious=0;s.pending=0;s.drops.clear();s.crystals.clear();}
// 0x2F39E0 (group 2, after the flag manager): cameraPosition = camera +0x20 (w lane included). visual() draws 0x4FF018.
inline OriginalSplashDraws originalSplashUpdate(OriginalCameraSplash& s,const weather::Quad& cameraPosition,uint32_t& lcgWord,
        const std::function<uint32_t()>& visual,const OriginalSplashTweaks& k={}){
    using namespace terrain_original;using namespace weather;OriginalRounding rounding;Lcg lcg{lcgWord};OriginalSplashDraws out;
    if(!k.enable)return out;
    if(s.hasPrevious){
        const Quad d=vsub(cameraPosition,s.previous);
        const float len=terrain_original::sqrt(vdot(d,d));
        s.speed=mul(mul(len,std::bit_cast<float>(0x426fffffu)),std::bit_cast<float>(0x3d1374bcu)); // gp-0x3904 59.999996, gp-0x3900 0.036
    }else{s.speed=0;s.hasPrevious=1;}
    if(1000.f<s.speed){originalSplashReset(s);s.speed=0;}
    s.previous=cameraPosition;
    // 0x2F3810
    {
        int32_t n=cvtWS(s.pending);s.pending=originalScalarSubtract(s.pending,float(n));
        while(n>0){
            const float r1=lcg.next();const float x=mul(unit(r1),640.f);
            const float r2=lcg.next();const float y=mul(unit(r2),480.f);
            const float r3=lcg.next();
            if(unit(r3)<k.crystalShare){
                const uint32_t r=visual();++out.visual;
                const uint32_t range=uint32_t(k.groupMax-k.groupMin);
                int32_t group=k.groupMin+int32_t(r%range);if(!(0<group))group=1;
                splash_detail::addCrystalGroup(s,x,y,group,k,lcg);n-=group;
            }else{splash_detail::addDrop(s,x,y,{k.dropSpawnSize,k.dropSpawnSize},k,lcg);n-=1;}
        }
    }
    for(size_t i=0;i<s.drops.size();){
        splash_detail::updateDrop(s.drops[i],s.speed,k,lcg);
        if(s.drops[i].alive){++i;continue;}
        s.drops[i]=s.drops.back();s.drops.pop_back();
    }
    int32_t chosen=-1;
    {
        const uint32_t r=visual();++out.visual; // 0x2F3BE8
        const float u=originalScalarSubtract(std::bit_cast<float>((r&0x7FFFFFu)|0x3F800000u),1.f);
        const float f=originalScalarDivide(mul(u,float(int32_t(s.crystals.size()))),float(k.maxCrystals));
        if(f<k.crystalSpawn&&!s.crystals.empty()){const uint32_t r2=visual();++out.visual;chosen=int32_t(r2%uint32_t(s.crystals.size()));} // 0x2F3C4C
    }
    for(size_t i=0;i<s.crystals.size();){
        auto& c=s.crystals[i];splash_detail::updateCrystal(c,s.speed,k,lcg);
        if(!c.alive){s.crystals[i]=s.crystals.back();s.crystals.pop_back();continue;}
        if(int32_t(i)==chosen&&c.spawned<k.spawnPerCrystal){
            ++c.spawned;const float sx=mul(c.w,.6f),sy=mul(c.h,.6f); // gp-0x38FC
            if(k.dropSizeMin<sx&&k.dropSizeMin<sy)splash_detail::addDrop(s,c.x,c.y,{sx,sy},k,lcg);
        }
        ++i;
    }
    out.lcg=lcg.draws;return out;
}
}
