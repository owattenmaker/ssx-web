// Full-original live harness runtime (development oracle only). Include after
// the generated live_registrations.inc (tools/original_live_build.py).
#pragma once
#include "ps2_runtime_macros.h"
#include "ps2_runtime.h"
#include <cfenv>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x440000,g_ps2RecompiledFunctionTableSlotCount=(0x440000-0x100000)/4;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[(0x440000-0x100000)/4]={};
namespace live {
inline std::vector<uint8_t> readFile(const std::string& path){std::ifstream f(path,std::ios::binary);if(!f)throw std::runtime_error("live input "+path);return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)),{});}
struct Machine {
    PS2Runtime runtime;std::vector<uint8_t> ee;
    // ee: raw 32 MiB eeMemory.bin; vu0 code/data from the same savestate.
    Machine(const std::string& eePath,const std::string& vuCodePath,const std::string& vuDataPath){
        ee=readFile(eePath);ee.resize(32*1024*1024);runtime.memory().initialize();
        runtime.setMissingFunctionPolicy(PS2Runtime::MissingFunctionPolicy::Stop);
        auto code=readFile(vuCodePath),data=readFile(vuDataPath);
        std::memcpy(runtime.memory().getVU0Code(),code.data(),std::min<size_t>(code.size(),PS2_VU0_CODE_SIZE));
        std::memcpy(runtime.memory().getVU0Data(),data.data(),std::min<size_t>(data.size(),PS2_VU0_DATA_SIZE));
        registerLiveOriginals(runtime);
    }
    template<class T> T get(uint32_t a)const{T v;std::memcpy(&v,ee.data()+(a&0x1ffffff),sizeof v);return v;}
    template<class T> void put(uint32_t a,const T& v){std::memcpy(ee.data()+(a&0x1ffffff),&v,sizeof v);}
    // Runs an original function to completion with the PS2 FPU/VU chop rounding.
    // Arguments: a0..a3,t0..t3 (integers), f12/f13. Stack in the 0xE0000..0x100000
    // capture arena (free in the reference savestates).
    R5900Context call(uint32_t pc,std::initializer_list<uint32_t> args={},std::initializer_list<float> floats={},uint32_t sp=0xFFF00){
        constexpr uint32_t done=0x12345678;R5900Context c{};c.vu0_vf[0]=_mm_set_ps(1,0,0,0);c.pc=pc;
        SET_GPR_U32(&c,29,sp);SET_GPR_U32(&c,31,done);SET_GPR_U32(&c,28,0x4a30f0);
        // SET_GPR_U32 evaluates its register argument more than once.
        const unsigned r[]={4,5,6,7,8,9,10,11};unsigned i=0;for(auto a:args){const unsigned reg=r[i];SET_GPR_U32(&c,reg,a);++i;}
        i=0;for(auto f:floats){c.f[12+i]=f;++i;}
        int previous=std::fegetround();std::fesetround(FE_TOWARDZERO);
        while(c.pc!=done){
            if(!runtime.hasFunction(c.pc)){std::fesetround(previous);char m[64];snprintf(m,sizeof m,"live original missing 0x%x",c.pc);throw std::runtime_error(m);}
            runtime.lookupFunction(c.pc)(ee.data(),&c,&runtime);
            if(runtime.isStopRequested()){std::fesetround(previous);char m[64];snprintf(m,sizeof m,"live original stopped at 0x%x",c.pc);throw std::runtime_error(m);}
        }
        std::fesetround(previous);return c;
    }
};
}
