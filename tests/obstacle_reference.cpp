// Development-only comparison against the original SSX3 obstacle response.
#include "ps2_runtime_macros.h"
#include "../engine/obstacle_collision.hpp"
#include "../engine/body_collision.hpp"
#include "../engine/collision_transform.hpp"
#include <fstream>
#include <cstring>
#include <iostream>
#include <random>
void sub_0013AA48_0x13aa48(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0013F488_0x13f488(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0032A1C0_0x32a1c0(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0032AA28_0x32aa28(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00329B90_0x329b90(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00334888_0x334888(uint8_t*,R5900Context*,PS2Runtime*);
void sub_001065B0_0x1065b0(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0031C228_0x31c228(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0031BE50_0x31be50(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0032B6A8_0x32b6a8(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00329590_0x329590(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0032B2B8_0x32b2b8(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000;
extern const uint32_t g_ps2RecompiledFunctionTableEnd=0x340000;
extern const uint32_t g_ps2RecompiledFunctionTableSlotCount=(0x340000-0x100000)/4;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[(0x340000-0x100000)/4]={};
static ssx::terrain_original::Vector translation,relativeDirection;
static bool moved,bounced;
struct TransformCaptured {};
static ssx::collision_transform::Matrix capturedMatrix;
static float capturedReciprocal;
static void captureTransform(uint8_t* m,R5900Context* c,PS2Runtime*) {
    std::memcpy(&capturedMatrix,m+GPR_U32(c,5),64);capturedReciprocal=c->f[12];throw TransformCaptured{};
}
static void noEffect(uint8_t*,R5900Context* c,PS2Runtime*){c->pc=GPR_U32(c,31);}
static void translate(uint8_t* m,R5900Context* c,PS2Runtime*) {
    std::memcpy(&translation,m+GPR_U32(c,5),12);moved=true;c->pc=GPR_U32(c,31);
}
static void notifyCollision(uint8_t* m,R5900Context* c,PS2Runtime*) {
    std::memcpy(&relativeDirection,m+GPR_U32(c,5)+16,12);bounced=true;c->pc=GPR_U32(c,31);
}
int main(int argc,char** argv) {
    using namespace ssx::terrain_original;
    PS2Runtime runtime;std::vector<uint8_t> memory(32*1024*1024);
    if(argc!=3)return 17;std::ifstream ee(argv[2],std::ios::binary);ee.read((char*)memory.data(),memory.size());if(!ee)return 18;
    auto write=[&](uint32_t at,const auto& v){std::memcpy(memory.data()+at,&v,sizeof(v));};
    for(uint32_t pc:{0x32f708,0x1065b0,0x11e098,0x28b180,0x2989a8})if(!runtime.registerFunction(pc,noEffect))return 2;
    if(!runtime.registerFunction(0x106538,translate)||!runtime.registerFunction(0x105d98,notifyCollision))return 3;
    write(0x4a30f0-0x6fa0,-.9998999834060669f);write(0x4a30f0-0x6f9c,1.100000023841858f);
    write(0x4a30f0-0x6f98,.05000000074505806f);write(0x4a30f0-0x6f94,27.77777862548828f);
    std::mt19937 random(0x4f425354);std::uniform_real_distribution<float> direction(-1,1),speed(-3000,3000),depth(0,90);
    unsigned moves=0,bounces=0;
    for(unsigned i=0;i<10000;++i) {
        Vector normal{direction(random),direction(random),direction(random)},ground{0,0,1},velocity{speed(random),speed(random),speed(random)},surface{speed(random)*.1f,speed(random)*.1f,speed(random)*.1f};
        if(i%100==0)normal={0,0,-1};
        float length=std::sqrt(dot(normal,normal));for(auto& x:normal)x/=length;
        float penetration=depth(random);
        auto native=ssx::originalObstacleResponse(penetration,ground,normal,velocity,surface);
        constexpr uint32_t stack=0x10000,owner=0x20000,rider=0x30000,done=0x12345678;
        write(owner+24,rider);write(rider+0x370,ground);write(rider+0x37c,0.f);write(rider+0x1e0,velocity);write(rider+0x1ec,0.f);
        write(stack+16,normal);write(stack+28,0.f);write(stack+32,surface);write(stack+44,0.f);write(stack+0x430,uint64_t(done));
        R5900Context c{};c.pc=0x13f5c4;c.f[20]=penetration;c.vu0_vf[0]=_mm_set_ps(1,0,0,0);
        SET_GPR_U32(&c,29,stack);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,18,owner);SET_GPR_U32(&c,19,0x40000);
        translation={};relativeDirection={};moved=bounced=false;
        Rounding rounding;sub_0013F488_0x13f488(memory.data(),&c,&runtime);
        if(c.pc!=done)return 4;
        Vector actual;std::memcpy(&actual,memory.data()+rider+0x1e0,12);
        if(moved!=native.moved||bounced!=native.bounced||actual!=native.velocityCmps||translation!=native.translationCm||relativeDirection!=native.relativeDirection) {
            std::cerr<<"Original obstacle response mismatch "<<i<<" moved "<<moved<<'/'<<native.moved<<" bounced "<<bounced<<'/'<<native.bounced<<'\n';
            for(unsigned k=0;k<3;++k)std::cerr<<"velocity "<<actual[k]<<'/'<<native.velocityCmps[k]<<" push "<<translation[k]<<'/'<<native.translationCm[k]<<'\n';return 1;
        }
        moves+=moved;bounces+=bounced;
    }
    std::cout<<"10000 original obstacle responses match exactly: "<<moves<<" pushes, "<<bounces<<" velocity responses\n";
    moves=bounces=0;
    for(unsigned i=0;i<10000;++i) {
        Vector normal{direction(random),direction(random),direction(random)},velocity{speed(random),speed(random),speed(random)};
        float length=std::sqrt(dot(normal,normal));for(auto&x:normal)x/=length;if(i%77==0)velocity={};
        float penetration=i%100?depth(random):-1.f;auto native=ssx::originalAirObstacleResponse(penetration,normal,velocity);
        constexpr uint32_t stack=0x10000,owner=0x20000,rider=0x30000,query=0x40000,out=0x50000,done=0x12345678;
        write(owner+4,rider);write(rider+0x1e0,velocity);write(rider+0x1ec,0.f);write(stack+16,normal);write(stack+28,0.f);write(stack+0x420,uint64_t(done));
        R5900Context c{};c.pc=0x13aaf0;c.f[0]=penetration;c.f[21]=0;c.vu0_vf[0]=_mm_set_ps(1,0,0,0);
        SET_GPR_U32(&c,29,stack);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,17,owner);SET_GPR_U32(&c,18,query);SET_GPR_U32(&c,19,query);SET_GPR_U32(&c,20,out);
        translation={};relativeDirection={};moved=bounced=false;Rounding rounding;sub_0013AA48_0x13aa48(memory.data(),&c,&runtime);if(c.pc!=done)return 30;
        Vector actual;std::memcpy(&actual,memory.data()+rider+0x1e0,12);
        if(moved!=native.moved||bounced!=native.bounced||actual!=native.velocityCmps||translation!=native.translationCm||relativeDirection!=native.relativeDirection||bool(GPR_U32((&c),2))!=native.bounced){std::cerr<<"Original air obstacle response mismatch "<<i<<'\n';return 31;}
        moves+=moved;bounces+=bounced;
    }
    std::cout<<"10000 original airborne obstacle responses match exactly: "<<moves<<" pushes, "<<bounces<<" velocity responses\n";
    if(!runtime.memory().initialize())return 5;
    std::ifstream vu(argv[1],std::ios::binary);vu.read((char*)runtime.memory().getVU0Code(),4096);if(!vu)return 6;
    if(!runtime.registerFunction(0x32b6a8,sub_0032B6A8_0x32b6a8))return 7;
    for(int offset:{-0x2de0,-0x2dd8,-0x2dd0,-0x2dc8})write(0x4a30f0+offset,9.999999747378752e-5f);
    for(int offset:{-0x2ddc,-0x2dd4,-0x2dcc,-0x2dc4})write(0x4a30f0+offset,10000000000.f);
    std::uniform_real_distribution<float> coordinate(-100,100),radius(10,45);
    unsigned contacts=0;
    for(unsigned i=0;i<5000;++i) {
        Rounding rounding;
        Vector a{coordinate(random),coordinate(random),coordinate(random)},b{coordinate(random),coordinate(random),coordinate(random)},d{coordinate(random),coordinate(random),coordinate(random)};
        auto normal=cross(difference(b,a),difference(d,a));float inverse=ssx::terrain_original::div(1.f,std::sqrt(dot(normal,normal)));for(auto& x:normal)x=mul(x,inverse);
        ssx::BodyCollisionVolume body;body.broadRadiusCm=250;body.count=10;body.activeMask=i%5?random()&1023:0;
        for(auto& sphere:body.spheres){sphere.centerCm={coordinate(random),coordinate(random),coordinate(random)};sphere.radiusCm=radius(random);}
        bool doubleSided=i%2;
        auto native=ssx::originalBodyTriangleContact(body,a,b,d,normal,doubleSided);
        constexpr uint32_t volume=0x20000,vertices=0x30000,output=0x40000,done=0x12345678;
        write(volume+16,body.broadCenterCm);write(volume+28,1.f);write(volume+32,body.broadRadiusCm);write(volume+40,body.activeMask);write(volume+44,body.count);
        for(unsigned j=0;j<body.count;++j){write(volume+48+j*32,body.spheres[j].centerCm);write(volume+60+j*32,1.f);write(volume+64+j*32,body.spheres[j].radiusCm);}
        write(vertices,a);write(vertices+12,1.f);write(vertices+16,b);write(vertices+28,1.f);write(vertices+32,d);write(vertices+44,1.f);write(vertices+48,normal);write(vertices+60,0.f);
        R5900Context c{};c.pc=doubleSided?0x32aa28:0x32a1c0;c.vu0_vf[0]=_mm_set_ps(1,0,0,0);
        SET_GPR_U32(&c,29,0x10000);SET_GPR_U32(&c,31,done);SET_GPR_U32(&c,28,0x4a30f0);
        SET_GPR_U32(&c,4,volume);SET_GPR_U32(&c,5,vertices);SET_GPR_U32(&c,6,vertices+16);SET_GPR_U32(&c,7,vertices+32);
        SET_GPR_U32(&c,8,vertices+48);SET_GPR_U32(&c,9,output);SET_GPR_U32(&c,10,output+16);
        if(doubleSided)sub_0032AA28_0x32aa28(memory.data(),&c,&runtime);else sub_0032A1C0_0x32a1c0(memory.data(),&c,&runtime);if(c.pc!=done)return 8;
        bool hit=GPR_U32((&c),2)!=0;
        Vector push,point;std::memcpy(&push,memory.data()+output,12);std::memcpy(&point,memory.data()+output+16,12);
        if(hit!=native.hit||(hit&&(push!=native.translationCm||point!=native.pointCm))) {
            std::cerr<<"Original sphere/triangle mismatch "<<i<<" hit "<<hit<<'/'<<native.hit<<'\n';return 9;
        }
        contacts+=hit;
    }
    std::cout<<"5000 original body-sphere/triangle cases match exactly; "<<contacts<<" contacts\n";
    if(!runtime.registerFunction(0x329590,sub_00329590_0x329590))return 10;
    write(0x4a30f0-0x2df0,1.0000000150474662e30f);write(0x4a30f0-0x2dec,1.0000000150474662e30f);
    write(0x4ff140,std::array<float,4>{1,0,0,0});write(0x4ff150,std::array<float,4>{0,1,0,0});write(0x4ff160,std::array<float,4>{0,0,1,0});
    unsigned boxContacts=0;
    for(unsigned i=0;i<5000;++i) {
        Rounding rounding;
        Vector low{coordinate(random),coordinate(random),coordinate(random)},high;
        for(unsigned k=0;k<3;++k)high[k]=low[k]+radius(random)*2;
        ssx::BodyCollisionVolume body;body.broadCenterCm={coordinate(random),coordinate(random),coordinate(random)};body.broadRadiusCm=radius(random);body.count=10;body.activeMask=i%5?random()&0x7fff:0;
        auto native=ssx::originalBodyBoxContact(body,low,high);
        constexpr uint32_t volume=0x20000,bounds=0x30000,output=0x40000,done=0x12345678;
        write(volume+16,body.broadCenterCm);write(volume+28,1.f);write(volume+32,body.broadRadiusCm);write(volume+40,body.activeMask);write(volume+44,body.count);
        write(bounds,low);write(bounds+12,1.f);write(bounds+16,high);write(bounds+28,1.f);
        R5900Context c{};c.pc=0x32b2b8;c.vu0_vf[0]=_mm_set_ps(1,0,0,0);
        SET_GPR_U32(&c,29,0x10000);SET_GPR_U32(&c,31,done);SET_GPR_U32(&c,28,0x4a30f0);
        SET_GPR_U32(&c,4,volume);SET_GPR_U32(&c,5,bounds);SET_GPR_U32(&c,6,bounds+16);SET_GPR_U32(&c,7,output);SET_GPR_U32(&c,8,output+16);
        sub_0032B2B8_0x32b2b8(memory.data(),&c,&runtime);if(c.pc!=done)return 11;
        bool hit=GPR_U32((&c),2)!=0;Vector push,point;std::memcpy(&push,memory.data()+output,12);std::memcpy(&point,memory.data()+output+16,12);
        if(hit!=native.hit||(hit&&(push!=native.translationCm||point!=native.pointCm))){std::cerr<<"Original sphere/box mismatch "<<i<<" hit "<<hit<<'/'<<native.hit<<'\n';return 12;}
        boxContacts+=hit;
    }
    std::cout<<"5000 original body-sphere/box cases match exactly; "<<boxContacts<<" contacts\n";
    if(!runtime.registerFunction(0x32f840,captureTransform))return 13;
    auto randomMatrix=[&]() {
        auto m=ssx::collision_transform::identity;float a=direction(random)*3;
        m[0]=m[5]=std::cos(a);m[1]=std::sin(a);m[4]=-m[1];
        m[12]=speed(random)*30;m[13]=speed(random)*30;m[14]=speed(random)*30;return m;
    };
    for(unsigned test=0;test<1000;++test) {
        using namespace ssx::collision_transform;
        auto instanceMatrix=randomMatrix();std::array<Matrix,3> nodes{randomMatrix(),randomMatrix(),randomMatrix()};float scale=.25f+radius(random)/10;if(test%7==0)scale=-scale;
        auto world=instanceMatrix;for(auto& node:nodes)world=scaledNode(node,world,scale);auto expected=inverseRigid(world);
        constexpr uint32_t instance=0x60000,model=0x61000,table=0x62000,descriptor=0x63000,query=0x64000,vtable=0x65000;
        std::memset(memory.data()+instance,0,0x6000);write(instance+16,instanceMatrix);write(instance+0x80,model);write(instance+0x84,scale);write(instance+0x88,descriptor);
        write(model+4,uint32_t(3));write(model+8,table);write(descriptor,uint32_t(2));
        write(query+0x50,vtable);write(vtable+12,uint32_t(0x32f840));
        for(unsigned i=0;i<3;++i){write(table+i*16,i?uint32_t(i-1):uint32_t(0xffffffff));write(table+i*16+4,i==2?uint32_t(0x66000):uint32_t(0));write(table+i*16+12,uint32_t(0x67000+i*64));write(0x67000+i*64,nodes[i]);}
        R5900Context c{};c.pc=0x334888;c.vu0_vf[0]=_mm_set_ps(1,0,0,0);SET_GPR_U32(&c,29,0x90000);SET_GPR_U32(&c,28,0x4a30f0);
        SET_GPR_U32(&c,4,query);SET_GPR_U32(&c,5,instance);SET_GPR_U32(&c,6,0xa0000);SET_GPR_U32(&c,7,64);SET_GPR_U32(&c,8,0);
        bool captured=false;try {Rounding rounding;sub_00334888_0x334888(memory.data(),&c,&runtime);}catch(const TransformCaptured&){captured=true;}
        float reciprocal;{Rounding rounding;reciprocal=ssx::collision_scalar::divide(1.f,scale);}
        if(!captured||capturedMatrix!=expected||capturedReciprocal!=reciprocal){std::cerr<<"Original node/instance transform mismatch "<<test<<" reciprocal "<<std::hexfloat<<capturedReciprocal<<'/'<<reciprocal<<'\n';for(unsigned k=0;k<16;++k)if(capturedMatrix[k]!=expected[k])std::cerr<<k<<':'<<capturedMatrix[k]<<'/'<<expected[k]<<' ' ;return 14;}
        ssx::BodyCollisionVolume body;body.broadCenterCm={speed(random),speed(random),speed(random)};body.broadRadiusCm=radius(random);body.count=10;
        for(auto& sphere:body.spheres){sphere.centerCm={speed(random),speed(random),speed(random)};sphere.radiusCm=radius(random);}
        auto local=toLocal(body,expected,scale);constexpr uint32_t volume=0x20000,transform=0x30000,done=0x12345678;
        write(volume+16,body.broadCenterCm);write(volume+28,1.f);write(volume+32,body.broadRadiusCm);write(volume+44,body.count);write(transform,expected);
        for(unsigned i=0;i<body.count;++i){write(volume+48+i*32,body.spheres[i].centerCm);write(volume+60+i*32,1.f);write(volume+64+i*32,body.spheres[i].radiusCm);}
        R5900Context context{};context.pc=0x329b90;context.f[12]=reciprocal;context.vu0_vf[0]=_mm_set_ps(1,0,0,0);
        SET_GPR_U32(&context,29,0x10000);SET_GPR_U32(&context,31,done);SET_GPR_U32(&context,4,volume);SET_GPR_U32(&context,5,transform);
        {Rounding rounding;sub_00329B90_0x329b90(memory.data(),&context,&runtime);}
        Vector point;float r;std::memcpy(&point,memory.data()+volume+16,12);std::memcpy(&r,memory.data()+volume+32,4);
        if(point!=local.broadCenterCm||r!=local.broadRadiusCm)return 15;
        for(unsigned i=0;i<body.count;++i){std::memcpy(&point,memory.data()+volume+48+i*32,12);std::memcpy(&r,memory.data()+volume+64+i*32,4);if(point!=local.spheres[i].centerCm||r!=local.spheres[i].radiusCm)return 16;}
    }
    std::cout<<"1000 original three-node/instance transforms and body-volume transforms match exactly\n";
    if(!runtime.registerFunction(0x31c228,sub_0031C228_0x31c228)||!runtime.registerFunction(0x31be50,sub_0031BE50_0x31be50))return 19;
    for(unsigned i=0;i<10000;++i) {
        std::array<float,4> q{direction(random),direction(random),direction(random),direction(random)};float qlen=std::sqrt(q[0]*q[0]+q[1]*q[1]+q[2]*q[2]+q[3]*q[3]);for(auto& x:q)x/=qlen;
        float x=q[0],y=q[1],z=q[2],w=q[3];Vector up{2*(x*z+y*w),2*(y*z-x*w),1-2*(x*x+y*y)},forward{2*(x*y-z*w),1-2*(x*x+z*z),2*(y*z+x*w)};
        Vector normal{direction(random),direction(random),direction(random)};float len=std::sqrt(dot(normal,normal));for(auto& a:normal)a/=len;if(i%100==0)normal=up;
        auto expected=ssx::originalObstacleOrientation(q,up,forward,normal);
        constexpr uint32_t rider=0x30000,hit=0x40000,done=0x12345678;
        write(rider+0x120,q);write(rider+0x1b0,forward);write(rider+0x1bc,0.f);write(rider+0x1c0,up);write(rider+0x1cc,0.f);write(hit+16,normal);write(hit+28,0.f);
        R5900Context c{};c.pc=0x1065b0;c.vu0_vf[0]=_mm_set_ps(1,0,0,0);SET_GPR_U32(&c,29,0x10000);SET_GPR_U32(&c,31,done);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,4,rider);SET_GPR_U32(&c,5,hit);
        {Rounding rounding;sub_001065B0_0x1065b0(memory.data(),&c,&runtime);}if(c.pc!=done)return 20;
        std::array<float,4> actual;std::memcpy(&actual,memory.data()+rider+0x120,16);
        if(actual!=expected){std::cerr<<"Original collision orientation mismatch "<<i<<'\n';for(unsigned k=0;k<4;++k)std::cerr<<actual[k]<<'/'<<expected[k]<<' ';std::cerr<<'\n';return 21;}
    }
    std::cout<<"10000 original collision-orientation cases match exactly\n";




}
