#pragma once
#include "terrain_contact_math.hpp"
namespace ssx {
using OriginalIrradianceCoefficients=std::array<std::array<float,4>,10>;
//389590: accumulate ten coefficient rows. Modulation is A,R,G,B; bank
//coefficient lanes are R,G,B,A. Weights are computed before the row loop.
inline void originalIrradianceAccumulate(OriginalIrradianceCoefficients& destination,
 const OriginalIrradianceCoefficients& source,const std::array<float,4>& modulation,float weight){
 terrain_original::Rounding rounding;
 std::array<float,4> scales={terrain_original::mul(weight,modulation[1]),terrain_original::mul(weight,modulation[2]),terrain_original::mul(weight,modulation[3]),terrain_original::mul(weight,modulation[0])};
 for(unsigned row=0;row<10;++row)for(unsigned lane=0;lane<4;++lane)destination[row][lane]=originalScalarAdd(destination[row][lane],terrain_original::mul(source[row][lane],scales[lane]));
}
}
namespace ssx {
//389308: add a directional RGB contribution. Fourth lanes remain untouched.
//Rows: constant,x²,y²,z²,xy,xz,yz,x,y,z. Direction normalization is caller-owned.
inline void originalIrradianceDirectional(OriginalIrradianceCoefficients& out,
 const std::array<float,3>& direction,const std::array<float,3>& color,float weight){
 terrain_original::Rounding rounding;using terrain_original::mul;
 auto f=[](uint32_t bits){return std::bit_cast<float>(bits);};
 const float x=direction[0],y=direction[1],z=direction[2];
 const float difference=originalScalarSubtract(mul(x,x),mul(y,y));
 const float zonal=originalScalarSubtract(mul(z,mul(z,3.f)),1.f);
 for(unsigned channel=0;channel<3;++channel){
  float c=mul(color[channel],weight),constant=mul(c,f(0x3e906ec1));
  float linear=mul(c,f(0x3efa2a2c));float lx=mul(linear,x),ly=mul(linear,y),lz=mul(linear,z);
  float cross=mul(c,f(0x3f8bd89d));float cx=mul(cross,x),cy=mul(cross,y);
  float xz=mul(cx,z),yz=mul(cy,z),xy=mul(cx,y);
  float q=mul(mul(c,f(0x3ea17b0f)),zonal);
  float d=mul(mul(c,f(0x3f0bd89d)),difference);
  float base=originalScalarSubtract(mul(constant,f(0x3f62dfc6)),mul(q,f(0x3e7da72a)));
  float diagonal=mul(d,f(0x3edbab86));
  out[0][channel]=originalScalarAdd(out[0][channel],base);
  out[1][channel]=originalScalarAdd(out[1][channel],diagonal);
  out[2][channel]=originalScalarSubtract(out[2][channel],diagonal);
  out[3][channel]=originalScalarAdd(out[3][channel],mul(q,f(0x3f3e3d71)));
  out[4][channel]=originalScalarAdd(out[4][channel],mul(xy,f(0x3f5bab86)));
  out[5][channel]=originalScalarAdd(out[5][channel],mul(xz,f(0x3f5bab86)));
  out[6][channel]=originalScalarAdd(out[6][channel],mul(yz,f(0x3f5bab86)));
  out[7][channel]=originalScalarAdd(out[7][channel],mul(lx,f(0x3f82fc69)));
  out[8][channel]=originalScalarAdd(out[8][channel],mul(ly,f(0x3f82fc69)));
  out[9][channel]=originalScalarAdd(out[9][channel],mul(lz,f(0x3f82fc69)));
 }
}
}
namespace ssx {
//389730 uses VU ADD, not the EE scalar ADD guard-bit behavior. The source
//stages all ten rows before copying, permitting destination/input aliasing.
inline OriginalIrradianceCoefficients originalIrradianceSum(const OriginalIrradianceCoefficients& a,const OriginalIrradianceCoefficients& b){
 terrain_original::Rounding rounding;OriginalIrradianceCoefficients result;
 for(unsigned row=0;row<10;++row)for(unsigned lane=0;lane<4;++lane)result[row][lane]=terrain_original::add(a[row][lane],b[row][lane]);return result;
}
//3897A0 copy/scale and389810 in-place scale use VU multiplication.
inline OriginalIrradianceCoefficients originalIrradianceScale(const OriginalIrradianceCoefficients& source,float weight){
 terrain_original::Rounding rounding;OriginalIrradianceCoefficients result;
 for(unsigned row=0;row<10;++row)for(unsigned lane=0;lane<4;++lane)result[row][lane]=terrain_original::mul(source[row][lane],weight);return result;
}
}
