#include "ps2_runtime_macros.h"
#include "json.hpp"
#include <fstream>
#include <cfenv>
#include <cstring>
#include <iostream>
#include "boost_letter_glow_registry.inc"
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x420000,g_ps2RecompiledFunctionTableSlotCount=0xc8000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xc8000]={};
using nlohmann::json;
template<class T>T read(uint8_t*m,unsigned p){T value;std::memcpy(&value,m+p,sizeof value);return value;}
template<class T>void write(uint8_t*m,unsigned p,T value){std::memcpy(m+p,&value,sizeof value);}
static json glyphs,glow;
static unsigned order(uint8_t*m){auto r=read<unsigned>(m,0x4a30f0-0x854);auto material=read<unsigned>(m,r+0xe84);return (read<unsigned>(m,material+8)>>5)&31;}
int main(int argc,char**argv){if(argc!=3)return 1;std::ifstream file(argv[1],std::ios::binary);std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(file)),{});if(bytes.size()!=32*1024*1024)return 2;auto*m=bytes.data();PS2Runtime rt;registerBoostLetterGlow(rt);
 rt.registerFunction(0x391cb0,[](uint8_t*m,R5900Context*c,PS2Runtime*){auto font=GPR_U32(c,4),str=GPR_U32(c,5);glyphs.push_back({{"order",order(m)},{"font",font},{"text",std::string(reinterpret_cast<char*>(m+str))},{"position",{c->f[12],c->f[13]}},{"font_base_scale",read<std::array<float,2>>(m,font+0x30)},{"scale",read<std::array<float,2>>(m,font+0x38)},{"argb",read<std::array<float,4>>(m,font+0x40)}});c->pc=GPR_U32(c,31);});
 unsigned owner=0;for(unsigned p=8;p+0x560<bytes.size();p+=4)if(read<unsigned>(m,p)==0x474160){if(owner)throw std::runtime_error("Ambiguous HUD owner");owner=p-8;}if(!owner)throw std::runtime_error("Missing HUD owner");
 json result;result["cases"]=json::array();
 rt.registerFunction(0x1f1190,[](uint8_t*m,R5900Context*c,PS2Runtime*){auto sprite=GPR_U32(c,5);glyphs.push_back({{"order",order(m)},{"sprite",sprite},{"uv",read<std::array<float,4>>(m,sprite+12)},{"texture",read<unsigned>(m,sprite)},{"position",read<std::array<float,2>>(m,GPR_U32(c,6))},{"size",read<std::array<float,2>>(m,GPR_U32(c,7))},{"scale",read<std::array<float,2>>(m,GPR_U32(c,8))},{"argb",read<std::array<float,4>>(m,GPR_U32(c,9))}});c->pc=GPR_U32(c,31);});
 rt.registerFunction(0x21ea00,[](uint8_t*m,R5900Context*c,PS2Runtime*){glow.push_back({{"order",order(m)},{"position",read<std::array<float,2>>(m,GPR_U32(c,6))},{"size",{c->f[12],c->f[13]}},{"uv",{c->f[14],c->f[15],c->f[16],c->f[17]}},{"argb",read<std::array<float,4>>(m,GPR_U32(c,5))}});c->pc=GPR_U32(c,31);});
 for(int palette=0;palette<4;palette++)for(float scale:{1.f,1.2f,3.f})for(float phase:{-1.f,0.f,.25f,.5f,.75f,1.f,1.25f,1.5f,1.75f,2.f})for(int letter=0;letter<9;letter++){
 const int mode=2;

 glyphs=json::array();glow=json::array();write<float>(m,0x91064,phase);write(m,0x91054,read<std::array<float,4>>(m,0x4c8428+palette*32));write(m,0x90000,std::array<float,2>{scale,scale});write(m,0x90010,std::array<float,2>{scale,scale});R5900Context c{};c.pc=0x21ed48;
 SET_GPR_U32(&c,4,owner);SET_GPR_U32(&c,5,0x4768b0);SET_GPR_U32(&c,6,0x91000);SET_GPR_U32(&c,7,0);SET_GPR_U32(&c,8,letter);SET_GPR_U32(&c,9,0x90000);SET_GPR_U32(&c,10,0x90010);SET_GPR_U32(&c,11,mode);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,29,0x10000);SET_GPR_U32(&c,31,0x12345678);write<unsigned>(m,0x10000,0);
 {std::fesetround(FE_TOWARDZERO);sub_0021ED48_0x21ed48(m,&c,&rt);std::fesetround(FE_TONEAREST);}if(c.pc!=0x12345678)throw std::runtime_error("Letter draw did not return");if(glyphs.size()!=size_t(mode==0?1:2))throw std::runtime_error("Unexpected letter submissions");result["cases"].push_back({{"letter",letter},{"mode",mode},{"phase",phase},{"palette",palette},{"scale",scale},{"draws",glyphs},{"glow",glow}});
 }
 std::ofstream out(argv[2]);out<<result.dump(2)<<'\n';std::cout<<"Captured1080 original letter glow cases\n";
}
