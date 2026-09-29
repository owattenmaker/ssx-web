// Development probe: executes the original single-player HUD slot draw cases of 0x1E9A30 (dispatch table
// 0x46EC70, called from the HUD draw 0x1EC3F8) for trick-scoring message slots and records the final font
// (0x391CB0) and sprite (0x1F1190) submissions. The case context (slot, ratio, text, arg, stack locals) is
// set as the slot loop at 0x1ECC28..0x1ECCB0 leaves it; execution stops at the loop continuation 0x1EFAC4.
#include "ps2_runtime_macros.h"
#include "json.hpp"
#include <fstream>
#include <cfenv>
#include <cstring>
#include <iostream>
#include <string>
#include "trick_hud_registry.inc"
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x440000,g_ps2RecompiledFunctionTableSlotCount=0xd0000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xd0000]={};
using nlohmann::json;
template<class T>T read(uint8_t*m,unsigned p){T value;std::memcpy(&value,m+p,sizeof value);return value;}
template<class T>void write(uint8_t*m,unsigned p,T value){std::memcpy(m+p,&value,sizeof value);}
static json draws;static unsigned renderer;
static std::string latin1(const char* t){std::string o;for(;*t;++t){unsigned char ch=*t;if(ch<0x80)o+=char(ch);else{o+=char(0xC0|(ch>>6));o+=char(0x80|(ch&0x3F));}}return o;} // bytes as U+00XX (HUDFONT icon glyphs)
static unsigned order(uint8_t*m){auto material=read<unsigned>(m,renderer+0xe84);return (read<unsigned>(m,material+8)>>5)&31;}
static std::string format(uint8_t*m,R5900Context*c){ // 0x417828 sprintf(a0,a1,...) for %d/%s/%c/%%(width)
 std::string f=reinterpret_cast<char*>(m+GPR_U32(c,5)),out;unsigned arg=6;
 auto next=[&]()->uint32_t{uint32_t v=arg<=11?GPR_U32(c,arg):0;++arg;return v;};
 for(size_t i=0;i<f.size();i++){if(f[i]!='%'){out+=f[i];continue;}std::string spec="%";while(++i<f.size()&&strchr("0123456789-+ .",f[i]))spec+=f[i];if(i>=f.size())break;char k=f[i];
  char buf[256];if(k=='d'||k=='i'){spec+='d';snprintf(buf,sizeof buf,spec.c_str(),int32_t(next()));out+=buf;}else if(k=='s'){spec+='s';snprintf(buf,sizeof buf,spec.c_str(),reinterpret_cast<char*>(m+next()));out+=buf;}
  else if(k=='c'){out+=char(next());}else if(k=='%')out+='%';else throw std::runtime_error("unsupported sprintf "+f);}
 return out;}
int main(int argc,char**argv){if(argc!=3)return 1;std::ifstream file(argv[1],std::ios::binary);std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(file)),{});if(bytes.size()!=32*1024*1024)return 2;auto*m=bytes.data();PS2Runtime rt;registerTrickHud(rt);
 renderer=read<unsigned>(m,0x4a30f0-0x854);
 rt.registerFunction(0x391cb0,[](uint8_t*m,R5900Context*c,PS2Runtime*){auto font=GPR_U32(c,4),str=GPR_U32(c,5);
  draws.push_back({{"kind","text"},{"order",order(m)},{"font",font},{"text",latin1(reinterpret_cast<char*>(m+str))},{"position",{c->f[12],c->f[13]}},
   {"offset",read<std::array<float,2>>(m,font+0x18)},{"shadow",read<std::array<float,2>>(m,font+0x28)},{"base_scale",read<std::array<float,2>>(m,font+0x30)},{"scale",read<std::array<float,2>>(m,font+0x38)},
   {"argb",read<std::array<float,4>>(m,font+0x40)},{"shadow_argb",read<std::array<float,4>>(m,font+0x50)}});c->pc=GPR_U32(c,31);});
 rt.registerFunction(0x1f1190,[](uint8_t*m,R5900Context*c,PS2Runtime*){auto sprite=GPR_U32(c,5);
  draws.push_back({{"kind","sprite"},{"order",order(m)},{"sprite",sprite},{"texture",sprite?read<unsigned>(m,sprite):0},{"uv",sprite?read<std::array<float,4>>(m,sprite+12):std::array<float,4>{}},
   {"position",read<std::array<float,2>>(m,GPR_U32(c,6))},{"size",read<std::array<float,2>>(m,GPR_U32(c,7))},{"scale",read<std::array<float,2>>(m,GPR_U32(c,8))},{"argb",read<std::array<float,4>>(m,GPR_U32(c,9))}});c->pc=GPR_U32(c,31);});
 rt.registerFunction(0x417828,[](uint8_t*m,R5900Context*c,PS2Runtime*){auto s=format(m,c);std::memcpy(m+GPR_U32(c,4),s.c_str(),s.size()+1);SET_GPR_U32(c,2,uint32_t(s.size()));c->pc=GPR_U32(c,31);});
 unsigned owner=0;for(unsigned p=8;p+0x560<bytes.size();p+=4)if(read<unsigned>(m,p)==0x474160){if(owner)throw std::runtime_error("Ambiguous HUD owner");owner=p-8;}if(!owner)throw std::runtime_error("Missing HUD owner");
 // Live score object / bank of the human rider (rider 0x14701A0 +0x790).
 const unsigned rider=0x14701a0,score=read<unsigned>(m,rider+0x790),bank=read<unsigned>(m,score+0x1b0);
 json spec=json::parse(std::ifstream(argv[2]+std::string(".cases")));json result;result["owner"]=owner;result["score"]=score;result["bank"]=bank;result["cases"]=json::array();
 const auto baseline=bytes; // every case starts from the untouched snapshot
 for(const auto& cs:spec){
  bytes=baseline;m=bytes.data();draws=json::array();
  const int type=cs["type"];const int index=type<0x22?type:0x23;const unsigned slot=bank+index*0x9c;const float maximum=cs.value("maximum",-1.f),ratio=cs["ratio"];
  write<int>(m,slot,type);write<float>(m,slot+4,maximum);write<float>(m,slot+8,maximum==-1.f?-ratio:ratio*maximum);write<int>(m,slot+12,cs.value("arg",0));write<int>(m,slot+16,cs.value("field10",0));write<int>(m,slot+20,cs.value("points",0));
  std::string text=cs.value("text",std::to_string(int(cs.value("points",0))));std::memcpy(m+slot+0x18,text.c_str(),text.size()+1);
  for(auto& [k,v]:cs.value("score",json::object()).items())write<int>(m,score+std::stoul(k,nullptr,16),v.get<int>());
  const unsigned sp=0x10000;std::memset(m+sp,0,0x480);
  write<float>(m,sp+0x30,1.f);write<float>(m,sp+0x34,1.f);write<float>(m,sp+0x40,1.f); // 0x1EC45C..0x1EC474 prologue: unit scale (1,1), shadow colour (1,0,0,0)
  write<unsigned>(m,sp+0x320,1);write<unsigned>(m,sp+0x324,0x4768b0);write<unsigned>(m,sp+0x330,0);write<unsigned>(m,sp+0x334,0);write<unsigned>(m,sp+0x338,owner+0x48);
  write<unsigned>(m,sp+0x340,rider);write<unsigned>(m,sp+0x344,score);write<unsigned>(m,sp+0x348,(read<unsigned>(m,owner+0x3cc)&~read<unsigned>(m,owner+0x48+0x80))|read<unsigned>(m,owner+0x48+0x84)); /*1ECB04..1ECB18 element enables*/write<unsigned>(m,sp+0x34c,0);write<unsigned>(m,sp+0x350,0);
  write<unsigned>(m,sp+0x354,index);write<unsigned>(m,sp+0x358,slot+0x18);write<int>(m,sp+0x35c,cs.value("arg",0));write<int>(m,sp+0x360,cs.value("field10",0));write<unsigned>(m,sp+0x370,1);
  write<float>(m,owner+0x48+0x70,-1.f); // per-player pulse timer (+0x70) idle
  if(cs.contains("flags"))write<unsigned>(m,sp+0x348,cs["flags"].get<unsigned>()); // element enables override (e.g. free ride 0x1530C380: career cash popups 0x2E/0x2F)
  if(cs.contains("mode88"))write<int>(m,owner+0x48+0x88,cs["mode88"].get<int>()); // pre-pass display mode (0x1EBB44: 3 while a 'Collect' popup 0x31 lives)
  R5900Context c{};c.vu0_vf[0]=_mm_set_ps(1,0,0,0);c.pc=cs["label"].get<unsigned>();
  SET_GPR_U32(&c,20,slot);SET_GPR_U32(&c,21,index+1);SET_GPR_U32(&c,22,owner);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,29,sp);SET_GPR_U32(&c,31,0x12345678);
  c.f[22]=ratio;c.f[25]=cs.value("pulse",1.f);c.f[26]=-1;c.f[27]=cs.value("flash",-1.f);c.f[28]=0;c.f[29]=1;
  {std::fesetround(FE_TOWARDZERO);sub_001E9A30_0x1e9a30(m,&c,&rt);std::fesetround(FE_TONEAREST);}
  if(c.pc!=0x12345678)throw std::runtime_error("HUD case did not reach the slot-loop continuation");
  json out=cs;out["draws"]=draws;result["cases"].push_back(out);
 }
 std::ofstream(argv[2])<<result.dump(1)<<'\n';std::cout<<"Captured "<<result["cases"].size()<<" original trick HUD slot draws\n";
}
