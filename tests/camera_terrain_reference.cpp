#include "ps2_runtime_macros.h"
#include "../engine/terrain_contact_math.hpp"
#include <fstream>
#include <cstring>
#include <iostream>
#include <random>
void sub_0032E100_0x32e100(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0032B6E0_0x32b6e0(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0032B6A8_0x32b6a8(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x340000,g_ps2RecompiledFunctionTableSlotCount=(0x340000-0x100000)/4;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[(0x340000-0x100000)/4]={};
int main(int argc,char**argv) {
    using namespace ssx::terrain_original;
    if(argc!=3)return 2;PS2Runtime runtime;runtime.memory().initialize();runtime.setMissingFunctionPolicy(PS2Runtime::MissingFunctionPolicy::Stop);
    std::vector<uint8_t> m(32*1024*1024);std::ifstream ee(argv[1],std::ios::binary);ee.read((char*)m.data(),m.size());std::ifstream vu(argv[2],std::ios::binary);vu.read((char*)runtime.memory().getVU0Code(),4096);if(!ee||!vu)return 3;
    runtime.registerFunction(0x32e9a0,sub_0032E100_0x32e100);runtime.registerFunction(0x32e4d0,sub_0032E100_0x32e100);runtime.registerFunction(0x32b6a8,sub_0032B6A8_0x32b6a8);
    auto get=[&](uint32_t at){uint32_t x;std::memcpy(&x,m.data()+at,4);return x;};auto put=[&](uint32_t at,const auto& x){std::memcpy(m.data()+at,&x,sizeof(x));};
    uint32_t nearby=get(0x14701a0+0x860),manager=get(get(nearby)+0xa4),buckets=get(manager+8),bucketCount=get(manager+12);
    std::mt19937 random(0x50524544);std::uniform_real_distribution<float> offset(-500,500),fraction(.01,.99);unsigned cases=0,hits=0,refinedChanges=0;
    for(unsigned patchIndex=0;patchIndex<get(nearby+0x10c);++patchIndex) {
        uint32_t patch=get(nearby+0x110+patchIndex*4),id=get(patch+0x150),entry=get(buckets+(id%bucketCount)*4);
        while(entry&&get(entry)!=id)entry=get(entry+12);if(!entry)continue;
        std::array<Vector,100> grid;for(unsigned i=0;i<100;++i)std::memcpy(&grid[i],m.data()+entry+16+i*16,12);
        for(unsigned trial=0;trial<1000;++trial) {
            Rounding rounding;auto pivot=grid[random()%100];Vector origin=pivot,end=pivot;
            for(unsigned k=0;k<3;++k){origin[k]=add(origin[k],offset(random));end[k]=add(end[k],offset(random));}
            GroundProbe probe;probe.origin=origin;probe.direction=difference(end,origin);probe.explicitEnd=end;probe.hasExplicitEnd=true;probe.preferredFraction=.5f;
            auto native=coarseContact(grid,probe);
            constexpr uint32_t stack=0x60000,query=0x70000,ends=0x71000,output=0x72000,done=0x12345678;
            put(ends,origin);put(ends+12,1.f);put(ends+16,end);put(ends+28,1.f);
            R5900Context c{};c.vu0_vf[0]=_mm_set_ps(1,0,0,0);c.pc=0x32e100;c.f[12]=.5f;
            SET_GPR_U32(&c,29,stack);SET_GPR_U32(&c,31,done);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,4,query);SET_GPR_U32(&c,5,ends);SET_GPR_U32(&c,6,ends+16);SET_GPR_U32(&c,7,2);
            sub_0032E100_0x32e100(m.data(),&c,&runtime);if(c.pc!=done)return 4;
            c.pc=0x32b6e0;SET_GPR_U32(&c,4,query);SET_GPR_U32(&c,5,patch);SET_GPR_U32(&c,6,entry);SET_GPR_U32(&c,7,output);SET_GPR_U32(&c,8,output+16);SET_GPR_U32(&c,9,output+32);SET_GPR_U32(&c,10,output+48);SET_GPR_U32(&c,11,0);SET_GPR_U32(&c,31,done);
            sub_0032B6E0_0x32b6e0(m.data(),&c,&runtime);if(c.pc!=done){std::cerr<<"Incomplete "<<std::hex<<c.pc<<'\n';return 5;}
            bool hit=GPR_U32((&c),2)!=0;Vector point,normal;float f,u,v;std::memcpy(&point,m.data()+output,12);std::memcpy(&normal,m.data()+output+16,12);std::memcpy(&f,m.data()+output+32,4);std::memcpy(&u,m.data()+output+48,4);std::memcpy(&v,m.data()+output+52,4);
            float nu=ssx::originalScalarAdd(mul(native.cellU,.1111111119389534f),.0555555559694767f),nv=ssx::originalScalarAdd(mul(native.cellV,.1111111119389534f),.0555555559694767f);
            if(native.hit){
                Coefficients coefficients;for(unsigned i=0;i<16;++i)std::memcpy(&coefficients[i],m.data()+patch+0x40+(15-i)*16,12);
                auto refined=refine(coefficients,origin,probe.direction,nu,nv);nu=refined.u;nv=refined.v;
                if(refined.valid){refinedChanges+=native.point!=refined.point||native.normal!=refined.normal;native.point=refined.point;native.normal=refined.normal;}
            }
            if(hit!=native.hit||(hit&&(point!=native.point||normal!=native.normal||f!=native.fraction||u!=nu||v!=nv))){std::cerr<<"Camera terrain mismatch "<<cases<<" RID "<<id<<" hit "<<hit<<'/'<<native.hit<<" fraction "<<f<<'/'<<native.fraction<<" UV "<<u<<','<<v<<'/'<<nu<<','<<nv<<'\n';for(unsigned k=0;k<3;++k)std::cerr<<point[k]<<'/'<<native.point[k]<<" normal "<<normal[k]<<'/'<<native.normal[k]<<'\n';return 1;}++cases;hits+=hit;
        }
    }
    std::cout<<cases<<" original kind2 camera terrain queries match exactly: "<<hits<<" contacts, point/normal/fraction/cell UV; "<<refinedChanges<<" contacts differ from coarse geometry\n";
}
