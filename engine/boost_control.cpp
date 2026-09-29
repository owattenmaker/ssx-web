#include "boost_control.hpp"
#include "original_float.hpp"
#include "terrain_contact_math.hpp"
#include <cfenv>
#include <stdexcept>
#pragma STDC FENV_ACCESS ON
namespace ssx {
OriginalBoostEffects originalBoostTick(OriginalBoostState& s,const OriginalBoostProfile& p,float timeScale,int motionMode,int controlState){
 OriginalRounding round;
 auto approach=[](float value,float delta){if(delta<value)return originalScalarSubtract(value,delta);if(value< -delta)return originalScalarAdd(value,delta);return 0.f;};
 OriginalBoostEffects effects;float dt=terrain_original::mul(timeScale,p.tickSeconds);
 s.window=approach(s.window,dt);
 if(s.modifier>p.modifierThreshold||motionMode!=1)s.modifier=approach(s.modifier,dt);
 if((motionMode==1||controlState==12)&&s.superTime>0&&s.superTime<=p.superTimerFloor)s.superTime=p.superTimerFloor;
 else if(s.superTime>0){s.superTime=approach(s.superTime,dt);effects.timerExpired=s.superTime==0;}
 if(s.tier==10){s.meter=1;if(s.superTime==0){s.tier=5;s.superTime=20;}return effects;}
 if(s.drainEnabled==0)s.meter=1;
 else if((s.drainEnabled==1||s.drainEnabled==2)&&controlState!=6)s.meter=approach(s.meter,terrain_original::mul(dt,(s.drainEnabled==1?p.normalDecay:p.fastDecay)));
 return effects;
}
OriginalBoostEffects originalBoostControl(OriginalBoostState& s,const OriginalBoostProfile& p,bool held,bool pressed){
 OriginalRounding round;
 OriginalBoostEffects effects;effects.pressed=pressed;
 if(pressed)s.feedbackFlags|=0x10;
 if(held&&s.meter>0){
  if(s.amount==0){s.amount=s.meter>p.fullThreshold?1.f:s.meter>p.mediumThreshold?.625f:.25f;effects.started=true;}
  if(s.window==0&&s.drainEnabled&&s.tier<10)s.meter=originalScalarSubtract(s.meter,p.drainPerTick);
 }else{
  effects.denied=held&&pressed;effects.stopped=s.amount>0;s.amount=0;
 }
 return effects;
}
}
