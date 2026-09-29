// Development-only shared harness for the original handplant instruction oracles
// (tests/handplant_*_reference.cpp). The original ELF is mapped at ram+0xFF000 so
// gp constants and tables resolve; every region below is private scratch memory.
#pragma once
#include "ps2_runtime_macros.h"
#include "../engine/handplant.hpp"
#include <fstream>
#include <cstring>
#include <cstdio>
#include <cfenv>
#include <random>
#include <vector>
#include <string>
#include <bit>
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x420000,g_ps2RecompiledFunctionTableSlotCount=0xc8000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xc8000]={};
namespace hp {
using Quad=ssx::HandplantQuad;
constexpr uint32_t RIDER=0x20000,OWNER=0x30000,MOTION=OWNER+0x110,CONTROL=OWNER+0x370,GEOMETRY=0x40000,BONES=0x41000,
    ANIM=0x50000,PRED=0x60000,SCORER=0x70000,WORLD=0x80000,SEQ=0x90000,DESC=0xA0000,STAT=0xB0000,OUT=0xC0000,WORD=0xD0000,
    STACK=0x1F000,DONE=0x12345678,BONE_INDEX=5;
inline uint8_t* M=nullptr;
template<class T> T rd(uint32_t a){T v;std::memcpy(&v,M+a,sizeof v);return v;}
template<class T> void wr(uint32_t a,const T& v){std::memcpy(M+a,&v,sizeof v);}
template<class T> void wrTo(std::vector<uint8_t>& img,uint32_t a,const T& v){std::memcpy(img.data()+a,&v,sizeof v);}
inline uint32_t fb(float f){return std::bit_cast<uint32_t>(f);}
inline bool same(float a,float b){return fb(a)==fb(b);}
inline bool sameQ(const Quad& a,const Quad& b){for(int k=0;k<4;k++)if(!same(a[k],b[k]))return false;return true;}
inline Quad rq(uint32_t a){return rd<Quad>(a);}

inline std::vector<uint8_t> loadElf(const char* path){
    std::ifstream f(path,std::ios::binary);std::vector<uint8_t> elf((std::istreambuf_iterator<char>(f)),{});
    if(elf.size()<0x100000)throw std::runtime_error("Missing original executable");
    std::vector<uint8_t> ram(32*1024*1024);std::memcpy(ram.data()+0xff000,elf.data(),elf.size());return ram;
}
inline void baseLayout(){
    wr(0x4ff120,Quad{0,0,0,0});wr(0x4ff140,Quad{1,0,0,0});wr(0x4ff160,Quad{0,0,1,0});
    wr(RIDER+0x77c,OWNER);wr(RIDER+0x780,GEOMETRY);wr(GEOMETRY+0x2c,BONES);wr(RIDER+0x8a8,BONE_INDEX);
    wr(RIDER+0x784,ANIM);wr(RIDER+0x788,PRED);wr(RIDER+0x790,SCORER);wr(RIDER+0x860,WORLD);
    wr(MOTION+0xa0,RIDER);wr(CONTROL+0x14,RIDER);
}
inline R5900Context context(uint32_t pc){
    R5900Context c{};c.pc=pc;c.vu0_vf[0]=_mm_set_ps(1,0,0,0);
    SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,29,STACK);SET_GPR_U32(&c,31,DONE);return c;
}
inline void ret(R5900Context* c){c->pc=GPR_U32(c,31);}

// Native <-> source memory mapping of the rider fields handplant touches.
inline void storeValue(std::vector<uint8_t>* img,uint32_t a,const ssx::GroundControlValue& v){
    if(img){wrTo(*img,a,v.current);wrTo(*img,a+4,v.rate);wrTo(*img,a+8,v.target);}else{wr(a,v.current);wr(a+4,v.rate);wr(a+8,v.target);}
}
template<class T> inline void put(std::vector<uint8_t>* img,uint32_t a,const T& v){if(img)wrTo(*img,a,v);else wr(a,v);}
// rebuilt: 0x11E098 also stores +0x1D0 = position quad and zero row w lanes.
inline void storeRider(const ssx::OriginalHandplantRider& r,std::vector<uint8_t>* img=nullptr,bool rebuilt=false,float rightW=0){
    auto R=RIDER;
    put(img,R+0x110,r.position);put(img,R+0x11c,r.positionW);put(img,R+0x120,r.quaternion);put(img,R+0x188,r.presentedUpZ);
    put(img,R+0x1a0,r.right);put(img,R+0x1ac,rebuilt?0.f:rightW);put(img,R+0x1b0,r.forward);put(img,R+0x1bc,r.forwardW);put(img,R+0x1c0,r.up);put(img,R+0x1cc,r.upW);
    if(rebuilt){put(img,R+0x1d0,r.position);put(img,R+0x1dc,r.positionW);}
    put(img,R+0x1e0,r.velocity);put(img,R+0x1ec,r.velocityW);
    storeValue(img,R+0x1f0,r.turn);storeValue(img,R+0x1fc,r.animationTurn);storeValue(img,R+0x208,r.extraLean);
    storeValue(img,R+0x244,r.lean244);storeValue(img,R+0x250,r.roll250);
    put(img,R+0x300,r.timeScale);put(img,R+0x320,r.reverseStance);put(img,R+0x32c,r.phase);
    put(img,BONES+BONE_INDEX*32,r.handBone);put(img,BONES+BONE_INDEX*32+12,r.handBoneW);
}
inline void storeMotion(const ssx::OriginalHandplantMotionState& m,std::vector<uint8_t>* img=nullptr){
    auto A=MOTION;put(img,A,m.start);put(img,A+0x10,m.exitVelocity);put(img,A+0x20,m.tangent);put(img,A+0x30,m.lip);put(img,A+0x40,m.toLip);
    put(img,A+0x50,m.predictorPosition);put(img,A+0x60,m.predictorVelocity);put(img,A+0x70,m.axis);
    put(img,A+0x80,m.rate);put(img,A+0x84,m.duration);put(img,A+0x88,m.elapsed);put(img,A+0x8c,m.entrySpeed);put(img,A+0x90,m.railFlag);put(img,A+0x94,m.side);
}
inline void storeControl(const ssx::OriginalHandplantControlState& c,std::vector<uint8_t>* img=nullptr){
    auto A=CONTROL;put(img,A,c.timer);put(img,A+4,c.smoothed);put(img,A+8,c.fast);put(img,A+0xc,c.railFlag);put(img,A+0x10,c.side);
}
// Byte-exact comparison of every watched region; reports the first difference.
struct Region{uint32_t begin,end;const char* name;};
inline const std::vector<Region>& regions(){static const std::vector<Region> r{{RIDER,RIDER+0xa00,"rider"},{OWNER,OWNER+0xe00,"owner"},{BONES,BONES+0x200,"bones"},
    {ANIM,ANIM+0x100,"animator"},{PRED,PRED+0x100,"predictor"},{SCORER,SCORER+0x200,"scorer"},{SEQ,SEQ+0x100,"sequence"},{DESC,DESC+0x40,"descriptor"},{0x4ff100,0x4ff180,"constants"}};return r;}
// Copies only the watched regions (the image is reused between cases).
inline std::vector<uint8_t>& snapshot(){
    static std::vector<uint8_t> image(0x500000);
    for(auto& g:regions())std::memcpy(image.data()+g.begin,M+g.begin,g.end-g.begin);
    return image;
}
inline bool compareImage(const std::vector<uint8_t>& expected,const char* what,unsigned test){
    for(auto& g:regions())for(uint32_t a=g.begin;a<g.end;a+=4){
        uint32_t x,y;std::memcpy(&x,M+a,4);std::memcpy(&y,expected.data()+a,4);
        if(x!=y){std::printf("%s mismatch case %u: %s+0x%x original %08x (%.9g) native %08x (%.9g)\n",what,test,g.name,a-g.begin,x,std::bit_cast<float>(x),y,std::bit_cast<float>(y));return false;}
    }
    return true;
}
// Ordered external call log: {address, integer argument, float argument bits}.
struct Call{uint32_t fn=0;int32_t arg=0;uint32_t value=0;bool operator==(const Call&)const=default;};
inline bool compareCalls(const std::vector<Call>& original,const std::vector<Call>& native,const char* what,unsigned test){
    if(original==native)return true;
    std::printf("%s call sequence mismatch case %u\n original:",what,test);for(auto& c:original)std::printf(" %x(%d,%08x)",c.fn,c.arg,c.value);
    std::printf("\n native:  ");for(auto& c:native)std::printf(" %x(%d,%08x)",c.fn,c.arg,c.value);std::printf("\n");return false;
}
// Random helpers.
struct Rand{
    std::mt19937 g;explicit Rand(uint32_t seed):g(seed){}
    float uni(float lo,float hi){return std::uniform_real_distribution<float>(lo,hi)(g);}
    unsigned n(unsigned k){return g()%k;}
    bool coin(unsigned k=2){return g()%k==0;}
    ssx::RailVector vec(float s){return {uni(-s,s),uni(-s,s),uni(-s,s)};}
    ssx::RailVector unit(){for(;;){auto v=vec(1);float l=v[0]*v[0]+v[1]*v[1]+v[2]*v[2];if(l>1e-4f&&l<=1){l=std::sqrt(l);return {v[0]/l,v[1]/l,v[2]/l};}}}
    Quad quat(){for(;;){Quad q{uni(-1,1),uni(-1,1),uni(-1,1),uni(-1,1)};float l=q[0]*q[0]+q[1]*q[1]+q[2]*q[2]+q[3]*q[3];if(l>1e-3f&&l<=1){l=std::sqrt(l);for(auto& x:q)x/=l;return q;}}}
    // Heading-only rotation about +Z, optionally tilted by a small angle.
    Quad yaw(float angle,float tilt=0){
        float h=angle*.5f;Quad z{0,0,std::sin(h),std::cos(h)};if(tilt==0)return z;
        auto axis=unit();axis[2]=0;float l=std::sqrt(axis[0]*axis[0]+axis[1]*axis[1]);if(l<1e-3f)axis={1,0,0};else{axis[0]/=l;axis[1]/=l;}
        float t=tilt*.5f;Quad d{axis[0]*std::sin(t),axis[1]*std::sin(t),0,std::cos(t)};
        return {d[3]*z[0]+z[3]*d[0]+d[1]*z[2]-d[2]*z[1],d[3]*z[1]+z[3]*d[1]+d[2]*z[0]-d[0]*z[2],d[3]*z[2]+z[3]*d[2]+d[0]*z[1]-d[1]*z[0],d[3]*z[3]-d[0]*z[0]-d[1]*z[1]-d[2]*z[2]};
    }
    // Values around a threshold: exact, one ulp either side, signed zeros, or spread.
    float near(float t,float spread){
        switch(n(6)){case 0:return t;case 1:return std::nextafter(t,INFINITY);case 2:return std::nextafter(t,-INFINITY);default:return t+uni(-spread,spread);}
    }
};
inline void setBasis(ssx::OriginalHandplantRider& r){
    auto b=ssx::originalRebuildOrientation(r.quaternion);r.right=b.right;r.forward=b.forward;r.up=b.up;
}
}
