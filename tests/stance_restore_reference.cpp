#include "ps2_runtime_macros.h"
#include "../engine/stance_restore.hpp"
#include "../engine/animation_motion.hpp"
#include <fstream>
#include <cfenv>
#include <cstdio>
#include <random>
#include <bit>
void sub_00115640_0x115640(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0011DFE0_0x11dfe0(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0011E098_0x11e098(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0011FE98_0x11fe98(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00311B48_0x311b48(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00311BF0_0x311bf0(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00314760_0x314760(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0031BE50_0x31be50(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00116930_0x116930(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x420000,g_ps2RecompiledFunctionTableSlotCount=0xc8000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xc8000]={};
struct Request {int semantic=-1,style=-1;float blend=0;uint32_t flags=0;ssx::AnimationTransform root;std::array<float,4> physical{};};
static std::vector<int> sourceEvents;static Request sourceRequest;
int main(int argc,char**argv){
 setbuf(stdout,nullptr);PS2Runtime rt;std::ifstream file(argv[1],std::ios::binary);std::vector<uint8_t> m((std::istreambuf_iterator<char>(file)),{});if(m.size()!=32*1024*1024)return 2;
 auto u=[&](unsigned a){uint32_t v;std::memcpy(&v,m.data()+a,4);return v;};auto f=[&](unsigned a){return std::bit_cast<float>(u(a));};auto w=[&](unsigned a,auto v){std::memcpy(m.data()+a,&v,sizeof(v));};
 rt.registerFunction(0x115640,sub_00115640_0x115640);rt.registerFunction(0x11fe98,sub_0011FE98_0x11fe98);rt.registerFunction(0x11e098,sub_0011E098_0x11e098);rt.registerFunction(0x311bf0,sub_00311BF0_0x311bf0);rt.registerFunction(0x314760,sub_00314760_0x314760);rt.registerFunction(0x31be50,sub_0031BE50_0x31be50);
 rt.registerFunction(0x11dfe0,[](uint8_t*m,R5900Context*c,PS2Runtime*r){sourceEvents.push_back(1);sub_0011DFE0_0x11dfe0(m,c,r);});
 rt.registerFunction(0x311b48,[](uint8_t*m,R5900Context*c,PS2Runtime*r){sourceEvents.push_back(2);sub_00311B48_0x311b48(m,c,r);});
 rt.registerFunction(0x116930,[](uint8_t*m,R5900Context*c,PS2Runtime*r){sourceEvents.push_back(4);sub_00116930_0x116930(m,c,r);});
 rt.registerFunction(0x3128e8,[](uint8_t*m,R5900Context*c,PS2Runtime*){sourceEvents.push_back(3);sourceRequest.semantic=GPR_S32(c,5);sourceRequest.blend=c->f[12];sourceRequest.flags=GPR_U32(c,6);std::memcpy(&sourceRequest.style,m+0x30328,4);std::memcpy(sourceRequest.root.position.data(),m+GPR_U32(c,4)+0x30,12);std::memcpy(sourceRequest.root.rotation.data(),m+GPR_U32(c,4)+0x40,16);std::memcpy(sourceRequest.physical.data(),m+0x30120,16);c->pc=GPR_U32(c,31);});
 std::mt19937 rng(0x115640);std::uniform_real_distribution<float> value(-1,1);std::fesetround(FE_TOWARDZERO);unsigned rotations=0,requests=0,roots=0;
 for(unsigned k=0;k<20000;++k){
  ssx::OriginalStanceRestoreState state;state.prewindStyle=int(k%9)-2;state.motionMode=int(k%7)-1;
  state.physical.quaternion={value(rng),value(rng),value(rng),value(rng)};
  state.physical.right={value(rng),value(rng),value(rng)};state.physical.forward={value(rng),value(rng),value(rng)};state.physical.up={value(rng),value(rng),value(rng)};
  const auto initial=state;ssx::AnimationTransform root;root.position={value(rng)*100,value(rng)*100,value(rng)*100};root.rotation={value(rng),value(rng),value(rng),value(rng)};
  w(0x30120,state.physical.quaternion);w(0x301a0,state.physical.right);w(0x301ac,0.f);w(0x301b0,state.physical.forward);w(0x301bc,0.f);w(0x301c0,state.physical.up);w(0x301cc,0.f);w(0x30328,state.prewindStyle);w(0x3077c,0x40000u);w(0x40de0,state.motionMode);w(0x30784,0x50000u);w(0x50030,root.position);w(0x5003c,1.f);w(0x50040,root.rotation);w(0x50018,0xABCDu);w(0x50050,0x60000u);
  std::vector<ssx::AnimationTransform> sequences;std::vector<unsigned> addresses;
  for(unsigned channel=0;channel<6;++channel){unsigned count=rng()%4,head=0x70000+channel*0x400;w(0x60000+channel*8,count);w(0x60004+channel*8,head);
   for(unsigned n=0;n<count;++n){unsigned at=head+n*0x100;ssx::AnimationTransform sequence;sequence.position={value(rng)*100,value(rng)*100,value(rng)*100};sequence.rotation={value(rng),value(rng),value(rng),value(rng)};sequences.push_back(sequence);addresses.push_back(at);w(at+0x60,sequence.position);w(at+0x6c,1.f);w(at+0x70,sequence.rotation);w(at+0x80,0x12340000u+n);w(at+0xc8,n+1<count?at+0x100:0u);}
  }
  sourceEvents.clear();sourceRequest={};R5900Context c{};c.pc=0x115640;c.vu0_vf[0]=_mm_set_ps(1,0,0,0);SET_GPR_U32(&c,4,0x30000);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,29,0x10000);SET_GPR_U32(&c,31,0x12345678);sub_00115640_0x115640(m.data(),&c,&rt);if(c.pc!=0x12345678){printf("Stance original continuation%x\n",c.pc);return 3;}
  std::vector<int> nativeEvents;Request nativeRequest;ssx::OriginalStanceRestoreCallbacks callbacks;
  callbacks.physicalChanged=[&](const auto&){nativeEvents.push_back(1);};
  callbacks.rotateSequenceRoots=[&](auto q){nativeEvents.push_back(2);ssx::AnimationTransform delta;delta.rotation=q;for(auto&s:sequences)s=ssx::originalAnimationCompose(delta,s);};
  callbacks.resetDefaultRoot=[&](auto position,auto q){root.position=position;root.rotation=q;};
  callbacks.requestAnimation=[&](int semantic,float blend,uint32_t flags){nativeEvents.push_back(3);nativeRequest={semantic,state.prewindStyle,blend,flags,root,state.physical.quaternion};};
  auto result=ssx::originalRestoreStance(state,callbacks);if(result.restored)nativeEvents.push_back(4);
  auto equal=[&](unsigned at,const auto& values){for(unsigned i=0;i<values.size();++i)if(u(at+i*4)!=std::bit_cast<uint32_t>(values[i])){printf("Stance field mismatchcase%u style%d mode%d offset%x i%u orig%08x native%08x\n",k,initial.prewindStyle,initial.motionMode,at,i,u(at+i*4),std::bit_cast<uint32_t>(values[i]));return false;}return true;};
  if(u(0x30328)!=uint32_t(state.prewindStyle)||!equal(0x30120,state.physical.quaternion)||!equal(0x301a0,state.physical.right)||!equal(0x301b0,state.physical.forward)||!equal(0x301c0,state.physical.up)||!equal(0x50030,root.position)||!equal(0x50040,root.rotation)||u(0x50018)!=0xABCD){return 4;}
  for(unsigned n=0;n<sequences.size();++n)if(!equal(addresses[n]+0x60,sequences[n].position)||!equal(addresses[n]+0x70,sequences[n].rotation)||u(addresses[n]+0x80)!=0x12340000u+((addresses[n]&0x3ff)/0x100))return 5;
  if(sourceEvents!=nativeEvents||sourceRequest.semantic!=nativeRequest.semantic||sourceRequest.style!=nativeRequest.style||std::bit_cast<uint32_t>(sourceRequest.blend)!=std::bit_cast<uint32_t>(nativeRequest.blend)||sourceRequest.flags!=nativeRequest.flags||std::memcmp(&sourceRequest.root,&nativeRequest.root,sizeof(sourceRequest.root))||std::memcmp(sourceRequest.physical.data(),nativeRequest.physical.data(),16)){printf("Stance callback mismatch%u style%d mode%d request%d/%d events%zu/%zu\n",k,initial.prewindStyle,initial.motionMode,sourceRequest.semantic,nativeRequest.semantic,sourceEvents.size(),nativeEvents.size());return 6;}
  rotations+=result.physicalRotated;requests+=result.animationSemantic>=0;roots+=result.defaultRootReset;
 }
 printf("20000 full original115640 restorations match physical quaternion/bases,all six-channel sequence roots,default root,style,mirror preservation and callback order (%u rotations,%u root resets,%u clip requests)\n",rotations,roots,requests);
}
