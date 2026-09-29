#include "ps2_runtime_macros.h"
#include "json.hpp"
#include <fstream>
#include <cfenv>
#include <cstring>
#include <iostream>
#include "boost_draw_registry.inc"
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x420000,g_ps2RecompiledFunctionTableSlotCount=0xc8000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xc8000]={};
using nlohmann::json;
template<class T>T read(uint8_t*m,unsigned p){T value;std::memcpy(&value,m+p,sizeof value);return value;}
template<class T>void write(uint8_t*m,unsigned p,T value){std::memcpy(m+p,&value,sizeof value);}
static json quads;static unsigned renderer;
int main(int argc,char**argv){if(argc!=3)return 1;std::ifstream file(argv[1],std::ios::binary);std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(file)),{});if(bytes.size()!=32*1024*1024)return 2;auto*m=bytes.data();PS2Runtime rt;registerBoostDraw(rt);
 renderer=read<unsigned>(m,0x4a30f0-0x854);const unsigned backend=read<unsigned>(m,renderer+0x10d8),draw=read<unsigned>(m,backend+0x23c);
 rt.registerFunction(draw,[](uint8_t*m,R5900Context*c,PS2Runtime*){if(GPR_U32(c,5)!=4)throw std::runtime_error("Expected original HUD quad");json q;auto material=read<unsigned>(m,renderer+0xe84);q["texture_handle"]=read<uint16_t>(m,material+0x10);q["material_words"]=read<std::array<uint32_t,5>>(m,material);q["vertices"]=json::array();auto p=GPR_U32(c,6);for(unsigned i=0;i<4;i++){auto at=p+i*48;q["vertices"].push_back({{"uv",read<std::array<float,2>>(m,at)},{"rgba128",read<std::array<uint32_t,4>>(m,at+16)},{"position",read<std::array<float,4>>(m,at+32)}});}quads.push_back(q);c->pc=GPR_U32(c,31);});
 // Captured object is discovered by its constructor's vtable at+8.
 unsigned owner=0;for(unsigned p=8;p+0x560<bytes.size();p+=4)if(read<unsigned>(m,p)==0x474160){if(owner)throw std::runtime_error("Ambiguous HUD owner");owner=p-8;}
 if(!owner)throw std::runtime_error("Missing HUD owner");json result;result["owner"]=owner;result["cases"]=json::array();result["single_player_settings"]=0x4768b0;result["sprites"]=json::array();
 for(unsigned group=0;group<5;group++)for(unsigned piece=0;piece<3;piece++){unsigned p=read<unsigned>(m,owner+0x4fc+group*12+piece*4);result["sprites"].push_back({{"group",group},{"piece",piece},{"handle",read<unsigned>(m,p)},{"uv",read<std::array<float,4>>(m,p+12)},{"name_hash",read<unsigned>(m,p+28)}});}
 result["scale"]=read<float>(m,owner+0x53c);result["repeat_count"]=read<int>(m,owner+0x540);
 for(auto [widget,group]:{std::pair{4,0},std::pair{5,1},std::pair{6,2},std::pair{7,4}})for(float fraction:{0.f,.1f,.35f,.5f,.99f,1.f}){
  quads=json::array();write(m,0x90000,std::array<float,4>{1,1,1,1});R5900Context c{};c.pc=0x21d1a0;SET_GPR_U32(&c,4,owner);SET_GPR_U32(&c,5,0x4768b0);SET_GPR_U32(&c,6,widget);SET_GPR_U32(&c,7,group);SET_GPR_U32(&c,8,owner+0x53c);SET_GPR_U32(&c,9,0x90000);SET_GPR_U32(&c,10,0);SET_GPR_U32(&c,11,0);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,29,0x10000);SET_GPR_U32(&c,31,0x12345678);c.f[12]=fraction;c.f[13]=c.f[14]=1;
  {std::fesetround(FE_TOWARDZERO);sub_0021D1A0_0x21d1a0(m,&c,&rt);std::fesetround(FE_TONEAREST);}
  if(c.pc!=0x12345678)throw std::runtime_error("HUD draw did not return");result["cases"].push_back({{"widget",widget},{"group",group},{"fraction",fraction},{"quads",quads}});
 }
 std::ofstream out(argv[2]);out<<result.dump(2)<<'\n';std::cout<<"Captured24 original HUD fill draws with vertex/UV/material payloads\n";
}
