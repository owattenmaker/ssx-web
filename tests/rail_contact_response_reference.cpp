#include "ps2_runtime_macros.h"
#include "../engine/rail_contact_response.hpp"
#include <fstream>
#include <random>
#include <cstring>
#include <cstdio>
void sub_0013C140_0x13c140(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x420000,g_ps2RecompiledFunctionTableSlotCount=0xc8000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xc8000]={};
using V=ssx::terrain_original::Vector;static float depth,impulse;static V normal,nextForward,translation,incoming,eventNormal;static bool moved,notified,rebuilt;
template<class T>T get(uint8_t*m,unsigned a){T v;std::memcpy(&v,m+a,sizeof v);return v;}template<class T>void put(uint8_t*m,unsigned a,T v){std::memcpy(m+a,&v,sizeof v);}
static void helper(uint8_t*m,R5900Context*c,PS2Runtime*){
 switch(c->pc){
 case 0x11fee8:SET_GPR_U32(c,2,0);break;
 case 0x123400:SET_GPR_U32(c,2,1);break;
 case 0x32f650:case 0x32f708:break;
 case 0x3342d0:{auto p=GPR_U32(c,6);std::memset(m+p,0,128);put(m,p+16,normal);c->f[0]=depth;break;}
 case 0x106538:{translation=get<V>(m,GPR_U32(c,5));auto position=get<V>(m,0x20110);for(unsigned k=0;k<3;k++)position[k]=ssx::terrain_original::add(position[k],translation[k]);put(m,0x20110,position);moved=true;break;}
 case 0x11e098:put(m,0x201b0,nextForward);rebuilt=true;break;
 case 0x105d98:{auto p=GPR_U32(c,5);incoming=get<V>(m,p+16);eventNormal=get<V>(m,p+32);impulse=get<float>(m,p+48);notified=true;break;}
 default:throw std::runtime_error("Unknown rail contact helper");}
 c->pc=GPR_U32(c,31);
}
int main(int argc,char**argv){if(argc!=2)return 1;std::ifstream f(argv[1],std::ios::binary);std::vector<uint8_t>ram((std::istreambuf_iterator<char>(f)),{});if(ram.size()!=32*1024*1024)return 2;auto*m=ram.data();PS2Runtime rt;
 for(unsigned pc:{0x11fee8u,0x123400u,0x32f650u,0x32f708u,0x3342d0u,0x106538u,0x11e098u,0x105d98u})rt.registerFunction(pc,helper);
 put(m,0x10050,0x20000u);put(m,0x206c0,0x30000u);put(m,0x30040,int16_t(0));put(m,0x30044,0x123400u);put(m,0x2037c,0.f);put(m,0x201bc,0.f);put(m,0x201ec,0.f);put(m,0x2011c,1.f);put(m,0x1003c,1.f);
 std::mt19937 rng(0x13c140);std::uniform_real_distribution<float> value(-1,1);auto vec=[&](float scale){return V{value(rng)*scale,value(rng)*scale,value(rng)*scale};};auto unit=[](V v){float n=std::sqrt(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]);for(auto&x:v)x/=n;return v;};
 unsigned events=0,forces=0;
 for(unsigned n=0;n<20000;n++){
  ssx::OriginalRailContactState state;state.position=vec(100000);state.previousPosition=state.position;state.velocity=vec(3000);state.groundNormal=unit(vec(1));state.forward=unit(vec(1));nextForward=unit(vec(1));normal=unit(vec(1));state.stationarySeconds=value(rng)+.5f;depth=n%5==0?-1:n%5==1?0:std::abs(value(rng))*8;
  if(n%11==0){state.groundNormal={0,0,1};normal={1,0,0};nextForward={-1,0,0};depth=0;state.stationarySeconds=.5f;}
  put(m,0x20110,state.position);put(m,0x201e0,state.velocity);put(m,0x20370,state.groundNormal);put(m,0x201b0,state.forward);put(m,0x10030,state.previousPosition);put(m,0x10040,state.stationarySeconds);moved=notified=rebuilt=false;
  R5900Context c{};c.pc=0x13c140;c.vu0_vf[0]=_mm_set_ps(1,0,0,0);SET_GPR_U32(&c,4,0x10000);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,29,0x70000);SET_GPR_U32(&c,31,0x12345678);
  ssx::OriginalRounding rounding;sub_0013C140_0x13c140(m,&c,&rt);bool nativeRebuilt=false;auto out=ssx::originalRailContactResponse(state,depth,normal,[&](auto&s,const auto&delta){if(moved&&delta!=translation)throw std::runtime_error("Companion translation mismatch");s.forward=nextForward;nativeRebuilt=true;});
  bool okay=c.pc==0x12345678&&out.accepted==moved&&nativeRebuilt==rebuilt&&out.notify==notified&&state.position==get<V>(m,0x20110)&&state.velocity==get<V>(m,0x201e0)&&state.previousPosition==get<V>(m,0x10030)&&std::bit_cast<uint32_t>(state.stationarySeconds)==get<uint32_t>(m,0x10040);
  if(moved)okay&=out.translation==translation;if(notified)okay&=out.incomingDirection==incoming&&out.normal==eventNormal&&out.impulse==impulse;
  if(!okay){printf("Rail contact mismatch %u pos%d vel%d timer%d notify%d/%d\n",n,state.position==get<V>(m,0x20110),state.velocity==get<V>(m,0x201e0),state.stationarySeconds==get<float>(m,0x10040),out.notify,notified);return 3;}events+=notified;forces+=state.stationarySeconds>.5f&&out.accepted;
 }
 printf("20,000 original rail contact responses match; %u notifications, %u stationary responses\n",events,forces);
}
