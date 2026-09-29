#include "ps2_runtime_macros.h"
#include <vector>
#include <cstring>
#include <cstdio>
void sub_00354F98_0x354f98(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00354EA8_0x354ea8(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00354EF8_0x354ef8(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00354C08_0x354c08(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x420000,g_ps2RecompiledFunctionTableSlotCount=0xc8000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xc8000]={};
static unsigned read(uint8_t*m,unsigned p){unsigned v;std::memcpy(&v,m+p,4);return v;}static void put(uint8_t*m,unsigned p,unsigned v){std::memcpy(m+p,&v,4);}static std::vector<unsigned> visited;
int main(int,char**){std::vector<uint8_t> memory(32*1024*1024);auto*m=memory.data();PS2Runtime rt;rt.registerFunction(0x354ea8,sub_00354EA8_0x354ea8);rt.registerFunction(0x354ef8,sub_00354EF8_0x354ef8);
const unsigned group=0x30044;put(m,0x20004,0x30000);for(unsigned f:{group+4,group+0x24}){put(m,f+4,f+16);put(m,f+24,f);}
for(unsigned node:{0x40000u,0x40100u}){put(m,node+12,0x50000);R5900Context c{};SET_GPR_U32(&c,4,0x20000);SET_GPR_U32(&c,5,node);SET_GPR_U32(&c,6,1);SET_GPR_U32(&c,31,0x12345678);sub_00354C08_0x354c08(m,&c,&rt);}
put(m,group,1);put(m,0x50014,0x123400);
rt.registerFunction(0x123400,[](uint8_t*m,R5900Context*c,PS2Runtime*r){unsigned node=GPR_U32(c,4);visited.push_back(node);if(node==0x40100){unsigned next=read(m,node+4),prev=read(m,node+8);put(m,prev+4,next);put(m,next+8,prev);put(m,node+4,0);put(m,node+8,0);R5900Context child{};SET_GPR_U32(&child,4,0x20000);SET_GPR_U32(&child,5,0x40200);SET_GPR_U32(&child,6,1);SET_GPR_U32(&child,31,0x12345678);sub_00354C08_0x354c08(m,&child,r);}c->pc=GPR_U32(c,31);});
R5900Context c{};c.pc=0x354f98;SET_GPR_U32(&c,4,0x20000);SET_GPR_U32(&c,5,1);SET_GPR_U32(&c,29,0x10000);SET_GPR_U32(&c,31,0x12345678);sub_00354F98_0x354f98(m,&c,&rt);
if(c.pc!=0x12345678||visited!=std::vector<unsigned>{0x40100,0x40000}||read(m,group+8)!=0x40000||read(m,group+0x28)!=0x40200||GPR_U32((&c),2)!=0x40000)return 1;
puts("Original update loop safely removes current node; deferred child is not visited; active iteration follows offset4");}
