#include "ps2_runtime_macros.h"
#include "../engine/sphere_tree_collision.hpp"
#include <fstream>
#include <cstring>
#include <iostream>
#include <random>
void sub_0032CDB0_0x32cdb0(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00327F18_0x327f18(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000;
extern const uint32_t g_ps2RecompiledFunctionTableEnd=0x340000;
extern const uint32_t g_ps2RecompiledFunctionTableSlotCount=(0x340000-0x100000)/4;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[(0x340000-0x100000)/4]={};
// Generated self-JAL uses a local goto; resume its saved internal return PC.
static void treeOracle(uint8_t* m,R5900Context* c,PS2Runtime* r) {
    do {sub_0032CDB0_0x32cdb0(m,c,r);} while(c->pc==0x32cf34);
}
int main(int argc,char** argv) {
    using namespace ssx::terrain_original;
    PS2Runtime runtime;std::vector<uint8_t> memory(32*1024*1024);
    if(argc!=2)return 2;std::ifstream ee(argv[1],std::ios::binary);ee.read((char*)memory.data(),memory.size());if(!ee)return 3;
    if(!runtime.registerFunction(0x32cdb0,treeOracle))return 4;
    auto write=[&](uint32_t at,const auto& v){std::memcpy(memory.data()+at,&v,sizeof(v));};
    constexpr uint32_t treeAt=0x20000,header=0x20100,levelAt=0x21000,maskAt=0x22000,bodyAt=0x30000,out=0x31000,done=0x12345678;
    std::mt19937 random(0x53504852);std::uniform_real_distribution<float> coordinate(-350,350),radius(5,150);
    unsigned hits=0;
    for(unsigned trial=0;trial<20000;++trial) {
        Rounding rounding;ssx::CollisionSphereTree tree;unsigned depth=trial%4;unsigned stride=1,total=0;
        tree.centerCm={coordinate(random)*.1f,coordinate(random)*.1f,coordinate(random)*.1f};tree.radiusScale=trial%3?1.f:.85f;
        for(unsigned i=0;i<=depth;++i){tree.levels.push_back({300.f/float(1u<<i),150.f/float(1u<<i),stride});total+=stride;stride*=8;}
        tree.masks.resize(total);for(auto& value:tree.masks)value=uint8_t(random());if(trial%7==0)tree.masks[0]=0;
        ssx::BodyCollisionVolume body;body.broadCenterCm={coordinate(random),coordinate(random),coordinate(random)};body.broadRadiusCm=radius(random)+100;body.count=trial%3?10:0;body.activeMask=random()&1023;
        for(unsigned i=0;i<body.count;++i){body.spheres[i].centerCm=body.broadCenterCm;for(auto& x:body.spheres[i].centerCm)x=add(x,coordinate(random)*.1f);body.spheres[i].radiusCm=radius(random)*.2f;}
        auto native=ssx::originalBodySphereTreeContact(body,tree);
        for(unsigned child=0;child<8;++child){std::array<float,4> direction{child&4?1.f:-1.f,child&2?1.f:-1.f,child&1?1.f:-1.f,0};write(treeAt+child*16,direction);}
        write(treeAt+0x80,tree.centerCm);write(treeAt+0x8c,1.f);write(treeAt+0x90,tree.radiusScale);write(treeAt+0x98,header);
        write(header+8,0u);write(header+12,depth);write(header+32,levelAt);write(header+40,maskAt);
        std::memcpy(memory.data()+levelAt,tree.levels.data(),tree.levels.size()*12);std::memcpy(memory.data()+maskAt,tree.masks.data(),tree.masks.size());
        write(bodyAt+16,body.broadCenterCm);write(bodyAt+28,1.f);write(bodyAt+32,body.broadRadiusCm);write(bodyAt+40,body.activeMask);write(bodyAt+44,body.count);
        for(unsigned i=0;i<body.count;++i){write(bodyAt+48+i*32,body.spheres[i].centerCm);write(bodyAt+60+i*32,1.f);write(bodyAt+64+i*32,body.spheres[i].radiusCm);}
        R5900Context c{};c.pc=0x327f18;c.vu0_vf[0]=_mm_set_ps(1,0,0,0);SET_GPR_U32(&c,29,0x10000);SET_GPR_U32(&c,31,done);SET_GPR_U32(&c,28,0x4a30f0);
        SET_GPR_U32(&c,4,treeAt);SET_GPR_U32(&c,5,bodyAt);SET_GPR_U32(&c,6,out);SET_GPR_U32(&c,7,out+16);
        sub_00327F18_0x327f18(memory.data(),&c,&runtime);if(c.pc!=done){std::cerr<<"Incomplete PC "<<std::hex<<c.pc<<" trial "<<std::dec<<trial<<"\n";return 5;}
        bool hit=GPR_U32((&c),2)!=0;Vector push,point;std::memcpy(&push,memory.data()+out,12);std::memcpy(&point,memory.data()+out+16,12);
        if(hit!=native.hit||(hit&&(push!=native.translationCm||point!=native.pointCm))){std::cerr<<"Sphere tree mismatch "<<trial<<" hit "<<hit<<'/'<<native.hit<<'\n';for(unsigned k=0;k<3;++k)std::cerr<<push[k]<<'/'<<native.translationCm[k]<<" point "<<point[k]<<'/'<<native.pointCm[k]<<'\n';return 1;}hits+=hit;
    }
    std::cout<<"20000 original sphere-tree/body queries match exactly: "<<hits<<" contacts\n";
}
