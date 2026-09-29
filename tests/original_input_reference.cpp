// Development-only conformance against original PS2 input instructions.
#include "ps2_runtime_macros.h"
#include "../engine/original_input.hpp"
#include "../engine/original_command.hpp"
#include "../engine/rail_uber_entry.hpp"
#include <fstream>
#include <cstdio>
#include <cfenv>
#include <random>
void sub_00326DF0_0x326df0(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00321298_0x321298(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00127848_0x127848(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00321108_0x321108(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00325250_0x325250(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00325450_0x325450(uint8_t*,R5900Context*,PS2Runtime*);
void sub_003252E8_0x3252e8(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00325260_0x325260(uint8_t*,R5900Context*,PS2Runtime*);
void sub_003252F8_0x3252f8(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00325430_0x325430(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000;
extern const uint32_t g_ps2RecompiledFunctionTableEnd=0x410000;
extern const uint32_t g_ps2RecompiledFunctionTableSlotCount=0xc4000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xc4000]={nullptr};
static std::array<float,48> actions;
static int selectedControl=0,selectedGrab=-1;
int main(int argc,char**argv){
 PS2Runtime rt;std::ifstream input(argv[1],std::ios::binary);std::vector<uint8_t> ram((std::istreambuf_iterator<char>(input)),{});if(ram.size()!=32*1024*1024)return 2;
 auto wr=[&](unsigned at,const auto&v){std::memcpy(ram.data()+at,&v,sizeof(v));};
 auto rf=[&](unsigned at){float x;std::memcpy(&x,ram.data()+at,4);return x;};
 auto ru=[&](unsigned at){unsigned x;std::memcpy(&x,ram.data()+at,4);return x;};
 auto context=[](){R5900Context c{};c.vu0_vf[0]=_mm_set_ps(1,0,0,0);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,29,0x10000);SET_GPR_U32(&c,31,0x12345678);return c;};
 rt.registerFunction(0x3E6448,[](uint8_t*m,R5900Context*c,PS2Runtime*){std::memset(m+GPR_U32(c,4),GPR_U32(c,5)&255,GPR_U32(c,6));c->pc=GPR_U32(c,31);});
 rt.registerFunction(0x11FEE8,[](uint8_t*,R5900Context*c,PS2Runtime*){SET_GPR_U32(c,2,selectedControl);c->pc=GPR_U32(c,31);});
 rt.registerFunction(0x320BF0,[](uint8_t*,R5900Context*c,PS2Runtime*){c->f[0]=actions.at(GPR_U32(c,5));c->pc=GPR_U32(c,31);});
 rt.registerFunction(0x320C48,[](uint8_t*,R5900Context*c,PS2Runtime*){SET_GPR_U32(c,2,actions.at(GPR_U32(c,5))!=0);c->pc=GPR_U32(c,31);});
 rt.registerFunction(0x1276F0,[](uint8_t*,R5900Context*c,PS2Runtime*){SET_GPR_S32(c,2,selectedGrab);c->pc=GPR_U32(c,31);});
 std::fesetround(FE_TOWARDZERO);std::mt19937 rng(0x325250);
 ssx::OriginalPadState nativeState{};static_assert(sizeof(ssx::OriginalButtonState)==28);wr(0x60000,uint32_t(24));
 std::memcpy(ram.data()+0x60004,nativeState.data(),sizeof(nativeState));
 for(unsigned test=0;test<8192;test++){
  ssx::OriginalPadPacket packet{};for(auto& x:packet)x=uint8_t(rng());
  // First256 cover every stick byte in every channel; later samples add arbitrarychords.
  if(test<256)for(unsigned i=4;i<8;i++)packet[i]=uint8_t(test);
  bool pressure=test&1,analog=test<256||test%7!=0;
  std::memcpy(ram.data()+0x10000,packet.data(),packet.size());wr(0x50054,uint32_t(pressure));wr(0x50058,uint32_t(analog));
  auto c=context();c.pc=0x327210;SET_GPR_U32(&c,2,packet[2]);SET_GPR_U32(&c,16,0x50000);SET_GPR_U32(&c,17,0x40000);
  sub_00326DF0_0x326df0(ram.data(),&c,&rt);if(c.pc!=0x12345678){printf("bad driver continuation%x\n",c.pc);return 3;}
  auto values=ssx::originalDecodePad(packet,pressure,analog);
  for(unsigned i=0;i<24;i++)if(values[i]!=rf(0x40000+i*4)){printf("driver mismatch%u button%u expected%.9g native%.9g\n",test,i,rf(0x40000+i*4),values[i]);return 4;}
  c=context();SET_GPR_U32(&c,4,0x60000);SET_GPR_U32(&c,5,24);SET_GPR_U32(&c,6,0x40000);c.pc=0x321298;
  sub_00321298_0x321298(ram.data(),&c,&rt);if(c.pc!=0x12345678)return 5;
  ssx::originalUpdatePad(nativeState,values);
  if(std::memcmp(nativeState.data(),ram.data()+0x60004,sizeof(nativeState))){printf("button history mismatch%u\n",test);return 6;}
 }
 for(unsigned i=0;i<20000;i++){
  float value=float(int(rng()%200001)-100000)/100000.f;actions={};actions[0]=value;
  wr(0x70018,uint32_t(0x30000));wr(0x70df0,uint32_t(0));wr(0x70df8,uint32_t(0));
  auto c=context();SET_GPR_U32(&c,4,0x70000);SET_GPR_U32(&c,5,0x40000);c.pc=0x127998;
  sub_00127848_0x127848(ram.data(),&c,&rt);if(c.pc!=0x12345678){printf("bad providercontinuation%x\n",c.pc);return 7;}
  int encoded=(ru(0x40000)>>20)&63;if(encoded>=32)encoded-=64;
  float expected=float(encoded)*rf(0x4a30f0-0x75f4);
  if(ssx::originalQuantizeAxis(value)!=expected){printf("quant mismatch%u\n",i);return 8;}
 }
 // Round-trip the original provider's independent per-state packing through
 // the new native runtime decoder. Mapping functions are semantic boundaries.
 constexpr std::array<unsigned char,15> grabs={1,2,4,8,3,5,9,6,10,12,7,11,13,14,15};
 for(int state:{0,2,5})for(unsigned n=0;n<10000;n++){
  selectedControl=state;selectedGrab=state==5?int(n%16)-1:-1;actions={};
  auto sample=[&](){return float(int(rng()%200001)-100000)/100000.f;};
  auto quant=[&](unsigned action){return ssx::originalQuantizeAxis(actions[action]);};
  ssx::RiderInput expected;
  if(state==0){
   for(unsigned k:{0,1,2,3})actions[k]=sample();for(unsigned k:{17,18,19,20})actions[k]=rng()%2;
   expected.turn=quant(0);expected.crouch=quant(1);expected.brake=quant(2);expected.boardPress=quant(3);
   expected.jumpPressed=actions[17];expected.jumpHeld=actions[18];expected.boostPressed=actions[19];expected.boostHeld=actions[20];
  }else if(state==2){
   for(unsigned k:{7,8,9})actions[k]=sample();for(unsigned k:{18,20})actions[k]=rng()%2;
   expected.prewindTurn=quant(7);expected.turn=std::clamp(expected.prewindTurn,-.5f,.5f);expected.spin=quant(8);expected.flip=quant(9);
   expected.jumpHeld=actions[18];expected.boostHeld=actions[20];expected.crouch=expected.jumpHeld?1:0;
  }else{
   for(unsigned k:{10,11,14,15})actions[k]=sample();actions[3]=int(n%3)-1;actions[21]=rng()%2;
   expected.spin=quant(14);expected.flip=quant(15);expected.airAdjustLR=quant(11);expected.airAdjustFB=quant(10);
   expected.boardPress=actions[3];expected.boostHeld=actions[21];expected.grabMask=selectedGrab<0?0:grabs[selectedGrab];
  }
  wr(0x70018,uint32_t(0x30000));wr(0x70df0,uint32_t(0));wr(0x70df8,uint32_t(0));
  auto c=context();SET_GPR_U32(&c,4,0x70000);SET_GPR_U32(&c,5,0x40000);c.pc=0x127998;
  sub_00127848_0x127848(ram.data(),&c,&rt);if(c.pc!=0x12345678){printf("command writer pc %x state %d\n",c.pc,state);return 11;}
  auto got=ssx::originalDecodeCommand(state,ru(0x40000),ru(0x40004));
  for(auto member:{&ssx::RiderInput::turn,&ssx::RiderInput::crouch,&ssx::RiderInput::brake,&ssx::RiderInput::boardPress,&ssx::RiderInput::prewindTurn,&ssx::RiderInput::spin,&ssx::RiderInput::flip,&ssx::RiderInput::airAdjustLR,&ssx::RiderInput::airAdjustFB})
   if(got.*member!=expected.*member){printf("command axis state %d case %u expected %.9g got %.9g words %08x %08x\n",state,n,expected.*member,got.*member,ru(0x40000),ru(0x40004));return 12;}
  if(got.jumpPressed!=expected.jumpPressed||got.jumpHeld!=expected.jumpHeld||got.boostPressed!=expected.boostPressed||got.boostHeld!=expected.boostHeld||got.grabMask!=expected.grabMask){printf("command flags state %d case %u\n",state,n);return 13;}
 }
 selectedControl=0;selectedGrab=-1;
 printf("30,000 original command-provider roundtrips match native control0/2/5 decoding\n");
 // Validate the offline snapshot reader's main action expressions against
 // the original mapping VM, including property lookups from frozen pad data.
 rt.registerFunction(0x321108,sub_00321108_0x321108);
 rt.registerFunction(0x325250,sub_00325250_0x325250);
 rt.registerFunction(0x325450,sub_00325450_0x325450);
 rt.registerFunction(0x3252e8,sub_003252E8_0x3252e8);
 rt.registerFunction(0x325260,sub_00325260_0x325260);
 rt.registerFunction(0x3252f8,sub_003252F8_0x3252f8);
 rt.registerFunction(0x325430,sub_00325430_0x325430);
 std::ifstream expectedFile(argv[2],std::ios::binary);std::array<float,27> expectedActions;
 if(!expectedFile.read(reinterpret_cast<char*>(expectedActions.data()),sizeof(expectedActions)))return 9;
 unsigned owner=ru(0x14701a0+0x77c),padContext=ru(owner+0xdf0);
 for(unsigned action=0;action<27;action++){
  auto c=context();SET_GPR_U32(&c,4,ru(padContext+4));SET_GPR_U32(&c,5,ru(padContext));SET_GPR_U32(&c,6,action);c.pc=0x321108;
  //321108 is a boolean getter, so execute the scalar sibling320BF0 path's
  // underlying321098 entry instead through direct mapping-VM invocation.
  unsigned mapping=ru(padContext+4),entrypoints=ru(mapping+4),program=ru(mapping+20);
  SET_GPR_U32(&c,4,0x80000);SET_GPR_U32(&c,5,ru(padContext));SET_GPR_U32(&c,6,program+ru(entrypoints+action*4)*4);wr(0x80000,uint32_t(0));
  sub_00325450_0x325450(ram.data(),&c,&rt);
  if(c.pc!=0x12345678||c.f[0]!=expectedActions[action]){printf("snapshot mapping action%u expected%.9g original%.9g pc%x\n",action,expectedActions[action],c.f[0],c.pc);return 10;}
 }
 for(unsigned mask=0;mask<16;mask++){
  unsigned pad=ru(padContext);for(unsigned i=0;i<24;i++){wr(pad+4+i*28,0.f);wr(pad+4+i*28+12,uint32_t(0));}
  constexpr unsigned shoulders[]={12,14,13,15};
  for(unsigned i=0;i<4;i++)if(mask&(1u<<i)){wr(pad+4+shoulders[i]*28,1.f);wr(pad+4+shoulders[i]*28+12,uint32_t(1));}
  int selected=-1;
  for(unsigned action=31;action<=45;action++){
   auto c=context();unsigned mapping=ru(padContext+4),program=ru(mapping+20),entries=ru(mapping+4);
   SET_GPR_U32(&c,4,0x80000);SET_GPR_U32(&c,5,pad);SET_GPR_U32(&c,6,program+ru(entries+action*4)*4);wr(0x80000,uint32_t(0));
   sub_00325450_0x325450(ram.data(),&c,&rt);if(c.pc!=0x12345678)return 11;
   if(c.f[0]!=0){selected=int(action)-31;break;}
  }
  actions.fill(0);
  for(unsigned action=27;action<=30;action++){
   auto c=context();unsigned mapping=ru(padContext+4),program=ru(mapping+20),entries=ru(mapping+4);
   SET_GPR_U32(&c,4,0x80000);SET_GPR_U32(&c,5,pad);SET_GPR_U32(&c,6,program+ru(entries+action*4)*4);wr(0x80000,uint32_t(0));
   sub_00325450_0x325450(ram.data(),&c,&rt);if(c.pc!=0x12345678)return 13;actions[action]=c.f[0];
  }
  rt.registerFunction(0x127848,sub_00127848_0x127848);
  auto rail=context();rail.pc=0x127848;SET_GPR_U32(&rail,4,owner);sub_00127848_0x127848(ram.data(),&rail,&rt);
  int railIdentity=ssx::originalRailUberIdentity(uint8_t(mask));
  if(rail.pc!=0x12345678||int32_t(GPR_U32((&rail),2))!=railIdentity){printf("Rail shoulder mapping mismatch %u\n",mask);return 14;}
  //This probe checks input mapping/packing, not optional movie recording.
  wr(owner+0xdf8,uint32_t(0));
  for(int control:{7,12}){
   selectedControl=control;auto encoded=context();encoded.pc=0x127998;SET_GPR_U32(&encoded,4,owner);SET_GPR_U32(&encoded,5,0x90000);sub_00127848_0x127848(ram.data(),&encoded,&rt);
   //The generated same-function JAL returns at its guest continuation; resume it.
   if(encoded.pc==0x127ee8||encoded.pc==0x127fe0)sub_00127848_0x127848(ram.data(),&encoded,&rt);
   int decoded=control==12?ssx::originalRailUberCommandIdentity(ru(0x90000)):int8_t((ru(0x90000)>>17)&255);
   if(encoded.pc!=0x12345678||decoded!=railIdentity){printf("Rail provider identity mismatch mask%u control%d expected%d decoded%d pc%x word%x\n",mask,control,railIdentity,decoded,encoded.pc,ru(0x90000));return 15;}
  }
  constexpr int expected[]={-1,0,1,4,2,5,7,10,3,6,8,11,9,12,13,14};
  if(selected!=expected[mask]){printf("original grab chord mismatch%u\n",mask);return 12;}
 }
 printf("All16original shoulder masks match grab and Uber-grind mappings, including control7/12 packing\n");
 printf("27 snapshot main-action expressions match original input VM exactly\n");
 printf("8192 original pad decodes/all24channels and sample histories match bit-for-bit;20000 original provider axis quantizations exact\n");
}
