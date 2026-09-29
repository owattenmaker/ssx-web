#include "terrain_contact_math.hpp"
#include "snow_particles.hpp"
#include "original_float.hpp"
#include <algorithm>
#include <bit>
#include <cfenv>
#include <cmath>
#include <stdexcept>
#pragma STDC FENV_ACCESS ON
namespace ssx {namespace {
using Round=OriginalRounding;
template<size_t N>std::array<float,N> scale(std::array<float,N> v,float x){for(auto&a:v)a=terrain_original::mul(a,x);return v;}
template<size_t N>std::array<float,N> add(std::array<float,N>a,std::array<float,N>b){for(size_t i=0;i<N;++i)a[i]=terrain_original::add(a[i],b[i]);return a;}
template<size_t N>std::array<float,N> sub(std::array<float,N>a,std::array<float,N>b){for(size_t i=0;i<N;++i)a[i]=terrain_original::sub(a[i],b[i]);return a;}
float next(uint32_t&state){state=(((state<<1)^((state>>4)&1)^((state>>22)&1))&0x7fffff)|0x3f800000;return std::bit_cast<float>(state);}
}
OriginalSnowParticleKernel originalSnowParticleKernel(const OriginalSnowParticleProfile&p){
    Round round;if(p.particlesPerBirth==0||p.damping<=0||p.life<=0)throw std::runtime_error("Invalid original snow particle profile");
    OriginalSnowParticleKernel k;k.particlesPerBirth=p.particlesPerBirth;float half=terrain_original::mul(p.lifeRange,.5f),maxLife=originalScalarAdd(p.life,half),minLife=originalScalarSubtract(p.life,half);
    float ticks=terrain_original::mul(maxLife,60.f);float truncated=float(int(ticks));if(truncated<ticks)truncated=originalScalarAdd(truncated,1.f);k.birthCapacity=unsigned(truncated);
    k.ageStep=originalScalarDivide(terrain_original::mul(maxLife,p.damping),float(k.birthCapacity*p.particlesPerBirth));
    k.sizeRange=p.sizeRange;k.sizeDelta=originalScalarSubtract(p.finalSize,p.size);k.sizeBase=originalScalarSubtract(p.size,terrain_original::mul(p.sizeRange,1.5f));
    k.lifeRange=terrain_original::mul(originalScalarSubtract(maxLife,minLife),p.damping);k.lifeBase=originalScalarSubtract(terrain_original::mul(minLife,p.damping),k.lifeRange);
    float inverse=terrain_original::div(1.f,p.damping);k.force=scale(scale(p.force,inverse),inverse);
    k.positionRange0=p.positionRange0;k.positionRange1=p.positionRange1;k.positionBase=sub(p.offset,scale(add(p.positionRange0,p.positionRange1),1.5f));
    for(unsigned i=0;i<3;++i)k.velocityRanges[i]=scale(p.velocityRanges[i],inverse);
    k.velocityBase=sub(scale(p.velocity,inverse),scale(add(add(k.velocityRanges[0],k.velocityRanges[1]),k.velocityRanges[2]),1.5f));
    k.colourRange0=scale(p.colourRange0,128.f);k.colourRange1=scale(p.colourRange1,128.f);auto start=scale(p.startColour,128.f),end=scale(p.endColour,128.f);
    k.colourBase=sub(start,scale(add(k.colourRange0,k.colourRange1),1.5f));k.colourSlope=scale(sub(end,start),terrain_original::div(1.f,(terrain_original::mul(maxLife,p.damping))));return k;
}
std::optional<OriginalSnowParticle> originalSnowParticle(const OriginalSnowParticleKernel&k,const OriginalSnowBirth&a,const OriginalSnowBirth&b,float fraction,float age,uint32_t&random){
    Round round;if(a.seed<1)return std::nullopt;
    float rPosition0=next(random),rPosition1=next(random),rVelocity0=next(random),rVelocity1=next(random),rVelocity2=next(random),rColour0=next(random),rColour1=next(random),rSize=next(random),rLife=next(random);
    float complement=terrain_original::sub(1.f,fraction);auto birthVelocity=add(scale(a.velocityCoefficient,complement),scale(b.velocityCoefficient,fraction));
    auto velocity=add(birthVelocity,k.velocityBase);velocity=add(velocity,scale(k.velocityRanges[0],rVelocity0));velocity=add(velocity,scale(k.velocityRanges[1],rVelocity1));velocity=add(velocity,scale(k.velocityRanges[2],rVelocity2));
    auto position=add(scale(a.positionCm,complement),scale(b.positionCm,fraction));position=add(position,k.positionBase);position=add(position,scale(k.positionRange0,rPosition0));position=add(position,scale(k.positionRange1,rPosition1));
    float lifetime=terrain_original::add(k.lifeBase,terrain_original::mul(k.lifeRange,rLife));if(!(age<lifetime))return std::nullopt;
    float t=std::min(age,2.700000047683716f),squared=terrain_original::mul(t,t),polynomial=terrain_original::mul(t,-.7300000190734863f);polynomial=terrain_original::add(polynomial,terrain_original::mul(squared,.11299999803304672f));
    position=add(scale(k.force,age),position);position=add(position,scale(sub(k.force,velocity),polynomial));
    float size=terrain_original::mul(k.sizeDelta,age);size=terrain_original::mul(size,(terrain_original::div(1.f,lifetime)));size=terrain_original::add(size,k.sizeBase);size=terrain_original::add(size,terrain_original::mul(k.sizeRange,rSize));
    SnowColour colour;for(unsigned i=0;i<4;++i){float tint=terrain_original::mul(float(a.colour[i]),complement);tint=terrain_original::add(tint,terrain_original::mul(float(b.colour[i]),fraction));tint=terrain_original::mul(tint,.003921568859368563f);float value=terrain_original::mul(k.colourBase[i],tint);value=terrain_original::add(value,terrain_original::mul(k.colourRange0[i],rColour0));value=terrain_original::add(value,terrain_original::mul(k.colourSlope[i],age));value=terrain_original::add(value,terrain_original::mul(k.colourRange1[i],rColour1));colour[i]=std::max(i<3?std::min(value,128.f):value,0.f);}
    OriginalSnowParticle out;out.positionCm=position;out.halfExtentCm=std::abs(size);out.scaledAge=age;out.scaledLifetime=lifetime;for(unsigned i=0;i<4;++i)out.colourGs[i]=uint8_t(std::clamp(int(colour[i]),0,255));return out;
}
void OriginalSnowParticles::emit(const OriginalSnowEmission&e,OriginalRandomState&random,bool enabled){
    Round round;auto&b=births[cursor];
    //3717C0 updates inherited colour and this ring slot even when3710D0
    //will reject the birth because emitter+174 is disabled.
    if(e.colour)lastColour=*e.colour;
    for(unsigned i=0;i<4;++i){float x=terrain_original::mul(lastColour[i],255.f);b.colour[i]=uint8_t(int(std::clamp(x,0.f,255.f)));}
    if(!enabled)return;
    b.positionCm=e.positionCm;if(e.velocityCmps)lastVelocity=*e.velocityCmps;b.velocityCoefficient=lastVelocity;
    b.seed=e.active?std::bit_cast<float>((random.next()&0x7fffff)|0x3f800000):0;cursor=cursor==0?unsigned(births.size()-1):cursor-1;
}
std::vector<OriginalSnowParticle> OriginalSnowParticles::particles()const{
    Round round;std::vector<OriginalSnowParticle> result;float age=0;float invCount=terrain_original::div(1.f,float(kernel.particlesPerBirth));
    for(unsigned group=0;group<births.size();++group){if(group%43==0){float begin=terrain_original::mul(float((group/43)*43),float(kernel.particlesPerBirth));age=terrain_original::mul(begin,kernel.ageStep);}
        const auto&a=births[(cursor+1+group)%births.size()];const auto&b=births[(cursor+1+std::min(group+1,unsigned(births.size()-1)))%births.size()];
        if(a.seed<1){age=terrain_original::add(age,terrain_original::mul(kernel.ageStep,float(kernel.particlesPerBirth)));continue;}
        uint32_t random=(std::bit_cast<uint32_t>(a.seed)&0x7fffff)|0x3f800000;float fraction=0;
        for(unsigned i=0;i<kernel.particlesPerBirth;++i){auto p=originalSnowParticle(kernel,a,b,fraction,age,random);if(p)result.push_back(*p);age=terrain_original::add(age,kernel.ageStep);fraction=terrain_original::add(fraction,invCount);}
    }return result;
}
}
