#include "ps2_runtime_macros.h"
#include "../engine/board_trail.hpp"
#include <fstream>
#include <cstring>
#include <iostream>
#include <random>
void sub_002E8938_0x2e8938(uint8_t*,R5900Context*,PS2Runtime*);
void sub_002E87E8_0x2e87e8(uint8_t*,R5900Context*,PS2Runtime*);
void sub_002E86F0_0x2e86f0(uint8_t*,R5900Context*,PS2Runtime*);
void sub_002EA538_0x2ea538(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0031C228_0x31c228(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000;
extern const uint32_t g_ps2RecompiledFunctionTableEnd=0x400000;
extern const uint32_t g_ps2RecompiledFunctionTableSlotCount=(0x400000-0x100000)/4;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[(0x400000-0x100000)/4]={};
using namespace ssx;using namespace ssx::board_trail;
template<class T>static void wr(uint8_t*m,uint32_t p,T v){std::memcpy(m+p,&v,sizeof(v));}
template<class T>static T rd(uint8_t*m,uint32_t p){T v;std::memcpy(&v,m+p,sizeof(v));return v;}
static Input input;static DrawWindow originalWindow;
static void external(uint8_t*m,R5900Context*c,PS2Runtime*){if(c->pc==0x386e78){originalWindow={};if(GPR_U32(c,6)){auto p=GPR_U32(c,5);originalWindow={rd<int>(m,p+4),rd<int>(m,p+8),rd<float>(m,p+12),rd<float>(m,p+16)};}c->pc=GPR_U32(c,31);return;}uint32_t out=0;switch(c->pc){case 0x11fe98:out=input.motion;break;case 0x312aa0:out=input.semantic;break;case 0x311b20:out=0x50000;break;case 0x1446a0:out=GPR_U32(c,5)?input.marker1:input.marker0;break;}SET_GPR_U32(c,2,out);c->pc=GPR_U32(c,31);}
int main(int argc,char**argv){if(argc!=2)return 1;PS2Runtime runtime;std::vector<uint8_t> memory(32*1024*1024);auto*m=memory.data();std::ifstream ee(argv[1],std::ios::binary);ee.read((char*)m,memory.size());if(!ee)return 2;
 for(uint32_t pc:{0x11fe98,0x312aa0,0x311b20,0x1446a0,0x33fff0,0x386e78})runtime.registerFunction(pc,external);
 runtime.registerFunction(0x2e87e8,sub_002E87E8_0x2e87e8);runtime.registerFunction(0x2e86f0,sub_002E86F0_0x2e86f0);runtime.registerFunction(0x31c228,sub_0031C228_0x31c228);
 constexpr uint32_t actor=0x10000,trail=0x20000,bands=0x30000,matrix=0x40000,geometry=0x50000,motion=0x60000,stack=0x80000,gp=0x4a30f0;
 wr(m,trail,actor);wr(m,actor+0x780,geometry);wr(m,geometry+0x34,matrix);wr(m,actor+0x77c,motion);wr(m,actor+0x6c0,0x70000u);wr(m,0x70038,int16_t(0));wr(m,0x7003c,0x33fff0u);
 for(int b=0;b<6;++b)wr(m,trail+0x10+b*4,bands+b*0xa20);for(auto [off,index]:{std::pair{0x8a4,0},std::pair{0x8b0,1},std::pair{0x8b8,2},std::pair{0x918,3},std::pair{0x8e8,4}})wr(m,actor+off,index);
 auto vec=[&](uint32_t p,Vector v,float w=0){wr(m,p,v);wr(m,p+12,w);};
 std::mt19937 random(0x5452414b);std::uniform_real_distribution<float> unit(-1,1),color(0,1),coord(-150000,150000);
 Profile profile;
 for(unsigned n=0;n<30000;++n){State s(profile);s.head=random()%54;s.count=random()%55;s.phase=random()%3;s.cooldown=random()%4;s.jitterLeft=1+3*color(random);s.jitterRight=1+3*color(random);s.depth=5;s.height=8;s.parity=random()%2;
  input={};input.motion=n%5==0?2:(n%7==0?1:0);input.crashSubmode=n%3==0;input.surface=random()%20;input.semantic=n%4?0:22;input.marker0=random()%2;input.marker1=random()%2;input.flagAC4=random()%2;input.flagAD0=random()%2;input.flagAFC=random()%2;input.flagB00=random()%2;input.manual=random()%2;input.reverseStance=random()%2;input.detached=random()%2;
  input.contact={coord(random),coord(random),coord(random)};input.normal={unit(random)*.2f,unit(random)*.2f,1};{Rounding r;input.normal=normalize(input.normal);}input.velocity={unit(random)*3000,unit(random)*3000,unit(random)*100};input.board.position=plus(input.contact,scale(input.normal,20));{Rounding r;input.board.axis0=normalize(Vector{unit(random),unit(random),unit(random)});input.board.axis2=normalize(cross(input.board.axis0,Vector{unit(random),unit(random),unit(random)}));}input.contactDistance=-color(random)*10;input.environmentARGB={color(random),color(random),color(random),color(random)};
  input.bone8B0=plus(input.contact,Vector{50,30,10});input.bone8B8=plus(input.contact,Vector{50,-30,10});input.bone918=plus(input.contact,Vector{-50,30,10});input.bone8E8=plus(input.contact,Vector{-50,-30,10});
  if(n%17==0)input.velocity={0,0,0};s.previousDirection=n%17?Vector{1,0,0}:Vector{0,0,0};s.previousLateral={0,-1,0};s.lastContact=plus(input.contact,Vector{-50,0,0});for(auto& band:s.bands)for(auto& v:band){v.set(plus(input.contact,Vector{unit(random)*20,unit(random)*20,unit(random)*5}));v.rgba={50,60,70,80};}
  for(int b=0;b<6;++b)wr(m,bands+b*0xa20,s.bands[b]);wr(m,trail+4,s.jitterLeft);wr(m,trail+8,s.jitterRight);wr(m,trail+12,s.parity);wr(m,trail+0x8c,s.head);wr(m,trail+0x90,s.count);wr(m,trail+0x94,s.phase);wr(m,trail+0x9c,s.depth);wr(m,trail+0xa0,s.height);vec(trail+0xb0,s.lastContact,1);vec(trail+0xc0,s.previousDirection);vec(trail+0xd0,s.previousLateral);wr(m,trail+0xe0,s.cooldown);
  wr(m,motion+0x30,input.crashSubmode);wr(m,actor+0x438,input.surface);wr(m,actor+0xac4,int(input.flagAC4));wr(m,actor+0xad0,int(input.flagAD0));wr(m,actor+0xafc,int(input.flagAFC));wr(m,actor+0xb00,int(input.flagB00));wr(m,actor+0x330,int(input.manual));wr(m,actor+0x320,int(input.reverseStance));wr(m,actor+0x150,int(input.detached));vec(actor+0x460,input.contact,1);vec(actor+0x370,input.normal);vec(actor+0x1e0,input.velocity);wr(m,actor+0x454,input.contactDistance);wr(m,0x4fa398,input.environmentARGB);
  vec(matrix,input.board.axis0);vec(matrix+32,input.board.axis2);vec(matrix+48,input.board.position,1);vec(matrix+64+48,input.bone8B0,1);vec(matrix+128+48,input.bone8B8,1);vec(matrix+192+48,input.bone918,1);vec(matrix+256+48,input.bone8E8,1);
  uint32_t rng=(random()&0x7fffff)|0x3f800000;wr(m,gp+0xa0c,rng);
  R5900Context c{};c.pc=0x2e8938;c.vu0_vf[0]=_mm_set_ps(1,0,0,0);SET_GPR_U32(&c,4,trail);SET_GPR_U32(&c,29,stack);SET_GPR_U32(&c,28,gp);SET_GPR_U32(&c,31,0x12345678);{Rounding r;sub_002E8938_0x2e8938(m,&c,&runtime);}update(s,input,rng,profile);
  auto check=[&](auto expected,uint32_t address,const char*label){auto actual=rd<decltype(expected)>(m,address);if(actual!=expected){std::cerr<<"case "<<n<<" "<<label<<" original "<<actual<<" native "<<expected<<"\n";throw std::runtime_error("Trail state mismatch");}};
  check(s.head,trail+0x8c,"head");check(s.count,trail+0x90,"count");check(s.phase,trail+0x94,"phase");check(s.cooldown,trail+0xe0,"cooldown");check(s.height,trail+0xa0,"height");check(s.depth,trail+0x9c,"depth");check(s.parity,trail+12,"parity");for(int k=0;k<3;++k){check(s.previousDirection[k],trail+0xc0+k*4,"previousDirection");check(s.previousLateral[k],trail+0xd0+k*4,"previousLateral");check(s.lastContact[k],trail+0xb0+k*4,"lastContact");}check(s.jitterLeft,trail+4,"jitterLeft");check(s.jitterRight,trail+8,"jitterRight");check(rng,gp+0xa0c,"RNG");
  for(int b=0;b<6;++b)for(int i=0;i<54;++i){auto a=rd<Vertex>(m,bands+b*0xa20+i*48);auto e=s.bands[b][i];if(a.position!=e.position||a.rgba!=e.rgba){std::cerr<<"case "<<n<<" band "<<b<<" index "<<i<<" head "<<s.head<<" motion "<<input.motion<<"\n";for(int k=0;k<4;++k)std::cerr<<std::hexfloat<<a.position[k]<<" / "<<e.position[k]<<" color "<<a.rgba[k]<<" / "<<e.rgba[k]<<"\n";return 3;}}
 }
 for(int head=0;head<54;++head)for(int count=0;count<=54;++count){State s;s.head=head;s.count=count;wr(m,trail+0x8c,head);wr(m,trail+0x90,count);wr(m,0x90000,trail);R5900Context c{};c.pc=0x2ea538;SET_GPR_U32(&c,4,0x90000);SET_GPR_U32(&c,5,1);SET_GPR_U32(&c,29,stack);SET_GPR_U32(&c,28,gp);SET_GPR_U32(&c,31,0x12345678);{Rounding r;sub_002EA538_0x2ea538(m,&c,&runtime);}auto e=drawWindow(s);if(e.start!=originalWindow.start||e.count!=originalWindow.count||e.fade!=originalWindow.fade||e.fadeStep!=originalWindow.fadeStep)throw std::runtime_error("Board trail draw window mismatch");}
 std::cout<<"2970 original render windows match every valid head/count combination exactly\n";
 std::cout<<"30000 complete original board-trail updates match all six 54-vertex bands, ring state and shared visual RNG exactly\n";
}
