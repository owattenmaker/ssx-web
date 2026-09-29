#include "original_command.hpp"
#include "original_input_provider.hpp"
#include <bit>
#include <cfenv>
#include <stdexcept>
#include <string>
#pragma STDC FENV_ACCESS ON
namespace ssx {namespace {
struct Round {int old=std::fegetround();Round(){if(std::fesetround(FE_TOWARDZERO))throw std::runtime_error("Command rounding");}~Round(){std::fesetround(old);}};
float axis(uint32_t word,unsigned shift){int value=(word>>shift)&63;if(value>=32)value-=64;return float(value)*std::bit_cast<float>(0x3d042108u);}
void require(bool valid,const char* what){if(!valid)throw std::runtime_error(what);}
}
RiderInput originalDecodeCommand(int state,uint32_t a,uint32_t b){
 // Controllers without a pre-existing native decoder use the complete 127998
 // layout (original_input_provider.cpp); 0/2/3/4/5 keep their strict checks.
 if(state==1||(state>=6&&state<=13))return originalCommandRiderInput(state,a,b);
 Round round;RiderInput out;
 require(!(a&0xfff),"Original command still contains run-length bits");
 require(!(a&0x1000),"Original reset/recovery command is not implemented");
 switch(state){
 case 0: //131620/131850, provider127998 cruise packing.
  require(!(a&0xc2000)&&!(b&~0xfffu),"Original cruise attack/handplant command is not implemented");
  out.turn=axis(a,20);out.crouch=axis(a,26);out.brake=axis(b,0);out.boardPress=axis(b,6);
  out.jumpPressed=a&0x4000;out.jumpHeld=a&0x8000;out.boostPressed=a&0x10000;out.boostHeld=a&0x20000;break;
 case 2: //12E9B8 control2 prewind and release.
  require(!(a&0xf8000000u)&&!(b&~0xfffu),"Unknown original prewind command bits");
  require(!(b&0xfc0),"Original prewind secondary axis is not implemented");
  out.jumpHeld=a&0x2000;out.boostHeld=a&0x4000;
  out.prewindTurn=axis(b,0);out.turn=std::clamp(out.prewindTurn,-.5f,.5f);
  out.spin=axis(a,15);out.flip=axis(a,21);out.crouch=out.jumpHeld?1.f:0.f;break;
 case 5: { //1333E0/13366C; FF is no grab, zero selects the first grab.
  require(!(a&0xc000a000u)&&!(b&~0xfffffu),"Original airborne handplant/late-spin command is not implemented");
  constexpr std::array<uint8_t,15> masks={1,2,4,8,3,5,9,6,10,12,7,11,13,14,15};unsigned grab=(a>>16)&255;
  require(grab==255||grab<masks.size(),"Invalid original airborne grab index");
  int press=(b>>18)&3;require(press!=2,"Invalid original airborne board press");if(press==3)press=-1;
  out.boostHeld=a&0x4000;out.spin=axis(a,24);out.flip=axis(b,0);out.airAdjustFB=axis(b,6);out.airAdjustLR=axis(b,12);
  out.boardPress=float(press);out.grabMask=grab==255?0:masks[grab];out.tweak=out.boostHeld;break; //bit14 is action21 Tweak
 }
 case 3: //12E778 soft collision, distinct from the rail controller7.
  require(!(a&0xf8000000u)&&!b,"Unknown original soft collision command bits");
  out.boostPressed=a&(1u<<13);out.boostHeld=a&(1u<<14);out.turn=axis(a,15);out.boardPress=axis(a,21);
  out.railBalance=out.boardPress;break; //127998 writes action23 RailBalance at bits21..26.
 case 4: //12F730 passive air. Preserve its signed identity byte separately.
  require(!(a&0xc0000000u)&&!(b&~63u),"Unknown original passive-air command bits");
  out.turn=axis(a,24);out.crouch=axis(b,0);out.passiveInputCode=int8_t((a>>16)&255);
  out.handplant=a&0x2000;out.passiveUpper14=a&0x4000;out.passiveUpper15=a&0x8000;
  out.attackLeft=out.passiveUpper14;out.attackRight=out.passiveUpper15;break; //actions5/6
 default:throw std::runtime_error("Original command controller not implemented: "+std::to_string(state));
 }
 return out;
}
}
