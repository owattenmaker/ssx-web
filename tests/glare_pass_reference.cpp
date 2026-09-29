// Development oracle: runs the original framebuffer glare pass 36C790 (and its callees
// 36C740 / 36B9D8 / 36C188 / 36C398 / 368138) on a live EE RAM image
// and dumps the GS packet stream it builds, for tools/test_glare_pass_native.py.
// The DMA chain allocator 38F460 / 38F668 is replaced by an in-place writer so the
// whole pass lands in one contiguous buffer in submission order.
#include "ps2_runtime_macros.h"
#include <cfenv>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <random>
#include <string>
#include <vector>
void sub_0036C790_0x36c790(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0036C740_0x36c740(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0036B9D8_0x36b9d8(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0036C188_0x36c188(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0036C398_0x36c398(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00368138_0x368138(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x420000,g_ps2RecompiledFunctionTableSlotCount=0xc8000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xc8000]={};
template<class T>static T rd(uint8_t*m,unsigned a){T v;std::memcpy(&v,m+a,sizeof v);return v;}
template<class T>static void wr(uint8_t*m,unsigned a,T v){std::memcpy(m+a,&v,sizeof v);}
static const unsigned GP=0x4a30f0,STACK=0x1f70000,MAIN=0x1e00000,SIDE=0x1d00000,PTRS=0x1cff000;
int main(int argc,char**argv){
 if(argc!=3)return 1;std::ifstream in(argv[1],std::ios::binary);std::vector<uint8_t> ram((std::istreambuf_iterator<char>(in)),{});
 if(ram.size()!=32*1024*1024)return 2;uint8_t*m=ram.data();PS2Runtime rt;
 rt.registerFunction(0x36c790,sub_0036C790_0x36c790);rt.registerFunction(0x36c740,sub_0036C740_0x36c740);
 rt.registerFunction(0x36b9d8,sub_0036B9D8_0x36b9d8);rt.registerFunction(0x36c188,sub_0036C188_0x36c188);
 rt.registerFunction(0x36c398,sub_0036C398_0x36c398);rt.registerFunction(0x368138,sub_00368138_0x368138);
 // 395330 / 395350 (renderer vtable 493260 +50/+58, not a separate recompiled unit): current
 // display buffer record (+59C8 * 0xA4 + +59CC) words +88 / +8C, transcribed from the ELF.
 rt.registerFunction(0x395330,[](uint8_t*m,R5900Context*c,PS2Runtime*){unsigned a=GPR_U32(c,4);SET_GPR_U32(c,2,rd<unsigned>(m,rd<unsigned>(m,a+0x59c8)*0xa4+rd<unsigned>(m,a+0x59cc)+0x88));c->pc=GPR_U32(c,31);});
 rt.registerFunction(0x395350,[](uint8_t*m,R5900Context*c,PS2Runtime*){unsigned a=GPR_U32(c,4);SET_GPR_U32(c,2,rd<unsigned>(m,rd<unsigned>(m,a+0x59c8)*0xa4+rd<unsigned>(m,a+0x59cc)+0x8c));c->pc=GPR_U32(c,31);});
 // 38F460(pool, current, qwc=-1, flags): new chain segment; written in place.
 rt.registerFunction(0x38f460,[](uint8_t*,R5900Context*c,PS2Runtime*){if(GPR_U32(c,7)&4)throw std::runtime_error("unexpected 38F768 flush");SET_GPR_U32(c,2,GPR_U32(c,5));c->pc=GPR_U32(c,31);});
 // 38F668(pool, end, 4): closes the segment (qword aligned) and returns the chain pointer.
 rt.registerFunction(0x38f668,[](uint8_t*,R5900Context*c,PS2Runtime*){unsigned e=GPR_U32(c,5);SET_GPR_U32(c,2,(e+15)&~15u);c->pc=GPR_U32(c,31);});
 const unsigned renderer=rd<unsigned>(m,GP-0x854);
 const std::vector<uint8_t> base=ram;
 std::string out="{\"renderer\":"+std::to_string(renderer)+",\"cases\":[\n";
 std::mt19937 rng(0x36c790);std::uniform_real_distribution<float> u(0,1.5f);
 for(int k=0;k<40;k++){
  ram=base;m=ram.data();
  if(k>0){ // synthetic painter / debug values (k==0 keeps the live state)
   for(int i=0;i<7;i++)wr<float>(m,renderer+0x6cd4+i*4,(k%5==1&&i>=5)?0.f:u(rng));
   wr<float>(m,GP+0x12e8,rd<float>(m,renderer+0x6cd4+4));
   wr<float>(m,GP+0x12f8,k%3==0?u(rng):0.f);wr<float>(m,GP+0x12fc,k%4==0?u(rng):0.f);
   if(k%7==3)wr<float>(m,GP+0x1308,u(rng)*2);
   if(k%11==5)wr<int>(m,GP+0x12e0,7);
  }
  wr<unsigned>(m,PTRS,MAIN);wr<unsigned>(m,PTRS+4,SIDE);std::memset(m+MAIN,0,0x40000);std::memset(m+SIDE,0,0x100);
  R5900Context c{};c.pc=0x36c790;SET_GPR_U32(&c,28,GP);SET_GPR_U32(&c,29,STACK);SET_GPR_U32(&c,31,0x12345678);
  SET_GPR_U32(&c,4,renderer);SET_GPR_U32(&c,5,0);SET_GPR_U32(&c,6,PTRS+4);SET_GPR_U32(&c,7,PTRS);
  std::fesetround(FE_TOWARDZERO);sub_0036C790_0x36c790(m,&c,&rt);std::fesetround(FE_TONEAREST);
  if(c.pc!=0x12345678)throw std::runtime_error("36C790 did not return");
  const unsigned end=rd<unsigned>(m,PTRS),side=rd<unsigned>(m,PTRS+4);
  char buf[128];std::string words;
  for(unsigned a=MAIN;a<end;a+=8){std::snprintf(buf,sizeof buf,"%s\"%016llx\"",a==MAIN?"":",",(unsigned long long)rd<uint64_t>(m,a));words+=buf;}
  std::string sw;for(unsigned a=SIDE;a<side;a+=8){std::snprintf(buf,sizeof buf,"%s\"%016llx\"",a==SIDE?"":",",(unsigned long long)rd<uint64_t>(m,a));sw+=buf;}
  std::string params;for(int i=0;i<7;i++){std::snprintf(buf,sizeof buf,"%s%.9g",i?",":"",rd<float>(m,renderer+0x6cd4+i*4));params+=buf;}
  std::snprintf(buf,sizeof buf,"\"debug\":[%.9g,%.9g,%.9g,%d,%d,%d]",rd<float>(m,GP+0x12f8),rd<float>(m,GP+0x12fc),rd<float>(m,GP+0x1308),rd<int>(m,GP+0x12e0),rd<int>(m,GP+0x12d8),rd<int>(m,GP+0x12dc));
  out+=std::string(k?",\n":"")+"{\"params\":["+params+"],"+buf+",\"gp12e8\":"+std::to_string(rd<float>(m,GP+0x12e8))+",\"main\":["+words+"],\"side\":["+sw+"]}";
 }
 out+="\n]}\n";std::ofstream(argv[2])<<out;std::printf("captured 40 original 36C790 glare packets (renderer %08x)\n",renderer);
}
