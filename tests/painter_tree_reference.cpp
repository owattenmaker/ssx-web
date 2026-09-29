#include "ps2_runtime_macros.h"
#include "../engine/painter_tree.hpp"
#include <vector>
#include <fstream>
#include <random>
#include <cstring>
#include <cstdio>
void sub_002C1CD8_0x2c1cd8(uint8_t*,R5900Context*,PS2Runtime*);
void sub_002BAF90_0x2baf90(uint8_t*,R5900Context*,PS2Runtime*);
void sub_002C0A10_0x2c0a10(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x420000,g_ps2RecompiledFunctionTableSlotCount=0xc8000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xc8000]={};
int main(int argc,char**argv){if(argc!=2)return 2;std::ifstream f(argv[1],std::ios::binary);std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(f)),{});auto u=[&](size_t p){uint32_t v;std::memcpy(&v,bytes.data()+p,4);return v;};size_t header=u(0),count=u(header+12);if(header+40+count*8>bytes.size())return 3;
 std::vector<uint8_t> m(32*1024*1024);std::memcpy(m.data()+0x20000,bytes.data()+header,40+count*8);uint32_t nodesAt=0x20028;std::memcpy(m.data()+0x20020,&nodesAt,4);
 std::vector<std::array<uint16_t,4>> nodes(count);std::memcpy(nodes.data(),bytes.data()+header+40,count*8);ssx::OriginalPainterTree tree;std::memcpy(&tree.scale,bytes.data()+header,4);std::memcpy(&tree.originX,bytes.data()+header+4,4);std::memcpy(&tree.originY,bytes.data()+header+8,4);std::memcpy(&tree.root,bytes.data()+header+20,2);tree.nodes=nodes;
 std::mt19937 rng(0x2c1cd8);PS2Runtime rt;
 rt.registerFunction(0x2baf90,sub_002BAF90_0x2baf90);rt.registerFunction(0x2c1cd8,sub_002C1CD8_0x2c1cd8);
 uint32_t treeAt=0x20000,tableAt=0x40000;std::memcpy(m.data()+0x30000,&treeAt,4);std::memcpy(m.data()+0x30008,&tableAt,4);
 unsigned outside=0;std::vector<bool> visited(count);
 for(unsigned n=0;n<30000;++n){float x=tree.originX+(float(int(rng()%40001)-4000)/tree.scale),y=tree.originY+(float(int(rng()%40001)-4000)/tree.scale);if(n<8){x=tree.originX+(n<4?-.5f:32767.5f)/tree.scale;y=tree.originY+float(n%4)/tree.scale;}
 R5900Context c{};c.pc=0x2c1cd8;c.f[12]=x;c.f[13]=y;SET_GPR_U32(&c,4,0x20000);SET_GPR_U32(&c,31,0x12345678);
 {ssx::terrain_original::Rounding round;sub_002C1CD8_0x2c1cd8(m.data(),&c,&rt);}
 auto found=ssx::originalPainterPoint(tree,x,y);auto expected=found?nodesAt+*found*8:0x20018;
 if(c.pc!=0x12345678||GPR_U32((&c),2)!=expected)throw std::runtime_error("Painter point query mismatch");if(found)visited[*found]=true;else ++outside;
 c={};c.pc=0x2c0a10;c.f[12]=x;c.f[13]=y;SET_GPR_U32(&c,5,0x30000);SET_GPR_U32(&c,29,0x10000);SET_GPR_U32(&c,31,0x12345678);
 {ssx::terrain_original::Rounding round;sub_002C0A10_0x2c0a10(m.data(),&c,&rt);}
 auto payload=ssx::originalPainterPayload(tree,x,y,u(header+28),u(4));
 if(c.pc!=0x12345678||GPR_U32((&c),2)!=(payload?tableAt+8*(*payload):0))throw std::runtime_error("Painter payload selection mismatch");
 }
 printf("30000 original ARA1 fog-tree point queries and payload-table selections match; %u outside, %zu distinct leaves.\n",outside,size_t(std::count(visited.begin(),visited.end(),true)));
}
