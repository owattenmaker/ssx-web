// Literal ports of the original trick-score object routines (docs/tricks-scoring.md).
// EE scalar arithmetic: ADD/SUB keep PCSX2's guard bit (originalScalarAdd/Subtract), MUL rounds
// toward zero (terrain_original::mul under OriginalRounding), DIV is nearest, CVT.W.S truncates.
#include "score_object.hpp"
#include "ground_pose_motion.hpp"
#include "original_float.hpp"
#include "terrain_contact_math.hpp"
#include <algorithm>
#include <cfenv>
#include <cmath>
#include <cstdio>
#pragma STDC FENV_ACCESS ON
namespace ssx {
float originalAtan(float); //0x31C228
namespace {
constexpr float F(uint32_t bits){return std::bit_cast<float>(bits);}
float A(float a,float b){return originalScalarAdd(a,b);}
float S(float a,float b){return originalScalarSubtract(a,b);}
float M(float a,float b){return terrain_original::mul(a,b);}
float D(float a,float b){return originalScalarDivide(a,b);}
int32_t W(float x){ // cvt.w.s (chop, saturating like PCSX2)
 if(!(x==x))return 0;if(x>=2147483648.f)return INT32_MAX;if(x<=-2147483648.f)return INT32_MIN;return int32_t(std::trunc(x));}
float I2F(int32_t v){ // cvt.s.w in the chop rounding mode
#if defined(__EMSCRIPTEN__)
 const int64_t a=v<0?-int64_t(v):int64_t(v);if(a<=(1<<24))return float(v);
 float r=float(a);if(int64_t(r)>a)r=std::nextafter(r,0.f);return v<0?-r:r;
#else
 volatile int32_t x=v;return float(x);
#endif
}
int32_t addw(int32_t a,int32_t b){return std::bit_cast<int32_t>(uint32_t(a)+uint32_t(b));}
int32_t subw(int32_t a,int32_t b){return std::bit_cast<int32_t>(uint32_t(a)-uint32_t(b));}
// Signed division/remainder as the EE DIV (truncating); callers never divide by zero.
int32_t quo(int32_t a,int32_t b){if(b==0)throw std::runtime_error("score division by zero");if(a==INT32_MIN&&b==-1)return a;return a/b;}
int32_t rem(int32_t a,int32_t b){if(b==0)throw std::runtime_error("score division by zero");if(b==-1)return 0;return a%b;}
OriginalHudSlot& slot(OriginalScoreObject& o,int index){if(index<0||index>=kOriginalHudSlotCount)throw std::runtime_error("HUD slot outside bank");return o.hud[index];}
void writeText(OriginalHudSlot& s,const char* text){ //117008: copies until NUL
 size_t n=std::strlen(text);if(n>=0x80)throw std::runtime_error("HUD text exceeds slot");std::memcpy(s.text.data(),text,n);s.text[n]=0;}
void printPoints(OriginalHudSlot& s,int32_t points){char buffer[16];std::snprintf(buffer,sizeof buffer,"%d",points);writeText(s,buffer);}
// Shared "approach displayed fraction" (clamp target to previous +-step).
float approach(float target,float previous,float step){
 if(A(target,step)<previous)return S(previous,step);
 if(previous<S(target,step))return A(previous,step);
 return target;
}
// 117FE0's slot ratio: value/maximum with the EE DIV.
float ratio(const OriginalHudSlot& s){return D(s.value,s.maximum);}
// direction(y,x) as 12F118/135F70: atan with quadrant fix-up, x==0 -> +-pi/2.
float direction(float y,float x){
 if(x==0){if(y==0)return y;return 0<=y?F(0x3fc90fdb):F(0xbfc90fdb);}
 float a=originalAtan(D(y,x));
 if(x<0)a=0<y?A(a,F(0x40490fdb)):S(a,F(0x40490fdb));
 return a;
}
float signum(float v){if(0<=v)return 0<v?1.f:0.f;return -1.f;}
}

OriginalScoreTables originalScoreTables(){
 OriginalScoreTables t;
 t.air1C={{6,5000},{8,5000},{10,10000},{-1,0}};
 t.hold20={{3,4000},{6,2000},{9,1000},{-1,0}};
 t.style1E={{5,1000},{10,2000}};for(int k=15;k<=120;k+=5)t.style1E.push_back({float(k),4000});t.style1E.push_back({-1,0});
 t.timer1F={};for(int k=2;k<=12;k++)t.timer1F.push_back({float(k),2000});t.timer1F.push_back({-1,0});
 t.distance1D={{10000,1000},{12000,3000},{14000,5000},{16000,7000},{18000,9000},{20000,12000},{22000,16000},{24000,20000},{26000,30000},{28000,40000},{30000,50000},{-1,0}};
 return t;
}

// ---- HUD slots ----
void originalHudFree(OriginalScoreObject& o,int type){ //1179E0
 if(!o.hudPresent)return;
 if(type<0x22){slot(o,type).type=kOriginalHudFree;return;}
 for(int k=0x23;k<kOriginalHudSlotCount;k++)if(o.hud[k].type==type)o.hud[k].type=kOriginalHudFree;
}
int originalHudAllocate(OriginalScoreObject& o,int type){ //117A58
 if(type<0x22)return type;
 OriginalRounding round;float best=1.f;int found=-1;
 for(int k=0x23;k<kOriginalHudSlotCount;k++){
  if(o.hud[k].type==kOriginalHudFree)return k;
  const float r=ratio(o.hud[k]);if(r<best){best=r;found=k;}
 }
 return found;
}
void originalHudSetValue(OriginalHudSlot& s,int type,int32_t points,int32_t arg,float value){ //1171A8
 printPoints(s,points);s.points=points;s.type=type;s.value=-value;s.arg=arg;s.maximum=-1;
}
float originalScoreMultiplierPickup(OriginalScoreObject& o,float multiplier){ //119448
 o.i(0x130)=int32_t(uint32_t(o.i(0x130))+1u);
 if(o.hudPresent&&o.f(0x18)<multiplier){ // c.lt.s +0x18, f12
  const float v=multiplier;const int32_t n=!(v==v)||v>=2147483648.f?0x7fffffff:v<=-2147483648.f?int32_t(0x80000000u):int32_t(v); // cvt.w.s
  originalHudSetValue(slot(o,4),4,n,0,0.f);o.f(0x18)=multiplier;}
 return 0.f;
}
static void setPoints(OriginalHudSlot& s,int type,int32_t points,int32_t arg,float duration){ //1170A8
 printPoints(s,points);s.points=points;s.type=type;s.maximum=duration;s.arg=arg;s.value=0;
}
static void setText(OriginalHudSlot& s,int type,const std::string& text,int32_t arg,float duration){ //117048
 writeText(s,text.c_str());s.type=type;s.maximum=duration;s.arg=arg;s.points=0;s.value=0;
}
int originalHudPostPoints(OriginalScoreObject& o,int type,int32_t points,int32_t arg,float duration){ //117B88
 if(!o.hudPresent)return -1;
 const int k=originalHudAllocate(o,type);if(k<0)throw std::runtime_error("HUD bank full (117B88 would write before the bank)");
 setPoints(slot(o,k),type,points,arg,duration);return k;
}
int originalHudPostText(OriginalScoreObject& o,int type,const std::string& text,int32_t arg,float duration){ //117AE8
 if(!o.hudPresent)return -1;
 const int k=originalHudAllocate(o,type);if(k<0)throw std::runtime_error("HUD bank full (117AE8 would write before the bank)");
 setText(slot(o,k),type,text,arg,duration);return k;
}
void originalHudTick(OriginalHudSlot& s){ //116FB8
 if(s.type==kOriginalHudFree||s.maximum==-1.f)return;
 OriginalRounding round;s.value=A(s.value,F(0x3c888889));
 if(s.maximum<=s.value)s.type=kOriginalHudFree;
}

// ---- scoring helpers ----
void originalScoreReset(OriginalScoreObject& o){ //117838
 o.f(0x78)=-1;o.f(0x18)=1;o.u(0x14)=0;o.u(0x1C)=0;
 for(unsigned k:{0x00u,0x04u,0x08u,0x0Cu,0x10u,0x20u,0x28u,0x34u,0x38u,0x3Cu,0x44u,0x48u,0x4Cu,0x50u,0x54u,0x58u,0x5Cu,0x70u,0x74u,0x7Cu,0x80u,0x84u,0x88u,0x8Cu,0x90u,0x94u,0x98u})o.u(k)=0;
 o.f(0x24)=-1;o.f(0x2C)=-1;o.f(0x30)=-1;o.f(0x40)=-1;o.f(0x6C)=-1;
 o.u(0x60)=o.u(0x64)=o.u(0x68)=0;
}
void originalScoreComboReset(OriginalScoreObject& o){ //1175F8
 originalHudFree(o,3);o.u(0x9C)=0;o.f(0xA4)=-1;o.u(0xA0)=0;
}
void originalScoreRunReset(OriginalScoreObject& o){ //117540
 for(unsigned k=0xFC;k<0x1A8;k+=4)o.u(k)=0; //1175B8 memset FC..1A7
 o.i(0x18C)=-1;o.u(0x198)=o.u(0x1C8);
 if(o.hudPresent)for(auto& s:o.hud)s.type=kOriginalHudFree; //1173B8
 for(unsigned k=0xA8;k<0xF8;k+=4)o.u(k)=0;o.u(0xF8)=0;
 originalScoreReset(o);originalScoreComboReset(o);
 o.u(0x1A8)=0;o.f(0xA4)=-1;o.u(0x1B8)=o.u(0x1BC)=0;o.u(0x1C0)=1;
}
void originalScoreComboAdd(OriginalScoreObject& o,int32_t points){ //117638
 if(points<=0)return;
 OriginalRounding round;
 const int32_t count=addw(o.i(0x9C),1);o.i(0x9C)=count;
 float m=M(A(I2F(count),10.f),F(0x3d4ccccd));float scale=.5f;if(.5f<=m)scale=std::min(m,2.f);
 const float added=M(I2F(points),scale);o.f(0xA4)=-1;
 o.i(0xA0)=addw(o.i(0xA0),W(added));
 if(!o.hudPresent)return;
 auto& s=o.hud[3];const int32_t shown=s.type==kOriginalHudFree?0:s.points;
 originalHudSetValue(s,3,shown,count,0.f);
}
int32_t originalScoreGrade(int32_t p){ //119310
 if(p<1000)return 0;if(p<2500)return 1;if(p<4000)return 2;if(p<7500)return 3;return p<11500?4:5;
}
int32_t originalScoreInvertedPoints(const OriginalScoreObject& o){ //117908
 OriginalRounding round;const int32_t v=W(A(M(o.f(0x1C),F(0x461c4000)),5.f));return v-rem(v,10);
}
int32_t originalScorePendingPoints(const OriginalScoreObject& o){ //117948
 OriginalRounding round;float p=M(o.f(0x1C4),o.f(0x14));p=M(p,10000.f);const int32_t v=W(A(p,5.f));return subw(v,rem(v,10));
}
int32_t originalScoreMultipliedPoints(const OriginalScoreObject& o){ //117990
 OriginalRounding round;float p=M(o.f(0x1C4),o.f(0x18));p=M(p,o.f(0x14));p=M(p,F(0x461c4000));const int32_t v=W(A(p,5.f));return v-rem(v,10);
}
float originalScoreCareerAward(OriginalScoreObject& o,int kind,int32_t amount,const OriginalScoreEnvironment& env,const OriginalScoreHooks& hooks){ //119EF8
 if(amount==0||!env.careerAwards)return 0;
 if(hooks.career)hooks.career(kind,amount);
 originalHudPostPoints(o,0x18,amount,0,F(0x3f333333));
 switch(kind){ // jump table 0x457870
  case 0:originalHudPostPoints(o,0x2F,amount,0,2.5f);break;
  case 1:originalHudPostPoints(o,0x2E,amount,0,2.5f);break;
  case 2:originalHudPostPoints(o,0x30,amount,o.i(0x9C),2.5f);break;
  case 3:originalHudPostPoints(o,0x31,amount,amount,1.5f);break;
  default:break;
 }
 return 0;
}
int32_t originalScoreThreshold(OriginalScoreObject& o,unsigned indexOffset,const std::vector<OriginalScoreThreshold>& table,int type,float value){ //119210
 OriginalRounding round;const int32_t index=o.i(indexOffset);
 if(index<0||size_t(index)>=table.size())throw std::runtime_error("score threshold index outside authored table");
 const auto t=table[size_t(index)];if(t.at<0||!(t.at<=value))return 0;
 const int32_t points=W(A(M(t.points,o.f(0x1C4)),.5f));o.i(indexOffset)=index+1;
 for(int k:{0x1C,0x1D,0x1E,0x1F,0x20})originalHudFree(o,k);
 originalHudPostPoints(o,type,points,W(t.at),1.5f);
 return points;
}
void originalScoreComboExpire(OriginalScoreObject& o,const OriginalScoreEnvironment& env,const OriginalScoreHooks& hooks){ //117718
 const int32_t count=o.i(0x9C);
 if(count>=2&&o.i(0xA0)>0){
  const int32_t points=o.i(0xA0);
  o.i(0x198)=addw(o.i(0x198),points);o.i(0x134)=addw(o.i(0x134),count);
  if(o.i(0x188)<points)o.i(0x188)=points;
  if(o.i(0x184)<o.i(0x9C))o.i(0x184)=o.i(0x9C);
  if(!env.pointsToCareer){originalHudPostPoints(o,0x26,o.i(0xA0),o.i(0x9C),1.5f);originalHudPostPoints(o,0x18,o.i(0xA0),0,F(0x3f333333));}
  else {const int32_t amount=quo(o.i(0xA0),500);originalScoreCareerAward(o,2,amount<21?amount:20,env,hooks);}
 }
 originalScoreComboReset(o);
}

// ---- 11A228 ----
OriginalScoreCommitResult originalScoreCommit(OriginalScoreObject& o,const OriginalTrickCommitProfile& p,
 int32_t stance,int32_t alternate,int32_t style,int32_t flag,int32_t takeoff,bool riderStance,
 const OriginalScoreEnvironment& env,const OriginalScoreNameBuilder& name,const OriginalScoreHooks& hooks){
 OriginalRounding round;OriginalScoreCommitResult r;
 for(int k:{1,2,4,0x1C,0x1D,0x1E,0x1F,0x20})originalHudFree(o,k);
 if(env.commitBlocked)return r;
 // 11A8C8 identity (may negate +34/+38 in place).
 auto view=originalScoreView(o);
 OriginalTrickIdentityInput input{stance!=0,alternate!=0,style,flag,riderStance};
 auto id=originalTrickIdentity(view.identity,p.identity,input);
 o.f(0x34)=view.identity.spin34;o.f(0x38)=view.identity.flip38;
 r.identity=id.identity;r.valid=id.valid;
 float meter=0;
 if(originalScorePendingPoints(o)>0){
  int32_t repeats=0;
  if(id.valid){
   const int32_t named=originalNamedTrickBonus(r.identity,p.named); //11B1A8
   // 11A334: only a monster trick (11B1A8 > 0) posts popup 0x32 and calls 29B7E0 (Arcade_Uber variant 8).
   if(named>0){o.f(0x14)=A(o.f(0x14),M(I2F(named),p.namedPointScale));originalHudPostPoints(o,0x32,named,0,1.5f);if(hooks.named)hooks.named(r.identity);}
   auto history=originalScoreHistory(o);
   repeats=originalTrickRepeatCount(history,r.identity,{o.i(0x08),o.i(0x20),o.i(0x70),o.i(0x7C),o.i(0x28)}); //1190F0
   originalStoreScoreHistory(o,history);
   if(o.hudPresent)originalHudPostText(o,0,name?name(r.identity):std::string(),repeats>0?1:0,3.f); //118FF8
  }
  r.repeats=repeats;
  const int32_t pending=originalScorePendingPoints(o),divisor=repeats+1;
  int32_t multiplied=originalScoreMultipliedPoints(o);
  const int32_t grade=originalScoreGrade(pending);
  const int32_t inverted=originalScoreInvertedPoints(o);
  originalScoreComboAdd(o,quo(pending,divisor));
  meter=D(o.f(0x14),I2F(divisor));
  multiplied=quo(multiplied,divisor);multiplied-=rem(multiplied,10);
  o.i(0x19C)=addw(o.i(0x19C),inverted);o.i(0x110)=addw(o.i(0x110),1);
  o.i(0x198)=addw(o.i(0x198),addw(multiplied,inverted));
  o.i(0x16C+4*grade)=addw(o.i(0x16C+4*grade),1);
  if(o.i(0x18C)<pending){o.i(0x18C)=pending;o.u(0x190)=r.identity[0];o.u(0x194)=r.identity[1];}
  auto quantize=[](float angle,float scale){const int32_t d=W(M(angle,scale));const int32_t rest=rem(d,180);
   if(rest< -90)return d-rest-180;if(rest<91)return d-rest;return d-rest+180;};
  const int32_t spin=quantize(o.f(0x34),F(0x42652ee0)),flip=quantize(o.f(0x38),F(0x42652ee0));
  o.i(0xFC)=addw(o.i(0xFC),spin<0?-spin:spin);o.i(0x100)=addw(o.i(0x100),flip<0?-flip:flip);
  o.f(0x144)=A(o.f(0x144),o.f(0x44));
  o.i(0x104)=addw(o.i(0x104),o.i(0x4C));o.i(0x108)=addw(o.i(0x108),o.i(0x80));
  const int32_t total=addw(addw(multiplied,o.i(0x84)),inverted);
  o.i(0x114)=addw(o.i(0x114),o.i(0x54));o.i(0x118)=addw(o.i(0x118),o.i(0x58));o.i(0x11C)=addw(o.i(0x11C),o.i(0x74));
  o.i(0x198)=addw(o.i(0x198),o.i(0x84));
  o.f(0x14C)=std::max(o.f(0x30),o.f(0x14C));o.f(0x150)=std::max(o.f(0x48),o.f(0x150));
  o.f(0x154)=std::max(o.f(0x24),o.f(0x154));o.f(0x158)=std::max(o.f(0x78),o.f(0x158));
  if(!env.pointsToCareer){
   originalHudPostPoints(o,repeats>0?0x24:0x23,total,grade,2.5f);
   originalHudPostPoints(o,0x18,total,0,F(0x3f333333));
  }else{const int32_t amount=quo(total,500);originalScoreCareerAward(o,0,amount<21?amount:20,env,hooks);}
  r.points=total;r.grade=grade;
 }
 // Tail: a finished ordinary trick (or an idle combo clock) keeps the combo open for 1.5 s.
 bool comboCheck;
 if(takeoff)comboCheck=o.i(0x20)==0&&o.i(0x28)==0;
 else comboCheck=style==0&&flag==0;
 if(comboCheck&&(0<o.f(0x14)||o.f(0xA4)<0))o.f(0xA4)=std::max(o.f(0xA4),1.5f); //117708
 r.meterDelta=meter;return r;
}

// ---- 117FE0 ----
void originalScoreHud(OriginalScoreObject& o,const OriginalScoreEnvironment& env,const OriginalScoreHooks& hooks,bool boostWidgets){
 if(!o.hudPresent)return;
 OriginalRounding round;
 const int32_t pending=originalScorePendingPoints(o);
 if(pending>0)originalHudPostPoints(o,1,pending,originalScoreGrade(pending),-1.f);
 // Board-press depth widget 0xB (control 8 = 0x12FC80 board press, substate owner+0x2C0 != 3).
 if(env.control==8&&env.owner2C0!=3)originalHudSetValue(o.hud[0xB],0xB,0,0,std::min(env.owner330,1.f));
 else originalHudFree(o,0xB);
 for(int k=0x10;k<=0x17;k++)originalHudFree(o,k);
 const auto freeMessages=[&](){originalHudFree(o,0xF);originalHudFree(o,0xE);};
 bool updateSlot2=false;int32_t message=0,previous=0;
 if(env.control!=2&&env.control!=5){originalHudFree(o,0xC);originalHudFree(o,0xD);freeMessages();}
 else{
  bool none=false;float angle;
  if(env.control==2){ //12F118: filtered right stick, 0.1 dead zone
   float y=env.stick2B0,x=env.stick2A4;if(std::fabs(y)<.1f)y=0;if(std::fabs(x)<.1f)x=0;
   none=y==0&&x==0;angle=none?0.f:direction(y,x);
  }else{none=env.air50==F(0x7149f2ca);angle=none?0.f:env.air50;} //1360C8
  if(none)originalHudFree(o,0xD);
  else{
   const float wobble=originalCosine(M(I2F(int32_t(env.globalTick)),F(0x3dd67751)));
   float target=A(M(angle,F(0x3e22f983)),M(wobble,F(0x3cf5c28f)));
   if(target<0)target=A(target,1.f);
   auto& d=o.hud[0xD];
   if(d.type!=kOriginalHudFree){
    const float shown=ratio(d),gap=S(shown,target);constexpr float step=F(0x3d088889);
    if(.5f<gap){target=approach(target,S(shown,1.f),step);if(target<0)target=A(target,1.f);}
    else if(gap< -.5f){target=approach(target,A(shown,1.f),step);if(1.f<target)target=S(target,1.f);}
    else target=approach(target,shown,step);
   }
   originalHudSetValue(d,0xD,0,0,target);
  }
  if(env.control==2){
   if(none)originalHudFree(o,0xC);else originalHudPostPoints(o,0xC,0,0,-1.f);
   // falls through to the boost widgets
  }else{
   // 135F70: direction of the rotation already performed (signs of +18/+1C).
   const float fy=signum(env.air18),fx=signum(env.air1C);
   const bool still=fy==0&&fx==0;
   if(still){originalHudFree(o,0xC);originalHudFree(o,0xD);freeMessages();}
   else{
    const float stall=direction(fy,fx);
    originalHudPostPoints(o,0xC,0,0,-1.f);
    int s0,s2=0x34,s3=0x34,s4=0x34,s5=0x34;const float mag=std::fabs(stall);
    if(F(0x402fede0)<mag)s0=0x10;
    else if(F(0x3ffb53d3)<stall){s0=0x17;s2=0x15;s3=0x16;s4=0x11;s5=0x10;}
    else if(F(0x3f96cbe5)<stall)s0=0x16;
    else if(F(0x3ec90fdc)<stall){s0=0x15;s2=0x17;s3=0x16;s4=0x13;s5=0x14;}
    else if(stall<F(0xbffb53d3)){s0=0x11;s2=0x13;s3=0x12;s4=0x17;s5=0x10;}
    else if(stall<F(0xbf96cbe5))s0=0x12;
    else if(stall<F(0xbec90fdc)){s0=0x13;s2=0x11;s3=0x12;s4=0x15;s5=0x14;}
    else s0=0x14;
    // 136100 / 136168
    float spinOff,flipOff;
    {float x=A(M(env.air18,F(0x3e22f983)),.5f);float whole=I2F(W(x));if(x<whole)whole=S(whole,1.f);
     float v=S(M(std::fabs(S(env.air18,M(whole,F(0x40c90fdb)))),F(0x3ea2f983)),.5f);v=std::max(v,0.f);spinOff=A(v,v);}
    {float x=A(M(env.air1C,F(0x3e22f983)),.5f);float whole=I2F(W(x));if(x<whole)whole=S(whole,1.f);
     float v=M(std::fabs(S(env.air1C,M(whole,F(0x40c90fdb)))),F(0x3f22f983));if(1.f<v)v=S(2.f,v);
     v=S(v,.25f);v=std::max(v,0.f);flipOff=M(v,F(0x3faaaaab));}
    originalHudSetValue(o.hud[s0],s0,0,0,1.f);
    if(s2!=0x34)originalHudSetValue(o.hud[s2],s2,0,1,spinOff);
    if(s3!=0x34)originalHudSetValue(o.hud[s3],s3,0,1,spinOff);
    if(s4!=0x34)originalHudSetValue(o.hud[s4],s4,0,2,flipOff);
    if(s5!=0x34)originalHudSetValue(o.hud[s5],s5,0,2,flipOff);
    previous=o.hud[0xE].type!=kOriginalHudFree?o.hud[0xE].arg:0;
    if(!none){
     // Landing-quality messages (0x457820..): the stall direction against the stick direction.
     float delta=S(stall,angle);float x=A(M(delta,F(0x3e22f983)),.5f);float whole=I2F(W(x));if(x<whole)whole=S(whole,1.f);
     delta=S(delta,M(whole,F(0x40c90fdb)));const float off=std::fabs(delta);float bonus=0;
     if(F(0x402fede0)<off){
      originalHudPostText(o,0xE,"STALLED!",1,-1.f);message=1;
      if(F(0x402fede0)<mag||mag<F(0x3ec90fdc))bonus=1;
      else{bonus=2;if(F(0x3f96cbe5)<mag&&mag<F(0x3ffb53d3))bonus=5;}
     }else if(F(0x3f96cbe5)<off){
      const float product=M(delta,stall);bool offAxis=false;
      if(0<product){if(F(0x3fc90fdc)<mag)offAxis=true;}
      if(!offAxis&&product<0&&mag<F(0x3fc90fdc))offAxis=true;
      if(offAxis){if(F(0x3dcccccd)<flipOff){originalHudPostText(o,0xE,"OFF-AXIS!",2,-1.f);bonus=3;message=2;}}
      else if(F(0x3dcccccd)<spinOff){originalHudPostText(o,0xE,"INVERTED!",3,-1.f);bonus=2;message=3;}
     }else if(F(0x3ec90fdc)<off){
      const float a=std::fabs(angle);
      if(a<F(0x3f490fdc)||F(0x4016cbe5)<a){originalHudPostText(o,0xE,"LATE SPIN!",4,-1.f);bonus=1;message=4;}
      else{originalHudPostText(o,0xE,"LATE FLIP!",5,-1.f);bonus=2;message=5;}
     }
     if(bonus==0)freeMessages();
     else{
      originalHudPostPoints(o,0xF,W(bonus),0,-1.f);
      o.f(0x1C)=A(o.f(0x1C),M(bonus,F(0x3a5a740e)));
      updateSlot2=message!=previous;
     }
    }
   }
  }
 }
 if(updateSlot2){const int32_t inverted=originalScoreInvertedPoints(o);originalHudSetValue(o.hud[2],2,addw(o.i(0x84),inverted),inverted,0.f);}
 if(boostWidgets){
  // 1188F8..118D4C (slots 5/6/8/10/9) as engine/boost_*hud.hpp; kept here for the bit-exact bank.
  constexpr float step=F(0x3c888889);
  {auto& p=o.hud[5];float preview;
   if(p.type==kOriginalHudFree)preview=env.meter2F8;
   else{const float target=std::min(env.drain304==3?env.meter2F8:A(env.meter2F8,o.f(0x14)),1.f);preview=approach(target,ratio(p),step);}
   auto& q=o.hud[6];float stored;
   if(q.type==kOriginalHudFree)stored=env.meter2F8;else stored=approach(env.meter2F8,ratio(q),step);
   originalHudSetValue(p,5,0,0,preview);originalHudSetValue(q,6,0,0,stored);}
  if(env.tier2F4==0)originalHudFree(o,8);
  else{auto& s=o.hud[8];if(s.type==kOriginalHudFree)originalHudSetValue(s,8,0,0,0.f);
   const int32_t count=(env.tier2F4<11?env.tier2F4:10)-1;const float target=std::min(M(I2F(count),F(0x3de38e39)),1.f);
   originalHudSetValue(s,8,0,count,approach(target,ratio(s),F(0x3bda740f)));}
  {const int32_t remaining=10-(env.tier2F4<11?env.tier2F4:10);const int32_t active=o.i(0x5C);
   if((active==0&&o.i(0x54)==0)||remaining<=0)originalHudFree(o,10);
   else{int32_t count=o.i(0x54);if(active)count=count+1;if(remaining<count)count=remaining;auto& s=o.hud[10];
    if(s.type==kOriginalHudFree)originalHudSetValue(s,10,0,0,0.f);
    const float target=std::min(M(I2F(count),F(0x3de38e39)),1.f);originalHudSetValue(s,10,0,count,approach(target,ratio(s),step));}}
  {auto& s=o.hud[9];
   if(s.type==kOriginalHudFree){if(0<env.superTime2F0)originalHudSetValue(s,9,0,0,0.f);}
   else{const float current=ratio(s);
    if(current==1.f&&env.superTime2F0==0)originalHudFree(o,9);
    else{const float target=S(1.f,M(env.superTime2F0,F(0x3d4ccccd)));originalHudSetValue(s,9,0,env.tier2F4<12?env.tier2F4:11,approach(target,current,step));}}}
 }
 originalHudSetValue(o.hud[7],7,o.i(0x198),0,0.f);
 {auto& s=o.hud[3];
  if(s.type!=kOriginalHudFree){
   const int32_t goal=o.i(0xA0),shown=s.points;int32_t points;
   if(addw(goal,0x46)<shown)points=shown-0x46;else if(shown<subw(goal,0x46))points=shown+0x46;else points=goal;
   const float r=ratio(s);float value;
   if(F(0x3f822222)<r)value=S(r,F(0x3c888889));else if(r<F(0x3f7bbbbc))value=A(r,F(0x3c888889));else value=1.f;
   originalHudSetValue(s,3,points,o.i(0x9C),value);
   if(o.f(0xA4)<0)o.hud[3].field10=4;else o.hud[3].field10=W(M(o.f(0xA4),F(0x40511112)));
  }}
 {const int32_t inverted=originalScoreInvertedPoints(o);const int32_t bonus=o.i(0x84);const int32_t total=addw(bonus,inverted);auto& s=o.hud[2];
  if(s.type!=kOriginalHudFree){
   float value=0;
   if(subw(s.points,s.arg)==bonus){const float r=ratio(s);if(F(0x3f822222)<r)value=S(r,F(0x3c888889));else if(r<F(0x3f7bbbbc))value=A(r,F(0x3c888889));else value=1.f;}
   originalHudSetValue(s,2,total,inverted,value);
  }else if(total>0)originalHudSetValue(s,2,total,inverted,0.f);}
 // 117FE0 reads the character block's cash C+0xAC4 when it runs, after the tick's awards (a combo expiry's 119EF8 kind 2 above: PS2
 // ctm-parity/mountain fr-dra4a-full 7218, the $1 and slot 0x19 on the same tick); env.slot19Points is the cash before them.
 originalHudSetValue(o.hud[0x19],0x19,hooks.careerCash?hooks.careerCash():env.slot19Points,0,0.f);
 {auto& s=o.hud[4];
  if(s.type!=kOriginalHudFree){const float r=ratio(s);float value;
   if(F(0x3f822222)<r)value=S(r,F(0x3c888889));else if(r<F(0x3f7bbbbc))value=A(r,F(0x3c888889));else value=1.f;
   originalHudSetValue(s,4,s.points,0,value);}}
 (void)hooks;
}

// ---- 117C28 ----
void originalScoreTick(OriginalScoreObject& o,const OriginalScoreTables& t,const OriginalScoreEnvironment& env,const OriginalScoreHooks& hooks,bool boostWidgets){
 OriginalRounding round;
 const auto& v=env.velocity;
 // VU: vmul, vadday/vmaddaz/vmaddw (sum of the four squares), vsqrt.
 float sq=terrain_original::add(terrain_original::mul(v[0],v[0]),terrain_original::mul(v[1],v[1]));
 sq=terrain_original::add(sq,terrain_original::mul(v[2],v[2]));sq=terrain_original::add(sq,terrain_original::mul(v[3],v[3]));
 const float speed=terrain_original::sqrt(sq);
 const float dt=M(env.timeScale,F(0x3c888889));
 if(0<=o.f(0xA4)&&env.motionMode==0&&env.control!=1){
  o.f(0xA4)=S(o.f(0xA4),dt);
  if(o.f(0xA4)<=0)originalScoreComboExpire(o,env,hooks);
 }
 if(0<=o.f(0x30)){o.f(0x30)=A(o.f(0x30),dt);o.i(0x84)=addw(o.i(0x84),originalScoreThreshold(o,0x88,t.air1C,0x1C,o.f(0x30)));}
 if(0<=o.f(0x40)){o.f(0x40)=A(o.f(0x40),dt);o.f(0x14)=A(o.f(0x14),o.f(0x3C));o.i(0x84)=addw(o.i(0x84),originalScoreThreshold(o,0x8C,t.hold20,0x20,o.f(0x40)));}
 if(0<=o.f(0x24)){
  const float distance=M(speed,dt);
  o.f(0x24)=A(o.f(0x24),distance);o.f(0x14)=A(o.f(0x14),M(distance,F(0x3851b717)));
  if(env.up2<0)o.f(0x1C)=A(o.f(0x1C),M(distance,F(0x3951b717)));
  o.i(0x84)=addw(o.i(0x84),originalScoreThreshold(o,0x98,t.distance1D,0x1D,o.f(0x24)));
 }
 if(0<=o.f(0x2C)){o.f(0x2C)=A(o.f(0x2C),dt);o.f(0x14)=A(o.f(0x14),M(dt,F(0x3c23d70a)));o.i(0x84)=addw(o.i(0x84),originalScoreThreshold(o,0x90,t.style1E,0x1E,o.f(0x2C)));}
 if(0<=o.f(0x6C)){o.f(0x6C)=A(o.f(0x6C),dt);o.f(0x14)=A(o.f(0x14),M(dt,F(0x3d4ccccc)));}
 if(0<=o.f(0x78)){o.f(0x78)=A(o.f(0x78),dt);o.f(0x14)=A(o.f(0x14),M(dt,F(0x3d4ccccc)));o.i(0x84)=addw(o.i(0x84),originalScoreThreshold(o,0x94,t.timer1F,0x1F,o.f(0x78)));}
 const float smoothed=A(M(o.f(0x1A8),F(0x3f666666)),M(speed,F(0x3dcccccd)));
 o.f(0x164)=A(o.f(0x164),speed);o.f(0x168)=A(o.f(0x168),1.f);o.f(0x1A8)=smoothed;
 if(o.f(0x160)<smoothed)o.f(0x160)=smoothed;
 if(o.f(0x15C)<speed)o.f(0x15C)=speed;
 originalScoreHud(o,env,hooks,boostWidgets);
 if(o.hudPresent)for(auto& s:o.hud)originalHudTick(s);
 o.u(0x1B8)=o.u(0x1BC)=0;o.u(0x1C0)=1;
}

// ---- crash / pickups ----
int32_t originalScoreLostPoints(OriginalScoreObject& o){ //11A7A8
 const int32_t pending=originalScorePendingPoints(o);
 if(pending>0){originalHudFree(o,1);originalHudPostPoints(o,0x25,pending,0,1.5f);}
 if(o.i(0xA0)>0){originalHudFree(o,3);originalHudPostPoints(o,0x27,o.i(0xA0),0,1.5f);}
 const int32_t bonus=addw(o.i(0x84),originalScoreInvertedPoints(o));
 if(bonus>0){for(int k:{2,0x1D,0x1C,0x1E,0x1F,0x20})originalHudFree(o,k);originalHudPostPoints(o,0x28,bonus,0,1.5f);}
 originalHudFree(o,4);
 return pending;
}
float originalScoreCrash(OriginalScoreObject& o,bool penalize,float meter){ //119368
 OriginalRounding round;
 o.i(0x1A4)=addw(o.i(0x1A4),originalScoreLostPoints(o));
 originalScoreReset(o);originalScoreComboReset(o);o.i(0x120)=addw(o.i(0x120),1);
 return M(meter,penalize?F(0xbf333333):F(0xbdcccccd));
}
float originalScorePickup(OriginalScoreObject& o,int32_t points,const OriginalScoreEnvironment& env,const OriginalScoreHooks& hooks){ //119608
 OriginalRounding round;const int32_t scaled=W(A(M(I2F(points),o.f(0x1C4)),.5f));
 if(!env.pointsToCareer){originalHudPostPoints(o,0x23,scaled,0,2.5f);originalHudPostPoints(o,0x18,scaled,0,F(0x3f333333));}
 else{const int32_t amount=quo(scaled,500);originalScoreCareerAward(o,1,amount<21?amount:20,env,hooks);}
 o.i(0x198)=addw(o.i(0x198),scaled);return 0;
}
float originalScoreBail(OriginalScoreObject& o,bool attacked){ //119B08
 if(attacked){o.i(0x12C)=addw(o.i(0x12C),1);originalHudPostPoints(o,0x2D,0,0,1.5f);}
 else o.i(0x124)=addw(o.i(0x124),1);
 o.i(0x1A0)=addw(o.i(0x1A0),originalScoreLostPoints(o));
 const float penalty=originalScorePendingPoints(o)>0?F(0xbe800000):0.f;
 originalScoreReset(o);originalScoreComboReset(o);
 return penalty;
}
float originalScoreRecovery(OriginalScoreObject& o,bool quick){ //119BB0
 if(!quick)return 0;originalHudPostPoints(o,0x21,0,0,1.5f);return F(0x3dcccccd);
}
float originalScoreTrickStart(OriginalScoreObject& o,const OriginalTrickCommitProfile& p,int32_t stance,bool riderStance,
 const OriginalScoreNameBuilder& name,const OriginalScoreHooks& hooks){ //119C98 -> 11A168(score,0,0,0,0)
 OriginalRounding round;
 auto view=originalScoreView(o);OriginalTrickIdentityInput input{false,false,0,0,riderStance};
 auto id=originalTrickIdentity(view.identity,p.identity,input); //11A8C8
 o.f(0x34)=view.identity.spin34;o.f(0x38)=view.identity.flip38;
 if(id.valid){
  auto identity=id.identity;const int32_t named=originalNamedTrickBonus(identity,p.named); //11B1A8
  // 11A1A8: as in 11A228, the popup and 29B7E0 only for a monster trick.
  if(named>0){o.f(0x14)=A(o.f(0x14),M(I2F(named),F(0x38d1b717)));originalHudPostPoints(o,0x32,named,0,1.5f);if(hooks.named)hooks.named(identity);}
  auto history=originalScoreHistory(o);
  const int repeats=originalTrickRepeatCount(history,identity,{o.i(0x08),o.i(0x20),o.i(0x70),o.i(0x7C),o.i(0x28)}); //1190F0
  originalStoreScoreHistory(o,history);
  if(o.hudPresent)originalHudPostText(o,0,name?name(identity):std::string(),repeats>0?1:0,3.f); //118FF8
 }
 if(0<o.f(0x14)){
  o.i(0x00)=stance;o.f(0x34)=0;o.f(0x38)=0;o.u(0x04)=0;o.u(0x0C)=0;o.u(0x10)=0;o.f(0x40)=-1;
  o.u(0x60)=o.u(0x64)=o.u(0x68)=0;o.i(0x08)=1;
 }
 o.f(0xA4)=-1; //1176F8
 return 0;
}
}

