// Development-only original board correction oracle.
#include "ps2_runtime_macros.h"
#include "../engine/rider_pose_motion.hpp"
#include "../engine/ground_pose_motion.hpp"
#include "../engine/original_float.hpp"
#include "../engine/ground_animation_control.hpp"
#include <fstream>
#include <cfenv>
#include <cstdio>
#include <random>
void sub_00131620_0x131620(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0012EE30_0x12ee30(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0013D818_0x13d818(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0031C228_0x31c228(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0031C040_0x31c040(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0011FA10_0x11fa10(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0011EB98_0x11eb98(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0031BE50_0x31be50(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0031C128_0x31c128(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x320000,g_ps2RecompiledFunctionTableSlotCount=0x88000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0x88000]={nullptr};
static unsigned animationClass=7,animationIndex=5,selectedPrewind=0;
int main(int argc,char**argv){PS2Runtime rt;std::ifstream file(argv[1],std::ios::binary);std::vector<uint8_t>ram((std::istreambuf_iterator<char>(file)),{});if(ram.size()!=32*1024*1024)return 2;
 auto wr=[&](unsigned at,const auto&v){memcpy(ram.data()+at,&v,sizeof(v));};auto rd=[&](unsigned at){float f;memcpy(&f,ram.data()+at,4);return f;};
 rt.registerFunction(0x3128E8,[](uint8_t*,R5900Context*c,PS2Runtime*){selectedPrewind=GPR_U32(c,5);c->pc=GPR_U32(c,31);});
 rt.registerFunction(0x311AE8,[](uint8_t*,R5900Context*c,PS2Runtime*){SET_GPR_U32(c,2,animationClass);c->pc=GPR_U32(c,31);});
 rt.registerFunction(0x312AA0,[](uint8_t*,R5900Context*c,PS2Runtime*){SET_GPR_U32(c,2,animationIndex);c->pc=GPR_U32(c,31);});
 rt.registerFunction(0x31C228,sub_0031C228_0x31c228);
 rt.registerFunction(0x31C040,sub_0031C040_0x31c040);
 rt.registerFunction(0x31BE50,sub_0031BE50_0x31be50);rt.registerFunction(0x31C128,sub_0031C128_0x31c128);
 std::fesetround(FE_TOWARDZERO);std::mt19937 rng(0x504f5345);std::uniform_real_distribution<float> value(-1,1);
 for(unsigned i=0;i<10000;++i){ssx::RiderPoseContact p;p.boardRoot=0;p.boardChild=1;p.boardAlignment=.2f+float(i%9)*.1f;p.boardLiftCm=float(i%10)*.1f;
  std::vector<ssx::AnimationTransform>world(2),local(2);float sum=0;for(float&v:world[0].rotation){v=value(rng);sum+=v*v;}float scale=1.f/std::sqrt(sum);for(float&v:world[0].rotation)v*=scale;
  p.boardDirection={value(rng),value(rng),value(rng)};sum=0;for(float v:p.boardDirection)sum+=v*v;scale=1.f/std::sqrt(sum);for(float&v:p.boardDirection)v*=scale;p.normal=p.boardDirection;
  world[0].position={value(rng)*1000,value(rng)*1000,value(rng)*1000};wr(0x20000+0x780,uint32_t(0x30000));wr(0x20000+0x8a0,uint32_t(0));wr(0x30000+0x2c,uint32_t(0x40000));wr(0x202bc,p.boardAlignment);wr(0x2031c,p.boardLiftCm);wr(0x20390,p.boardDirection);wr(0x2039c,0.f);wr(0x20370,p.normal);wr(0x2037c,0.f);wr(0x40000,world[0].position);wr(0x4000c,1.f);wr(0x40010,world[0].rotation);
  R5900Context c{};c.vu0_vf[0]=_mm_set_ps(1,0,0,0);SET_GPR_U32((&c),28,0x4a30f0);SET_GPR_U32((&c),29,0x10000);SET_GPR_U32((&c),19,0x20000);c.pc=0x11ed80;sub_0011EB98_0x11eb98(ram.data(),&c,&rt);if(c.pc!=0x12345678)return 3;
  ssx::originalRiderPoseContact(world,local,{1,1,1},p);
  for(unsigned k=0;k<4;++k)if(world[0].rotation[k]!=rd(0x40010+k*4)){printf("board quaternion mismatch %u/%u original%.9g native%.9g\n",i,k,rd(0x40010+k*4),world[0].rotation[k]);return 4;}
  for(unsigned k=0;k<3;++k)if(world[0].position[k]!=rd(0x40000+k*4)){printf("board position mismatch %u\n",i);return 5;}
 }
 for(unsigned i=0;i<20000;++i){ssx::AnimationTransform physical;ssx::AnimationVector pivot,scale;ssx::RiderRootPresentation state;
  for(unsigned k=0;k<3;++k){physical.position[k]=value(rng)*10000;pivot[k]=value(rng)*100;scale[k]=.8f+value(rng)*.2f;state.lateral[k]=value(rng);}
  for(float&q:physical.rotation)q=value(rng);state.turn=i%4?value(rng):0;state.extraLean=i%3?value(rng):0;state.brake=i%5?value(rng):0;state.roll=i%7?value(rng):0;
  wr(0x201f0,state.turn);wr(0x20208,state.extraLean);wr(0x20214,state.brake);wr(0x20250,state.roll);wr(0x203b0,state.lateral);wr(0x203bc,0.f);wr(0x30024,uint32_t(0x45000));wr(0x30140,scale);wr(0x3014c,1.f);wr(0x45000,pivot);wr(0x4500c,1.f);wr(0x50000,physical.position);wr(0x5000c,1.f);wr(0x50010,physical.rotation);
  R5900Context c{};c.vu0_vf[0]=_mm_set_ps(1,0,0,0);SET_GPR_U32((&c),28,0x4a30f0);SET_GPR_U32((&c),29,0x10000);SET_GPR_U32((&c),4,0x20000);SET_GPR_U32((&c),5,0x50000);SET_GPR_U32((&c),31,0x12345678);sub_0011FA10_0x11fa10(ram.data(),&c,&rt);
  auto native=ssx::originalRiderRootPresentation(physical,pivot,scale,state);if(c.pc!=0x12345678)return 6;
  for(unsigned k=0;k<3;++k)if(native.position[k]!=rd(0x50000+k*4)){printf("presentation position mismatch%u/%u %.9g %.9g\n",i,k,native.position[k],rd(0x50000+k*4));return 7;}
  for(unsigned k=0;k<4;++k)if(native.rotation[k]!=rd(0x50010+k*4)){printf("presentation rotation mismatch%u/%u %.9g %.9g\n",i,k,native.rotation[k],rd(0x50010+k*4));return 8;}
 }
 for(unsigned i=0;i<20000;++i){ssx::OriginalGroundState state;state.boardBouncePhase=value(rng)*8;state.turn.current=value(rng);state.brake.current=value(rng);std::array<float,3> velocity{value(rng)*3000,value(rng)*3000,value(rng)*3000};
  wr(0x60000,state.boardBouncePhase);wr(0x60018,uint32_t(0x20000));wr(0x20214,state.brake.current);wr(0x10000,velocity);wr(0x1000c,0.f);
  R5900Context c{};c.vu0_vf[0]=_mm_set_ps(1,0,0,0);SET_GPR_U32((&c),28,0x4a30f0);SET_GPR_U32((&c),29,0x10000);SET_GPR_U32((&c),17,0x60000);c.f[27]=-state.turn.current;c.pc=0x13f068;sub_0013D818_0x13d818(ram.data(),&c,&rt);if(c.pc!=0x12345678)return 9;
  ssx::originalGroundBoardLift(state,velocity);if(state.boardBouncePhase!=rd(0x60000)||state.boardLift!=rd(0x2031c)){printf("board bounce mismatch%u phase%.9g/%.9g lift%.9g/%.9g\n",i,state.boardBouncePhase,rd(0x60000),state.boardLift,rd(0x2031c));return 10;}
 }
 for(unsigned i=0;i<20000;++i){ssx::OriginalGroundProfile profile;profile.bodyScale=.8f+value(rng)*.2f;ssx::OriginalGroundState state;state.animationClass=animationClass=i%13;state.animationIndex=animationIndex=i%3?5:22;state.crouch.current=(value(rng)+1)*.5f;state.presentationLift.current=value(rng)*20;
  ssx::OriginalGroundDiagnostics diagnostic;diagnostic.stepTime=1.f/60;diagnostic.poseReferenceHeight=value(rng)*10000;diagnostic.leaningNormal={value(rng),value(rng),value(rng)};diagnostic.boardNormalForPose=diagnostic.leaningNormal;state.position={value(rng)*1000,value(rng)*1000,value(rng)*1000};
  wr(0x20110,state.position);wr(0x2011c,1.f);wr(0x20390,diagnostic.leaningNormal);wr(0x2039c,0.f);wr(0x20220,state.crouch.current);wr(0x202c8,state.presentationLift.current);wr(0x60018,uint32_t(0x20000));
  R5900Context c{};c.vu0_vf[0]=_mm_set_ps(1,0,0,0);SET_GPR_U32((&c),28,0x4a30f0);SET_GPR_U32((&c),29,0x10000);SET_GPR_U32((&c),17,0x60000);SET_GPR_U32((&c),4,0x20000);c.pc=0x13e148;c.f[3]=diagnostic.poseReferenceHeight;c.f[1]=1;c.f[5]=10;c.f[6]=-20;c.f[25]=diagnostic.stepTime;c.f[28]=profile.bodyScale;c.f[4]=ssx::originalScalarSubtract(1.f,diagnostic.stepTime*20.f);sub_0013D818_0x13d818(ram.data(),&c,&rt);if(c.pc!=0x12345678)return 11;
  ssx::originalGroundPresentationTarget(profile,state,diagnostic);if(state.presentationLift.rate!=rd(0x202cc)||state.presentationLift.target!=rd(0x202d0)){printf("root lift target mismatch%u %.9g/%.9g rate%.9g/%.9g\n",i,state.presentationLift.target,rd(0x202d0),state.presentationLift.rate,rd(0x202cc));return 12;}
 }
 for(unsigned i=0;i<20000;++i){ssx::OriginalGroundProfile profile;ssx::OriginalGroundState state;static constexpr int types[]{5,6,7,8,11};state.animationIndex=types[i%5];state.animationClass=7;state.turn.current=value(rng);state.animationTurn.current=value(rng);state.brake.current=i%2?value(rng):0;state.crouch.current=(value(rng)+1)*.5f;state.boost=i%4==0?1:0;state.lateral={0,1,value(rng)*.5f};state.velocity={1000+std::abs(value(rng))*1500,0,0};
  wr(0x60000,uint32_t(0x20000));wr(0x201e0,state.velocity);wr(0x201ec,0.f);wr(0x201f0,state.turn.current);wr(0x201fc,state.animationTurn.current);wr(0x20214,state.brake.current);wr(0x20220,state.crouch.current);wr(0x202fc,state.boost);wr(0x203b0,state.lateral);wr(0x20438,uint32_t(0));
  R5900Context c{};c.vu0_vf[0]=_mm_set_ps(1,0,0,0);SET_GPR_U32((&c),28,0x4a30f0);SET_GPR_U32((&c),29,0x10000);SET_GPR_U32((&c),17,0x60000);SET_GPR_U32((&c),3,0x20000);SET_GPR_U32((&c),16,state.animationIndex);SET_GPR_U32((&c),18,state.animationIndex);c.pc=0x131980;sub_00131620_0x131620(ram.data(),&c,&rt);if(c.pc!=0x12345678)return 13;
  if(!ssx::originalSelectGroundAnimation(profile,state,{})||state.animationIndex!=int(GPR_U32((&c),16))){printf("normal animation selection mismatch%u %d/%u\n",i,state.animationIndex,GPR_U32((&c),16));return 14;}
 }
 for(unsigned style=0;style<3;++style){wr(0x202a4,0.f);wr(0x202b0,0.f);wr(0x20320,uint32_t(0));wr(0x20328,style);R5900Context c{};c.vu0_vf[0]=_mm_set_ps(1,0,0,0);SET_GPR_U32((&c),28,0x4a30f0);SET_GPR_U32((&c),29,0x10000);SET_GPR_U32((&c),31,0x12345678);SET_GPR_U32((&c),4,0x60000);SET_GPR_U32((&c),5,0x43d788);selectedPrewind=0;sub_0012EE30_0x12ee30(ram.data(),&c,&rt);ssx::OriginalGroundState s;s.controlState=2;s.prewindStyle=style;if(!ssx::originalSelectGroundAnimation({},s,{})||s.animationIndex!=int(selectedPrewind))return 15;}
 for(unsigned i=0;i<20000;++i){float spin=i%7?value(rng)*2:0,flip=i%9?value(rng)*2:0;unsigned style=i%5;bool reverse=i&1;wr(0x60000,uint32_t(0x20000));wr(0x202a4,spin);wr(0x202b0,flip);wr(0x20320,uint32_t(reverse));wr(0x20328,style);animationIndex=5;
  R5900Context c{};SET_GPR_U32((&c),28,0x4a30f0);SET_GPR_U32((&c),29,0x10000);SET_GPR_U32((&c),31,0x12345678);SET_GPR_U32((&c),4,0x60000);SET_GPR_U32((&c),5,0x43d788);selectedPrewind=0;sub_0012EE30_0x12ee30(ram.data(),&c,&rt);
  int actual=ssx::originalPrewindAnimation(spin,flip,reverse,style);if(c.pc!=0x12345678||actual!=int(selectedPrewind)){printf("prewind semantic mismatch%u spin%.9g flip%.9g reverse%d style%u %d/%u\n",i,spin,flip,reverse,style,actual,selectedPrewind);return 16;}
  SET_GPR_U32((&c),4,0x60000);SET_GPR_U32((&c),5,0x43d840);SET_GPR_U32((&c),31,0x12345678);c.pc=0x12ee30;selectedPrewind=0;sub_0012EE30_0x12ee30(ram.data(),&c,&rt);
  actual=ssx::originalAirReleaseAnimation(spin,flip,reverse,style);if(c.pc!=0x12345678||actual!=int(selectedPrewind)){printf("release semantic mismatch%u %d/%u\n",i,actual,selectedPrewind);return 17;}}
 puts("20,000 complete five-style air release selections exact");
 puts("20,000 complete five-style directional prewind selections exact");
 puts("20,000 ordinary ground semantic selections +3 zero-axis prewind styles exact");
 puts("20,000 original root lift target stages bit-identical");
 puts("20,000 original board-bounce phase/lift stages bit-identical");
 puts("20,000 original root lean/pivot presentation stages bit-identical");
 puts("10,000 original board correction stages bit-identical");}
