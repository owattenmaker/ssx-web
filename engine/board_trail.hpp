#pragma once
#include "terrain_contact_math.hpp"
#include "collision_scalar.hpp"
#include <bit>
#include <cstdint>
#include <optional>

namespace ssx::board_trail {
using terrain_original::Vector;using terrain_original::Rounding;
using terrain_original::mul;using terrain_original::add;using terrain_original::sub;
using terrain_original::dot;using terrain_original::difference;using terrain_original::cross;
inline Vector scale(Vector a,float s){for(auto& x:a)x=mul(x,s);return a;}
inline Vector plus(Vector a,Vector b){for(unsigned k=0;k<3;++k)a[k]=add(a[k],b[k]);return a;}
inline Vector normalize(Vector a){
 // PCSX2 VUops.cpp::_vuRSQRT saturates 1/sqrt(0) to0x7F7FFFFF.
 // Preserve zero components instead of host infinity*zero NaNs.
 float squared=dot(a,a);float q=squared==0?std::bit_cast<float>(0x7f7fffffu):terrain_original::div(1,std::sqrt(squared));
 return scale(a,q);
}
// Defaults are authored PS2 SLUS_207.72 values, exported by import_board_trail.py.
struct Profile {
 float innerWidth=5,outerWidth=2.9f,height=9,depthHeight=.2f;
 std::array<float,4> depths{5,0,-20,-40};float innerJitter=3;
 float middleU=.375f,innerU=.25f,outerU=0;
 float planeOffset=0,backwardsOffset=5;
 float turnCosine=.9990000128746033f,minimumSpeed=.009999999776482582f;
 float anglePeak=.7853982448577881f,angleScale=1.2732393741607666f,speedScale=.0036000001709908247f;
 int fadeSegments=16;
};
// Source geometry+34 matrix axes/position, all Z-up centimeters; not Metal axes.
struct BoardFrame {Vector axis0{},axis2{},position{};};
struct Input {
 int motion=0,crashSubmode=0,surface=0,semantic=0;
 bool marker0=false,marker1=false,flagAC4=false,flagAD0=false,flagAFC=false,flagB00=false;
 bool manual=false,reverseStance=false,detached=false;
 BoardFrame board;Vector bone8B0{},bone8B8{},bone918{},bone8E8{};
 Vector contact{},normal{0,0,1},velocity{};float contactDistance=0;
 // Original environment table at 0x4FA398 + index*0xF0, in A,R,G,B order.
 std::array<float,4> environmentARGB{};
 bool special()const{return flagAC4&&!flagAD0&&flagAFC;}
};
struct Vertex {
 std::array<float,4> uvq{0,0,1,0};std::array<uint32_t,4> rgba{};
 std::array<float,4> position{0,0,0,1};
 Vector xyz()const{return {position[0],position[1],position[2]};}
 void set(Vector p){position={p[0],p[1],p[2],1};}
};
static_assert(sizeof(Vertex)==48);
struct State {
 std::array<std::array<Vertex,54>,6> bands{};
 float jitterLeft=1,jitterRight=1;int parity=0,head=0,count=0,phase=2,cooldown=0;
 float depth=0,height=0;Vector lastContact{},previousDirection{},previousLateral{};
 explicit State(const Profile& p={}){std::array<float,6> u{p.innerU,originalScalarSubtract(1,p.innerU),p.outerU,p.middleU,originalScalarSubtract(1,p.middleU),originalScalarSubtract(1,p.outerU)};for(int b=0;b<6;++b)for(int i=0;i<54;++i)bands[b][i].uvq={u[b],float(i&1),1,0};}
};
inline bool eligible(State& state,const Input& in,const Profile& p={}){
 if(in.motion==2){if(in.crashSubmode)return false;}else if(in.motion!=0&&!(in.special()&&in.flagB00))return false;
 if(in.semantic==22&&in.marker0&&!in.marker1)return false;
 constexpr int types[14]={0,1,2,3,-1,0,0,-1,-1,-1,-1,-1,0,2};
 if(unsigned(in.surface)>=14||types[in.surface]<0)return false;
 state.depth=p.depths[types[in.surface]];return true;
}
// 0x2E86F0; called after replacing the pending end slice, before ring advancement.
inline bool commitSample(State& s,const Input& in,float directionCosine,const Profile& p={}){
 Rounding rounding;if(s.cooldown>0){--s.cooldown;return false;}
 auto d=difference(in.contact,s.lastContact);float distance=std::sqrt(dot(d,d));
 if(s.phase==1)s.cooldown=2;else if(distance<(in.special()?30.f:110.f)&&!(directionCosine<p.turnCosine))return false;
 s.lastContact=in.contact;return true;
}
// 0x2EA2C0: shared visual RNG GP+0xA0C, not the six-word gameplay RNG.
inline float randomJitter(uint32_t& visualState,float amount){
 visualState=((visualState*0x18fcdu+0xe9507cu)&0x7fffffu)|0x3f800000u;
 return originalScalarAdd(mul(amount,originalScalarSubtract(std::bit_cast<float>(visualState),1)),1);
}
struct DrawWindow {int start=0,count=0;float fade=0,fadeStep=0;};
inline DrawWindow drawWindow(const State& s,const Profile& p={}){
 if(s.count<2)return {};int start=s.head-(s.count-57),count=s.count-2,excess=s.count-50;
 if(excess>0){start+=excess;count-=excess;}if(count<2)return {};
 return {start%54,count,0,originalScalarDivide(1,float(p.fadeSegments))};
}
// Original VU1 0x3AB0..0x3B28: clamp fade, ITOF0 alpha, MUL, FTOI0.
inline uint32_t fadedAlpha(uint32_t alpha,float fade){Rounding rounding;return uint32_t(mul(float(alpha),std::clamp(fade,0.f,1.f)));}
inline constexpr std::array<int,6> bandOrder{2,0,3,4,1,5};
inline void copyPayload(Vertex& dest,const Vertex& src){dest.position=src.position;dest.rgba=src.rgba;}
// Native translation of 0x2E8938. There is no fixed timestep or guessed trail
// lifetime: caller invokes this in original visual-FX phase and supplies live pose.
inline void update(State& s,const Input& in,uint32_t& visualRng,const Profile& p={}){
 Rounding rounding;int prev=(s.head+53)%54;
 if(!eligible(s,in,p)){
  if(s.phase==0)return;
  if(s.phase==1){s.head=(s.head+51)%54;s.count=std::max(0,s.count-3);}
  else{
   s.bands[3][prev].position=s.bands[2][prev].position=s.bands[0][prev].position;
   s.bands[4][prev].position=s.bands[5][prev].position=s.bands[1][prev].position;
   s.bands[1][prev].rgba[3]=0;
   for(auto& band:s.bands){copyPayload(band[s.head],s.bands[1][prev]);band[s.head].position[3]=1;band[s.head].rgba[3]=0;}
  }
  s.phase=0;return;
 }
 if(s.phase==0){s.head=(s.head+3)%54;prev=(s.head+53)%54;}
 s.height=originalScalarSubtract(p.height,mul(s.depth,p.depthHeight));
 auto longAxis=scale(in.board.axis0,87.5f),crossAxis=scale(in.board.axis2,17.5f);
 std::array<Vector,4> corners;
 if(in.motion==0||(in.special()&&in.flagB00)){
  auto rear=difference(in.board.position,longAxis),front=plus(in.board.position,longAxis);
  if(in.manual){bool frontManual=unsigned(in.semantic-23)<8;float sign=(frontManual!=in.reverseStance)?-1.f:1.f;
   if(sign>0)rear=plus(rear,scale(in.board.axis0,std::min(mul(sign,175),140.f)));
   else if(sign<0)front=plus(front,scale(in.board.axis0,std::max(mul(sign,175),-140.f)));
  }
  corners={plus(front,crossAxis),difference(front,crossAxis),plus(rear,crossAxis),difference(rear,crossAxis)};
 }else{
  auto rear=difference(in.board.position,longAxis);
  corners={in.bone8B0,in.bone8B8,in.detached?in.bone918:plus(rear,crossAxis),in.detached?in.bone8E8:difference(rear,crossAxis)};
 }
 float speed=std::sqrt(dot(in.velocity,in.velocity));
 auto direction=speed<p.minimumSpeed?s.previousDirection:scale(in.velocity,terrain_original::div(1,speed));
 direction=normalize(difference(direction,scale(in.normal,dot(direction,in.normal))));
 auto lateral=cross(direction,in.normal);if(dot(lateral,s.previousLateral)<0)lateral=scale(lateral,-1);
 std::array<float,4> extent;for(int i=0;i<4;++i)extent[i]=dot(difference(corners[i],in.contact),lateral);
 auto select=[&](bool maximum){for(int i:{1,0,3}){bool wins=true;for(int j=0;j<4;++j)if(i!=j&&!(maximum?extent[i]>extent[j]:extent[i]<extent[j]))wins=false;if(wins)return i;}return 2;};
 auto project=[&](int i){return difference(corners[i],scale(in.normal,originalScalarSubtract(dot(in.normal,difference(corners[i],in.contact)),p.planeOffset)));};
 auto maxPoint=project(select(true)),minPoint=project(select(false));
 float maxAlong=dot(difference(maxPoint,in.contact),direction),minAlong=dot(difference(minPoint,in.contact),direction);
 float maxAcross=dot(difference(maxPoint,in.contact),lateral),minAcross=dot(difference(minPoint,in.contact),lateral);
 if(in.motion==2){maxPoint=plus(plus(in.contact,scale(lateral,mul(maxAcross,.75f))),scale(in.normal,p.planeOffset));minPoint=plus(plus(in.contact,scale(lateral,mul(minAcross,.75f))),scale(in.normal,p.planeOffset));}
 for(int b:{2,0,3})s.bands[b][s.head].set(maxPoint);for(int b:{4,1,5})s.bands[b][s.head].set(minPoint);
 float x=std::abs(maxAcross),y=std::abs(maxAlong);
 float angle=x==0?(y==0?0.f:1.5707963705062866f):collision_scalar::atan(originalScalarDivide(y,x));
 float shape=angle>p.anglePeak?std::clamp(originalScalarSubtract(2,mul(angle,p.angleScale)),0.f,1.f):std::clamp(mul(angle,p.angleScale),0.f,1.f);
 float speedFactor=mul(speed,p.speedScale);if(speedFactor<=1)shape=mul(shape,speedFactor);
 auto center=plus(plus(in.contact,scale(in.normal,p.planeOffset)),scale(direction,mul(shape,maxAlong<minAlong?maxAlong:minAlong)));
 auto a=plus(center,scale(lateral,maxAcross)),b=plus(center,scale(lateral,minAcross));
 auto across=normalize(difference(b,a));auto inner=scale(across,p.innerWidth);
 auto outer=scale(scale(across,p.outerWidth),originalScalarAdd(std::abs(std::min(in.contactDistance,0.f)),10));
 auto height=scale(in.normal,s.height),depth=scale(in.normal,std::min(in.contactDistance,0.f));
 s.bands[2][prev].set(plus(plus(difference(a,outer),depth),depth));
 s.bands[0][prev].set(plus(difference(a,scale(inner,s.jitterLeft)),height));
 s.bands[3][prev].set(plus(a,depth));s.bands[4][prev].set(plus(b,depth));
 s.bands[1][prev].set(plus(plus(b,scale(inner,s.jitterRight)),height));
 s.bands[5][prev].set(plus(plus(plus(b,outer),depth),depth));
 std::array<uint32_t,4> argb;for(int k=0;k<4;++k)argb[k]=uint32_t(std::clamp(mul(in.environmentARGB[k],255),0.f,255.f));
 for(auto& band:s.bands){band[prev].rgba=band[s.head].rgba={argb[1],argb[2],argb[3],argb[0]};}
 bool backA=dot(difference(s.bands[0][s.head].xyz(),s.bands[0][prev].xyz()),direction)<0;
 bool backB=dot(difference(s.bands[1][s.head].xyz(),s.bands[1][prev].xyz()),direction)<0;
 if(backA&&backB){auto d=scale(direction,p.backwardsOffset);for(int k:{0,2,3})s.bands[k][s.head].set(plus(s.bands[0][prev].xyz(),d));for(int k:{1,4,5})s.bands[k][s.head].set(plus(s.bands[1][prev].xyz(),d));}
 else if(backA){for(int k:{0,2,3})s.bands[k][s.head].position=s.bands[1][s.head].position;}
 else if(backB){for(int k:{1,4,5})s.bands[k][s.head].position=s.bands[0][s.head].position;}
 if(s.phase==0){int cap=(prev+53)%54;for(auto& band:s.bands){copyPayload(band[cap],s.bands[1][prev]);band[cap].rgba[3]=band[prev].rgba[3]=0;}
  for(int k:{3,2})s.bands[k][prev].position=s.bands[0][prev].position;
  for(int k:{4,5})s.bands[k][prev].position=s.bands[1][prev].position;
  s.count=std::min(54,s.count+3);s.phase=1;
 }
 if(commitSample(s,in,dot(direction,s.previousDirection),p)){
  s.previousLateral=lateral;s.jitterLeft=randomJitter(visualRng,p.innerJitter);s.jitterRight=randomJitter(visualRng,p.innerJitter);
  s.phase=2;int old=s.head;s.head=(s.head+1)%54;s.count=std::min(54,s.count+1);for(auto& band:s.bands)copyPayload(band[s.head],band[old]);
 }
 s.parity=1-s.parity;s.previousDirection=direction;
}
} // namespace ssx::board_trail
