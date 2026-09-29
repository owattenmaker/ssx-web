// Development-only original instruction conformance. Never linked into the app.
#include "ps2_runtime_macros.h"
#include "../engine/terrain_contact_math.hpp"
#include <cstring>
#include <fstream>
#include <iostream>
#include <random>
void sub_0032E100_0x32e100(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0032B6A8_0x32b6a8(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0013D1B8_0x13d1b8(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x32b6a8;
extern const uint32_t g_ps2RecompiledFunctionTableEnd=0x32b6ac;
extern const uint32_t g_ps2RecompiledFunctionTableSlotCount=1;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[1]={nullptr};
int main(int argc,char** argv) {
    using namespace ssx::terrain_original;
    if(argc!=3)return 2;
    std::ifstream input(argv[1]);unsigned count,ignored;input>>count>>ignored;
    std::vector<Coefficients> patches(count);
    for(auto& patch:patches) {
        unsigned resource;input>>resource;
        for(auto& c:patch) {double x,y,z;input>>x>>y>>z;c={float(x*100),float(-z*100),float(y*100)};}int authoredSurface;input>>authoredSurface;
    }
    PS2Runtime runtime;std::vector<uint8_t> ram(32*1024*1024);
    auto write=[&](uint32_t at,const auto& value){std::memcpy(ram.data()+at,&value,sizeof(value));};
    std::mt19937 random(0x535358);std::uniform_real_distribution<float> uv(.05,.95);
    unsigned passed=0;float maxPoint=0,maxNormal=0,maxUV=0;
    for(unsigned test=0;test<1000;++test) {
        const auto& patch=patches[(test*17)%count];float u=uv(random),v=uv(random);
        auto e=evaluate(patch,u,v);
        Vector direction{0,0,300};
        if(std::abs(e.du[0]*e.dv[1]-e.du[1]*e.dv[0])<100)continue;
        Vector origin{e.point[0],e.point[1],e.point[2]-100};
        float initialU=(std::floor(u*9)+.5f)/9,initialV=(std::floor(v*9)+.5f)/9;
        auto native=refine(patch,origin,direction,initialU,initialV);if(!native.valid)continue;
        constexpr uint32_t query=0x20000,surface=0x30000,params=0x40000,output=0x50000,done=0x12345678;
        R5900Context ctx{};ctx.vu0_vf[0]=_mm_set_ps(1,0,0,0);ctx.pc=0x32e9a0;
        SET_GPR_U32(&ctx,29,0x10000);SET_GPR_U32(&ctx,31,done);
        SET_GPR_U32(&ctx,4,query);SET_GPR_U32(&ctx,5,surface);SET_GPR_U32(&ctx,6,params);SET_GPR_U32(&ctx,7,params+4);
        SET_GPR_U32(&ctx,8,output);SET_GPR_U32(&ctx,9,output+16);SET_GPR_U32(&ctx,10,output+32);
        write(query+0x60,origin);write(query+0x6c,1.f);write(query+0x70,direction);write(query+0x7c,0.f);
        for(unsigned i=0;i<16;++i){write(surface+0x40+i*16,patch[15-i]);write(surface+0x4c+i*16,0.f);}
        write(params,initialU);write(params+4,initialV);
        Rounding rounding;sub_0032E100_0x32e100(ram.data(),&ctx,&runtime);
        if(ctx.pc!=done)return 3;
        Vector point,normal;float originalUV[2];
        std::memcpy(&point,ram.data()+output,12);std::memcpy(&normal,ram.data()+output+16,12);std::memcpy(originalUV,ram.data()+params,8);
        float pointError=0,normalError=0;
        for(unsigned k=0;k<3;++k){pointError=std::max(pointError,std::abs(point[k]-native.point[k]));normalError=std::max(normalError,std::abs(normal[k]-native.normal[k]));}
        float uvError=std::max(std::abs(originalUV[0]-native.u),std::abs(originalUV[1]-native.v));
        maxPoint=std::max(maxPoint,pointError);maxNormal=std::max(maxNormal,normalError);maxUV=std::max(maxUV,uvError);
        if(pointError>.0001f||normalError>2e-6f||uvError>2e-7f){std::cerr<<"Mismatch "<<test<<" point "<<pointError<<" normal "<<normalError<<" uv "<<uvError<<'\n';return 1;}
        ++passed;
    }
    std::cout<<passed<<" original refine cases passed; max point(cm) "<<maxPoint<<", normal "<<maxNormal<<", UV "<<maxUV<<'\n';
    if(!runtime.memory().initialize())return 4;
    std::ifstream vu(argv[2],std::ios::binary);vu.read(reinterpret_cast<char*>(runtime.memory().getVU0Code()),4096);if(!vu)return 5;
    std::uniform_real_distribution<float> randomCoordinate(-1000,1000),barycentric(-.25,1.25);
    unsigned inside=0,outside=0;
    for(unsigned i=0;i<10000;++i) {
        Vector a{randomCoordinate(random),randomCoordinate(random),randomCoordinate(random)};
        Vector b{randomCoordinate(random),randomCoordinate(random),randomCoordinate(random)};
        Vector c{randomCoordinate(random),randomCoordinate(random),randomCoordinate(random)};
        float u=barycentric(random),v=barycentric(random);Vector p;
        for(unsigned k=0;k<3;++k)p[k]=a[k]+(b[k]-a[k])*u+(c[k]-a[k])*v;
        R5900Context ctx{};ctx.vu0_vf[0]=_mm_set_ps(1,0,0,0);
        auto set=[&](unsigned reg,Vector v){ctx.vu0_vf[reg]=_mm_set_ps(1,v[2],v[1],v[0]);};
        set(10,a);set(11,b);set(12,c);set(13,p);
        Rounding rounding;runtime.executeVU0Microprogram(ram.data(),&ctx,0x6e0);
        bool original=ctx.vi[2]!=0,native=pointInTriangle(a,b,c,p);
        if(original!=native){std::cerr<<"Original VU barycentric mismatch "<<i<<" u "<<u<<" v "<<v<<" original "<<original<<" native "<<native<<'\n';return 6;}
        original?++inside:++outside;
    }
    std::cout<<"10000 original VU0 triangle inclusion cases passed: "<<inside<<" inside, "<<outside<<" outside\n";
    if(!runtime.registerFunction(0x32b6a8,sub_0032B6A8_0x32b6a8))return 7;
    write(0x4a30f0-0x2d7c,1.000000013351432e-10f);write(0x4a30f0-0x2d78,9.999999747378752e-5f);
    unsigned planeHits=0;
    for(unsigned i=0;i<1000;++i) {
        Rounding rounding;
        Vector a{randomCoordinate(random),randomCoordinate(random),randomCoordinate(random)};
        Vector b{randomCoordinate(random),randomCoordinate(random),randomCoordinate(random)};
        Vector c{randomCoordinate(random),randomCoordinate(random),randomCoordinate(random)};
        auto n=cross(difference(a,b),difference(b,c));float inverse=ssx::terrain_original::div(1.f,std::sqrt(dot(n,n)));for(auto&x:n)x=mul(x,inverse);
        GroundProbe probe;
        float u=barycentric(random),v=barycentric(random);
        for(unsigned k=0;k<3;++k){probe.origin[k]=a[k]+(b[k]-a[k])*u+(c[k]-a[k])*v-n[k]*100;probe.direction[k]=n[k]*300;}
        auto native=triangleContact(probe,a,b,c);
        constexpr uint32_t query=0x20000,vertices=0x30000,output=0x50000,done=0x12345678;
        R5900Context ctx{};ctx.vu0_vf[0]=_mm_set_ps(1,0,0,0);ctx.pc=0x32e4d0;
        SET_GPR_U32(&ctx,29,0x10000);SET_GPR_U32(&ctx,31,done);SET_GPR_U32(&ctx,28,0x4a30f0);
        SET_GPR_U32(&ctx,4,query);SET_GPR_U32(&ctx,5,vertices);SET_GPR_U32(&ctx,6,vertices+16);SET_GPR_U32(&ctx,7,vertices+32);
        SET_GPR_U32(&ctx,8,vertices+48);SET_GPR_U32(&ctx,9,output);SET_GPR_U32(&ctx,10,output+16);SET_GPR_U32(&ctx,11,output+32);
        write(query+0x60,probe.origin);write(query+0x6c,1.f);write(query+0x70,probe.direction);write(query+0x7c,0.f);
        write(vertices,a);write(vertices+12,1.f);write(vertices+16,b);write(vertices+28,1.f);write(vertices+32,c);write(vertices+44,1.f);write(vertices+48,n);write(vertices+60,0.f);
        sub_0032E100_0x32e100(ram.data(),&ctx,&runtime);
        if(ctx.pc!=done)return 8;
        bool original=GPR_U32((&ctx),2)!=0;
        if(original!=native.hit){std::cerr<<"Original coarse plane membership differs "<<i<<'\n';return 9;}
        if(original) {
            Vector point;float fraction;std::memcpy(&point,ram.data()+output,12);std::memcpy(&fraction,ram.data()+output+32,4);
            if(point!=native.point||fraction!=native.fraction){std::cerr<<"Original coarse plane arithmetic differs "<<i<<'\n';return 10;}
            ++planeHits;
        }
    }
    std::cout<<"1000 original coarse plane cases passed; "<<planeHits<<" hits match point and fraction exactly\n";
    write(0x4a30f0-0x7068,.30000001192092896f);
    std::uniform_real_distribution<float> normalHeight(-.95f,.95f);
    for(unsigned test=0;test<3000;++test) {
        constexpr uint32_t stack=0x10000,rider=0x50000,owner=0x51000,output=0x52000,clearance=0x53000,done=0x12345678;
        unsigned count=1+random()%12,selected=0;bool have=false,aligned=false;float metric=INFINITY;unsigned resource=0;
        Vector oldNormal{0,0,1},forward{1,0,0};write(rider+0x370,oldNormal);write(rider+0x37c,0.f);write(rider+0x1b0,forward);write(rider+0x1bc,0.f);
        write(owner+0x18,rider);write(stack+0x2140,uint64_t(done));
        for(unsigned i=0;i<count;++i) {
            uint32_t at=stack+0x100+i*0x80,patch=0x60000+i*0x200;
            float z=normalHeight(random);Vector normal{std::sqrt(1-z*z),0,z},point{float(i),12,34};
            float fraction=float(random()%17)/16;unsigned id=random()%1000;
            std::memset(ram.data()+at,0,0x80);write(at,point);write(at+12,1.f);write(at+16,normal);write(at+0x40,fraction);write(at+0x54,patch);write(patch+0x150,id);
            bool newAligned=z>=.30000001192092896f;float newMetric=std::abs(fraction-.5f);
            if(preferGroundCandidate(have,aligned,metric,resource,newAligned,newMetric,id)) {have=true;aligned=newAligned;metric=newMetric;resource=id;selected=i;}
        }
        R5900Context ctx{};ctx.vu0_vf[0]=_mm_set_ps(1,0,0,0);ctx.pc=0x13d42c;
        SET_GPR_U32(&ctx,29,stack);SET_GPR_U32(&ctx,28,0x4a30f0);SET_GPR_U32(&ctx,3,rider);SET_GPR_U32(&ctx,11,count);
        SET_GPR_U32(&ctx,17,owner);SET_GPR_U32(&ctx,18,output);SET_GPR_U32(&ctx,19,clearance);
        Rounding rounding;sub_0013D1B8_0x13d1b8(ram.data(),&ctx,&runtime);
        if(ctx.pc!=done)return 11;
        float actual;std::memcpy(&actual,ram.data()+output,4);
        if(actual!=float(selected)){std::cerr<<"Cruise candidate selection differs "<<test<<" expected "<<selected<<" actual "<<actual<<'\n';return 12;}
    }
    std::cout<<"3000 original cruise candidate-selection cases passed\n";



}
