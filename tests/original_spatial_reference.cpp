#include "ps2_runtime_macros.h"
#include "../engine/original_spatial.hpp"
#include <fstream>
#include <iostream>
#include <random>
#include <cstring>
void sub_00328F28_0x328f28(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x340000,g_ps2RecompiledFunctionTableSlotCount=(0x340000-0x100000)/4;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[(0x340000-0x100000)/4]={};
int main(int argc,char**argv){
    using namespace ssx;using namespace terrain_original;if(argc!=2)return 2;
    std::vector<uint8_t> m(32*1024*1024);std::ifstream in(argv[1],std::ios::binary);in.read((char*)m.data(),m.size());PS2Runtime runtime;
    auto put=[&](uint32_t at,const auto& x){std::memcpy(m.data()+at,&x,sizeof(x));};
    std::mt19937 random(0x4f435452);std::uniform_real_distribution<float> center(-1000000,1000000),extent(0,30000);
    for(unsigned i=0;i<30000;++i){Rounding rounding;Vector low{center(random),center(random),center(random)},high=low;for(unsigned k=0;k<3;++k)high[k]=add(high[k],i%3?extent(random):0);
        auto native=originalSpatialCell(low,high);put(0x20000,low);put(0x2000c,1.f);put(0x20010,high);put(0x2001c,1.f);
        R5900Context c{};c.vu0_vf[0]=_mm_set_ps(1,0,0,0);c.pc=0x328f28;SET_GPR_U32(&c,4,0x30000);SET_GPR_U32(&c,5,0x20000);SET_GPR_U32(&c,29,0x10000);SET_GPR_U32(&c,31,0x12345678);SET_GPR_U32(&c,28,0x4a30f0);
        sub_00328F28_0x328f28(m.data(),&c,&runtime);OriginalSpatialCell original;std::memcpy(&original,m.data()+0x30000,16);
        if(original.level!=native.level||original.coordinate!=native.coordinate){std::cerr<<"Original spatial cell mismatch "<<i<<" level "<<original.level<<'/'<<native.level<<'\n';return 1;}
    }
    std::cout<<"30000 original loose-octree cell assignments match exactly\n";
}
