#include "ps2_runtime_macros.h"
#include <vector>
#include <cstring>
#include <cstdio>
void sub_00354C08_0x354c08(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x420000,g_ps2RecompiledFunctionTableSlotCount=0xc8000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xc8000]={};
int main(int,char**){std::vector<uint8_t> m(32*1024*1024);PS2Runtime rt;auto put=[&](unsigned p,unsigned v){std::memcpy(m.data()+p,&v,4);};auto get=[&](unsigned p){unsigned v;std::memcpy(&v,m.data()+p,4);return v;};put(0x20004,0x30000);
for(unsigned flags=0;flags<16;flags++){unsigned group=0x30044,active=group+4,pending=group+0x24;put(group,flags);for(unsigned s:{active,pending}){put(s+4,s);put(s+8,s);}std::vector<unsigned> nodes{0x40000,0x40100,0x40200};for(auto node:nodes){R5900Context c{};c.pc=0x354c08;SET_GPR_U32(&c,4,0x20000);SET_GPR_U32(&c,5,node);SET_GPR_U32(&c,6,1);SET_GPR_U32(&c,31,0x12345678);sub_00354C08_0x354c08(m.data(),&c,&rt);if(c.pc!=0x12345678)return 1;}auto selected=(flags&1)?pending:active,untouched=(flags&1)?active:pending;unsigned previous=selected,current=get(selected+8);for(auto expected:nodes){if(current!=expected||get(current+4)!=previous)return 2;previous=current;current=get(current+8);}if(current!=selected||get(selected+4)!=nodes.back()||get(untouched+4)!=untouched||get(untouched+8)!=untouched)return 3;}
puts("Original scheduler insertion preserves order and defers new nodes while group flag1 is set (16 flag cases)");}
