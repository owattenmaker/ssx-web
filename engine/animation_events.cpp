#include "animation_events.hpp"
#include "original_float.hpp"
#include <cfenv>
#include <cmath>
#pragma STDC FENV_ACCESS ON
namespace ssx {
void originalAnimationPrimaryStep(OriginalAnimationSlot&slot,float rate,float dt,
        OriginalAnimationEventFlags&state,std::span<const OriginalAnimationEventMarker>markers){
    struct Round{int old=std::fegetround();Round(){std::fesetround(FE_TOWARDZERO);}~Round(){std::fesetround(old);}}round;
    if(!slot.enabled)return;
    float old=slot.time,delta=slot.rate*rate;delta=delta*dt;
    float advanced=originalScalarAdd(old,delta);
    state.raised=0;state.completed=false;
    auto raise=[&](unsigned bit){uint64_t flag=uint64_t(1)<<(bit&63);state.raised|=flag&~state.latched;state.latched|=flag;};
    if(state.seekPending){for(const auto&marker:markers)if(!marker.end&&marker.time==old)raise(marker.bit);state.seekPending=false;}
    state.completed=originalAnimationSlotStep(slot,rate,dt);
    if(state.completed)for(const auto&marker:markers)if(marker.end)raise(marker.bit);
    auto interval=[&](float from,float to){
        if(from==to)return;
        for(const auto&marker:markers)if(!marker.end&&
            (from<=to?(from<marker.time&&marker.time<=to):(to<=marker.time&&marker.time<from)))raise(marker.bit);
    };
    if(slot.loop&&(advanced<0||advanced>=slot.duration)){
        float quotient=originalScalarDivide(advanced,slot.duration),count=float(int(quotient));if(quotient<count)count=originalScalarSubtract(count,1.f);
        int wraps=int(count);
        if(wraps>=2)interval(0,slot.duration);
        else if(wraps<-1)interval(slot.duration,0);
        else if(wraps==1){if(old<=slot.time)interval(0,slot.duration);else{interval(old,slot.duration);interval(0,slot.time);}}
        else if(wraps==-1){if(slot.time<=old)interval(slot.duration,0);else{interval(old,0);interval(slot.duration,slot.time);}}
    }else interval(old,slot.time);
}
}
