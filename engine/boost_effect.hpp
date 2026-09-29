#pragma once
#include "original_float.hpp"
#include "terrain_contact_math.hpp"
#include <array>
namespace ssx {
struct OriginalBoostEffectState {
 float trailLength=0,alpha=0,width=0,primaryDistance=0;int primaryCount=0; //+18 float distance, +14 integer count
 int texture=57,trailCount=0;bool wasAirborne=false;
};
struct OriginalBoostEffectInput {
 std::array<float,3> velocity{};float boost=0,pickup=0;
 int motionMode=0,tier=0;bool forcePickup=false,forceHigh=false,forceMedium=false;
};
struct OriginalBoostEffectParameters {bool emitting=false;float scrollStep=0,speed=0;};
//2E66B8..2E68A4. Geometry/history advancement follows separately.
inline OriginalBoostEffectParameters originalBoostEffectParameters(OriginalBoostEffectState&s,const OriginalBoostEffectInput&i){
 OriginalRounding rounding;using namespace terrain_original;OriginalBoostEffectParameters out;
 bool air=i.motionMode==1;out.emitting=!air&&(i.boost>0||i.pickup>0);
 if(s.wasAirborne&&!air){s.trailCount=0;s.primaryCount=0;s.primaryDistance=0;}s.wasAirborne=air;
 out.scrollStep=.0020000000949949026f;
 if(i.tier==11){s.texture=61;out.scrollStep=.009999999776482582f;}
 else if(i.pickup>.009999999776482582f){s.texture=60;out.scrollStep=.009999999776482582f;}
 else if(i.forcePickup){s.texture=60;out.scrollStep=.009999999776482582f;}
 else if(i.tier>=10||i.forceHigh){s.texture=59;out.scrollStep=.009999999776482582f;}
 else if(i.tier>=5||i.forceMedium){s.texture=58;out.scrollStep=.004999999888241291f;}
 else s.texture=57;
 out.speed=terrain_original::sqrt(add(dot(i.velocity,i.velocity),0.f));
 if(out.emitting){float amount=std::max(i.pickup,i.boost);amount=amount>=0?std::min(amount,1.f):0.f;
  s.trailLength=mul(originalScalarAdd(mul(amount,400.f),400.f),originalScalarDivide(out.speed,3333.33349609375f));
  s.width=i.tier==11?50.f:originalScalarAdd(mul(amount,9.f),6.f);s.alpha=1;
 }else{s.trailLength=mul(s.trailLength,.949999988079071f);s.alpha=mul(s.alpha,.949999988079071f);s.width=mul(s.width,.949999988079071f);}
 return out;
}
}
