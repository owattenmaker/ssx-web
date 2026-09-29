#include "terrain_contact_math.hpp"
#include "trick_identity.hpp"
#include <algorithm>
#include <cmath>
#include <cfenv>
#include <stdexcept>
#pragma STDC FENV_ACCESS ON
namespace ssx {
OriginalTrickIdentityResult originalTrickIdentity(OriginalTrickIdentityState& s,const OriginalTrickIdentityProfile& p,const OriginalTrickIdentityInput& in){
 OriginalRounding round;
 OriginalTrickIdentityResult r;auto& a=r.identity[0];auto& b=r.identity[1];
 auto bits=[](uint32_t& word,uint32_t mask,uint32_t value){word=(word&~mask)|(value&mask);};
 if(s.active70){a=uint32_t(s.style0C)&7;b=uint32_t(s.active70)&7;if(in.style)b|=0x03000000;r.valid=true;return r;}
 if(s.stance00)s.spin34=-s.spin34;if(s.style20==4)s.flip38=-s.flip38;
 auto quantize=[](float angle,float scale){volatile float product=terrain_original::mul(angle,scale);int degree=int(product),rest=degree%180;return degree-rest+(rest< -90?-180:rest>90?180:0);};
 const int spin=quantize(s.spin34,p.spinDegrees),flip=quantize(s.flip38,p.flipDegrees);
 const int spins=std::abs(spin/180),flips=std::abs(flip/360);
 if(s.flag28){
  int kind=0;
  if(spins>=2)kind=spins<6?13:spins<12?14:15;
  else if(spins==1&&s.time2C>=3){int base=s.flag28==1?0:1;kind=(s.time2C>=12?11:s.time2C>=9?9:s.time2C>=6?7:5)+base;}
  else {kind=s.flag28==1?(s.style20?3:1):(s.style20?4:2);bits(a,0x78,(std::min(spins,10)&15)<<3);}
  bits(b,0x00f00000,uint32_t(kind)<<20);r.valid=true;return r;
 }
 if(s.style20){
  if(s.time24<=50)a=5;
  else {a=(uint32_t(s.style20)&7)|((uint32_t(std::min(spins,10))&15)<<3);if(in.riderStance){if(s.style20==1)bits(a,7,2);else if(s.style20==2)bits(a,7,1);}}
  r.valid=true;return r;
 }
 const int corks=std::min(flips,spins/3);
 const int spinIndex=(spin>0||spins>=15)?spins+14:spins;
 const int flipIndex=(flip<0||flips>=6)?flips+5:flips;
 int grabs=0;while(grabs<3&&s.grabs60[grabs])++grabs;
 int stance=s.flag10==1?5:s.flag10==2?6:s.style0C?(s.stance00?3:2):s.field08?4:s.stance00?1:0;
 bits(a,0x380,uint32_t(stance)<<7);
 if(spinIndex>=29||flipIndex>=11){r.identity={0x00400000,0};r.valid=true;return r;}
 const auto& table=s.field04&&in.alternate?p.alternate:p.ordinary;
 bits(a,0x0fc00000,uint32_t(table[spinIndex*11+flipIndex]&63)<<22);
 if(!(a&0x0fc00000)){
  if(spins>0)bits(a,0xc00,spin>0?0x800:0x400);
  if(flips==0)bits(a,0xf000,uint32_t(spins)<<12);
  bits(a,0x70000,uint32_t(flips)<<16);
  if(flips>0){int kind=corks>0&&corks==flips?(s.flip38>0?4:3):(s.flip38>0?1:2);bits(a,0x380000,uint32_t(kind)<<19);}
 }
 if(flips>0&&spins!=flips*3&&spins<15)bits(a,0xf0000000,uint32_t(spins)<<28);
 if(in.flag==1)bits(b,0x07000000,0x04000000);
 else if(in.flag==2)bits(b,0x07000000,0x05000000);
 else if(in.style)bits(b,0x07000000,0x03000000);
 else if(!(a&0x0fc00000)&&(!corks||(a&0xf0000000))){
  if(in.stanceChanged&&!s.stance00)bits(b,0x07000000,0x02000000);
  else if(spins==0&&flips==0&&grabs>0)bits(b,0x07000000,0x01000000);
 }
 if(grabs>0){bits(b,0x3f8,uint32_t(s.grabs60[0])<<3);if(grabs>=2){b|=0x400;bits(b,0x3f800,grabs==2?uint32_t(s.grabs60[1])<<11:0x800);}}
 if(s.field7C)bits(b,0xc0000,uint32_t(s.field7C)<<18);
 r.valid=flips>0||spins>0||grabs>0||s.field7C;return r;
}
}
