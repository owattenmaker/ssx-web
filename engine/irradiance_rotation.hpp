#pragma once
#include "irradiance_angles.hpp"
namespace ssx {
namespace irradiance_rotation {
using Matrix=std::array<float,16>;
inline Matrix multiply(const Matrix&a,const Matrix&b){Matrix result;for(unsigned col=0;col<4;++col)for(unsigned row=0;row<4;++row){float v=terrain_original::mul(a[row],b[col*4]);for(unsigned k=1;k<4;++k)v=terrain_original::add(v,terrain_original::mul(a[k*4+row],b[col*4+k]));result[col*4+row]=v;}return result;}
inline Matrix axis(float angle,std::array<float,3> axis,std::array<float,4> translation){
 using terrain_original::mul;const auto sc=collision_scalar::sincos(angle);float s=sc[0],c=sc[1],t=originalScalarSubtract(1.f,c),x=axis[0],y=axis[1],z=axis[2];float tz=mul(t,z),tx=mul(t,x),ty=mul(t,y),sz=mul(s,z),sx=mul(s,x),sy=mul(s,y);
 Matrix m{};m[0]=originalScalarAdd(mul(tx,x),c);m[1]=originalScalarSubtract(mul(ty,x),sz);m[2]=originalScalarAdd(mul(tz,x),sy);m[4]=originalScalarAdd(mul(tx,y),sz);m[5]=originalScalarAdd(mul(ty,y),c);m[6]=originalScalarSubtract(mul(tz,y),sx);m[8]=originalScalarSubtract(mul(tx,z),sy);m[9]=originalScalarAdd(mul(ty,z),sx);m[10]=originalScalarAdd(mul(tz,z),c);for(unsigned k=0;k<4;++k)m[12+k]=translation[k];return m;
}
}
//389F9C..38A4D0, packed 3x3 passed to389840. Includes the source axis conversion.
inline std::array<float,9> originalIrradianceRimRotation(float pitch,float yaw,const std::array<float,4>& eye){
 terrain_original::Rounding rounding;using namespace irradiance_rotation;
 constexpr Matrix identity{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};Matrix initial=identity;
 for(unsigned row=0;row<4;++row){float v=terrain_original::mul(identity[row],eye[0]);for(unsigned k=1;k<4;++k)v=terrain_original::add(v,terrain_original::mul(identity[k*4+row],eye[k]));initial[12+row]=v;}
 for(auto rotation:std::array<std::pair<float,std::array<float,3>>,2>{{{-pitch,{0,1,0}},{-yaw,{0,0,1}}}}){
  std::array<float,4> translation;for(unsigned k=0;k<4;++k){translation[k]=initial[12+k];initial[12+k]=k==3?1.f:0.f;}initial=multiply(axis(rotation.first,rotation.second,translation),initial);
 }
 Matrix transposed;for(unsigned c=0;c<4;++c)for(unsigned r=0;r<4;++r)transposed[c*4+r]=initial[r*4+c];
 constexpr Matrix conversion{0,0,1,0,-1,0,0,0,0,1,0,0,0,0,0,1};auto result=multiply(conversion,transposed);
 return {result[0],result[1],result[2],result[4],result[5],result[6],result[8],result[9],result[10]};
}
}
