#include "ps2_runtime_macros.h"
#include "../engine/snow_particles.hpp"
#include "../engine/snow_flipbook.hpp"
#include <fstream>
#include <random>
#include <cstring>
#include <cstdio>
#include <cfenv>
void sub_003717C0_0x3717c0(uint8_t*,R5900Context*,PS2Runtime*);
void sub_003710D0_0x3710d0(uint8_t*,R5900Context*,PS2Runtime*);
void sub_003177F0_0x3177f0(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00317A08_0x317a08(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x380000,g_ps2RecompiledFunctionTableSlotCount=0xa0000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xa0000]={};

int main(int argc,char**argv){
 if(argc!=3)return 2;std::ifstream in(argv[1],std::ios::binary);std::vector<uint8_t> original((std::istreambuf_iterator<char>(in)),{});if(original.size()!=16384)return 3;

 std::ifstream eeFile(argv[2],std::ios::binary);std::vector<uint8_t>ee((std::istreambuf_iterator<char>(eeFile)),{});if(ee.size()!=32*1024*1024)return 6;
 auto word=[&](uint32_t at){uint32_t v;memcpy(&v,ee.data()+at,4);return v;};uint32_t emitter=0;
 for(uint32_t at=0;at+0x30<ee.size();at+=4)if(word(at)==0x14701a0){uint32_t e=word(at+0x28),d=word(at+0x2c);if(e>0&&e<ee.size()-0x210&&d>0&&d<ee.size()-232&&word(d)==2&&word(d+0xc4)==5&&word(d+0x10)==0x41200000u){emitter=e;break;}}
 if(!emitter)return 7;auto prepared=ssx::originalSnowParticleKernel({});uint32_t particle=emitter+0x20;
 auto check=[&](uint32_t off,const auto&value){if(memcmp(&value,ee.data()+particle+off,sizeof(value))){printf("ParticleCPUprofile mismatch%02x\n",off);exit(8);}};
 check(0x0c,prepared.ageStep);check(0x10,prepared.sizeRange);check(0x14,prepared.lifeRange);check(0x18,prepared.sizeBase);check(0x1c,prepared.lifeBase);check(0x20,prepared.sizeDelta);check(0x60,prepared.force);check(0x70,prepared.velocityBase);for(unsigned n=0;n<3;++n)check(0x80+n*16,prepared.velocityRanges[n]);check(0xb0,prepared.positionBase);check(0xc0,prepared.positionRange0);check(0xd0,prepared.positionRange1);check(0xf0,prepared.colourBase);check(0x100,prepared.colourRange0);check(0x110,prepared.colourRange1);check(0x120,prepared.colourSlope);if(prepared.birthCapacity!=word(emitter+0x178)||prepared.birthCapacity*prepared.particlesPerBirth!=word(particle))return 9;
 puts("Original liveSnowTrail CPUparameters matchnativekernel preparation: capacity,age,lifetime,size,force,allscatter andcolour coefficients");
 PS2Runtime rt;std::mt19937 random(0x439a40);std::uniform_real_distribution<float> unit(-1,1);std::array<uint8_t,16384>data{};
 auto q=[&](unsigned index,std::array<float,4>v){memcpy(data.data()+index*16,v.data(),16);};auto v=[&](unsigned index,ssx::SnowVector x){q(index,{x[0],x[1],x[2],0});};constexpr unsigned top=32;
 auto run=[&](uint32_t begin,uint32_t stop){auto code=original;uint32_t word;memcpy(&word,code.data()+stop+4,4);word|=0x40000000;memcpy(code.data()+stop+4,&word,4);rt.vu1().execute(code.data(),code.size(),data.data(),data.size(),rt.gs(),nullptr,begin,top,0,2000);};

 rt.registerFunction(0x3710d0,sub_003710D0_0x3710d0);rt.registerFunction(0x3177f0,sub_003177F0_0x3177f0);rt.registerFunction(0x317a08,sub_00317A08_0x317a08);
 constexpr uint32_t emitAt=0x1a00000,posAt=0x1a01000,velAt=0x1a02000,colAt=0x1a03000,argsAt=0x1a04000;
 auto write=[&](uint32_t at,const auto&value){memcpy(ee.data()+at,&value,sizeof(value));};memset(ee.data()+emitAt,0,0x5000);write(emitAt+0x174,1u);write(emitAt+0x178,prepared.birthCapacity);write(emitAt+0x1a0,posAt);write(emitAt+0x1a4,velAt);write(emitAt+0x204,colAt);write(emitAt+0x0c,1u);write(emitAt+0x14,20.f);
 ssx::OriginalRandomState visual{{1,2,3,4,5,6}};write(0x4ff018,visual.words);ssx::OriginalSnowParticles ring;float flipPhase=0;
 for(unsigned tick=0;tick<20000;++tick){std::fesetround(FE_TOWARDZERO);ssx::OriginalSnowEmission request;request.active=tick%5;ssx::SnowVector velocity;ssx::SnowColour colour;
  for(unsigned k=0;k<3;++k){request.positionCm[k]=unit(random)*100000;velocity[k]=unit(random)*3000;}for(auto&v:colour)v=unit(random)*2;
  if(tick%3)request.velocityCmps=velocity;if(tick%7)request.colour=colour;write(argsAt,request.positionCm);write(argsAt+12,1.f);write(argsAt+16,velocity);write(argsAt+28,0.f);write(argsAt+32,colour);
  const int flipCount=tick%2?8:1;const float flipRate=tick%3?45.f:30.f;
  const float times[]={0.f,1.f/120.f,1.f/60.f,1.f/30.f,.05f};request.stepSeconds=times[tick%5];
  const bool enabled=tick%7!=0;write(emitAt+0x174,unsigned(enabled));write(emitAt+0xc,flipCount);write(emitAt+0x14,flipRate);
  R5900Context c{};SET_GPR_U32((&c),4,emitAt);SET_GPR_U32((&c),5,argsAt);SET_GPR_U32((&c),6,request.velocityCmps?argsAt+16:0);SET_GPR_U32((&c),7,request.colour?argsAt+32:0);SET_GPR_U32((&c),8,request.active);SET_GPR_U32((&c),28,0x4a30f0);SET_GPR_U32((&c),29,0x10000);SET_GPR_U32((&c),31,0x12345678);c.f[12]=request.stepSeconds;sub_003717C0_0x3717c0(ee.data(),&c,&rt);ring.emit(request,visual,enabled);flipPhase=ssx::originalSnowFlipbookStep(flipPhase,flipCount,flipRate,request.stepSeconds,enabled);
  bool good=std::bit_cast<uint32_t>(flipPhase)==word(emitAt+0x10)&&c.pc==0x12345678&&ring.nextBirthSlot()==word(emitAt+0x17c)&&memcmp(visual.words.data(),ee.data()+0x4ff018,24)==0;
  for(unsigned j=0;j<prepared.birthCapacity;++j){const auto&birth=ring.birthHistory()[j];good&=memcmp(birth.positionCm.data(),ee.data()+posAt+j*16,12)==0&&memcmp(birth.velocityCoefficient.data(),ee.data()+velAt+j*16,12)==0&&std::bit_cast<uint32_t>(birth.seed)==word(velAt+j*16+12)&&memcmp(birth.colour.data(),ee.data()+colAt+j*4,4)==0;}
  if(!good){printf("Particlebirthring mismatchtick%u\n",tick);return 10;}
 }
 puts("20,000 original3717C0/3710D0 births exact: enabled/disabled births, flipbook phase with varied counts/rates/dt, ringcursor,allposition/velocity/colour/seedrecords andseparatevisualRNGstate");
 for(unsigned i=0;i<2000;++i){std::fesetround(FE_TOWARDZERO);ssx::OriginalSnowParticleProfile p;auto k=ssx::originalSnowParticleKernel(p);ssx::OriginalSnowBirth a,b;
  for(unsigned axis=0;axis<3;++axis){a.positionCm[axis]=unit(random)*1000;b.positionCm[axis]=unit(random)*1000;a.velocityCoefficient[axis]=unit(random)*1000;b.velocityCoefficient[axis]=unit(random)*1000;}for(unsigned n=0;n<4;++n){a.colour[n]=random()%256;b.colour[n]=random()%256;}
  a.seed=std::bit_cast<float>(0x3f800000u|(random()&0x7fffffu));float age=std::abs(unit(random))*k.lifeBase*.95f,fraction=i%2?.5f:0.f;
  q(0,{.0001f,0,0,0});q(1,{0,.0001f,0,0});q(2,{0,0,.0001f,0});q(3,{0,0,0,1});q(4,{1,1,1,1});q(5,{0,0,0,0});q(6,{1,1,1,1});
  q(top,{std::bit_cast<float>(72u),0,1.5f,k.ageStep});q(top+1,{k.sizeRange,k.lifeRange,k.sizeBase,k.lifeBase});q(top+2,{k.sizeDelta,0,0,0});q(top+3,{0,0,0,0});
  v(top+6,k.force);v(top+7,k.velocityBase);for(unsigned n=0;n<3;++n)v(top+8+n,k.velocityRanges[n]);v(top+11,k.positionBase);v(top+12,k.positionRange0);v(top+13,k.positionRange1);v(top+14,{});
  q(top+15,k.colourBase);q(top+16,k.colourRange0);q(top+17,k.colourRange1);q(top+18,k.colourSlope);q(top+19,{0,0,0,0});q(top+20,{1,1,0,0});q(top+21,{std::bit_cast<float>(1u),std::bit_cast<float>(2u),2.f,age});
  v(top+22,a.positionCm);v(top+23,b.positionCm);q(top+66,{a.velocityCoefficient[0],a.velocityCoefficient[1],a.velocityCoefficient[2],a.seed});q(top+67,{b.velocityCoefficient[0],b.velocityCoefficient[1],b.velocityCoefficient[2],1.f});
  for(unsigned n=0;n<4;++n){uint32_t ca=a.colour[n],cb=b.colour[n];memcpy(data.data()+(top+110)*16+n*4,&ca,4);memcpy(data.data()+(top+111)*16+n*4,&cb,4);}
  rt.vu1().reset();run(0xa00,0xb48);auto initial=rt.vu1().state();initial.vf[14][0]=fraction;
  uint32_t state=std::bit_cast<uint32_t>(a.seed);auto native=ssx::originalSnowParticle(k,a,b,fraction,age,state);if(!native){puts("Unexpecteddead");return 4;}
  rt.vu1().state()=initial;run(0xb58,0xc78);float size=std::abs(rt.vu1().state().vf[25][0]);float life=rt.vu1().state().vf[13][1];
  rt.vu1().state()=initial;run(0xb58,0xcf0);auto position=rt.vu1().state();
  rt.vu1().state()=initial;run(0xb58,0xd60);auto colour=rt.vu1().state();
  bool good=native->halfExtentCm==size&&native->scaledLifetime==life;
  for(unsigned n=0;n<3;++n)good&=native->positionCm[n]==position.vf[25][n];
  for(unsigned n=0;n<4;++n){uint32_t component;memcpy(&component,&colour.vf[30][n],4);good&=native->colourGs[n]==component;}
  if(!good){printf("Particle%u size%.9g/%.9g life%.9g/%.9g\n",i,native->halfExtentCm,size,native->scaledLifetime,life);for(unsigned n=0;n<3;++n)printf("position%u %.9g/%.9g\n",n,native->positionCm[n],position.vf[25][n]);for(unsigned n=0;n<4;++n){uint32_t c;memcpy(&c,&colour.vf[30][n],4);printf("colour%u %u/%u\n",n,native->colourGs[n],c);}return 5;}
 }

 // Couple the real birth ring to the unmodified original inner-loop scheduling.
 // Disable only XGKICK submission; observe model-space values before projection.
 auto kernel=ring.parameters();q(0,{1,0,0,0});q(1,{0,1,0,0});q(2,{0,0,1,0});q(3,{0,0,0,1000000});q(top,{std::bit_cast<float>(kernel.birthCapacity*kernel.particlesPerBirth),0,1.5f,kernel.ageStep});q(top+21,{std::bit_cast<float>(kernel.birthCapacity),std::bit_cast<float>(kernel.particlesPerBirth),float(kernel.particlesPerBirth),0});
 for(unsigned row=0;row<44;++row){unsigned group=std::min(row,kernel.birthCapacity-1);const auto&birth=ring.birthHistory()[(ring.nextBirthSlot()+1+group)%kernel.birthCapacity];v(top+22+row,birth.positionCm);q(top+66+row,{birth.velocityCoefficient[0],birth.velocityCoefficient[1],birth.velocityCoefficient[2],birth.seed});for(unsigned n=0;n<4;++n){uint32_t c=birth.colour[n];memcpy(data.data()+(top+110+row)*16+n*4,&c,4);}}
 auto fullCode=original;for(unsigned at:{0xe08,0xed8}){uint32_t nop=0x8000033c;memcpy(fullCode.data()+at,&nop,4);}rt.vu1().reset();rt.vu1().execute(fullCode.data(),fullCode.size(),data.data(),data.size(),rt.gs(),nullptr,0xa00,top,0,1);
 std::vector<ssx::OriginalSnowParticle> observed;ssx::OriginalSnowParticle observation;unsigned steps=0,lastPC=~0u;
 while(rt.vu1().state().pc!=0xee0&&rt.vu1().state().pc!=0xef0&&steps++<100000){auto&state=rt.vu1().state();if(state.pc!=lastPC){lastPC=state.pc;if(state.pc==0xc88)observation.halfExtentCm=std::abs(state.vf[25][0]);if(state.pc==0xd00){for(unsigned n=0;n<3;++n)observation.positionCm[n]=state.vf[25][n];}if(state.pc==0xd78){for(unsigned n=0;n<4;++n){uint32_t value;memcpy(&value,data.data()+(state.vi[6]+1)*16+n*4,4);observation.colourGs[n]=uint8_t(value);}observed.push_back(observation);}}rt.vu1().resume(fullCode.data(),fullCode.size(),data.data(),data.size(),rt.gs(),nullptr,top,0,1);}
 auto nativeParticles=ring.particles();size_t originalTotal=observed.size(),nativeTotal=nativeParticles.size();std::erase_if(observed,[](const auto&p){return p.colourGs[3]==0;});std::erase_if(nativeParticles,[](const auto&p){return p.colourGs[3]==0;});printf("Original/native traversal totals%zu/%zu (zero-alpha quads excludedfrompixelcomparison)\n",originalTotal,nativeTotal);if(steps>=100000||nativeParticles.size()!=observed.size()){printf("Fullparticlering count%zu/%zu steps%u pc%x\n",nativeParticles.size(),observed.size(),steps,rt.vu1().state().pc);return 11;}
 for(unsigned i=0;i<observed.size();++i){auto&a=nativeParticles[i];auto&b=observed[i];if(a.positionCm!=b.positionCm||a.halfExtentCm!=b.halfExtentCm||a.colourGs!=b.colourGs){printf("Fullparticlering mismatch%u size%.9g/%.9g\n",i,a.halfExtentCm,b.halfExtentCm);for(unsigned n=0;n<3;++n)printf("p %.9g/%.9g\n",a.positionCm[n],b.positionCm[n]);return 12;}}
 printf("Full36-birth originalVU visible traversal matchesnative particles() exactly (%zu visibleparticles)\n",observed.size());
 puts("2,000 originalparticle VU A00 cases exact: worldposition,size,lifetime andGScolour, fractionalbirthinterpolation");
}
