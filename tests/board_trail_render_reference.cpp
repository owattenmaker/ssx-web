#include "ps2_runtime_macros.h"
#include "runtime/ps2_vu1.h"
#include "../engine/board_trail.hpp"
#include "../engine/wake_render.hpp"
#include <fstream>
#include <cstring>
#include <iostream>
#include <random>
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000;
extern const uint32_t g_ps2RecompiledFunctionTableEnd=0x100004;
extern const uint32_t g_ps2RecompiledFunctionTableSlotCount=1;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[1]={};
int main(int argc,char**argv){if(argc!=2)return 1;std::array<uint8_t,16384> micro{},data{};std::ifstream f(argv[1],std::ios::binary);f.read((char*)micro.data(),micro.size());if(!f)return 2;
 // Stop after the complete source alpha/interleave pass, before shared clip/GS.
 uint32_t nop=0x8000033c,halt=0x400002ff;std::memcpy(micro.data()+0x3be0,&nop,4);std::memcpy(micro.data()+0x3be4,&halt,4);
 PS2Runtime runtime;std::mt19937 rng(0x46414445);std::uniform_real_distribution<float> initial(-1,1),step(.001f,.2f);
 for(int n=0;n<20000;++n){int count=2+rng()%(n%2?29:35);float fade=n%2?1.25f:initial(rng),inc=n%2?-step(rng):step(rng);auto wakeFades=ssx::originalWakeRowFades({0,count,fade,inc});data.fill(0);uint32_t sizes[2]={uint32_t(count*3),uint32_t(count)};std::memcpy(data.data()+16,sizes,8);std::memcpy(data.data()+24,&inc,4);std::memcpy(data.data()+28,&fade,4);std::array<std::array<ssx::board_trail::Vertex,36>,2> bands{};
  for(int b=0;b<2;++b)for(int i=0;i<count;++i){auto& v=bands[b][i];v.uvq={float(b),float(i&1),1,0};v.rgba={uint32_t(rng()%256),uint32_t(rng()%256),uint32_t(rng()%256),uint32_t(rng()%256)};v.position={float(i),float(b),0,1};std::memcpy(data.data()+32+(b*count+i)*48,&v,48);}
  VU1Interpreter vu;{ssx::terrain_original::Rounding r;vu.execute(micro.data(),micro.size(),data.data(),data.size(),runtime.gs(),nullptr,0x3a08,0,0,3000);}
  float current=fade;for(int i=0;i<count;++i){for(int b=0;b<2;++b){ssx::board_trail::Vertex got;std::memcpy(&got,data.data()+32+(i*2+b)*48,48);auto expected=bands[b][i];if(n%2)current=wakeFades[i];{ssx::terrain_original::Rounding r;expected.rgba[3]=ssx::board_trail::fadedAlpha(expected.rgba[3],current);}
    if(got.rgba!=expected.rgba||got.uvq!=expected.uvq||got.position!=expected.position){std::cerr<<"case "<<n<<" slice "<<i<<" band "<<b<<" alpha "<<got.rgba[3]<<'/'<<expected.rgba[3]<<" RGB "<<got.rgba[0]<<","<<got.rgba[1]<<","<<got.rgba[2]<<" expected "<<expected.rgba[0]<<","<<expected.rgba[1]<<","<<expected.rgba[2]<<" UV "<<got.uvq[0]<<","<<got.uvq[1]<<" expected "<<expected.uvq[0]<<","<<expected.uvq[1]<<" pos "<<got.position[0]<<","<<got.position[1]<<" expected "<<expected.position[0]<<","<<expected.position[1]<<" pc "<<std::hex<<vu.state().pc<<"\n";return 3;}}
   {ssx::terrain_original::Rounding r;current=ssx::terrain_original::add(current,inc);}
  }
 }
 std::cout<<"20000 original VU1 board/wake-strip passes (including decreasing wake fades) match native alpha quantization, unmodified RGB/UV/positions and interleaved band ordering\n";
}
