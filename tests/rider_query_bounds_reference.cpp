#include "ps2_runtime_macros.h"
#include "../engine/rider_query_bounds.hpp"
#include <random>
#include <vector>
#include <cstring>
#include <cstdio>
#include <fstream>
void sub_0011E150_0x11e150(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x420000,g_ps2RecompiledFunctionTableSlotCount=0xc8000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xc8000]={};
int main(int argc,char** argv){
    std::vector<uint8_t> memory(32*1024*1024);PS2Runtime runtime;std::mt19937 rng(0x11e150);
    auto put=[&](unsigned at,auto value){std::memcpy(memory.data()+at,&value,sizeof(value));};
    auto random=[&](){return float(int(rng()%20001)-10000)/13.f;};
    for(unsigned n=0;n<20000;++n){
        std::array<float,4> position{random()*100,random()*100,random()*100,1},override{random()*100,random()*100,random()*100,0};
        std::array<float,4> right{},forward{},up{};
        for(unsigned i=0;i<3;++i){right[i]=random()/770;forward[i]=random()/770;up[i]=random()/770;}
        if(n%5==0){right={1,0,0,0};forward={0,1,0,0};up={0,0,1,0};}
        put(0x20110,position);put(0x201a0,right);put(0x201b0,forward);put(0x201c0,up);put(0x30000,override);
        R5900Context c{};c.pc=0x11e150;SET_GPR_U32(&c,4,0x20000);SET_GPR_U32(&c,5,n%2?0x30000:0);SET_GPR_U32(&c,29,0x10000);SET_GPR_U32(&c,31,0x12345678);
        {ssx::terrain_original::Rounding rounding;sub_0011E150_0x11e150(memory.data(),&c,&runtime);}
        const auto result=ssx::originalRiderQueryBounds(n%2?override:position,right,forward,up);
        if(c.pc!=0x12345678||std::memcmp(result.minimum.data(),memory.data()+0x20400,16)||std::memcmp(result.maximum.data(),memory.data()+0x20410,16)||std::memcmp(result.sphere.data(),memory.data()+0x20420,16)){
            std::printf("Rider query bounds differ case%u\n",n);return 1;
        }
    }
    if(argc==2){
        std::ifstream file(argv[1],std::ios::binary);std::array<float,28> row;unsigned total=0,exact=0;float maximum=0;
        while(file.read(reinterpret_cast<char*>(row.data()),sizeof(row))){
            std::array<float,4> position,right,forward,up;
            std::copy_n(row.begin(),4,position.begin());std::copy_n(row.begin()+4,4,right.begin());std::copy_n(row.begin()+8,4,forward.begin());std::copy_n(row.begin()+12,4,up.begin());
            auto bounds=ssx::originalRiderQueryBounds(position,right,forward,up);std::array<float,12> actual;
            std::copy(bounds.minimum.begin(),bounds.minimum.end(),actual.begin());std::copy(bounds.maximum.begin(),bounds.maximum.end(),actual.begin()+4);std::copy(bounds.sphere.begin(),bounds.sphere.end(),actual.begin()+8);
            exact+=std::memcmp(actual.data(),row.data()+16,48)==0;++total;
            for(unsigned i=0;i<12;++i)maximum=std::max(maximum,std::abs(actual[i]-row[16+i]));
        }
        if(!file.eof()||file.gcount())return 2;
        std::printf("Captured current-field bounds audit: %u/%u exact; maximum retained-bound difference %.9g cm.\n",exact,total,maximum);
        if(total!=122||exact!=total)return 3;
    }
    std::puts("20000 complete original rider query bounds match all twelve words, including explicit position override and retained W lanes.");
}
