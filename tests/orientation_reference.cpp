// Development-only original instruction conformance. Product uses native helper.
#include "ps2_runtime_macros.h"
#include "../engine/orientation_motion.hpp"
#include <fstream>
#include <cfenv>
#include <cstdio>
#include <bit>
#include <random>
void sub_0031BE50_0x31be50(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0031C228_0x31c228(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0031C128_0x31c128(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0013D818_0x13d818(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0011E098_0x11e098(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0011DFE0_0x11dfe0(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000;
extern const uint32_t g_ps2RecompiledFunctionTableEnd=0x320000;
extern const uint32_t g_ps2RecompiledFunctionTableSlotCount=0x88000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0x88000]={nullptr};
static unsigned headingControl;static bool captureHeading;static float headingAngle;
int main(int argc,char**argv){
 PS2Runtime rt;std::ifstream input(argv[1],std::ios::binary);std::vector<uint8_t> ram((std::istreambuf_iterator<char>(input)),{});if(ram.size()!=32*1024*1024)return 2;
 auto wr=[&](unsigned at,const auto&v){std::memcpy(ram.data()+at,&v,sizeof(v));};
 auto rd=[&](unsigned at){float x;std::memcpy(&x,ram.data()+at,4);return x;};
 auto ctx=[](){R5900Context c{};c.vu0_vf[0]=_mm_set_ps(1,0,0,0);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,29,0x10000);SET_GPR_U32(&c,31,0x12345678);return c;};
 rt.registerFunction(0x31BE50,sub_0031BE50_0x31be50);
 rt.registerFunction(0x31C128,sub_0031C128_0x31c128);rt.registerFunction(0x31C228,sub_0031C228_0x31c228);
 rt.registerFunction(0x11FEE8,[](uint8_t*,R5900Context*c,PS2Runtime*){SET_GPR_U32(c,2,headingControl);c->pc=GPR_U32(c,31);});
 rt.registerFunction(0x11DFE0,[](uint8_t*m,R5900Context*c,PS2Runtime*rt){if(captureHeading){headingAngle=c->f[12];c->pc=GPR_U32(c,31);}else sub_0011DFE0_0x11dfe0(m,c,rt);});
 rt.registerFunction(0x11E098,[](uint8_t*,R5900Context*c,PS2Runtime*){c->pc=GPR_U32(c,31);});
 std::fesetround(FE_TOWARDZERO);std::mt19937 rng(0x11DFE0);std::uniform_real_distribution<float> a(-20,20),v(-1,1);
 for(unsigned k=0;k<20000;k++){
  float angle=a(rng);auto c=ctx();SET_GPR_U32(&c,4,0x20000);SET_GPR_U32(&c,5,0x20004);c.f[12]=angle;
  sub_0031BE50_0x31be50(ram.data(),&c,&rt);auto sc=ssx::originalSinCos(angle);
  if(sc[0]!=rd(0x20000)||sc[1]!=rd(0x20004)){printf("sincos mismatch %u angle%.9g expected%.9g %.9g actual%.9g %.9g\n",k,angle,rd(0x20000),rd(0x20004),sc[0],sc[1]);return 3;}
  float input=v(rng);c=ctx();c.f[12]=input;sub_0031C128_0x31c128(ram.data(),&c,&rt);
  if(ssx::originalAsin(input)!=c.f[0]){printf("asin mismatch %u\n",k);return 5;}
  std::array<float,4> q={v(rng),v(rng),v(rng),v(rng)};std::array<float,3>axis={v(rng),v(rng),v(rng)};
  for(int i=0;i<4;i++)wr(0x30120+i*4,q[i]);for(int i=0;i<3;i++)wr(0x40000+i*4,axis[i]);wr(0x4000c,0.f);
  c=ctx();SET_GPR_U32(&c,4,0x30000);SET_GPR_U32(&c,5,0x40000);c.f[12]=angle;
  sub_0011DFE0_0x11dfe0(ram.data(),&c,&rt);auto result=ssx::originalRotateOrientation(q,axis,angle);
  for(int i=0;i<4;i++)if(result[i]!=rd(0x30120+i*4)){printf("quaternion mismatch %u/%d expected%.9g native%.9g\n",k,i,rd(0x30120+i*4),result[i]);return 4;}
  for(int i=0;i<4;i++)wr(0x30120+i*4,q[i]);c=ctx();SET_GPR_U32(&c,4,0x30000);sub_0011E098_0x11e098(ram.data(),&c,&rt);
  auto rebuilt=ssx::originalRebuildOrientation(q);
  for(int i=0;i<4;i++)if(rebuilt.quaternion[i]!=rd(0x30120+i*4)){printf("normalized quaternion mismatch%u/%d\n",k,i);return 10;}
  std::array<std::array<float,3>,3> columns={rebuilt.right,rebuilt.forward,rebuilt.up};
  for(int j=0;j<3;j++)for(int i=0;i<3;i++)if(columns[j][i]!=rd(0x301a0+j*16+i*4)){printf("matrix mismatch%u/%d/%d expected%.9g native%.9g\n",k,j,i,rd(0x301a0+j*16+i*4),columns[j][i]);return 11;}
  auto unit=[](auto x){float sum=0;for(float z:x)sum+=z*z;float norm=std::sqrt(sum);for(float& z:x)z/=norm;return x;};
  std::array<float,3> up=unit(std::array<float,3>{v(rng),v(rng),v(rng)}),normal=unit(axis);
  float clearance=k%7==0?10.f:0.f,rate=float(k%60)/10.f;
  for(int i=0;i<4;i++)wr(0x30120+i*4,q[i]);for(int i=0;i<3;i++){wr(0x30370+i*4,normal[i]);wr(0x301c0+i*4,up[i]);}
  wr(0x3037c,0.f);wr(0x301cc,0.f);wr(0x50018,uint32_t(0x30000));wr(0x60038,rate);
  c=ctx();c.pc=0x13ed68;c.f[1]=clearance;SET_GPR_U32(&c,3,0x30000);SET_GPR_U32(&c,17,0x50000);SET_GPR_U32(&c,18,0x60000);
  sub_0013D818_0x13d818(ram.data(),&c,&rt);
  if(c.pc!=0x12345678){printf("Unexpected alignment continuation%x\n",c.pc);return 7;}
  auto aligned=ssx::originalGroundAlignment(q,normal,up,clearance,rate);
  for(int i=0;i<4;i++)if(aligned[i]!=rd(0x30120+i*4)){printf("alignment mismatch %u/%d expected%.9g native%.9g\n",k,i,rd(0x30120+i*4),aligned[i]);return 6;}
 }
 captureHeading=true;
 for(unsigned k=0;k<20000;k++){
  ssx::OriginalHeadingProfile p;
  p.surface28=v(rng);p.surface2C=v(rng);p.surface30=v(rng)*60;p.surface34=v(rng)*200;
  for(int i=0;i<4;i++){
   p.crouchTurnCurve[i]={float(i)*40,v(rng)*90};p.crouchSpeedCurve[i]={float(i)*40,v(rng)};p.directSteerCurve[i]={float(i)*40,v(rng)*.2f};
  }
  ssx::OriginalHeadingState state;state.relativeVelocity={v(rng)*3000,v(rng)*3000,v(rng)*3000};
  auto unit=[](auto x){float sum=0;for(float z:x)sum+=z*z;float norm=std::sqrt(sum);for(float& z:x)z/=norm;return x;};
  state.normal=unit(std::array<float,3>{v(rng),v(rng),v(rng)});state.forward=unit(std::array<float,3>{v(rng),v(rng),v(rng)});
  state.lateral=unit(std::array<float,3>{v(rng),v(rng),v(rng)});state.bodyForward=state.forward;
  state.turn=v(rng);state.charge=(v(rng)+1)*.5f;state.manualSpin=k%3==0?v(rng)*9:0;
  state.dt=1.f/60;state.reverseStance=k%2;state.controlState=headingControl=k%3==1?0:2;
  auto vec=[&](unsigned at,auto x){for(int i=0;i<3;i++)wr(at+4*i,x[i]);wr(at+12,0.f);};
  vec(0x10000,state.relativeVelocity);vec(0x30370,state.normal);vec(0x303a0,state.forward);vec(0x303b0,state.lateral);vec(0x301b0,state.bodyForward);
  wr(0x30220,state.charge);wr(0x301f0,state.turn);wr(0x302dc,state.manualSpin);wr(0x30320,uint32_t(state.reverseStance));
  wr(0x60028,p.surface28);wr(0x6002c,p.surface2C);wr(0x60030,p.surface30);wr(0x60034,p.surface34);
  auto curve=[&](unsigned ptr,unsigned at,auto values){wr(ptr,uint32_t(at));for(int i=0;i<4;i++){wr(at+i*8,values[i].x);wr(at+i*8+4,values[i].y);}};
  curve(0x4a30f0-0x1fc0,0x70000,p.crouchTurnCurve);curve(0x4a30f0-0x1fb8,0x70020,p.crouchSpeedCurve);curve(0x4a30f0-0x1f98,0x70040,p.directSteerCurve);
  auto c=ctx();c.pc=0x13e22c;c.f[23]=0;c.f[25]=state.dt;c.f[27]=-state.turn;SET_GPR_U32(&c,17,0x50000);SET_GPR_U32(&c,18,0x60000);
  headingAngle=0;sub_0013D818_0x13d818(ram.data(),&c,&rt);
  if(c.pc!=0x12345678){printf("bad heading continuation%x\n",c.pc);return 8;}
  float actual=ssx::originalGroundHeading(p,state);
  if(actual!=headingAngle||state.manualSpin!=rd(0x302dc)){printf("heading mismatch%u angle %.9g expected%.9g spin%.9g expected%.9g\n",k,actual,headingAngle,state.manualSpin,rd(0x302dc));return 9;}
 }
 printf("20000 original cruise heading stages match exactly\n");
 printf("20000 original sin/cos, asin, world-axis quaternion rotations and cruise alignment stages match with zero float error\n");
}
