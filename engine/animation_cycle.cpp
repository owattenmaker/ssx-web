#include "animation_cycle.hpp"
#include "original_float.hpp"
#include "terrain_contact_math.hpp"
#include <bit>
#include <cfenv>
#include <stdexcept>
#pragma STDC FENV_ACCESS ON
namespace ssx {
void originalFiveWayAnimationStep(OriginalAnimationCyclePair&s,const std::array<uint32_t,5>&choices,const std::array<float,5>&durations,float amount,float timeScale){
    OriginalRounding rounding;
    unsigned first,second;float weight;
    if(amount>.5f){first=4;second=3;weight=originalScalarSubtract(amount,.5f);weight=originalScalarAdd(weight,weight);}
    else if(amount>0){first=3;second=2;weight=originalScalarAdd(amount,amount);}
    else if(amount>-.5f){first=2;second=1;weight=originalScalarAdd(amount,amount);weight=originalScalarAdd(weight,1.f);}
    else{first=1;second=0;weight=originalScalarAdd(amount,1.f);weight=originalScalarAdd(weight,weight);}
    if(s.durations[0]<=0||durations[first]<=0||durations[second]<=0){throw std::runtime_error("Invalid original animation cycle duration");}
    if(s.clips[0]!=choices[first]){float phase=originalScalarDivide(s.times[0],s.durations[0]);s.clips[0]=choices[first];s.durations[0]=durations[first];s.times[0]=terrain_original::mul(s.durations[0],phase);}
    s.clips[1]=choices[second];s.durations[1]=durations[second];s.weights={weight,originalScalarSubtract(1.f,weight)};
    float step=terrain_original::mul(timeScale,std::bit_cast<float>(0x3c888889u));float delta=terrain_original::mul(s.rates[0],s.sequenceRate);delta=terrain_original::mul(delta,step);s.times[0]=originalScalarAdd(s.times[0],delta);
    s.completed=s.times[0]<0||s.times[0]>=s.durations[0];
    if(s.completed){float quotient=originalScalarDivide(s.times[0],s.durations[0]);float count=float(int(quotient));if(quotient<count)count=originalScalarSubtract(count,1.f);float span=terrain_original::mul(s.durations[0],count);s.times[0]=originalScalarSubtract(s.times[0],span);}
    float phase=originalScalarDivide(s.times[0],s.durations[0]);s.times[1]=terrain_original::mul(phase,s.durations[1]);
}
}
namespace ssx {
void originalThreeWayAnimationStep(OriginalAnimationCyclePair&s,const std::array<uint32_t,3>&choices,const std::array<float,3>&durations,float amount,float timeScale){
    OriginalRounding rounding;unsigned second=amount>0?2:0;float weight=amount>0?amount:-amount;
    if(durations[1]<=0||durations[second]<=0){throw std::runtime_error("Invalid original animation cycle duration");}
    s.clips={choices[1],choices[second]};s.durations={durations[1],durations[second]};s.weights={originalScalarSubtract(1.f,weight),weight};
    float step=terrain_original::mul(timeScale,std::bit_cast<float>(0x3c888889u));float delta=terrain_original::mul(s.rates[0],s.sequenceRate);delta=terrain_original::mul(delta,step);s.times[0]=originalScalarAdd(s.times[0],delta);
    s.completed=s.times[0]<0||s.times[0]>=s.durations[0];
    if(s.completed){float quotient=originalScalarDivide(s.times[0],s.durations[0]);float count=float(int(quotient));if(quotient<count)count=originalScalarSubtract(count,1.f);float span=terrain_original::mul(s.durations[0],count);s.times[0]=originalScalarSubtract(s.times[0],span);}
    float phase=originalScalarDivide(s.times[0],s.durations[0]);s.times[1]=terrain_original::mul(phase,s.durations[1]);
}
}
