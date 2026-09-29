// Development oracle: engine/score_object.cpp against the complete original score routines
// (117C28 + 117FE0 + HUD clocks, 11A228, 117718, 117638, 119210, 11A7A8, 119368, 119608).
// Rider/owner/HUD bank are synthetic memory; audio/career/game-mode externals are intercepted.
#include "ps2_runtime_macros.h"
#include "../engine/score_object.hpp"
#include "../engine/trick_name.hpp"
#include <fstream>
#include <random>
#include <cstdio>
#include <cstring>
#include <cfenv>
#include <cctype>
#include "score_registry.inc"
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x420000,g_ps2RecompiledFunctionTableSlotCount=0xc8000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xc8000]={nullptr};
namespace {
constexpr uint32_t SCORE=0x20000,BANK=0x30000,RIDER=0x40000,OWNER=0x50000,VTABLE=0x48000,RIDERFN=0x3FFF00,TICKCELL=0x60000;
std::vector<uint8_t> m;
template<class T> void w(uint32_t a,T v){std::memcpy(m.data()+a,&v,sizeof v);}
uint32_t u(uint32_t a){uint32_t v;std::memcpy(&v,m.data()+a,4);return v;}
float fl(uint32_t a){return std::bit_cast<float>(u(a));}
int32_t slot19=0,careerCalls=0,lastCareer=0,namedCalls=0;uint32_t blocked=0;
void ret0(uint8_t*,R5900Context*c,PS2Runtime*){SET_GPR_U32(c,2,0);c->pc=GPR_U32(c,31);}
void load(ssx::OriginalScoreObject& o){
 std::memcpy(o.w.data(),m.data()+SCORE,ssx::OriginalScoreObject::kBytes);
 std::memcpy(o.hud.data(),m.data()+BANK,sizeof(ssx::OriginalHudSlot)*ssx::kOriginalHudSlotCount);o.hudPresent=true;
}
void store(const ssx::OriginalScoreObject& o){
 std::memcpy(m.data()+SCORE,o.w.data(),ssx::OriginalScoreObject::kBytes);w(SCORE+0x1AC,RIDER);w(SCORE+0x1B0,BANK);w(SCORE+0x1B4,44u);
 std::memcpy(m.data()+BANK,o.hud.data(),sizeof(ssx::OriginalHudSlot)*ssx::kOriginalHudSlotCount);
}
bool same(const ssx::OriginalScoreObject& o,const char* what,unsigned n){
 for(unsigned k=0;k<0x1CC;k+=4){if(k==0x1AC||k==0x1B0||k==0x1B4)continue;if(u(SCORE+k)!=o.w[k/4]){printf("%s %u: score+%X original %08x port %08x\n",what,n,k,u(SCORE+k),o.w[k/4]);return false;}}
 const uint8_t* p=reinterpret_cast<const uint8_t*>(o.hud.data());
 for(unsigned k=0;k<44u*0x9C;k++)if(m[BANK+k]!=p[k]){printf("%s %u: HUD slot %u +%X original %02x port %02x (type %d/%d)\n",what,n,k/0x9C,k%0x9C,m[BANK+k],p[k],int(u(BANK+k/0x9C*0x9C)),o.hud[k/0x9C].type);return false;}
 return true;
}
}
int main(int argc,char**argv){
 if(argc<2)return 1;std::ifstream file(argv[1],std::ios::binary);m.assign(std::istreambuf_iterator<char>(file),{});if(m.size()!=32*1024*1024)return 2;
 PS2Runtime rt;registerScore(rt);
 for(unsigned at:{0x3e6448u,0x416210u})rt.registerFunction(at,[](uint8_t*mm,R5900Context*c,PS2Runtime*){std::memset(mm+GPR_U32(c,4),GPR_U32(c,5),GPR_U32(c,6));c->pc=GPR_U32(c,31);});
 rt.registerFunction(0x417828,[](uint8_t*mm,R5900Context*c,PS2Runtime*){ // sprintf(buf,"%d",a2)
  if(std::strcmp((char*)mm+GPR_U32(c,5),"%d"))throw std::runtime_error("unexpected sprintf format");
  std::snprintf((char*)mm+GPR_U32(c,4),16,"%d",int32_t(GPR_U32(c,6)));c->pc=GPR_U32(c,31);});
 rt.registerFunction(0x29b7e0,[](uint8_t*,R5900Context*c,PS2Runtime*){++namedCalls;c->pc=GPR_U32(c,31);}); // monster trick speech (counted)
 for(unsigned at:{0x14dc80u,0x14dd58u,0x28b180u,0x29a7d8u,0x29b0e0u,0x29b3c0u,0x299638u,0x2997b8u,0x290f58u,0x309918u,0x29b430u,0x2a3de0u,0x1597b0u})rt.registerFunction(at,ret0);
 rt.registerFunction(0x1e3760,[](uint8_t*,R5900Context*c,PS2Runtime*){++careerCalls;lastCareer=int32_t(GPR_U32(c,5));c->pc=GPR_U32(c,31);});
 rt.registerFunction(0x12a250,[](uint8_t*,R5900Context*c,PS2Runtime*){SET_GPR_U32(c,2,blocked);c->pc=GPR_U32(c,31);});
 rt.registerFunction(0x150960,[](uint8_t*,R5900Context*c,PS2Runtime*){SET_GPR_U32(c,2,uint32_t(slot19));c->pc=GPR_U32(c,31);});
 rt.registerFunction(RIDERFN,[](uint8_t*,R5900Context*c,PS2Runtime*){SET_GPR_U32(c,2,0);c->pc=GPR_U32(c,31);});
 // Name tables for 116950 (same source addresses as tests/trick_name_reference.cpp).
 const unsigned addresses[]={0x43d118,0x43d130,0x43cf40,0x43cf60,0x43cf70,0x43cfb0,0x43cfc8,0x43cfe0,0x43d0c0,0x43d160,0x4a0ff0,0x43d160,0x43d2b8,0x43d2d0,0x43d2e0,0x43d100,0x43d320};
 const unsigned count[]={6,12,8,4,15,6,5,56,16,86,2,86,6,4,16,6,26};
 ssx::OriginalTrickNameTables tables;
 for(unsigned i=0;i<17;i++)for(unsigned j=0;j<count[i];j++){uint32_t p=u(addresses[i]+4*j);if(p)tables[i].push_back(std::string((char*)m.data()+p));else tables[i].push_back(std::nullopt);}
 auto name=[&](const ssx::OriginalTrickIdentity& id){return ssx::originalTrickName(id,tables);};
 ssx::OriginalTrickCommitProfile p;auto f=[&](unsigned a){return fl(a);};
 p.identity.spinDegrees=f(0x49b70c);p.identity.flipDegrees=f(0x49b710);p.namedPointScale=f(0x49b6fc);p.scoreScale=f(0x49b5ec);p.spinScale=f(0x49b6dc);p.flipScale=f(0x49b6e0);
 std::memcpy(p.identity.ordinary.data(),m.data()+0x43d388,319);std::memcpy(p.identity.alternate.data(),m.data()+0x43d4c8,319);
 for(unsigned n=0;n<24;n++){auto a=0x43d608+n*16;p.named[n].id=u(a);p.named[n].points=int32_t(u(a+4));std::memcpy(p.named[n].identityFields.data(),m.data()+a+8,7);}
 const auto tablesT=ssx::originalScoreTables();
 // Global tick cell for 1298C8: *(gp-848)->+84->+C->+8.
 const uint32_t gtick=u(u(u(0x4a30f0-0x848)+0x84)+0xC)+8;
 std::fesetround(FE_TOWARDZERO);std::mt19937 rng(0x117c28);
 auto R=[&](float lo,float hi){return std::uniform_real_distribution<float>(lo,hi)(rng);};
 auto coin=[&](int k){return rng()%k==0;};
 const int STRINGS=5;const char* messages[STRINGS]={"STALLED!","OFF-AXIS!","INVERTED!","LATE SPIN!","LATE FLIP!"};(void)messages;
 unsigned checked[10]{};unsigned coverage[64]{};unsigned messageCount[6]{};unsigned namesPosted=0,commits=0,repeatedCommits=0,monsterSeeded=0,monsterCommits=0;int32_t commitPoints=0,commitRepeats=0;constexpr int kOriginalHudFreeCheck=0x34;
 const unsigned N=argc>2?unsigned(atoi(argv[2])):40000;
 for(unsigned n=0;n<N;n++){
  // ---- randomized object ----
  std::memset(m.data()+SCORE,0,0x200);std::memset(m.data()+BANK,0,44*0x9C);std::memset(m.data()+RIDER,0,0x900);std::memset(m.data()+OWNER,0,0xE00);
  auto sf=[&](unsigned o,float v){w(SCORE+o,v);};auto si=[&](unsigned o,int32_t v){w(SCORE+o,v);};
  si(0,coin(2));si(4,coin(3));si(8,coin(8));si(0xC,coin(4)?int(rng()%5):0);si(0x10,coin(5)?int(rng()%3):0);
  sf(0x14,coin(5)?0.f:R(0,coin(4)?2.f:.2f));sf(0x18,coin(6)?R(.5f,2):1.f);sf(0x1C,coin(3)?R(0,.05f):0.f);
  si(0x20,coin(5)?int(rng()%5):0);sf(0x24,coin(3)?R(0,40000):-1.f);si(0x28,coin(6)?int(1+rng()%2):0);sf(0x2C,coin(4)?R(0,130):-1.f);
  sf(0x30,coin(2)?R(0,12):-1.f);sf(0x34,R(-40,40));sf(0x38,R(-30,30));sf(0x3C,coin(3)?R(0,.001f):0.f);sf(0x40,coin(3)?R(0,11):-1.f);
  sf(0x44,R(0,5));sf(0x48,R(0,5));for(unsigned k=0x4C;k<=0x58;k+=4)si(k,int(rng()%4));si(0x5C,coin(4));
  for(unsigned k=0;k<3;k++)si(0x60+4*k,coin(2)?int(1+rng()%60):0);
  sf(0x6C,coin(4)?R(0,5):-1.f);si(0x70,coin(8)?int(1+rng()%3):0);si(0x74,int(rng()%3));sf(0x78,coin(4)?R(0,14):-1.f);si(0x7C,coin(8)?int(rng()%3):0);si(0x80,int(rng()%3));
  si(0x84,int(rng()%5)*1000);si(0x88,int(rng()%3));si(0x8C,int(rng()%3));si(0x90,int(rng()%24));si(0x94,int(rng()%11));si(0x98,int(rng()%11));
  si(0x9C,int(rng()%5));si(0xA0,coin(3)?0:int(rng()%20000));sf(0xA4,coin(3)?-1.f:R(-.02f,1.6f));
  for(unsigned k=0xA8;k<0xF8;k+=4)w(SCORE+k,coin(2)?uint32_t(rng()):0u);si(0xF8,int(rng()%10));
  for(unsigned k=0xFC;k<0x1A8;k+=4){if(k>=0x144&&k<=0x168)sf(k,R(0,3000));else si(k,int(rng()%3000));}
  sf(0x1A8,R(0,3000));si(0x18C,coin(3)?-1:int(rng()%20000));si(0x1C0,1);sf(0x1C4,coin(4)?R(.5f,2.f):1.f);si(0x1C8,int(rng()%100));
  w(SCORE+0x1AC,RIDER);w(SCORE+0x1B0,BANK);w(SCORE+0x1B4,44u);
  for(unsigned k=0;k<44;k++){const uint32_t s=BANK+k*0x9C;w(s+0x98,k);
   if(coin(2)){w(s,uint32_t(0x34));continue;}
   const bool dyn=k>=0x23;const int type=dyn?int(0x23+rng()%16):int(k);w(s,type);
   if(coin(2)){w(s+4,-1.f);w(s+8,-R(0,1.1f));}else{const float d=R(.2f,3);w(s+4,d);w(s+8,R(0,d));}
   w(s+0xC,int32_t(rng()%6));w(s+0x14,int32_t(rng()%20000));std::snprintf((char*)m.data()+s+0x18,16,"%d",int32_t(u(s+0x14)));}
  // ---- environment ----
  ssx::OriginalScoreEnvironment env;
  env.velocity={R(-3000,3000),R(-3000,3000),R(-1500,1500),coin(8)?R(-1,1):0.f};env.timeScale=coin(5)?R(.2f,1):1.f;env.up2=R(-1,1);
  env.motionMode=coin(2)?0:int(rng()%3);const int controls[]={0,1,2,4,5,5,5,8,7,12};env.control=controls[rng()%10];env.owner2C0=int(rng()%5);env.owner330=R(0,1.5f);
  env.air18=coin(6)?0.f:R(-30,30);env.air1C=coin(6)?0.f:R(-20,20);env.air50=coin(4)?std::bit_cast<float>(0x7149f2cau):R(-3.3f,3.3f);
  env.stick2A4=coin(4)?R(-.09f,.09f):R(-1,1);env.stick2B0=coin(4)?R(-.09f,.09f):R(-1,1);env.globalTick=rng()%100000;
  env.superTime2F0=coin(2)?0.f:R(0,60);env.meter2F8=R(0,1);env.tier2F4=int(rng()%13);env.drain304=coin(4)?3:1;
  env.slot19Points=int32_t(rng()%9);slot19=env.slot19Points;env.pointsToCareer=coin(6);env.careerAwards=true;env.commitBlocked=coin(20);blocked=env.commitBlocked;
  w(0x535c10,uint8_t(env.pointsToCareer?4:0));w(0x535c11,uint8_t(0));
  w(RIDER+0x77C,OWNER);w(RIDER+0x1E0,env.velocity);w(RIDER+0x300,env.timeScale);w(RIDER+0x1C8,env.up2);w(RIDER+0x2A4,env.stick2A4);w(RIDER+0x2B0,env.stick2B0);
  w(RIDER+0x2F0,env.superTime2F0);w(RIDER+0x2F4,env.tier2F4);w(RIDER+0x2F8,env.meter2F8);w(RIDER+0x304,env.drain304);w(RIDER+0x6C0,VTABLE);w(VTABLE+0x38,int16_t(0));w(VTABLE+0x3C,RIDERFN);
  w(OWNER+0xDE0,env.motionMode);w(OWNER+0xDE4,env.control);w(OWNER+0x2C0,env.owner2C0);w(OWNER+0x330,env.owner330);
  w(OWNER+0x230+0x18,env.air18);w(OWNER+0x230+0x1C,env.air1C);w(OWNER+0x230+0x50,env.air50);w(OWNER+0x1F0,RIDER);
  w(gtick,env.globalTick);
  ssx::OriginalScoreObject o;load(o);
  if(getenv("SCORE_DEBUG")&&unsigned(atoi(getenv("SCORE_DEBUG")))==n){printf("case %u: acc14 %a hold40 %a inc3C %a dist24 %a t2C %a 6C %a 78 %a air30 %a A4 %a ts %a vel %a %a %a %a mode %d ctl %d\n",n,o.f(0x14),o.f(0x40),o.f(0x3C),o.f(0x24),o.f(0x2C),o.f(0x6C),o.f(0x78),o.f(0x30),o.f(0xA4),env.timeScale,env.velocity[0],env.velocity[1],env.velocity[2],env.velocity[3],env.motionMode,env.control);}
  const unsigned kind=n%10;++checked[kind];
  R5900Context c{};c.vu0_vf[0]=_mm_set_ps(1,0,0,0);SET_GPR_U32(&c,4,SCORE);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,29,0x10000);SET_GPR_U32(&c,31,0x12345678);
  careerCalls=0;namedCalls=0;int forced=-1;
  // Monster tricks (11B1A8 table 0x43D608): for half of the commits / trick starts, search the spin, flip, grabs and stance
  // for a state whose identity is one of the 24 authored combinations, so 0x32 / 29B7E0 / the identity rewrite run.
  if((kind==3||kind==9)&&coin(2)){const auto& e=p.named[rng()%24];
   for(unsigned t=0;t<400;t++){sf(0x34,float(M_PI)*float(int(rng()%13)-6)+R(-.3f,.3f));sf(0x38,2*float(M_PI)*float(int(rng()%9)-4)+R(-.3f,.3f));
    si(0x60,e.identityFields[5]);si(0x64,e.identityFields[6]);si(0x68,0);si(0,coin(2));si(4,coin(3));si(0xC,coin(4)?int(rng()%5):0);si(0x10,coin(5)?int(rng()%3):0);
    const bool rs=coin(2);w(RIDER+0x324,uint32_t(rs));ssx::OriginalScoreObject probe;load(probe);auto view=ssx::originalScoreView(probe);
    ssx::OriginalTrickIdentityInput in{false,false,0,0,rs};auto id=ssx::originalTrickIdentity(view.identity,p.identity,in);auto idc=id.identity;
    if(id.valid&&ssx::originalNamedTrickBonus(idc,p.named)>0){++monsterSeeded;forced=rs;break;}}
   load(o);}
  ssx::OriginalScoreHooks hooks;int32_t portCareer=0,portNamed=0;hooks.career=[&](int,int32_t){++portCareer;};hooks.named=[&](const ssx::OriginalTrickIdentity&){++portNamed;};
  float portF0=0;int32_t portV0=0;
  switch(kind){
   case 0:case 1:case 2: // 117C28 (117FE0, slot clocks)
    c.pc=0x117c28;sub_00117C28_0x117c28(m.data(),&c,&rt);ssx::originalScoreTick(o,tablesT,env,hooks);break;
   case 3:{ // 11A228
    int32_t stance=coin(2),alt=coin(3),style=coin(4)?int(rng()%5):0,flag=coin(5)?int(1+rng()%2):0;const int32_t takeoff=coin(2);bool riderStance=coin(2);
    if(forced>=0){alt=style=flag=0;riderStance=forced;}
    // 11A8C8 reads the rider stance at *(score+1AC)+0x324.
    w(RIDER+0x324,uint32_t(riderStance));
    if(coin(3)){ // seed repeats: the identity this commit will produce, after the named-trick substitution
     auto view=ssx::originalScoreView(o);ssx::OriginalTrickIdentityInput in{stance!=0,alt!=0,style,flag,riderStance};
     auto id=ssx::originalTrickIdentity(view.identity,p.identity,in);ssx::originalNamedTrickBonus(id.identity,p.named);
     const unsigned copies=1+rng()%4;for(unsigned k=0;k<copies;k++){const unsigned e=rng()%10;w(SCORE+0xA8+8*e,id.identity[0]);w(SCORE+0xAC+8*e,id.identity[1]);}
     load(o);}
    SET_GPR_U32(&c,5,stance);SET_GPR_U32(&c,6,alt);SET_GPR_U32(&c,7,style);SET_GPR_U32(&c,8,flag);SET_GPR_U32(&c,9,takeoff);
    c.pc=0x11a228;sub_0011A228_0x11a228(m.data(),&c,&rt);
    auto r=ssx::originalScoreCommit(o,p,stance,alt,style,flag,takeoff,riderStance,env,name,hooks);portF0=r.meterDelta;commitPoints=r.points;commitRepeats=r.repeats;if(getenv("SCORE_COMMITS"))printf("commit %u pending %d points %d valid %d blocked %d\n",n,ssx::originalScorePendingPoints(o),r.points,r.valid,int(env.commitBlocked));
    if(std::bit_cast<uint32_t>(c.f[0])!=std::bit_cast<uint32_t>(portF0)){printf("11A228 %u meter original %08x port %08x\n",n,std::bit_cast<uint32_t>(c.f[0]),std::bit_cast<uint32_t>(portF0));return 3;}
    break;}
   case 4: c.pc=0x117718;sub_00117718_0x117718(m.data(),&c,&rt);ssx::originalScoreComboExpire(o,env,hooks);break;
   case 5:{const int32_t pts=coin(4)?0:int(rng()%30000);SET_GPR_U32(&c,5,pts);c.pc=0x117638;sub_00117638_0x117638(m.data(),&c,&rt);ssx::originalScoreComboAdd(o,pts);break;}
   case 6:{const bool pen=coin(2);SET_GPR_U32(&c,5,pen);c.pc=0x119368;sub_00119368_0x119368(m.data(),&c,&rt);portF0=ssx::originalScoreCrash(o,pen,env.meter2F8);
    if(std::bit_cast<uint32_t>(c.f[0])!=std::bit_cast<uint32_t>(portF0)){printf("119368 %u meter mismatch\n",n);return 3;}break;}
   case 8:{const bool attacked=coin(2);SET_GPR_U32(&c,5,attacked);c.pc=0x119b08;sub_00119B08_0x119b08(m.data(),&c,&rt);portF0=ssx::originalScoreBail(o,attacked);
    if(std::bit_cast<uint32_t>(c.f[0])!=std::bit_cast<uint32_t>(portF0)){printf("119B08 %u penalty mismatch\n",n);return 3;}break;}
   case 9:{const int32_t stance=coin(2);const bool riderStance=forced>=0?bool(forced):coin(2);w(RIDER+0x324,uint32_t(riderStance));SET_GPR_U32(&c,5,stance);
    c.pc=0x119c98;sub_00119C98_0x119c98(m.data(),&c,&rt);portF0=ssx::originalScoreTrickStart(o,p,stance,riderStance,name,hooks);
    if(std::bit_cast<uint32_t>(c.f[0])!=std::bit_cast<uint32_t>(portF0)){printf("119C98 %u return mismatch\n",n);return 3;}break;}
   case 7:{const int32_t pts=int(rng()%5000);SET_GPR_U32(&c,5,pts);c.pc=0x119608;sub_00119608_0x119608(m.data(),&c,&rt);ssx::originalScorePickup(o,pts,env,hooks);break;}
  }
  if(c.pc!=0x12345678){printf("case %u kind %u did not return (pc %x)\n",n,kind,c.pc);return 4;}
  if(namedCalls!=portNamed){printf("case %u kind %u monster speech calls original %d port %d\n",n,kind,namedCalls,portNamed);return 7;}
  monsterCommits+=namedCalls;
  if(careerCalls!=portCareer){printf("case %u kind %u career awards original %d port %d\n",n,kind,careerCalls,portCareer);return 5;}
  if(!same(o,kind<3?"117C28":kind==3?"11A228":kind==4?"117718":kind==5?"117638":kind==6?"119368":kind==7?"119608":kind==8?"119B08":"119C98",n))return 6;
  (void)portV0;
  for(auto& sl:o.hud){if(sl.type==kOriginalHudFreeCheck)continue;if(sl.type<64)++coverage[sl.type];}
  if(o.hud[0xE].type==0xE&&std::strchr(o.hud[0xE].text.data(),'!'))++messageCount[std::min(5,o.hud[0xE].arg)];
  if(kind==3&&o.hud[0].type==0&&std::isalpha((unsigned char)o.hud[0].text[0]))++namesPosted;if(kind==3&&commitPoints>0)++commits;if(kind==3&&commitRepeats>0)++repeatedCommits;
 }
 printf("coverage (slot types seen after a case):");for(int k=0;k<64;k++)if(coverage[k])printf(" %X:%u",k,coverage[k]);printf("\ncommits with points %u, trick names posted %u, repeat-penalised %u\nmessages STALLED %u OFF-AXIS %u INVERTED %u LATE-SPIN %u LATE-FLIP %u\n",commits,namesPosted,repeatedCommits,messageCount[1],messageCount[2],messageCount[3],messageCount[4],messageCount[5]);
 printf("monster-trick states seeded %u, monster awards (29B7E0 calls, equal in the port) %u\n",monsterSeeded,monsterCommits);
 printf("%u original score cases match the port byte-for-byte (score object 0..0x1CC and all 44 HUD slots): 117C28 %u, 11A228 %u, 117718 %u, 117638 %u, 119368 %u, 119608 %u, 119B08 %u, 119C98 %u\n",
  N,checked[0]+checked[1]+checked[2],checked[3],checked[4],checked[5],checked[6],checked[7],checked[8],checked[9]);
}
