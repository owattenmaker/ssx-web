#include "ps2_runtime_macros.h"
#include "json.hpp"
#include <fstream>
#include <cfenv>
#include <cstring>
#include <iostream>
#include "boost_coil_glow_registry.inc"
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x420000,g_ps2RecompiledFunctionTableSlotCount=0xc8000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xc8000]={};
using nlohmann::json;
template<class T>T read(uint8_t*m,unsigned p){T value;std::memcpy(&value,m+p,sizeof value);return value;}
template<class T>void write(uint8_t*m,unsigned p,T value){std::memcpy(m+p,&value,sizeof value);}
static json glyphs,glow,quads;
static unsigned order(uint8_t*m){auto r=read<unsigned>(m,0x4a30f0-0x854);auto material=read<unsigned>(m,r+0xe84);return (read<unsigned>(m,material+8)>>5)&31;}
int main(int argc,char**argv){if(argc!=3)return 1;std::ifstream file(argv[1],std::ios::binary);std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(file)),{});if(bytes.size()!=32*1024*1024)return 2;auto*m=bytes.data();PS2Runtime rt;registerBoostCoilGlow(rt);
 rt.registerFunction(0x391cb0,[](uint8_t*m,R5900Context*c,PS2Runtime*){auto font=GPR_U32(c,4),str=GPR_U32(c,5);glyphs.push_back({{"order",order(m)},{"font",font},{"text",std::string(reinterpret_cast<char*>(m+str))},{"position",{c->f[12],c->f[13]}},{"font_base_scale",read<std::array<float,2>>(m,font+0x30)},{"scale",read<std::array<float,2>>(m,font+0x38)},{"argb",read<std::array<float,4>>(m,font+0x40)}});c->pc=GPR_U32(c,31);});
 unsigned owner=0;for(unsigned p=8;p+0x560<bytes.size();p+=4)if(read<unsigned>(m,p)==0x474160){if(owner)throw std::runtime_error("Ambiguous HUD owner");owner=p-8;}if(!owner)throw std::runtime_error("Missing HUD owner");
 json result;result["cases"]=json::array();
 rt.registerFunction(0x1f1190,[](uint8_t*m,R5900Context*c,PS2Runtime*){auto sprite=GPR_U32(c,5);glyphs.push_back({{"order",order(m)},{"sprite",sprite},{"uv",read<std::array<float,4>>(m,sprite+12)},{"texture",read<unsigned>(m,sprite)},{"position",read<std::array<float,2>>(m,GPR_U32(c,6))},{"size",read<std::array<float,2>>(m,GPR_U32(c,7))},{"scale",read<std::array<float,2>>(m,GPR_U32(c,8))},{"argb",read<std::array<float,4>>(m,GPR_U32(c,9))}});c->pc=GPR_U32(c,31);});
 rt.registerFunction(0x21ea00,[](uint8_t*m,R5900Context*c,PS2Runtime*){auto renderer=read<unsigned>(m,0x4a30f0-0x854),material=read<unsigned>(m,renderer+0xe84);glow.push_back({{"material",read<std::array<unsigned,5>>(m,material)},{"position",read<std::array<float,2>>(m,GPR_U32(c,6))},{"size",{c->f[12],c->f[13]}},{"uv",{c->f[14],c->f[15],c->f[16],c->f[17]}},{"argb",read<std::array<float,4>>(m,GPR_U32(c,5))}});c->pc=GPR_U32(c,31);});
 rt.registerFunction(0x379860,[](uint8_t*m,R5900Context*c,PS2Runtime*){if(GPR_U32(c,5)!=4)throw std::runtime_error("Expected coil quad");auto p=GPR_U32(c,6);quads.push_back({{"rgba128",read<std::array<unsigned,4>>(m,p+16)},{"position",read<std::array<float,4>>(m,p+32)},{"right",read<std::array<float,4>>(m,p+80)},{"bottom",read<std::array<float,4>>(m,p+128)},{"uv",read<std::array<float,2>>(m,p)},{"rightUV",read<std::array<float,2>>(m,p+48)},{"bottomUV",read<std::array<float,2>>(m,p+96)}});c->pc=GPR_U32(c,31);});
 for(int palette=0;palette<4;palette++)for(int step=-1;step<=200;step++){
 const float phase=step<0?-1.f:float(step)/100;glyphs=json::array();glow=json::array();quads=json::array();write<float>(m,0x91064,phase);write(m,0x91054,read<std::array<float,4>>(m,0x4c8428+palette*32));R5900Context c{};c.pc=0x1efc58;c.f[25]=1;write(m,0x10328,owner+0x53c);
 SET_GPR_U32(&c,22,owner);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,29,0x10000);write(m,0x10338,0x91000u);write(m,0x10324,0x4768b0u);write(m,0x10334,0);write(m,0x10030,std::array<float,2>{1,1});
 {std::fesetround(FE_TOWARDZERO);sub_001E9A30_0x1e9a30(m,&c,&rt);std::fesetround(FE_TONEAREST);}if(c.pc!=0x12345678||glyphs.size()!=1)throw std::runtime_error("Unexpected orb submission");result["cases"].push_back({{"palette",palette},{"phase",phase},{"draw",glyphs[0]},{"glow",glow},{"coilQuads",quads}});
 }
 std::ofstream out(argv[2]);out<<result.dump(2)<<'\n';std::cout<<"Captured808 original orb, coil and glow submissions\n";
}
