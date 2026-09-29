#pragma once
// Presentation fast mode (web/quality.js low tier; docs/sim-performance.md "Phase 3"). Off by default: every core export
// and every test is exact. When a rider context has set_presentation_fast(1), two pure presentation outputs are
// computed with plain binary32 arithmetic (nearest rounding) instead of the EE toward-zero emulation:
//   * the skin palette (310120 pose matrix, 3106CC bind product, 386BD0 weighted sum) that rider-skinning.js uploads;
//   * the snow sprites (particle VU program 439A40) that snow-renderer.js draws.
// Nothing in the simulation reads either: they are built on demand from state the tick has finished writing (the
// cached pose, the emitters' birth rings), draw no random numbers (the per-birth LFSR is local and integer) and write
// only their own output buffers. The same formulas in the same order; results differ from the exact ones by rounding
// (an ulp or so per operation: well below a pixel).
#include "../engine/snow_particles.hpp"
#include "../engine/skin_palette.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <stdexcept>
#include <vector>
namespace presentation_fast {
// originalPoseMatrices(...).scaled then originalSkinPoseMatrix(pose, bind), plain arithmetic.
inline std::array<float,16> skinMatrix(const std::array<float,4>& position,const std::array<float,4>& q,const std::array<float,4>& scale,const std::array<float,16>& bind){
 float twice[3],square[3],weighted[3];
 for(unsigned i=0;i<3;++i){twice[i]=q[i]+q[i];square[i]=twice[i]*q[i];weighted[i]=twice[i]*q[3];}
 const float yz=twice[1]*q[2],zx=twice[2]*q[0],xy=twice[0]*q[1];
 const std::array<float,16> rotation{
  1.f-square[1]-square[2],xy+weighted[2],zx-weighted[1],0,
  xy-weighted[2],1.f-square[2]-square[0],yz+weighted[0],0,
  zx+weighted[1],yz-weighted[0],1.f-square[0]-square[1],0,
  position[0],position[1],position[2],position[3]};
 std::array<float,16> pose,out;
 for(unsigned col=0;col<4;++col)for(unsigned row=0;row<4;++row)pose[col*4+row]=rotation[col*4+row]*scale[col];
 for(unsigned col=0;col<4;++col)for(unsigned row=0;row<4;++row){float value=pose[row]*bind[col*4];for(unsigned k=1;k<4;++k)value+=pose[k*4+row]*bind[col*4+k];out[col*4+row]=value;}
 return out;
}
// originalSkinPalette, plain arithmetic.
inline void skinPalette(const std::vector<std::array<float,16>>& bones,const std::vector<ssx::OriginalSkinWeights>& groups,std::vector<ssx::OriginalSkinMatrix>& out){
 constexpr float percent=std::bit_cast<float>(0x3c23d70au);
 out.resize(groups.size());
 for(size_t g=0;g<groups.size();++g){
  const auto& group=groups[g];if(group.empty())throw std::runtime_error("Empty original skin weight group");
  auto& matrix=out[g];
  for(unsigned i=0;i<group.size();++i){const auto& influence=group[i];if(influence.bone>=bones.size())throw std::runtime_error("Skin bone outside palette");
   const float weight=float(influence.weight)*percent;const auto& bone=bones[influence.bone];
   for(unsigned lane=0;lane<16;++lane){const float value=bone[lane]*weight;matrix[lane]=i?matrix[lane]+value:value;}
  }
 }
}
// OriginalSnowParticles::particles() + the snow buffer layout of web/animation_bridge.cpp (position xyz, half extent,
// colour rgba/128; sprites with zero alpha skipped), plain arithmetic.
inline void snowSprites(const ssx::OriginalSnowParticles& emitter,std::vector<float>& out){
 const auto& k=emitter.parameters();const auto& births=emitter.birthHistory();const unsigned cursor=emitter.nextBirthSlot();
 if(births.empty())return;
 auto next=[](uint32_t& state){state=(((state<<1)^((state>>4)&1)^((state>>22)&1))&0x7fffff)|0x3f800000;return std::bit_cast<float>(state);};
 const float invCount=1.f/float(k.particlesPerBirth);float age=0;
 for(unsigned group=0;group<births.size();++group){
  if(group%43==0){const float begin=float((group/43)*43)*float(k.particlesPerBirth);age=begin*k.ageStep;}
  const auto& a=births[(cursor+1+group)%births.size()];const auto& b=births[(cursor+1+std::min(group+1,unsigned(births.size()-1)))%births.size()];
  if(a.seed<1){age=age+k.ageStep*float(k.particlesPerBirth);continue;}
  uint32_t random=(std::bit_cast<uint32_t>(a.seed)&0x7fffff)|0x3f800000;float fraction=0;
  for(unsigned n=0;n<k.particlesPerBirth;++n){
   const float rPosition0=next(random),rPosition1=next(random),rVelocity0=next(random),rVelocity1=next(random),rVelocity2=next(random),rColour0=next(random),rColour1=next(random),rSize=next(random),rLife=next(random);
   const float complement=1.f-fraction,lifetime=k.lifeBase+k.lifeRange*rLife;
   if(age<lifetime){
    ssx::SnowVector velocity,position;
    for(unsigned i=0;i<3;++i){
     velocity[i]=a.velocityCoefficient[i]*complement+b.velocityCoefficient[i]*fraction;
     velocity[i]=velocity[i]+k.velocityBase[i];velocity[i]=velocity[i]+k.velocityRanges[0][i]*rVelocity0;velocity[i]=velocity[i]+k.velocityRanges[1][i]*rVelocity1;velocity[i]=velocity[i]+k.velocityRanges[2][i]*rVelocity2;
     position[i]=a.positionCm[i]*complement+b.positionCm[i]*fraction;
     position[i]=position[i]+k.positionBase[i];position[i]=position[i]+k.positionRange0[i]*rPosition0;position[i]=position[i]+k.positionRange1[i]*rPosition1;
    }
    const float t=std::min(age,2.700000047683716f),squared=t*t;const float polynomial=t*-.7300000190734863f+squared*.11299999803304672f;
    for(unsigned i=0;i<3;++i){position[i]=k.force[i]*age+position[i];position[i]=position[i]+(k.force[i]-velocity[i])*polynomial;}
    float size=k.sizeDelta*age;size=size*(1.f/lifetime);size=size+k.sizeBase;size=size+k.sizeRange*rSize;
    std::array<uint8_t,4> colourGs;
    for(unsigned i=0;i<4;++i){
     float tint=float(a.colour[i])*complement;tint=tint+float(b.colour[i])*fraction;tint=tint*.003921568859368563f;
     float value=k.colourBase[i]*tint;value=value+k.colourRange0[i]*rColour0;value=value+k.colourSlope[i]*age;value=value+k.colourRange1[i]*rColour1;
     const float colour=std::max(i<3?std::min(value,128.f):value,0.f);colourGs[i]=uint8_t(std::clamp(int(colour),0,255));
    }
    if(colourGs[3]){for(float v:position)out.push_back(v);out.push_back(std::abs(size));for(auto c:colourGs)out.push_back(float(c)/128.f);}
   }
   age=age+k.ageStep;fraction=fraction+invCount;
  }
 }
}
}
