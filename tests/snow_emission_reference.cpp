#include "ps2_runtime_macros.h"
#include "../engine/snow_emission.hpp"
#include <fstream>
#include <random>
#include <cstring>
#include <cstdio>
#include <cfenv>
void sub_002DF920_0x2df920(uint8_t*,R5900Context*,PS2Runtime*);
void sub_002DE398_0x2de398(uint8_t*,R5900Context*,PS2Runtime*);
void sub_002DE4A8_0x2de4a8(uint8_t*,R5900Context*,PS2Runtime*);
void sub_002E02B8_0x2e02b8(uint8_t*,R5900Context*,PS2Runtime*);
void sub_002E1A80_0x2e1a80(uint8_t*,R5900Context*,PS2Runtime*);
void sub_002E1598_0x2e1598(uint8_t*,R5900Context*,PS2Runtime*);
void sub_002E0EE8_0x2e0ee8(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x380000,g_ps2RecompiledFunctionTableSlotCount=0xa0000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xa0000]={};
void sub_002DFE88_0x2dfe88(uint8_t*,R5900Context*,PS2Runtime*);
void sub_002E1F70_0x2e1f70(uint8_t*,R5900Context*,PS2Runtime*);
static int mode,control,semantic,breathClass;static float breathEnvironment;
void sub_002E1120_0x2e1120(uint8_t*,R5900Context*,PS2Runtime*);
static ssx::OriginalSnowEmission emitted;static std::vector<ssx::OriginalSnowEmission> allBirths;static bool secondary;static unsigned births=0,heap=0x1b00000;
static void callback(uint8_t*m,R5900Context*c,PS2Runtime*){
 if(c->pc==0x311ae8){SET_GPR_U32(c,2,breathClass);}
 else if(c->pc==0x300108){SET_GPR_U32(c,2,0);}
 else if(c->pc==0x2ee6f0){c->f[0]=breathEnvironment;}
 else if(c->pc==0x11fe98){SET_GPR_U32(c,2,mode);}
 else if(c->pc==0x11fee8){SET_GPR_U32(c,2,control);}
 else if(c->pc==0x312aa0){SET_GPR_U32(c,2,semantic);}
 else if(c->pc==0x317e30){unsigned size=GPR_U32(c,4);SET_GPR_U32(c,2,heap);memset(m+heap,0,size);heap=(heap+size+15)&~15u;}
 else if(c->pc==0x3714b8){}
 else if(c->pc==0x300100){SET_GPR_U32(c,2,0x50000);}
 else if(c->pc==0x2f4118){secondary=true;}
 else if(c->pc==0x3717c0){++births;emitted={};emitted.active=GPR_U32(c,8);emitted.emitter=(GPR_U32(c,4)-0xa0000)/0x210;emitted.stepSeconds=c->f[12];memcpy(emitted.positionCm.data(),m+GPR_U32(c,5),12);if(auto p=GPR_U32(c,6)){ssx::SnowVector v;memcpy(v.data(),m+p,12);emitted.velocityCmps=v;}if(auto p=GPR_U32(c,7)){ssx::SnowColour v;memcpy(v.data(),m+p,16);emitted.colour=v;}allBirths.push_back(emitted);}
 else throw std::runtime_error("Unknown snow oracle callback");
 c->pc=GPR_U32(c,31);
}
int main(int argc,char**argv){
 if(argc!=3)return 2;std::ifstream in(argv[1],std::ios::binary);std::vector<uint8_t>m((std::istreambuf_iterator<char>(in)),{});if(m.size()!=32*1024*1024)return 3;
 PS2Runtime rt;for(unsigned pc:{0x311ae8,0x300108,0x2ee6f0,0x317e30,0x3714b8,0x300100,0x3717c0,0x2f4118,0x11fe98,0x11fee8,0x312aa0})rt.registerFunction(pc,callback);rt.registerFunction(0x2de398,sub_002DE398_0x2de398);
 auto put=[&](unsigned at,const auto&v){memcpy(m.data()+at,&v,sizeof(v));};auto u=[&](unsigned at){uint32_t v;memcpy(&v,m.data()+at,4);return v;};auto vector=[&](unsigned at,ssx::SnowVector v,float w=0){put(at,v);put(at+12,w);};
 constexpr unsigned fx=0x30000,rider=0x40000,profile=0x60000,renderer=0x70000,surface=0x80000,iface=0x90000,done=0x12345678;
 auto context=[&](){R5900Context c{};c.vu0_vf[0]=_mm_set_ps(1,0,0,0);SET_GPR_U32((&c),4,fx);SET_GPR_U32((&c),28,0x4a30f0);SET_GPR_U32((&c),29,0x10000);SET_GPR_U32((&c),31,done);return c;};
 put(fx,rider);auto ctor=context();SET_GPR_U32((&ctor),5,0);sub_002DE4A8_0x2de4a8(m.data(),&ctor,&rt);if(ctor.pc!=done)return 4;std::ofstream raw(argv[2],std::ios::binary);raw.write(reinterpret_cast<char*>(m.data()+u(fx+0x2c)),0x910);raw.close();
 std::mt19937 gen(0x2e0ee8);std::uniform_real_distribution<float> unit(-1,1);unsigned active=0;
 put(fx,rider);put(fx+0x2c,profile);put(fx+0xd0,surface);put(rider+0x88c,renderer);put(rider+0x6c0,iface);put(iface+0x10,int16_t(0));put(iface+0x14,0x300100u);put(fx+0x28,0xa0000u);


 put(rider+0x780,0xb0000u);put(0xb0030,0xb1000u);put(rider+0x8a8,5u);put(iface+0x38,int16_t(0));put(iface+0x3c,0x300108u);


 unsigned kickerBirths=0;
 for(unsigned i=0;i<20000;i++){
  std::fesetround(FE_TOWARDZERO);ssx::OriginalSnowTrailContext c;
  for(unsigned k=0;k<3;k++){c.board.right[k]=unit(gen);c.board.forward[k]=unit(gen);c.board.up[k]=unit(gen);c.board.originCm[k]=unit(gen)*300000;c.velocityCmps[k]=unit(gen)*4000;}
  c.absoluteSpeedCmps=(unit(gen)+1)*2000;c.edgeBias=unit(gen);c.groundEmission=i%3!=0;for(auto&v:c.colour)v=unit(gen)+1;
  float buildup=(unit(gen)+1)*2,capacity=i%4==0?0:(unit(gen)+1)*2;ssx::OriginalSnowTrailProfile p{unit(gen),unit(gen)*500,unit(gen)};
  vector(fx+0x30,c.board.forward);vector(fx+0x40,c.board.up);vector(fx+0x50,c.board.right);vector(fx+0xa0,c.board.originCm,1);vector(0x50000,c.velocityCmps);put(fx+0x90,c.colour);put(fx+0xb0,uint32_t(c.groundEmission));put(fx+0xbc,c.absoluteSpeedCmps);put(fx+0x12c,c.edgeBias);put(fx+0x10,buildup);put(surface+0x5c,capacity);
  put(profile+0x818,p.velocityScale);put(profile+0x81c,p.normalScale);put(profile+0x820,p.normalSpeedScale);
  ssx::OriginalSnowRandom random{uint32_t(gen())};put(0x4a30f0+0xa0c,random.word);births=0;auto ctx=context();sub_002E1F70_0x2e1f70(m.data(),&ctx,&rt);auto expected=ssx::originalSnowKickerEmission(buildup,p,c,capacity,random);
  if(ctx.pc!=done||births!=1||emitted.active!=expected.active||emitted.emitter!=8||emitted.positionCm!=expected.positionCm||emitted.velocityCmps!=expected.velocityCmps||emitted.colour!=expected.colour||emitted.stepSeconds!=expected.stepSeconds||random.word!=u(0x4a30f0+0xa0c)||u(fx+0x10)!=std::bit_cast<uint32_t>(buildup)){printf("Kicker mismatch %u active%d/%d pos%d vel%d colour%d\n",i,emitted.active,expected.active,emitted.positionCm==expected.positionCm,emitted.velocityCmps==expected.velocityCmps,emitted.colour==expected.colour);return 22;}kickerBirths+=expected.active;
 }
 printf("20,000 original kicker requests match; %u active births\n",kickerBirths);
 unsigned rocks=0;
 for(unsigned i=0;i<20000;i++){
  std::fesetround(FE_TOWARDZERO);ssx::OriginalSnowCloudContext cloud;auto&c=cloud.trail;
  for(unsigned k=0;k<3;k++){c.board.right[k]=unit(gen);c.board.forward[k]=unit(gen);c.board.up[k]=unit(gen);c.board.originCm[k]=unit(gen)*300000;c.velocityCmps[k]=unit(gen)*3000;c.groundNormal[k]=unit(gen);cloud.lateral[k]=unit(gen);cloud.direction[k]=unit(gen);}
  c.absoluteSpeedCmps=(unit(gen)+1)*1500;c.edgeBias=unit(gen);c.groundEmission=i%3!=0;cloud.turnAmount=unit(gen);for(auto&v:c.colour)v=unit(gen)+1;
  float chance=(unit(gen)+1)*.5f,brake=unit(gen);ssx::OriginalSnowChunkProfile p;p.velocityScale=unit(gen);p.normalScale=unit(gen)*500;p.normalSpeedScale=unit(gen);p.sideScale=unit(gen);
  vector(fx+0x30,c.board.forward);vector(fx+0x40,c.board.up);vector(fx+0x50,c.board.right);vector(fx+0xa0,c.board.originCm,1);vector(0x50000,c.velocityCmps);vector(rider+0x370,c.groundNormal);vector(rider+0x3b0,cloud.lateral);vector(fx+0xc0,cloud.direction);put(fx+0x90,c.colour);put(fx+0xb0,uint32_t(c.groundEmission));put(fx+0xb4,cloud.turnAmount);put(fx+0xb8,c.absoluteSpeedCmps);put(fx+0xbc,c.absoluteSpeedCmps);put(fx+0x12c,c.edgeBias);put(rider+0x214,brake);put(surface+0x6c,chance);
  put(profile+0x390,p.velocityScale);put(profile+0x394,p.normalScale);put(profile+0x398,p.normalSpeedScale);put(profile+0x39c,p.sideScale);
  ssx::OriginalSnowRandom random{uint32_t(gen())};put(0x4a30f0+0xa0c,random.word);births=0;auto ctx=context();sub_002DFE88_0x2dfe88(m.data(),&ctx,&rt);auto expected=ssx::originalRockSprayEmission(p,cloud,brake,chance,random);
  if(ctx.pc!=done||births!=1||emitted.active!=expected.active||emitted.emitter!=3||emitted.positionCm!=expected.positionCm||emitted.velocityCmps!=expected.velocityCmps||emitted.colour!=expected.colour||emitted.stepSeconds!=expected.stepSeconds||random.word!=u(0x4a30f0+0xa0c)){printf("Rock producer mismatch %u active%d/%d pos%d vel%d colour%d\n",i,emitted.active,expected.active,emitted.positionCm==expected.positionCm,emitted.velocityCmps==expected.velocityCmps,emitted.colour==expected.colour);return 21;}rocks+=expected.active;
 }
 printf("20,000 original rock spray requests match; %u active births\n",rocks);
 unsigned breathActive=0;
 for(unsigned i=0;i<20000;++i){
  std::fesetround(FE_TOWARDZERO);ssx::OriginalBreathState state;state.accumulator=unit(gen)+1;state.phase=i%2;state.effort=unit(gen)+1;state.clock=unit(gen)+1;state.duration=unit(gen)+1;
  ssx::OriginalBreathContext input;input.speedCmps=i%9==0?1111.111083984375f:(unit(gen)+1)*800;input.environmentValue=breathEnvironment=unit(gen)*40;input.animationClass=breathClass=i%3==0?30:7;
  for(unsigned j=0;j<4;j++){for(unsigned k=0;k<3;k++)input.headMatrix[j][k]=unit(gen)*(j==3?300000:1);input.headMatrix[j][3]=j==3?1:0;put(0xb1140+j*16,input.headMatrix[j]);}
  for(unsigned k=0;k<3;k++){input.velocityCmps[k]=unit(gen)*1000;input.groundNormal[k]=unit(gen);}
  ssx::OriginalSnowTrailProfile profileInput{unit(gen),unit(gen),unit(gen)};
  put(fx+0x14,state.accumulator);put(fx+0x18,state.phase);put(fx+0x1c,state.effort);put(fx+0x20,state.clock);put(fx+0x24,state.duration);put(fx+0xbc,input.speedCmps);
  vector(0x50000,input.velocityCmps);vector(rider+0x370,input.groundNormal);put(profile+0x478,profileInput.velocityScale);put(profile+0x47c,profileInput.normalScale);put(profile+0x480,profileInput.normalSpeedScale);
  ssx::OriginalSnowRandom random{uint32_t(gen())};put(0x4a30f0+0xa0c,random.word);births=0;auto c=context();sub_002E1120_0x2e1120(m.data(),&c,&rt);
  auto expected=ssx::originalBreathEmission(state,profileInput,input,random);
  bool okay=c.pc==done&&births==1&&emitted.active==expected.active&&emitted.emitter==4&&emitted.positionCm==expected.positionCm&&emitted.velocityCmps==expected.velocityCmps&&emitted.colour==expected.colour&&emitted.stepSeconds==expected.stepSeconds&&random.word==u(0x4a30f0+0xa0c);
  okay=okay&&u(fx+0x14)==std::bit_cast<uint32_t>(state.accumulator)&&u(fx+0x18)==uint32_t(state.phase)&&u(fx+0x1c)==std::bit_cast<uint32_t>(state.effort)&&u(fx+0x20)==std::bit_cast<uint32_t>(state.clock)&&u(fx+0x24)==std::bit_cast<uint32_t>(state.duration);
  if(!okay){printf("Breath producer mismatch %u active%d/%d pos%d vel%d colour%d\n",i,emitted.active,expected.active,emitted.positionCm==expected.positionCm,emitted.velocityCmps==expected.velocityCmps,emitted.colour==expected.colour);return 20;}breathActive+=expected.active;
 }
 printf("20,000 original breath calls match state, RNG and emitter requests; %u active births\n",breathActive);
 for(unsigned i=0;i<20000;++i){std::fesetround(FE_TOWARDZERO);ssx::OriginalSnowRiderInput in;for(auto&v:in.velocityCmps)v=unit(gen)*3000;in.turn=unit(gen);in.brake=unit(gen);in.manualState=i%4;in.animationSemantic=semantic=i%50;in.motionMode=mode=i%6;in.controlState=control=i%14;in.reverse=i&1;in.trackingInhibited=i%3==0;in.trackingAD0=i%5==0;in.trackingAFC=i%7;in.trackingB00=i%11;
  vector(0x50000,in.velocityCmps);vector(rider+0x1e0,in.velocityCmps);put(rider+0x1f0,in.turn);put(rider+0x214,in.brake);put(rider+0x330,in.manualState);put(rider+0x320,uint32_t(in.reverse));put(rider+0xac4,uint32_t(in.trackingInhibited));put(rider+0xad0,uint32_t(in.trackingAD0));put(rider+0xafc,uint32_t(in.trackingAFC));put(rider+0xb00,uint32_t(in.trackingB00));
  auto c=context();c.pc=0x2df960;SET_GPR_U32((&c),16,fx);sub_002DF920_0x2df920(m.data(),&c,&rt);auto out=ssx::originalSnowRiderCache(in);bool okay=c.pc==done&&std::bit_cast<uint32_t>(out.edgeBias)==u(fx+0x12c)&&std::bit_cast<uint32_t>(out.turnAmount)==u(fx+0xb4)&&out.groundEmission==bool(u(fx+0xb0))&&std::bit_cast<uint32_t>(out.speedCmps)==u(fx+0xbc);for(unsigned k=0;k<3;++k)okay&=std::bit_cast<uint32_t>(out.direction[k])==u(fx+0xc0+k*4);if(!okay){printf("Snow cache mismatch%u mode%d control%d\n",i,mode,control);return 7;}
 }
 puts("20,000 original2DF920 ridercache cases exact: bias,filteredturn,speed,direction andgroundemission gates");
 for(unsigned i=0;i<20000;++i){std::fesetround(FE_TOWARDZERO);ssx::OriginalSnowTrailProfile p;ssx::OriginalSnowTrailContext c;for(unsigned k=0;k<3;++k){c.board.right[k]=unit(gen);c.board.forward[k]=unit(gen);c.board.up[k]=unit(gen);c.board.originCm[k]=unit(gen)*100000;c.velocityCmps[k]=unit(gen)*3000;c.groundNormal[k]=unit(gen);}for(auto&x:c.colour)x=unit(gen);c.absoluteSpeedCmps=i%17==0?222.22222900390625f:std::abs(unit(gen))*3000;c.edgeBias=i%3?unit(gen):0;c.groundEmission=i%5;c.surfaceActive=i%7;c.suppressed=i%11==0;p.velocityScale=unit(gen);p.normalScale=unit(gen)*100;p.normalSpeedScale=unit(gen);
  vector(fx+0x30,c.board.forward);vector(fx+0x40,c.board.up);vector(fx+0x50,c.board.right);vector(fx+0xa0,c.board.originCm,1);vector(0x50000,c.velocityCmps);vector(rider+0x370,c.groundNormal);put(fx+0x90,c.colour);put(fx+0xb0,uint32_t(c.groundEmission));put(fx+0xbc,c.absoluteSpeedCmps);put(fx+0x12c,c.edgeBias);put(surface+0x58,uint32_t(c.surfaceActive));put(renderer+0xa0,uint32_t(c.suppressed));put(profile+0xd8,p.velocityScale);put(profile+0xdc,p.normalScale);put(profile+0xe0,p.normalSpeedScale);
  ssx::OriginalSnowRandom random{gen()};put(0x4a30f0+0xa0c,random.word);auto original=context();births=0;allBirths.clear();sub_002E0EE8_0x2e0ee8(m.data(),&original,&rt);auto actual=ssx::originalSnowTrailEmission(p,c,random);
  if(original.pc!=done||births!=1||actual.active!=emitted.active||actual.positionCm!=emitted.positionCm||actual.velocityCmps!=emitted.velocityCmps||actual.colour!=emitted.colour||actual.stepSeconds!=emitted.stepSeconds||random.word!=u(0x4a30f0+0xa0c)){printf("Snow trail mismatch%u active%d/%d\n",i,actual.active,emitted.active);return 5;}active+=actual.active;
 }

 for(unsigned i=0;i<20000;++i){std::fesetround(FE_TOWARDZERO);ssx::OriginalSnowImpactState state;ssx::OriginalSnowImpactContext c;state.strength=unit(gen)*4000;state.alpha=unit(gen)*2;state.buildup=unit(gen)*3;state.kind=i&1;state.wideScatter=i%3==0;c.smallActive=i%5;c.largeActive=i%7;c.trackingInhibited=i%4==0;c.decay=unit(gen);c.motionMode=i%5;
  for(unsigned k=0;k<3;++k){state.positionCm[k]=unit(gen)*100000;state.normal[k]=unit(gen);c.groundNormal[k]=unit(gen);c.velocityCmps[k]=unit(gen)*3000;c.boardOriginCm[k]=unit(gen)*100000;}for(auto&v:c.colour)v=unit(gen);
  put(fx+4,state.buildup);put(fx+0xe0,state.strength);put(fx+0x120,state.alpha);put(fx+0x114,uint32_t(state.kind));put(fx+0x124,uint32_t(state.wideScatter));put(fx+0xd4,c.motionMode);vector(fx+0xf0,state.positionCm,1);vector(fx+0x100,state.normal);vector(fx+0xa0,c.boardOriginCm,1);vector(0x50000,c.velocityCmps);vector(rider+0x370,c.groundNormal);put(fx+0x90,c.colour);put(rider+0xac4,uint32_t(c.trackingInhibited));put(surface+0x60,uint32_t(c.smallActive));put(surface+0x64,uint32_t(c.largeActive));put(surface+0x68,c.decay);
  ssx::OriginalSnowRandom random{gen()};put(0x4a30f0+0xa0c,random.word);auto original=context();births=0;allBirths.clear();secondary=false;sub_002E1598_0x2e1598(m.data(),&original,&rt);auto actual=ssx::originalSnowImpactEmission(state,c,random);
  bool okay=original.pc==done&&allBirths.size()==2&&actual.requestSecondaryImpact==secondary&&std::bit_cast<uint32_t>(state.buildup)==u(fx+4)&&std::bit_cast<uint32_t>(state.strength)==u(fx+0xe0)&&std::bit_cast<uint32_t>(state.alpha)==u(fx+0x120)&&random.word==u(0x4a30f0+0xa0c);
  if(allBirths.size()==2)for(unsigned k=0;k<2;++k){auto&a=actual.births[k];auto&b=allBirths[k];okay&=a.active==b.active&&a.emitter==b.emitter&&a.positionCm==b.positionCm&&a.velocityCmps==b.velocityCmps&&a.colour==b.colour&&a.stepSeconds==b.stepSeconds;}
  if(!okay){printf("Snow impact mismatch%u births%zu secondary%d/%d buildup%08x/%08x strength%08x/%08x alpha%08x/%08x\n",i,allBirths.size(),actual.requestSecondaryImpact,secondary,std::bit_cast<uint32_t>(state.buildup),u(fx+4),std::bit_cast<uint32_t>(state.strength),u(fx+0xe0),std::bit_cast<uint32_t>(state.alpha),u(fx+0x120));return 6;}
 }

 unsigned clouds=0;
 for(unsigned i=0;i<20000;++i){std::fesetround(FE_TOWARDZERO);ssx::OriginalSnowTrailProfile p;ssx::OriginalSnowCloudContext cloud;auto&c=cloud.trail;
  for(unsigned k=0;k<3;++k){c.board.right[k]=unit(gen);c.board.originCm[k]=unit(gen)*100000;c.velocityCmps[k]=unit(gen)*3000;c.groundNormal[k]=unit(gen);cloud.lateral[k]=unit(gen);cloud.direction[k]=unit(gen);}for(auto&x:c.colour)x=unit(gen);c.absoluteSpeedCmps=std::abs(unit(gen))*3000;c.edgeBias=i%3?0:unit(gen);c.groundEmission=i%5;c.surfaceActive=i%7;cloud.reverse=i&1;cloud.turn=unit(gen);cloud.turnAmount=unit(gen);cloud.maxHeightCm=std::abs(unit(gen))*100;p.velocityScale=unit(gen);p.normalScale=unit(gen)*100;p.normalSpeedScale=unit(gen);
  vector(fx+0x50,c.board.right);vector(fx+0xa0,c.board.originCm,1);vector(0x50000,c.velocityCmps);vector(rider+0x1e0,c.velocityCmps);vector(rider+0x370,c.groundNormal);vector(rider+0x3b0,cloud.lateral);vector(fx+0xc0,cloud.direction);put(fx+0x90,c.colour);put(fx+0xb0,uint32_t(c.groundEmission));put(fx+0xb8,c.absoluteSpeedCmps);put(fx+0xbc,c.absoluteSpeedCmps);put(fx+0x12c,c.edgeBias);put(fx+0xb4,cloud.turnAmount);put(rider+0x320,uint32_t(cloud.reverse));put(rider+0x1f0,cloud.turn);put(surface+0x50,uint32_t(c.surfaceActive));put(surface+0x54,cloud.maxHeightCm);put(profile+0x730,p.velocityScale);put(profile+0x734,p.normalScale);put(profile+0x738,p.normalSpeedScale);
  ssx::OriginalSnowRandom random{gen()};put(0x4a30f0+0xa0c,random.word);auto original=context();births=0;allBirths.clear();sub_002E1A80_0x2e1a80(m.data(),&original,&rt);auto actual=ssx::originalSnowCloudEmission(p,cloud,random);
  if(original.pc!=done||births!=1||actual.emitter!=emitted.emitter||actual.active!=emitted.active||actual.positionCm!=emitted.positionCm||actual.velocityCmps!=emitted.velocityCmps||actual.colour!=emitted.colour||random.word!=u(0x4a30f0+0xa0c)){printf("Cloud emission mismatch%u active%d/%d\n",i,actual.active,emitted.active);return 8;}clouds+=actual.active;
 }

 put(rider+0x77c,0xb0000u);put(0xb0000+0x3b8,0u);put(0xb0000+0x3bc,0xc0000u);put(0xb0000+0x464,1u);
 unsigned chunky=0;
 for(unsigned i=0;i<20000;++i){std::fesetround(FE_TOWARDZERO);ssx::OriginalSnowChunkContext chunk;auto&cloud=chunk.cloud;auto&c=cloud.trail;std::array<ssx::OriginalSnowChunkProfile,2> profiles;
  for(unsigned k=0;k<3;++k){c.board.right[k]=unit(gen);c.board.up[k]=unit(gen);c.board.originCm[k]=unit(gen)*100000;c.velocityCmps[k]=unit(gen)*3000;c.groundNormal[k]=unit(gen);cloud.lateral[k]=unit(gen);cloud.direction[k]=unit(gen);chunk.impact.positionCm[k]=unit(gen)*100000;chunk.impact.normal[k]=unit(gen);}for(auto&x:c.colour)x=unit(gen);
  c.absoluteSpeedCmps=std::abs(unit(gen))*3000;c.edgeBias=i%3?0:unit(gen);c.groundEmission=i%5;cloud.turnAmount=unit(gen);chunk.brake=unit(gen);chunk.secondaryBrake274=unit(gen);chunk.impact.strength=i%3?0:std::abs(unit(gen))*3000;chunk.impact.kind=i&1;chunk.motionMode=i%5;chunk.largeImpactActive=i%7;chunk.chanceScales={unit(gen),unit(gen)};chunk.minWakeScale=unit(gen);chunk.maxWakeScale=unit(gen);if(i%4==0)chunk.wakeVelocity=ssx::SnowVector{unit(gen)*100,unit(gen)*100,unit(gen)*100};
  vector(fx+0x50,c.board.right);vector(fx+0x40,c.board.up);vector(fx+0xa0,c.board.originCm,1);vector(0x50000,c.velocityCmps);vector(rider+0x1e0,c.velocityCmps);vector(rider+0x370,c.groundNormal);vector(rider+0x3b0,cloud.lateral);vector(fx+0xc0,cloud.direction);put(fx+0x90,c.colour);put(fx+0xb0,uint32_t(c.groundEmission));put(fx+0xb8,c.absoluteSpeedCmps);put(fx+0xbc,c.absoluteSpeedCmps);put(fx+0x12c,c.edgeBias);put(fx+0xb4,cloud.turnAmount);put(rider+0x214,chunk.brake);put(rider+0x274,chunk.secondaryBrake274);put(fx+0xe0,chunk.impact.strength);put(fx+0x114,uint32_t(chunk.impact.kind));vector(fx+0xf0,chunk.impact.positionCm,1);vector(fx+0x100,chunk.impact.normal);put(fx+0xd4,chunk.motionMode);put(surface+0x64,uint32_t(chunk.largeImpactActive));put(surface+0x70,chunk.chanceScales);put(surface+0x78,uint32_t(chunk.wakeVelocity.has_value()));put(0xb0000+0x3b4,uint32_t(chunk.wakeVelocity.has_value()));put(surface+0x7c,chunk.minWakeScale);put(surface+0x80,chunk.maxWakeScale);if(chunk.wakeVelocity)vector(0xc0000,*chunk.wakeVelocity);
  for(unsigned n=0;n<2;++n){auto&p=profiles[n];p.velocityScale=unit(gen);p.normalScale=unit(gen)*100;p.normalSpeedScale=unit(gen);p.sideScale=unit(gen);unsigned at=profile+(n+1)*232;put(at+0xd8,p.velocityScale);put(at+0xdc,p.normalScale);put(at+0xe0,p.normalSpeedScale);put(at+0xe4,p.sideScale);}
  ssx::OriginalSnowRandom random{gen()};put(0x4a30f0+0xa0c,random.word);auto original=context();births=0;allBirths.clear();sub_002E02B8_0x2e02b8(m.data(),&original,&rt);auto actual=ssx::originalSnowChunkEmission(profiles,chunk,random);
  bool okay=original.pc==done&&allBirths.size()==2&&random.word==u(0x4a30f0+0xa0c);if(allBirths.size()==2)for(unsigned n=0;n<2;++n){auto&a=actual[n];auto&b=allBirths[n];okay&=a.active==b.active&&a.emitter==b.emitter&&a.positionCm==b.positionCm&&a.velocityCmps==b.velocityCmps&&a.colour==b.colour;chunky+=a.active;}
  if(!okay){printf("Chunk emission mismatch%u count%zu seed%08x/%08x impact%.8g kind%d wake%d\n",i,allBirths.size(),random.word,u(0x4a30f0+0xa0c),chunk.impact.strength,chunk.impact.kind,bool(chunk.wakeVelocity));if(allBirths.size()==2)for(unsigned n=0;n<2;++n){auto&a=actual[n];auto&b=allBirths[n];printf("%u flags%d/%d emit%u/%u\n",n,a.active,b.active,a.emitter,b.emitter);for(unsigned k=0;k<3;++k)printf(" pos%.9g/%.9g vel%.9g/%.9g\n",a.positionCm[k],b.positionCm[k],a.velocityCmps?a.velocityCmps->at(k):0,b.velocityCmps?b.velocityCmps->at(k):0);}return 9;}
 }
 printf("20,000 original2E02B8 completechunkycarve/brake/wake/impactcasesexact (%u activebirths)\n",chunky);
 printf("20,000 original2E1A80 cloudycarvespray requests exact (%u active)\n",clouds);
 puts("20,000 full original2E1598 impact emissions exact: bothorderedbirths, FXLCG, buildup,alpha/strengthdecay andsecondaryeffectrequest");
 printf("20,000 full original2E0EE8+2DE398 requests exact: positions, velocities, colour, enable gates, dt and FXLCG (%u active)\n",active);
}
