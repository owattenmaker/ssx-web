// Instruction oracle for engine/rail_body_contact.hpp: the complete original
// 0x106F78 (with the real 0x1231A8) against originalRailBodyContact. Every other
// callee (0x11FE98 motion, 0x334680 spline query, 0x108A48 attach test, 0x106538,
// 0x1065B0, 0x11E098, 0x105D98) is a recording stub. The spline query result is
// scripted per case (point within/around the 50 cm hips segment, random tangent);
// spline layers leave out+0x20 = 0x4FF120 (zero) and out+0x50 = 0 (no entity).
// Compared: return value, rider+0x1E0 xyz, callee order and arguments (query
// point/list/mask/radius, 106538 push, the replaced hit normal given to 1065B0,
// the 105D98 record point/direction/normal/closing and kind). W lanes: the port
// keeps zero w lanes; the original's -0.0 w lane after the tangent flip is not
// modeled, so quads are compared on xyz.
#include "ps2_runtime_macros.h"
#include "../engine/rail_body_contact.hpp"
#include <cstring>
#include <cstdio>
#include <random>
#include <vector>
void sub_00106F78_0x106f78(uint8_t*,R5900Context*,PS2Runtime*);
void sub_001231A8_0x1231a8(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x420000,g_ps2RecompiledFunctionTableSlotCount=0xc8000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xc8000]={};
using namespace ssx;
struct Event {uint32_t id;std::vector<uint32_t> w;bool operator==(const Event&)const=default;};
static std::vector<Event> events;
static uint8_t* mem;
static constexpr uint32_t rider=0x100000,owner=0x180000,skeleton=0x1d0000,bones=0x1e0000,world=0x700000,frame=0x7a0000,done=0x12345678,gp=0x4a30f0;
static uint32_t bits(float f){return std::bit_cast<uint32_t>(f);}
static void put(uint32_t a,const auto&v){std::memcpy(mem+a,&v,sizeof(v));}
static uint32_t word(uint32_t a){uint32_t v;std::memcpy(&v,mem+a,4);return v;}
static float real(uint32_t a){float v;std::memcpy(&v,mem+a,4);return v;}
static std::vector<uint32_t> xyz(uint32_t a){return {word(a),word(a+4),word(a+8)};}
static std::vector<uint32_t> xyz(const RailVector& v){return {bits(v[0]),bits(v[1]),bits(v[2])};}
static std::vector<uint32_t> xyz(const ContactQuad& v){return {bits(v[0]),bits(v[1]),bits(v[2])};}
static void append(std::vector<uint32_t>& a,const std::vector<uint32_t>& b){a.insert(a.end(),b.begin(),b.end());}
static std::vector<uint32_t> recordWords(uint32_t a){std::vector<uint32_t> w;for(uint32_t o:{0u,16u,32u})append(w,xyz(a+o));w.push_back(word(a+12));w.push_back(word(a+48));return w;}
static std::vector<uint32_t> recordWords(const OriginalInstanceContactRecord& r){std::vector<uint32_t> w;append(w,xyz(r.point));append(w,xyz(r.direction));append(w,xyz(r.normal));w.push_back(bits(r.point[3]));w.push_back(bits(r.closingSpeed));return w;}
static int stubMode=0;static bool stubAttach=false;static OriginalRailQueryResult scripted;static uint32_t boneRecord=0;
static void stub(uint8_t*,R5900Context*c,PS2Runtime*){
 const uint32_t pc=c->pc,a0=GPR_U32(c,4),a1=GPR_U32(c,5),a2=GPR_U32(c,6),a3=GPR_U32(c,7);
 switch(pc){
 case 0x11fe98:if(a0!=rider)throw std::runtime_error("11FE98 this");SET_GPR_U32(c,2,uint32_t(stubMode));break;
 case 0x334680:{
  if(a0!=world||a1!=boneRecord||a3!=1||bits(c->f[12])!=bits(300.f))throw std::runtime_error("334680 arguments");
  Event e{0x334680,xyz(a1)};events.push_back(e);
  std::memset(mem+a2,0xcd,0x70);
  const auto& r=scripted;put(a2,ContactQuad{r.point[0],r.point[1],r.point[2],1});put(a2+16,ContactQuad{r.tangent[0],r.tangent[1],r.tangent[2],0});
  put(a2+32,ContactQuad{0,0,0,0});put(a2+0x4c,r.surface);put(a2+0x50,0u);
  SET_GPR_U32(c,2,r.found?1u:0u);break;}
 case 0x108a48:if(a0!=rider)throw std::runtime_error("108A48 this");events.push_back({0x108a48,{}});SET_GPR_U32(c,2,stubAttach?1u:0u);break;
 case 0x106538:if(a0!=rider)throw std::runtime_error("106538 this");events.push_back({0x106538,xyz(a1)});break;
 case 0x1065b0:if(a0!=rider)throw std::runtime_error("1065B0 this");events.push_back({0x1065b0,xyz(a1+16)});break;
 case 0x11e098:if(a0!=rider)throw std::runtime_error("11E098 this");events.push_back({0x11e098,{}});break;
 case 0x105d98:{if(a0!=rider)throw std::runtime_error("105D98 this");auto w=recordWords(a1);w.push_back(a2);events.push_back({0x105d98,w});break;}
 default:throw std::runtime_error("Unexpected callee");
 }
 c->pc=GPR_U32(c,31);
}
static R5900Context context(uint32_t pc){R5900Context c{};c.pc=pc;c.vu0_vf[0]=_mm_set_ps(1,0,0,0);SET_GPR_U32(&c,28,gp);SET_GPR_U32(&c,29,0x90000);SET_GPR_U32(&c,31,done);return c;}
static std::mt19937 rng(0x106f78);
static float uniform(float a,float b){return std::uniform_real_distribution<float>(a,b)(rng);}
static RailVector unit(){RailVector q{uniform(-1,1),uniform(-1,1),uniform(-1,1)};float l=std::sqrt(q[0]*q[0]+q[1]*q[1]+q[2]*q[2]);if(l>1e-3)for(auto& x:q)x/=l;return q;}
int main(){
 std::vector<uint8_t> memory(32*1024*1024);mem=memory.data();PS2Runtime rt;rt.setMissingFunctionPolicy(PS2Runtime::MissingFunctionPolicy::Stop);
 for(uint32_t pc:{0x11fe98u,0x334680u,0x108a48u,0x106538u,0x1065b0u,0x11e098u,0x105d98u})rt.registerFunction(pc,stub);
 rt.registerFunction(0x1231a8,sub_001231A8_0x1231a8);
 put(gp-0x7ea4,.0010000000474974513f);put(gp-0x7ea0,-.20000000298023224f);put(gp-0x7e9c,.0010000000474974513f);put(gp-0x7e98,55.55555725097656f);
 put(gp-0x848,frame);put(frame+0x10,60);
 put(rider+0x77c,owner);put(rider+0x780,skeleton);put(skeleton+0x2c,bones);put(rider+0x860,world);
 unsigned stages[6]={},pushed=0,fallback=0,steered=0;
 for(unsigned n=0;n<100000;n++){
  events.clear();
  OriginalRailBodyContactRider state;state.motionMode=stubMode=int(std::array{0,1,2,4,0,1}[rng()%6]);state.ownerWord30=int(rng()%2);
  const uint32_t hips=rng()%4;put(rider+0x89c,hips);boneRecord=bones+hips*32;
  std::array<float,4> q{uniform(-1,1),uniform(-1,1),uniform(-1,1),uniform(-1,1)};{float l=std::sqrt(q[0]*q[0]+q[1]*q[1]+q[2]*q[2]+q[3]*q[3]);for(auto& x:q)x/=l;}
  RailVector P{uniform(-2e5f,2e5f),uniform(-2e5f,2e5f),uniform(-3e5f,0)};if(rng()%3==0)P={uniform(-300,300),uniform(-300,300),uniform(-300,300)};
  state.hipsPosition=P;state.hipsQuaternion=q;put(boneRecord,ContactQuad{P[0],P[1],P[2],1});put(boneRecord+16,q);
  // Rail point around the hips segment: along the bone Y axis +-80 cm, 0..70 cm off it.
  RailVector axis;{OriginalRounding r;axis=originalRailBoardAxes(q).y;}
  RailVector side=unit();float along=uniform(-80,80),off=rng()%6==0?uniform(0,5):uniform(0,70);
  scripted={};scripted.found=rng()%12!=0;for(unsigned k=0;k<3;k++)scripted.point[k]=P[k]+axis[k]*along+side[k]*off;
  scripted.tangent=unit();if(rng()%10==0){for(unsigned k=0;k<3;k++)scripted.tangent[k]=P[k]-scripted.point[k];float l=std::sqrt(scripted.tangent[0]*scripted.tangent[0]+scripted.tangent[1]*scripted.tangent[1]+scripted.tangent[2]*scripted.tangent[2]);if(l>1e-3)for(auto& x:scripted.tangent)x/=l;}
  scripted.surface=rng()%3?-1:int(rng()%19);
  state.velocity=n%25==0?RailVector{0,0,0}:n%40==1?RailVector{uniform(-5e-4f,5e-4f),0,0}:RailVector{uniform(-2500,2500),uniform(-2500,2500),uniform(-2500,2500)};
  state.groundNormal=unit();if(rng()%8==0)state.groundNormal=scripted.tangent;
  stubAttach=rng()%5==0;
  put(owner+0x30,state.ownerWord30);put(rider+0x1e0,ContactQuad{state.velocity[0],state.velocity[1],state.velocity[2],0});put(rider+0x370,ContactQuad{state.groundNormal[0],state.groundNormal[1],state.groundNormal[2],0});
  R5900Context c=context(0x106f78);SET_GPR_U32(&c,4,rider);
  {OriginalRounding rounding;sub_00106F78_0x106f78(mem,&c,&rt);}
  if(c.pc!=done)throw std::runtime_error("106F78 incomplete");
  R5900Context* cp=&c;const uint32_t originalResult=GPR_U32(cp,2);auto originalEvents=events;events.clear();
  OriginalRailBodyContactHost host;
  host.query=[&](RailVector p){events.push_back({0x334680,xyz(p)});return scripted;};
  host.attachable=[&]{events.push_back({0x108a48,{}});return stubAttach;};
  host.translate=[&](const RailVector& d){events.push_back({0x106538,xyz(d)});};
  host.steer=[&](const RailVector& e){events.push_back({0x1065b0,xyz(e)});};
  host.rebuild=[&]{events.push_back({0x11e098,{}});};
  host.notify=[&](const OriginalInstanceContactRecord& r,int kind){auto w=recordWords(r);w.push_back(uint32_t(kind));events.push_back({0x105d98,w});};
  auto result=originalRailBodyContact(state,host);
  auto dump=[&]{for(auto&e:originalEvents){printf(" o %x:",e.id);for(auto w:e.w)printf(" %08x",w);puts("");}for(auto&e:events){printf(" p %x:",e.id);for(auto w:e.w)printf(" %08x",w);puts("");}};
  if(originalEvents!=events){printf("106F78 event mismatch %u stage %d\n",n,result.stage);dump();return 1;}
  if(originalResult!=(result.contact?1u:0u)){printf("106F78 result mismatch %u\n",n);return 2;}
  if(xyz(rider+0x1e0)!=xyz(state.velocity)){printf("106F78 velocity mismatch %u stage %d\n",n,result.stage);return 3;}
  ++stages[result.stage];pushed+=result.pushed;steered+=result.contact&&state.motionMode==0;
  if(result.contact){auto t=scripted.tangent;auto m=RailVector{-t[0],-t[1],-t[2]};fallback+=xyz(result.record.normal)==xyz(t)||xyz(result.record.normal)==xyz(m);}
 }
 printf("100,000 complete original106F78 hips/rail contacts match (motion4 %u, no rail %u, behind %u, beyond 50 cm %u, attachable %u, contact %u; %u pushed, %u steered, %u tangent fallback): result, velocity, callee order and arguments\n",
  stages[0],stages[1],stages[2],stages[3],stages[4],stages[5],pushed,steered,fallback);
}
