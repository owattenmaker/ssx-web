// Development-only original-code air angular/pose conformance.
#include "ps2_runtime_macros.h"
#include "../engine/air_control.hpp"
#include "../engine/air_entry.hpp"
#include "../engine/ground_animation_control.hpp"
#include <fstream>
#include <random>
#include <cfenv>
#include <cstdio>
#include <bit>
void sub_00133128_0x133128(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00133308_0x133308(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00135B30_0x135b30(uint8_t*,R5900Context*,PS2Runtime*);
void sub_001158B8_0x1158b8(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0031BE50_0x31be50(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0031C228_0x31c228(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00134DD0_0x134dd0(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00135180_0x135180(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00114CC0_0x114cc0(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00115168_0x115168(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00134CB0_0x134cb0(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0011E098_0x11e098(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000;
extern const uint32_t g_ps2RecompiledFunctionTableEnd=0x410000;
extern const uint32_t g_ps2RecompiledFunctionTableSlotCount=0xc4000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xc4000]={nullptr};
static float trickStat;static bool landing,grabActive;
// 0x135BE0 (engine/air_switch.hpp, own oracle) is scripted here: the call sites, their conditions and the
// phase-3 skip are checked by applying the same marker mutation on both sides.
static bool switchResult;static unsigned switchCalls,nativeSwitchCalls;
static void switchMark(float* f){f[8]=f[8]+1.f;for(unsigned o:{4u,6u,9u,11u})f[o]=-f[o];}
int main(int argc,char**argv){
 PS2Runtime rt;std::ifstream file(argv[1],std::ios::binary);std::vector<uint8_t> ram((std::istreambuf_iterator<char>(file)),{});if(ram.size()!=32*1024*1024)return 2;
 auto wr=[&](unsigned a,const auto&v){std::memcpy(ram.data()+a,&v,sizeof(v));};auto rf=[&](unsigned a){float x;std::memcpy(&x,ram.data()+a,4);return x;};
 auto ctx=[](){R5900Context c{};c.vu0_vf[0]=_mm_set_ps(1,0,0,0);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,29,0x10000);SET_GPR_U32(&c,31,0x12345678);return c;};
 auto zero=[](uint8_t*,R5900Context*c,PS2Runtime*){SET_GPR_U32(c,2,0);c->pc=GPR_U32(c,31);};
 for(unsigned a:{0x14DC80u,0x14DD58u,0x10E098u,0x114130u})rt.registerFunction(a,zero);
 rt.registerFunction(0x135BE0,[](uint8_t*m,R5900Context*c,PS2Runtime*){++switchCalls;if(switchResult)switchMark(reinterpret_cast<float*>(m+GPR_U32(c,4)));SET_GPR_U32(c,2,switchResult);c->pc=GPR_U32(c,31);});
 rt.registerFunction(0x1352A8,[](uint8_t*,R5900Context*c,PS2Runtime*){SET_GPR_U32(c,2,grabActive);c->pc=GPR_U32(c,31);});
 rt.registerFunction(0x119C98,[](uint8_t*,R5900Context*c,PS2Runtime*){c->f[0]=0;c->pc=GPR_U32(c,31);});
 rt.registerFunction(0x1495A8,[](uint8_t*,R5900Context*c,PS2Runtime*){c->f[0]=trickStat;c->pc=GPR_U32(c,31);});
 rt.registerFunction(0x311B20,[](uint8_t*,R5900Context*c,PS2Runtime*){SET_GPR_U32(c,2,0x80000);c->pc=GPR_U32(c,31);});
 rt.registerFunction(0x312AA0,[](uint8_t*,R5900Context*c,PS2Runtime*){SET_GPR_U32(c,2,landing?0x120:0);c->pc=GPR_U32(c,31);});
 rt.registerFunction(0x135B30,sub_00135B30_0x135b30);rt.registerFunction(0x1158B8,sub_001158B8_0x1158b8);
 rt.registerFunction(0x31BE50,sub_0031BE50_0x31be50);rt.registerFunction(0x31C228,sub_0031C228_0x31c228);rt.registerFunction(0x135180,sub_00135180_0x135180);
 rt.registerFunction(0x311B48,zero);rt.registerFunction(0x115168,sub_00115168_0x115168);rt.registerFunction(0x311E88,zero);rt.registerFunction(0x134DD0,sub_00134DD0_0x134dd0);rt.registerFunction(0x11E098,sub_0011E098_0x11e098);
 std::fesetround(FE_TOWARDZERO);std::mt19937 rng(0x133308);std::uniform_real_distribution<float>d(-1,1);
 constexpr unsigned control=0x50000,rider=0x30000;wr(control+0x58,uint32_t(rider));
 auto stateWrite=[&](const ssx::OriginalAirControlState&s){
  wr(control,int32_t(s.mode));wr(control+12,int32_t(s.phase));const float values[]={s.targetFlip,s.targetSpin,s.progressFlip,s.progressSpin,s.totalSpin,s.totalFlip,s.adjustSpin,s.adjustFlip,s.scoredSpin,s.scoredFlip,s.maxSpin,s.maxFlip,s.axisBlend};
  for(unsigned i=0;i<13;i++)wr(control+16+i*4,values[i]);wr(control+0x44,int32_t(s.extended));
  wr(control+0x48,s.holdSpin);wr(control+0x4c,s.holdFlip);wr(control+0x50,s.inputAngle);wr(control+0x54,s.idleTime);wr(rider+0x2dc,s.spinRate);wr(rider+0x2e0,s.flipRate);
 };
 auto compare=[&](unsigned k,const ssx::OriginalAirControlState&s){
  int mode;std::memcpy(&mode,ram.data()+control,4);if(mode!=s.mode){printf("mode mismatch%u expected%d actual%d\n",k,mode,s.mode);return false;}
  int phase,extended;std::memcpy(&phase,ram.data()+control+12,4);std::memcpy(&extended,ram.data()+control+0x44,4);
  if(phase!=s.phase||extended!=s.extended){printf("phase mismatch%u %d/%d expected%d/%d\n",k,s.phase,s.extended,phase,extended);return false;}
  const float values[]={s.targetFlip,s.targetSpin,s.progressFlip,s.progressSpin,s.totalSpin,s.totalFlip,s.adjustSpin,s.adjustFlip,s.scoredSpin,s.scoredFlip,s.maxSpin,s.maxFlip,s.axisBlend};
  for(unsigned i=0;i<13;i++)if(values[i]!=rf(control+16+i*4)){printf("state mismatch%u offset%x expected%.9g actual%.9g\n",k,16+i*4,rf(control+16+i*4),values[i]);return false;}
  const std::pair<unsigned,float> extras[]={{0x48,s.holdSpin},{0x4c,s.holdFlip},{0x50,s.inputAngle},{0x54,s.idleTime}};
  for(auto [off,v]:extras)if(v!=rf(control+off)){printf("state mismatch%u offset%x expected%.9g actual%.9g\n",k,off,rf(control+off),v);return false;}
  if(s.spinRate!=rf(rider+0x2dc)||s.flipRate!=rf(rider+0x2e0)){printf("rate mismatch%u expected%.9g %.9g actual%.9g %.9g\n",k,rf(rider+0x2dc),rf(rider+0x2e0),s.spinRate,s.flipRate);return false;}
  return true;
 };
 for(unsigned k=0;k<20000;k++){
  float flip=d(rng),spin=d(rng),step=std::bit_cast<float>(0x3f490fdcu),dead=k%2?.2f:0.f;
  wr(0x40000,flip);wr(0x40004,spin);auto c=ctx();SET_GPR_U32(&c,5,0x40000);SET_GPR_U32(&c,6,0x40004);c.f[12]=step;c.f[13]=dead;
  sub_001158B8_0x1158b8(ram.data(),&c,&rt);auto direction=ssx::originalAirDirection(flip,spin,step,dead);
  if(direction[0]!=rf(0x40000)||direction[1]!=rf(0x40004)){printf("direction mismatch%u\n",k);return 3;}
  ssx::OriginalAirControlState s;s.phase=k%4;s.targetSpin=float(int(rng()%7)-3)*std::bit_cast<float>(0x40490fdcu);s.targetFlip=float(int(rng()%5)-2)*std::bit_cast<float>(0x40c90fdcu);
  s.progressSpin=d(rng)*8;s.progressFlip=d(rng)*12;s.totalSpin=d(rng)*8;s.totalFlip=d(rng)*12;s.adjustSpin=d(rng);s.adjustFlip=d(rng);
  s.maxSpin=3+d(rng);s.maxFlip=3+d(rng);s.spinRate=3;s.flipRate=3;s.holdSpin=(d(rng)+1)*.5f;s.holdFlip=(d(rng)+1)*.5f;s.idleTime=(d(rng)+1)*.5f;
  ssx::RiderInput input;input.spin=k%3?d(rng):0;input.flip=k%2?d(rng):0;input.airAdjustFB=d(rng);input.airAdjustLR=d(rng);input.boardPress=k%5==0?1:k%7==0?-1:0;
  ssx::OriginalAirControlProfile p;p.trickStat=trickStat=(d(rng)+1)*.5f;p.landingAnimation=landing=k%11==0;p.boostModifier=k%3==0;p.airSwitch=[](ssx::OriginalAirControlState& st){++nativeSwitchCalls;if(switchResult){float f[12];f[4]=st.targetFlip;f[6]=st.progressFlip;f[8]=st.totalSpin;f[9]=st.totalFlip;f[11]=st.adjustFlip;switchMark(f);st.targetFlip=f[4];st.progressFlip=f[6];st.totalSpin=f[8];st.totalFlip=f[9];st.adjustFlip=f[11];}return switchResult;};switchResult=k%3==1;switchCalls=nativeSwitchCalls=0;
  stateWrite(s);wr(rider+0x2ec,p.boostModifier?1.f:0.f);wr(0x10000,input.flip);wr(0x10004,input.spin);
  c=ctx();c.pc=0x13366c;SET_GPR_U32(&c,16,control);SET_GPR_U32(&c,18,0);SET_GPR_U32(&c,19,s.phase);
  c.f[23]=input.boardPress?input.boardPress:input.airAdjustFB;c.f[24]=input.airAdjustLR;c.f[27]=std::bit_cast<float>(input.boardPress?0x3eb2b8c4u:0x3fdf66f4u);
  sub_00133308_0x133308(ram.data(),&c,&rt);if(c.pc!=0x12345678){printf("bad angular continuation%x\n",c.pc);return 4;}
  ssx::OriginalAirControlFrame frame;ssx::originalAirControlStep(s,input,p,&frame);if(frame.phaseBefore!=int(GPR_S32((&c),19))||std::bit_cast<uint32_t>(frame.effectiveSpin)!=std::bit_cast<uint32_t>(rf(0x10004))||std::bit_cast<uint32_t>(frame.effectiveFlip)!=std::bit_cast<uint32_t>(rf(0x10000))){printf("Angular frame output mismatch%u\n",k);return 16;}if(!compare(k,s)){
   printf("input spin%.9g flip%.9g LR%.9g FB%.9g board%.9g stat%.9g phase%d\n",input.spin,input.flip,input.airAdjustLR,input.airAdjustFB,input.boardPress,p.trickStat,s.phase);
   printf("targets expected%.9g %.9g actual%.9g %.9g holdexpected%.9g %.9g actual%.9g %.9g\n",rf(control+0x10),rf(control+0x14),s.targetFlip,s.targetSpin,rf(control+0x48),rf(control+0x4c),s.holdSpin,s.holdFlip);
   printf("adjust expected%.9g %.9g actual%.9g %.9g ratesexp%.9g %.9g actual%.9g %.9g\n",rf(control+0x28),rf(control+0x2c),s.adjustSpin,s.adjustFlip,rf(rider+0x2dc),rf(rider+0x2e0),s.spinRate,s.flipRate);return 5;}
  if(switchCalls!=nativeSwitchCalls){printf("0x135BE0 call mismatch%u %u/%u\n",k,switchCalls,nativeSwitchCalls);return 18;}
 }
 for(unsigned k=0;k<20000;k++){
  float spin=k%3?d(rng):0,flip=k%2?d(rng):0;
  trickStat=(d(rng)+1)*.5f;wr(0x80090,-19.f);
  ssx::OriginalAirControlState s=ssx::originalAirControlBegin(spin,flip);
  wr(rider+0x2a4,spin);wr(rider+0x2b0,flip);auto c=ctx();SET_GPR_U32(&c,4,control);c.pc=0x133128;
  sub_00133128_0x133128(ram.data(),&c,&rt);if(c.pc!=0x12345678||!compare(k,s)){printf("air entry mismatch\n");return 10;}
  float expectedRate=(spin!=0||flip!=0)?ssx::originalAirReleaseAnimationRate(trickStat,s.spinRate,s.flipRate):-19.f;
  if(rf(0x80090)!=expectedRate){printf("Air release sequence rate mismatch%u %.9g/%.9g\n",k,rf(0x80090),expectedRate);return 15;}
  // Full original command-prefix stage, including prewind mode1 and grab-releasemode3.
  ssx::RiderInput in;int rs=int(rng()%63)-31,rfv=int(rng()%63)-31,lr=int(rng()%63)-31,fb=int(rng()%63)-31,bp=int(rng()%3)-1;
  float reciprocal=std::bit_cast<float>(0x3d042108u);in.spin=float(rs)*reciprocal;in.flip=float(rfv)*reciprocal;in.airAdjustLR=float(lr)*reciprocal;in.airAdjustFB=float(fb)*reciprocal;in.boardPress=float(bp);
  if(k%4==0){s.mode=1;rs=rfv=0;in.spin=in.flip=0;}if(k%7==0)s.mode=3;
  ssx::OriginalAirControlProfile profile;profile.trickStat=trickStat=(d(rng)+1)*.5f;profile.grabLifecycleResolved=true;profile.grabActive=grabActive=k%3==0;profile.landingAnimation=landing=false;profile.boostModifier=k%2;profile.airSwitch=[](ssx::OriginalAirControlState& st){++nativeSwitchCalls;if(switchResult){float f[12];f[4]=st.targetFlip;f[6]=st.progressFlip;f[8]=st.totalSpin;f[9]=st.totalFlip;f[11]=st.adjustFlip;switchMark(f);st.targetFlip=f[4];st.progressFlip=f[6];st.totalSpin=f[8];st.totalFlip=f[9];st.adjustFlip=f[11];}return switchResult;};switchResult=k%5==2;switchCalls=nativeSwitchCalls=0;
  s.phase=k%4;s.progressSpin=d(rng)*8;s.progressFlip=d(rng)*12;s.adjustSpin=d(rng);s.adjustFlip=d(rng);
  s.holdSpin=d(rng)+1;s.holdFlip=d(rng)+1;s.idleTime=d(rng)+1;
  stateWrite(s);wr(rider+0x2ec,profile.boostModifier?1.f:0.f);wr(0x90000,uint32_t(0xff0000)|((uint32_t(rs)&63)<<24));wr(0x90004,(uint32_t(rfv)&63)|((uint32_t(fb)&63)<<6)|((uint32_t(lr)&63)<<12)|((uint32_t(bp)&3)<<18));
  c=ctx();c.pc=0x1333e0;SET_GPR_U32(&c,16,control);SET_GPR_U32(&c,17,0x90000);
  sub_00133308_0x133308(ram.data(),&c,&rt);if(c.pc!=0x12345678){printf("bad air prefixcontinuation%x\n",c.pc);return 11;}
  ssx::OriginalAirControlFrame frame;ssx::originalAirControlStep(s,in,profile,&frame);if(frame.phaseBefore!=int(GPR_S32((&c),19))||std::bit_cast<uint32_t>(frame.effectiveSpin)!=std::bit_cast<uint32_t>(rf(0x10004))||std::bit_cast<uint32_t>(frame.effectiveFlip)!=std::bit_cast<uint32_t>(rf(0x10000))){printf("Angular prefix frame output mismatch%u\n",k);return 17;}if(!compare(k,s)){printf("air prefix mismatch\n");return 12;}if(switchCalls!=nativeSwitchCalls){printf("0x135BE0 prefix call mismatch%u %u/%u\n",k,switchCalls,nativeSwitchCalls);return 19;}
 }
 printf("20000 original prewind entry, release sequence rates and command-prefix/mode-continuation cases exact\n");
 for(unsigned k=0;k<20000;k++){
  ssx::OriginalAirControlState s;s.totalSpin=d(rng)*20;s.totalFlip=d(rng)*20;s.adjustSpin=d(rng);s.adjustFlip=d(rng);s.axisBlend=(d(rng)+1)*.5f;s.spinRate=d(rng)*10;s.flipRate=d(rng)*10;
  ssx::OriginalAirPresentation p;p.position={d(rng)*100000,d(rng)*100000,d(rng)*100000};p.quaternion={d(rng),d(rng),d(rng),d(rng)};
  std::array<float,3> pivot={d(rng)*100,d(rng)*100,d(rng)*100};
  stateWrite(s);wr(rider+0x780,uint32_t(0x70000));wr(rider+0x89c,uint32_t(0));wr(0x70024,uint32_t(0x71000));
  for(int i=0;i<4;i++){wr(0x70140+i*4,1.f);wr(0x72010+i*4,p.quaternion[i]);}
  for(int i=0;i<3;i++){wr(0x71000+i*4,pivot[i]);wr(0x72000+i*4,p.position[i]);}wr(0x7100c,1.f);wr(0x7200c,1.f);
  auto c=ctx();c.pc=0x134dd0;SET_GPR_U32(&c,4,control);SET_GPR_U32(&c,5,0x72000);sub_00134DD0_0x134dd0(ram.data(),&c,&rt);
  if(c.pc!=0x12345678){printf("bad pose continuation%x\n",c.pc);return 6;}
  auto pose=ssx::originalAirPresentation(s,p,pivot);
  for(int i=0;i<3;i++)if(pose.position[i]!=rf(0x72000+i*4)){printf("pose position mismatch%u/%d expected%.9g actual%.9g\n",k,i,rf(0x72000+i*4),pose.position[i]);return 7;}
  for(int i=0;i<4;i++)if(pose.quaternion[i]!=rf(0x72010+i*4)){printf("pose quaternion mismatch%u/%d expected%.9g actual%.9g\n",k,i,rf(0x72010+i*4),pose.quaternion[i]);return 8;}
  if(s.axisBlend!=rf(control+0x40)){printf("pose axisblend mismatch%u\n",k);return 9;}
 }
 for(unsigned k=0;k<20000;k++){
  ssx::OriginalAirControlState s;s.totalSpin=d(rng)*20;s.totalFlip=d(rng)*20;s.adjustSpin=d(rng);s.adjustFlip=d(rng);s.axisBlend=(d(rng)+1)*.5f;s.spinRate=d(rng)*10;s.flipRate=d(rng)*10;s.progressSpin=d(rng);s.targetSpin=d(rng);
  ssx::OriginalAirPrewindState pre;pre.spin={d(rng),d(rng),d(rng)};pre.flip={d(rng),d(rng),d(rng)};pre.jumpGate=1;
  ssx::OriginalAirExitState e;e.physical.position={d(rng)*100000,d(rng)*100000,d(rng)*100000};e.physical.quaternion={d(rng),d(rng),d(rng),d(rng)};e.adjustment28C={d(rng),d(rng),d(rng)};e.adjustment298={d(rng),d(rng),d(rng)};
  std::array<float,3> pivot={d(rng)*100,d(rng)*100,d(rng)*100};stateWrite(s);
  wr(rider+0x2a4,pre.spin);wr(rider+0x2b0,pre.flip);wr(rider+0x28c,e.adjustment28C);wr(rider+0x298,e.adjustment298);
  wr(rider+0x780,uint32_t(0x70000));wr(rider+0x89c,uint32_t(0));wr(0x70024,uint32_t(0x71000));
  for(int i=0;i<4;i++){wr(0x70140+i*4,1.f);wr(rider+0x120+i*4,e.physical.quaternion[i]);}
  for(int i=0;i<3;i++){wr(0x71000+i*4,pivot[i]);wr(rider+0x110+i*4,e.physical.position[i]);}wr(0x7100c,1.f);wr(rider+0x11c,1.f);
  auto c=ctx();c.pc=0x134cb0;SET_GPR_U32(&c,4,control);sub_00134CB0_0x134cb0(ram.data(),&c,&rt);
  ssx::originalAirControlExit(s,pre,e,pivot);
  if(c.pc!=0x12345678||!compare(k,s)){printf("air exit state mismatch%u pc%x\n",k,c.pc);return 20;}
  for(unsigned i=0;i<3;i++)if(e.physical.position[i]!=rf(rider+0x110+i*4)||e.right[i]!=rf(rider+0x1a0+i*4)||e.forward[i]!=rf(rider+0x1b0+i*4)||e.up[i]!=rf(rider+0x1c0+i*4)){printf("air exit pose mismatch%u/%u\n",k,i);return 21;}
  for(unsigned i=0;i<4;i++)if(e.physical.quaternion[i]!=rf(rider+0x120+i*4)){printf("air exit quaternion mismatch%u/%u\n",k,i);return 22;}
  const std::pair<unsigned,ssx::GroundControlValue> fields[]={{0x2a4,pre.spin},{0x2b0,pre.flip},{0x28c,e.adjustment28C},{0x298,e.adjustment298}};
  for(auto [off,v]:fields)if(v.current!=rf(rider+off)||v.rate!=rf(rider+off+4)||v.target!=rf(rider+off+8)){printf("air exit controls mismatch%u/%x\n",k,off);return 23;}
 }
 for(unsigned k=0;k<20000;k++){
  ssx::OriginalGroundState g;g.quaternion={d(rng),d(rng),d(rng),d(rng)};g.boardUp={d(rng),d(rng),d(rng)};g.forward={d(rng),d(rng),d(rng)};g.lateral={d(rng),d(rng),d(rng)};g.physicalForward={d(rng),d(rng),d(rng)};g.velocity={d(rng)*10000,d(rng)*10000,d(rng)*10000};g.reverseStance=k%2;g.state320Equals324=!g.reverseStance;
  g.turn={d(rng),d(rng),d(rng)};g.brake={k%3?0.f:d(rng),d(rng),d(rng)};g.extraLean={d(rng),d(rng),d(rng)};g.animationTurn={d(rng),d(rng),d(rng)};ssx::GroundControlValue balance{d(rng),d(rng),d(rng)};
  wr(rider+0x120,g.quaternion);wr(rider+0x1b0,g.physicalForward);wr(rider+0x1c0,g.boardUp);wr(rider+0x1cc,0.f);wr(rider+0x1e0,g.velocity);wr(rider+0x1ec,0.f);wr(rider+0x3a0,g.forward);wr(rider+0x3ac,0.f);wr(rider+0x3b0,g.lateral);wr(rider+0x3bc,0.f);
  wr(rider+0x320,uint32_t(g.reverseStance));wr(rider+0x784,uint32_t(0x80000));wr(rider+0x1f0,g.turn);wr(rider+0x214,g.brake);wr(rider+0x208,g.extraLean);wr(rider+0x1fc,g.animationTurn);wr(rider+0x280,balance);
  auto c=ctx();c.pc=0x114cc0;SET_GPR_U32(&c,4,rider);sub_00114CC0_0x114cc0(ram.data(),&c,&rt);auto result=ssx::originalReverseTurn(g,balance);
  if(c.pc!=0x12345678||GPR_U32((&c),2)!=unsigned(result.reversed)){printf("reverse result mismatch%u\n",k);return 24;}
  for(unsigned i=0;i<4;i++)if(g.quaternion[i]!=rf(rider+0x120+i*4)||(result.reversed&&result.animationRootQuaternion[i]!=rf(0x80040+i*4))){printf("reverse quaternion mismatch%u/%u expected%.9g actual%.9g\n",k,i,rf(rider+0x120+i*4),g.quaternion[i]);return 25;}
  for(unsigned i=0;i<3;i++)if(g.forward[i]!=rf(rider+0x3a0+i*4)||g.lateral[i]!=rf(rider+0x3b0+i*4)||g.physicalForward[i]!=rf(rider+0x1b0+i*4)||g.boardUp[i]!=rf(rider+0x1c0+i*4)){printf("reverse axis mismatch%u/%u\n",k,i);return 26;}
  const std::pair<unsigned,ssx::GroundControlValue> fields[]={{0x1f0,g.turn},{0x214,g.brake},{0x208,g.extraLean},{0x1fc,g.animationTurn},{0x280,balance}};
  for(auto [off,v]:fields)if(v.current!=rf(rider+off)||v.rate!=rf(rider+off+4)||v.target!=rf(rider+off+8)){printf("reverse control mismatch%u/%x\n",k,off);return 27;}
 }
 printf("20000 original reverse-stance decisions/physical poses/control flips match exactly\n");
 printf("20000 original airborne exit poses/controls/quantized spin rates match exactly\n");
 printf("20000 original airborne presentation poses/axisblend match exactly\n");
 printf("20000 original direction quantizations and no-contact/no-grab air angular stages match exactly\n");
}
