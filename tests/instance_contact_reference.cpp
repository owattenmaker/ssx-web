// Instruction oracle for engine/instance_contact.hpp: complete original
// 0x104E70, 0x105398 (+ real 0x104E70/0x1231A8) and 0x1057B8 with every
// other callee replaced by a recording stub.
#include "ps2_runtime_macros.h"
#include "../engine/instance_contact.hpp"
#include <cstring>
#include <cstdio>
#include <random>
#include <vector>
void sub_00104E70_0x104e70(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00105398_0x105398(uint8_t*,R5900Context*,PS2Runtime*);
void sub_001057B8_0x1057b8(uint8_t*,R5900Context*,PS2Runtime*);
void sub_001231A8_0x1231a8(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x420000,g_ps2RecompiledFunctionTableSlotCount=0xc8000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xc8000]={};
using namespace ssx;
struct Event {uint32_t id;std::vector<uint32_t> w;bool operator==(const Event&)const=default;};
static std::vector<Event> events;
static uint8_t* mem;
static constexpr uint32_t rider=0x100000,owner=0x180000,body=0x1c0000,shapeTable=0x800000,done=0x12345678,gp=0x4a30f0;
static constexpr uint32_t instances=0x400000,nodes=0x600000,entities=0x500000,entityTable=0x510000,riderTable=0x520000;
static std::vector<std::array<uint8_t,128>> packets;
static float radius,stubTime;static int stubControl,stubMode,stubClass,stubSemantic;static bool stubRigid;
static uint32_t bits(float f){return std::bit_cast<uint32_t>(f);}
static void put(uint32_t a,const auto&v){std::memcpy(mem+a,&v,sizeof(v));}
static uint32_t word(uint32_t a){uint32_t v;std::memcpy(&v,mem+a,4);return v;}
static float real(uint32_t a){float v;std::memcpy(&v,mem+a,4);return v;}
static ContactQuad quad(uint32_t a){ContactQuad q;std::memcpy(q.data(),mem+a,16);return q;}
static std::vector<uint32_t> qwords(const ContactQuad& q){return {bits(q[0]),bits(q[1]),bits(q[2]),bits(q[3])};}
static void append(std::vector<uint32_t>& a,const std::vector<uint32_t>& b){a.insert(a.end(),b.begin(),b.end());}
static std::vector<uint32_t> packetWords(uint32_t a){std::vector<uint32_t> w;for(uint32_t o:{0u,16u,32u})append(w,qwords(quad(a+o)));for(uint32_t o:{0x40u,0x4cu,0x50u,0x5cu})w.push_back(word(a+o));return w;}
static std::vector<uint32_t> packetWords(const OriginalInstanceContactPacket& p){std::vector<uint32_t> w;append(w,qwords(p.point));append(w,qwords(p.normal));append(w,qwords(p.surfaceVelocity));w.push_back(bits(p.depth));w.push_back(uint32_t(p.surface));w.push_back(p.instance);w.push_back(p.node);return w;}
static std::vector<uint32_t> recordWords(uint32_t a){std::vector<uint32_t> w;for(uint32_t o:{0u,16u,32u})append(w,qwords(quad(a+o)));w.push_back(word(a+48));return w;}
static std::vector<uint32_t> recordWords(const OriginalInstanceContactRecord& r){std::vector<uint32_t> w;append(w,qwords(r.point));append(w,qwords(r.direction));append(w,qwords(r.normal));w.push_back(bits(r.closingSpeed));return w;}
static uint32_t hitAddress=0,recordAddress=0;
static void stub(uint8_t*,R5900Context*c,PS2Runtime*){
 const uint32_t pc=c->pc,a0=GPR_U32(c,4),a1=GPR_U32(c,5),a2=GPR_U32(c,6),a3=GPR_U32(c,7);
 auto f12=bits(c->f[12]);
 switch(pc){
 case 0x334458:{if(a3!=64||GPR_U32(c,8)!=0||a0!=word(rider+0x860))throw std::runtime_error("334458 arguments");for(unsigned i=0;i<packets.size();i++)std::memcpy(mem+a2+128*i,packets[i].data(),128);SET_GPR_U32(c,2,packets.size());break;}
 case 0x123400:c->f[0]=radius;break;
 case 0x123500:{if(a0<entities||a0>=entities+0x10000)throw std::runtime_error("entity this");float d=-real(a1+0x40);put(a1+0x40,d);put(a1+0x20,7.f);events.push_back({0x34e698,{}});break;}
 case 0x11fee8:SET_GPR_U32(c,2,uint32_t(stubControl));break;
 case 0x11fe98:SET_GPR_U32(c,2,uint32_t(stubMode));break;
 case 0x106f78:events.push_back({0x106f78,{a0}});break;
 case 0x32f650:events.push_back({0x32f650,{a1,a2}});put(a0+0x50,shapeTable);break;
 case 0x1057b8:{hitAddress=a3;recordAddress=a2;Event e{0x1057b8,{a0,a1,f12}};append(e.w,recordWords(a2));append(e.w,packetWords(a3));events.push_back(e);break;}
 case 0x296088:events.push_back({0x296088,{a0,a1,a2,a3}});break;
 case 0x16d320:events.push_back({0x16d320,{a0,a1}});break;
 case 0x32f708:events.push_back({0x32f708,{a1}});break;
 // 1057B8 callees
 case 0x108388:events.push_back({0x108388,[&]{auto w=recordWords(a1);w.push_back(a2);return w;}()});break;
 case 0x123600:events.push_back({0x74,{}});SET_GPR_U32(c,2,stubRigid?1:0);break;
 case 0x106538:events.push_back({0x106538,qwords(quad(a1))});break;
 case 0x1065b0:if(a1!=hitAddress)throw std::runtime_error("1065B0 hit");events.push_back({0x1065b0,{}});break;
 case 0x11e098:events.push_back({0x11e098,{}});break;
 case 0x311ae8:if(a1!=2)throw std::runtime_error("311AE8 channel");events.push_back({0x311ae8,{}});SET_GPR_U32(c,2,uint32_t(stubClass));break;
 case 0x150000:if(a0!=rider)throw std::runtime_error("vtable88 this");events.push_back({0x8c,{f12}});break;
 case 0x312aa0:events.push_back({0x312aa0,{}});SET_GPR_U32(c,2,uint32_t(stubSemantic));break;
 case 0x312ab0:events.push_back({0x312ab0,{}});c->f[0]=stubTime;break;
 case 0x10e910:if(a1||a2||a3||GPR_U32(c,8)!=hitAddress)throw std::runtime_error("10E910 arguments");events.push_back({0x10e910,{f12}});break;
 case 0x119e38:events.push_back({0x119e38,{a0,a1}});break;
 case 0x11fec8:events.push_back({0x11fec8,{a1}});break;
 case 0x3128e8:events.push_back({0x3128e8,{a0,a1,a2,f12}});break;
 case 0x150010:SET_GPR_U32(c,2,rider+0x110);break;
 case 0x150020:SET_GPR_U32(c,2,rider+0x1e0);break;
 case 0x1135b8:events.push_back({0x1135b8,{a0,a1,a2,f12}});break;
 case 0x105d98:if(a1!=recordAddress)throw std::runtime_error("105D98 record");events.push_back({0x105d98,{a2}});break;
 default:throw std::runtime_error("Unexpected callee");
 }
 c->pc=GPR_U32(c,31);
}
static R5900Context context(uint32_t pc){R5900Context c{};c.pc=pc;c.vu0_vf[0]=_mm_set_ps(1,0,0,0);SET_GPR_U32(&c,28,gp);SET_GPR_U32(&c,29,0x90000);SET_GPR_U32(&c,31,done);return c;}
static std::mt19937 rng(0x105398);
static float uniform(float a,float b){return std::uniform_real_distribution<float>(a,b)(rng);}
static ContactQuad unitQuad(float w=0){ContactQuad q{uniform(-1,1),uniform(-1,1),uniform(-1,1),w};float l=std::sqrt(q[0]*q[0]+q[1]*q[1]+q[2]*q[2]);if(l>1e-3)for(unsigned k=0;k<3;k++)q[k]/=l;return q;}
// Writes one random contact packet/instance/node and returns the port's view.
static OriginalInstanceContactPacket randomPacket(unsigned i,bool entities_){
 OriginalInstanceContactPacket p;p.point={uniform(-300,300),uniform(-300,300),uniform(-300,300),1};p.normal=unitQuad(rng()%8==0?uniform(-.5f,.5f):0);
 p.surfaceVelocity=rng()%3?ContactQuad{0,0,0,0}:ContactQuad{uniform(-500,500),uniform(-500,500),uniform(-500,500),0};
 p.depth=rng()%10==0?uniform(-5,0):rng()%7==0?20.f:uniform(0,40);p.surface=rng()%3?-1:int(rng()%19);p.instance=instances+i*256;p.node=rng()%3;
 p.descriptor.flags=rng()%4;p.descriptor.value=rng()%3==0?0.f:rng()%5==0?1e30f:uniform(-50,500);p.descriptor.auxiliary=uniform(-.5f,1.5f);p.descriptor.surface=-1;
 p.instanceFlags=rng()%4==0?0x2000u:0x210000u;p.entity=entities_&&rng()%3==0;
 auto& raw=packets.emplace_back();raw.fill(0);auto store=[&](unsigned a,const auto&v){std::memcpy(raw.data()+a,&v,sizeof(v));};
 store(0,p.point);store(16,p.normal);store(32,p.surfaceVelocity);store(0x40,p.depth);store(0x4c,p.surface);store(0x50,p.instance);store(0x5c,p.node);
 uint32_t id=rng()%9;put(p.instance+0x78,id);put(p.instance+8,p.instanceFlags);put(p.instance+0xc,p.entity?entities+i*64:0u);put(p.instance+0x88,nodes+i*256);
 uint32_t node=nodes+i*256+p.node*12+0x10;put(node,p.descriptor.value);put(node+4,p.descriptor.auxiliary);put(node+8,p.descriptor.flags);put(node+10,p.descriptor.surface);
 put(entities+i*64+0xc,entityTable);
 p.instance=p.instance; // port identity is the pointer (ranking ties use instance+0x78)
 return p;
}
int main(){
 std::vector<uint8_t> memory(32*1024*1024);mem=memory.data();PS2Runtime rt;rt.setMissingFunctionPolicy(PS2Runtime::MissingFunctionPolicy::Stop);
 for(uint32_t pc:{0x334458u,0x123400u,0x123500u,0x11fee8u,0x11fe98u,0x106f78u,0x32f650u,0x1057b8u,0x296088u,0x16d320u,0x32f708u,0x108388u,0x123600u,0x106538u,0x1065b0u,0x11e098u,0x311ae8u,0x150000u,0x312aa0u,0x312ab0u,0x10e910u,0x119e38u,0x11fec8u,0x3128e8u,0x150010u,0x150020u,0x1135b8u,0x105d98u})rt.registerFunction(pc,stub);
 rt.registerFunction(0x104e70,sub_00104E70_0x104e70);rt.registerFunction(0x1231a8,sub_001231A8_0x1231a8);
 for(uint32_t o:{0x7f38u,0x7f34u,0x7ea4u})put(gp-o,.0010000000474974513f);
 put(gp-0x7f30,1.100000023841858f);put(gp-0x7f2c,55.55555725097656f);put(gp-0x7f28,1388.888916015625f);put(gp-0x7f24,833.3333740234375f);put(gp-0x7f20,.20000000298023224f);
 put(gp-0x7f1c,.30000001192092896f);put(gp-0x7f18,277.77777099609375f);put(gp-0x7f14,555.5555419921875f);put(gp-0x7f10,27.77777862548828f);put(gp-0x7f0c,.20000000298023224f);
 put(gp+0x410,0x777000u);
 put(shapeTable+0x18,uint16_t(0));put(shapeTable+0x1c,0x123400u);put(entityTable+0x150,uint16_t(0));put(entityTable+0x154,0x123500u);put(entityTable+0x70,uint16_t(0));put(entityTable+0x74,0x123600u);
 put(rider+0x77c,owner);put(rider+0xaa0,body);put(rider+0x860,0x700000u);put(rider+0x6c0,riderTable);put(rider+0x788,0x778000u);put(rider+0x790,0x779000u);
 put(riderTable+0x88,uint16_t(0));put(riderTable+0x8c,0x150000u);put(riderTable+0x28,uint16_t(0));put(riderTable+0x2c,0x150010u);put(riderTable+0x10,uint16_t(0));put(riderTable+0x14,0x150020u);
 auto portQuery=[&](const std::vector<OriginalInstanceContactPacket>& list,std::array<float,3> center){
  return originalInstanceContactQuery(list,center,radius,[&](OriginalInstanceContactPacket& p){p.depth=-p.depth;p.surfaceVelocity[0]=7.f;events.push_back({0x34e698,{}});});
 };
 // ---- 104E70 ----
 unsigned aggregated=0,callbacks=0;
 for(unsigned n=0;n<20000;n++){
  packets.clear();events.clear();unsigned count=n%50==0?0:n%40==1?1+rng()%64:1+rng()%6;std::vector<OriginalInstanceContactPacket> list;
  for(unsigned i=0;i<count;i++){auto p=randomPacket(i,true);list.push_back(p);}
  // ranking ties use the instance+0x78 identity; the port ranks packet.instance, so pass identities.
  std::array<float,3> center={uniform(-100,100),uniform(-100,100),uniform(-100,100)};put(body+0x10,center);put(body+0x1c,1.f);radius=n%2?-1.f:uniform(-50,50);
  put(0x200050,shapeTable);
  R5900Context c=context(0x104e70);SET_GPR_U32(&c,4,rider);SET_GPR_U32(&c,5,0x200000);SET_GPR_U32(&c,6,0x300000);
  OriginalRounding rounding;sub_00104E70_0x104e70(mem,&c,&rt);
  auto originalEvents=events;events.clear();
  std::vector<OriginalInstanceContactPacket> ported=list;for(auto&p:ported)p.instance=word(p.instance+0x78);
  auto expected=portQuery(ported,center);
  if(c.pc!=done)throw std::runtime_error("104E70 incomplete");
  if(bits(c.f[0])!=bits(expected.result)){printf("104E70 result mismatch %u\n",n);return 1;}
  if(expected.selected>=0){
   auto contact=expected.contact;contact.instance=list[expected.selected].instance;
   if(packetWords(0x300000)!=packetWords(contact)){printf("104E70 packet mismatch %u\n",n);return 2;}
   if(count>=2&&contact.normal!=list[expected.selected].normal)++aggregated;
  }
  for(unsigned i=0;i<expected.instances.size();i++)if(word(rider+0x5b8+4*i)!=expected.instances[i])return 3;
  if(count&&word(rider+0x5b8+4*expected.instances.size())!=0xffffffffu)return 4;
  if(originalEvents!=events)return 5;callbacks+=!events.empty();
 }
 printf("20,000 complete original104E70 selections/aggregated normals/entity callbacks match (%u aggregated, %u callbacks)\n",aggregated,callbacks);
 // ---- 105398 ----
 unsigned phases[4]={},stored=0,projected=0;
 for(unsigned n=0;n<30000;n++){
  packets.clear();events.clear();unsigned count=n%10==0?0:1+rng()%4;std::vector<OriginalInstanceContactPacket> list;
  for(unsigned i=0;i<count;i++)list.push_back(randomPacket(i,false));
  std::array<float,3> center={uniform(-100,100),uniform(-100,100),uniform(-100,100)};put(body+0x10,center);put(body+0x1c,1.f);radius=-1;
  OriginalInstanceContactRider state;state.controlState=stubControl=n%20==0?9:int(rng()%4);state.motionMode=stubMode=int(std::array{0,1,2,4}[rng()%4]);state.ownerWord30=int(rng()%2);
  state.groundNormal=n%25==0?ContactQuad{0,0,0,0}:unitQuad(rng()%10==0?uniform(-.2f,.2f):0);state.railDirection=unitQuad();
  state.velocity=n%30==0?ContactQuad{0,0,0,0}:ContactQuad{uniform(-2000,2000),uniform(-2000,2000),uniform(-2000,2000),rng()%10==0?uniform(-1,1):0};
  bool caller=rng()%2;ContactQuad callerQuad=unitQuad();
  put(owner+0x30,state.ownerWord30);put(owner+0xb0,state.railDirection);put(rider+0x370,state.groundNormal);put(rider+0x1e0,state.velocity);put(0x88000,callerQuad);
  std::array<uint8_t,0xc0> sentinel;for(auto&x:sentinel)x=uint8_t(rng());std::memcpy(mem+rider+0x9e0,sentinel.data(),0xc0);
  R5900Context c=context(0x105398);SET_GPR_U32(&c,4,rider);SET_GPR_U32(&c,5,caller?0x88000u:0u);
  {OriginalRounding rounding;sub_00105398_0x105398(mem,&c,&rt);}
  if(c.pc!=done)throw std::runtime_error("105398 incomplete");
  auto originalEvents=events;events.clear();
  std::vector<uint8_t> copy(mem+rider+0x9e0,mem+rider+0xaa0);std::memcpy(mem+rider+0x9e0,sentinel.data(),0xc0);
  // Rebuild the original addresses the port result is compared against.
  uint32_t sp=0x90000-0x4a0;
  OriginalInstanceContactPhaseHost host;
  host.boundaryContacts=[&]{events.push_back({0x106f78,{rider}});};
  host.query=[&](OriginalInstanceContactPacket& out){
   events.push_back({0x32f650,{body,1}});
   std::vector<OriginalInstanceContactPacket> ported=list;for(auto&p:ported)p.instance=word(p.instance+0x78);
   auto q=portQuery(ported,center);if(q.selected>=0){out=q.contact;out.instance=list[q.selected].instance;}
   return q.result;};
  host.store=[&](const auto& p,const auto& r){auto w=packetWords(p);(void)w;(void)r;};
  host.respond=[&](const auto& p,const auto& r,float depth){Event e{0x1057b8,{rider,p.instance,bits(depth)}};append(e.w,recordWords(r));append(e.w,packetWords(p));events.push_back(e);};
  host.audio=[&](const auto& p){events.push_back({0x296088,{0x777000u,rider,p.instance,p.node}});};
  host.finish=[&]{events.push_back({0x16d320,{0x4c5830u,rider}});events.push_back({0x32f708,{2}});};
  auto result=originalInstanceContactPhase(state,caller?&callerQuad:nullptr,host);(void)sp;
  if(originalEvents!=events){printf("105398 event mismatch %u (%zu/%zu)\n",n,originalEvents.size(),events.size());
   for(auto&e:originalEvents){printf(" o %x:",e.id);for(auto w:e.w)printf(" %08x",w);puts("");}for(auto&e:events){printf(" p %x:",e.id);for(auto w:e.w)printf(" %08x",w);puts("");}return 6;}
  if(result.stored){
   std::vector<uint8_t> expected(copy);auto w=packetWords(result.packet);auto r=recordWords(result.record);
   std::vector<uint32_t> ow;{uint32_t base=rider+0x9e0;std::memcpy(mem+base,copy.data(),0xc0);ow=packetWords(base);}
   if(ow!=w){printf("105398 9E0 mismatch %u\n",n);return 7;}
   if(recordWords(rider+0xa60)!=r){printf("105398 A60 mismatch %u\n",n);return 8;}
   std::memcpy(mem+rider+0x9e0,sentinel.data(),0xc0);
  }else if(std::memcmp(copy.data(),sentinel.data(),0xc0))return 9;
  ++phases[!result.ran?0:!result.contact?1:result.responded?3:2];stored+=result.stored;projected+=result.projected;
 }
 printf("30,000 complete original105398 phases match (skipped %u, no contact %u, contact %u, responded %u; %u projected, %u 9E0/A60 copies): projection, record, copies, callee order and arguments\n",phases[0],phases[1],phases[2],phases[3],projected,stored);
 // ---- 1057B8 ----
 unsigned paths[4]={};
 for(unsigned n=0;n<40000;n++){
  packets.clear();events.clear();auto hit=randomPacket(0,true);
  if(rng()%3==0)hit.normal=ContactQuad{uniform(-.5f,.5f),uniform(-.5f,.5f),rng()%2?uniform(.3f,1):uniform(-1,1),0};
  OriginalInstanceResponseRider state;state.controlState=stubControl=n%25==0?9:int(rng()%4);state.motionMode=stubMode=int(std::array{0,1,2,4,1,2}[rng()%6]);state.ownerWord30=int(rng()%3);
  state.velocity={uniform(-1500,1500),uniform(-1500,1500),uniform(-1500,1500),0};if(rng()%4==0)for(unsigned k=0;k<3;k++)state.velocity[k]*=.2f;
  state.presentationUp=unitQuad();if(rng()%2)state.presentationUp=ContactQuad{hit.normal[0],hit.normal[1],hit.normal[2],0};
  state.secondaryCounter=uniform(0,10);state.surface=int(rng()%19);state.stanceDiffers=rng()%2;state.speedLimit=uniform(1000,4000);
  stubClass=int(std::array{9,2,11,1,0,5,20}[rng()%7]);stubSemantic=rng()%2?0x10c:int(rng()%400);stubTime=uniform(0,.5f);stubRigid=rng()%2;
  OriginalInstanceContactRecord record{{uniform(-9,9),uniform(-9,9),uniform(-9,9),1},unitQuad(),hit.normal,uniform(0,1500)};
  float depth=uniform(0,30);
  constexpr uint32_t hitAt=0x88000,recordAt=0x88100;hitAddress=hitAt;recordAddress=recordAt;
  std::memcpy(mem+hitAt,packets[0].data(),128);put(hitAt+16,hit.normal);
  put(recordAt,record.point);put(recordAt+16,record.direction);put(recordAt+32,record.normal);put(recordAt+48,record.closingSpeed);
  put(owner+0x30,state.ownerWord30);put(rider+0x1e0,state.velocity);put(rider+0x180,state.presentationUp);put(rider+0x3f4,state.secondaryCounter);put(rider+0x438,state.surface);
  put(rider+0x320,1u);put(rider+0x324,state.stanceDiffers?0u:1u);put(rider+0x2e4,state.speedLimit);
  R5900Context c=context(0x1057b8);SET_GPR_U32(&c,4,rider);SET_GPR_U32(&c,5,hit.instance);SET_GPR_U32(&c,6,recordAt);SET_GPR_U32(&c,7,hitAt);c.f[12]=depth;
  {OriginalRounding rounding;sub_001057B8_0x1057b8(mem,&c,&rt);}
  if(c.pc!=done)throw std::runtime_error("1057B8 incomplete");
  auto originalEvents=events;events.clear();
  OriginalInstanceResponseHost host;
  host.softCollision=[&](const auto& r){auto w=recordWords(r);w.push_back(1);events.push_back({0x108388,w});};
  host.entityRigid=[&]{events.push_back({0x74,{}});return stubRigid;};
  host.translate=[&](const ContactQuad& q){events.push_back({0x106538,qwords(q)});};
  host.steer=[&](const auto&){events.push_back({0x1065b0,{}});};
  host.rebuild=[&]{events.push_back({0x11e098,{}});};
  host.channelClass=[&]{events.push_back({0x311ae8,{}});return stubClass;};
  host.surfaceLanding=[&](float f){events.push_back({0x8c,{bits(f)}});};
  host.requestedSemantic=[&]{events.push_back({0x312aa0,{}});return stubSemantic;};
  host.channelTime=[&]{events.push_back({0x312ab0,{}});return stubTime;};
  host.landingAward=[&](float f,const auto&){events.push_back({0x10e910,{bits(f)}});};
  host.scoreBoundary=[&](bool d){events.push_back({0x119e38,{0x779000u,uint32_t(d)}});};
  host.requestControl=[&](int s){events.push_back({0x11fec8,{uint32_t(s)}});};
  host.play=[&](int s){events.push_back({0x3128e8,{0x0u,uint32_t(s),1u,bits(-1.f)}});};
  host.beginPredictor=[&]{events.push_back({0x1135b8,{0x778000u,rider+0x110,rider+0x1e0,bits(state.speedLimit)}});};
  host.notify=[&](const auto&,int surface){events.push_back({0x105d98,{uint32_t(surface)}});};
  auto result=originalInstanceContactResponse(state,hit,record,depth,host);
  if(originalEvents!=events){printf("1057B8 event mismatch %u path %d\n",n,int(result.path));
   for(auto&e:originalEvents){printf(" o %x:",e.id);for(auto w:e.w)printf(" %08x",w);puts("");}for(auto&e:events){printf(" p %x:",e.id);for(auto w:e.w)printf(" %08x",w);puts("");}return 10;}
  if(qwords(quad(rider+0x1e0))!=qwords(state.velocity)||bits(real(rider+0x3f4))!=bits(state.secondaryCounter)||int(word(rider+0x438))!=state.surface){printf("1057B8 state mismatch %u path %d\n",n,int(result.path));return 11;}
  ++paths[int(result.path)];
 }
 printf("40,000 complete original1057B8 responses match (ignored %u, soft %u, surface landing %u, bounce %u): velocity, +3F4, +438, callee order and arguments\n",paths[0],paths[1],paths[2],paths[3]);
}
