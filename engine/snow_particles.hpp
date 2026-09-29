#pragma once
#include "snow_emission.hpp"
#include "original_random.hpp"
#include <vector>
namespace ssx {
struct OriginalSnowParticleProfile {
    unsigned particlesPerBirth=2;
    float life=.6000000238418579f,lifeRange=0,damping=2.5f,size=10,sizeRange=10,finalSize=35;
    SnowVector offset{},positionRange0{5,5,5},positionRange1{},velocity{},force{0,0,-150};
    std::array<SnowVector,3> velocityRanges{{{50,0,0},{0,50,0},{0,0,50}}};
    SnowColour startColour{1,1,1,.4000000059604645f},endColour{1,1,1,0},colourRange0{},colourRange1{};
};
struct OriginalSnowParticleKernel {
    unsigned particlesPerBirth=0,birthCapacity=0;
    float ageStep=0,sizeBase=0,sizeRange=0,sizeDelta=0,lifeBase=0,lifeRange=0;
    SnowVector positionBase{},positionRange0{},positionRange1{},velocityBase{},force{};
    std::array<SnowVector,3> velocityRanges{};
    SnowColour colourBase{},colourSlope{},colourRange0{},colourRange1{};
};
struct OriginalSnowBirth {
    SnowVector positionCm{},velocityCoefficient{};
    std::array<uint8_t,4> colour{}; //371600 initializes every retained colour to zero.
    float seed=0; //Original3710D0: zero inactive; float1..2 active.
};
struct OriginalSnowParticle {
    SnowVector positionCm{};
    float halfExtentCm=0,scaledAge=0,scaledLifetime=0;
    std::array<uint8_t,4> colourGs{}; //GS modulation/alpha scale128, not255.
};
OriginalSnowParticleKernel originalSnowParticleKernel(const OriginalSnowParticleProfile&); //36CBF8/36CE00/36CE28/370058
// Plain native equations from particle VU program439A40, entryA00. Caller
// supplies original fractional birth interpolation and per-group LFSR state.
std::optional<OriginalSnowParticle> originalSnowParticle(const OriginalSnowParticleKernel&,
    const OriginalSnowBirth& newest,const OriginalSnowBirth& older,float fraction,float scaledAge,uint32_t& lfsr);
class OriginalSnowParticles {
    OriginalSnowParticleKernel kernel;
    std::vector<OriginalSnowBirth> births;
    unsigned cursor=0;
    SnowVector lastVelocity{};
    SnowColour lastColour{};
public:
    explicit OriginalSnowParticles(const OriginalSnowParticleProfile& profile={}):kernel(originalSnowParticleKernel(profile)),births(kernel.birthCapacity){}
    void emit(const OriginalSnowEmission&,OriginalRandomState& visualRandom,bool enabled=true); //3717C0/3710D0, separate4FF018 RNG
    std::vector<OriginalSnowParticle> particles()const;
    const OriginalSnowParticleKernel& parameters()const{return kernel;}
    const std::vector<OriginalSnowBirth>& birthHistory()const{return births;}
    unsigned nextBirthSlot()const{return cursor;}
};
}
