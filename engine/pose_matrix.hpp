#pragma once
#include "terrain_contact_math.hpp"
namespace ssx {
struct OriginalPoseMatrices {std::array<float,16> unscaled{},scaled{};};
//310120: posed quaternion/position -> unscaled matrix, then per-column geometry
//scale. Quaternion is used directly, without host normalization.
inline OriginalPoseMatrices originalPoseMatrices(const std::array<float,4>& position,const std::array<float,4>& q,const std::array<float,4>& scale){
 terrain_original::Rounding rounding;using namespace terrain_original;
 std::array<float,3> twice,square,weighted;
 for(unsigned i=0;i<3;++i){twice[i]=add(q[i],q[i]);square[i]=mul(twice[i],q[i]);weighted[i]=mul(twice[i],q[3]);}
 const float yz=add(mul(twice[1],q[2]),mul(0.f,0.f)),zx=add(mul(twice[2],q[0]),mul(0.f,0.f)),xy=add(mul(twice[0],q[1]),mul(0.f,0.f));
 std::array<float,16> rotation{
  sub(sub(1.f,square[1]),mul(1.f,square[2])),add(add(0.f,xy),mul(1.f,weighted[2])),sub(add(0.f,zx),mul(1.f,weighted[1])),0,
  sub(add(0.f,xy),mul(1.f,weighted[2])),sub(sub(1.f,square[2]),mul(1.f,square[0])),add(add(0.f,yz),mul(1.f,weighted[0])),0,
  add(add(0.f,zx),mul(1.f,weighted[1])),sub(add(0.f,yz),mul(1.f,weighted[0])),sub(sub(1.f,square[0]),mul(1.f,square[1])),0,
  0,0,0,1};

 OriginalPoseMatrices result;for(unsigned i=0;i<4;++i)rotation[12+i]=position[i];result.unscaled=rotation;
 for(unsigned col=0;col<4;++col)for(unsigned row=0;row<4;++row)result.scaled[col*4+row]=mul(rotation[col*4+row],scale[col]);
 return result;
}
//3106CC..310778: multiply each scaled pose by its supplied bind-space matrix.
inline std::array<float,16> originalSkinPoseMatrix(const std::array<float,16>& pose,const std::array<float,16>& bind){
 terrain_original::Rounding rounding;using namespace terrain_original;std::array<float,16> out;
 for(unsigned col=0;col<4;++col)for(unsigned row=0;row<4;++row){float value=mul(pose[row],bind[col*4]);for(unsigned k=1;k<4;++k)value=add(value,mul(pose[k*4+row],bind[col*4+k]));out[col*4+row]=value;}return out;
}

}
