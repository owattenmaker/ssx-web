#pragma once
#include "irradiance_rotation.hpp"
namespace ssx {
//31B7A8: source matrix-to-quaternion branch/order, including diagonal ties.
inline std::array<float,4> originalMatrixQuaternion(const std::array<float,16>& m){
 terrain_original::Rounding rounding;
 const auto A=originalScalarAdd,S=originalScalarSubtract,M=terrain_original::mul;
 std::array<float,4> q{};
 const float trace=A(A(m[0],m[5]),m[10]);
 if(trace>0){
  const float root=originalScalarSqrt(A(trace,1.f)),scale=originalScalarDivide(.5f,root);
  q={M(S(m[6],m[9]),scale),M(S(m[8],m[2]),scale),M(S(m[1],m[4]),scale),M(root,.5f)};
 }else{
  unsigned i=m[0]<m[5]?1:0;if(m[i*5]<m[10])i=2;
  const unsigned j=(i+1)%3,k=(j+1)%3;
  float root=originalScalarSqrt(A(S(S(m[i*5],m[j*5]),m[k*5]),1.f));
  q[i]=M(root,.5f);if(root!=0)root=originalScalarDivide(.5f,root);
  q[3]=M(S(m[j*4+k],m[k*4+j]),root);
  q[j]=M(A(m[i*4+j],m[j*4+i]),root);
  q[k]=M(A(m[i*4+k],m[k*4+i]),root);
 }
 return q;
}
struct OriginalCameraTransform {
 std::array<float,16> matrix{}; //166F90 temporary before31B748, source coordinates
 std::array<float,4> position{},quaternion{}; //camera+60/+70; conjugated output
};
//166F90: position -> negative pitch about Y -> negative yaw about Z ->31B748.
//This is the algorithm transform, not the renderer's final view matrix.
inline OriginalCameraTransform originalCameraTransform(std::array<float,4> eye,float yaw,float pitch){
 terrain_original::Rounding rounding;using namespace irradiance_rotation;
 constexpr Matrix identity{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
 eye[3]=1;Matrix matrix=identity;
 for(unsigned row=0;row<4;++row){float v=terrain_original::mul(identity[row],eye[0]);for(unsigned k=1;k<4;++k)v=terrain_original::add(v,terrain_original::mul(identity[k*4+row],eye[k]));matrix[12+row]=v;}
 for(auto rotation:std::array<std::pair<float,std::array<float,3>>,2>{{{-pitch,{0,1,0}},{-yaw,{0,0,1}}}}){
  std::array<float,4> translation;for(unsigned k=0;k<4;++k){translation[k]=matrix[12+k];matrix[12+k]=k==3?1.f:0.f;}
  matrix=multiply(axis(rotation.first,rotation.second,translation),matrix);
 }
 auto quaternion=originalMatrixQuaternion(matrix);for(unsigned i=0;i<3;++i)quaternion[i]=-quaternion[i];
 return {matrix,{matrix[12],matrix[13],matrix[14],matrix[15]},quaternion};
}
//395750: renderer Euler view setter. Argument order follows f12/f13/f14;
//caller angle mapping/stack ownership must be recovered before live use.
inline std::array<float,16> originalRendererEulerView(const std::array<float,4>& eye,float first,float second,float third){
 terrain_original::Rounding rounding;using namespace irradiance_rotation;
 Matrix matrix{1,0,0,0,0,0,1,0,0,1,0,0,0,0,0,1};
 for(auto rotation:std::array<std::pair<float,std::array<float,3>>,3>{{{-first,{1,0,0}},{-second,{0,0,1}},{-third,{0,1,0}}}})
  matrix=multiply(matrix,axis(rotation.first,rotation.second,{0,0,0,1}));
 std::array<float,4> negative;for(unsigned k=0;k<4;++k)negative[k]=terrain_original::mul(eye[k],-1.f);negative[3]=-negative[3];
 std::array<float,4> translation;
 for(unsigned row=0;row<4;++row){float v=terrain_original::mul(matrix[row],negative[0]);for(unsigned k=1;k<4;++k)v=terrain_original::add(v,terrain_original::mul(matrix[k*4+row],negative[k]));translation[row]=v;}
 for(unsigned k=0;k<4;++k)matrix[12+k]=translation[k];return matrix;
}

//15E968..15EAE8: outer camera quaternion -> source render axis conversion ->
//negative eye translation. Preserve VU term order, without normalizing q.
inline std::array<float,16> originalCameraRenderView(const std::array<float,4>& eye,const std::array<float,4>& q){
 terrain_original::Rounding rounding;using namespace terrain_original;
 std::array<float,3> twice,square,weighted;
 for(unsigned i=0;i<3;++i){twice[i]=add(q[i],q[i]);square[i]=mul(twice[i],q[i]);weighted[i]=mul(twice[i],q[3]);}
 const float yz=add(mul(twice[1],q[2]),mul(0.f,0.f)),zx=add(mul(twice[2],q[0]),mul(0.f,0.f)),xy=add(mul(twice[0],q[1]),mul(0.f,0.f));
 irradiance_rotation::Matrix rotation{
  sub(sub(1.f,square[1]),mul(1.f,square[2])),add(add(0.f,xy),mul(1.f,weighted[2])),sub(add(0.f,zx),mul(1.f,weighted[1])),0,
  sub(add(0.f,xy),mul(1.f,weighted[2])),sub(sub(1.f,square[2]),mul(1.f,square[0])),add(add(0.f,yz),mul(1.f,weighted[0])),0,
  add(add(0.f,zx),mul(1.f,weighted[1])),sub(add(0.f,yz),mul(1.f,weighted[0])),sub(sub(1.f,square[0]),mul(1.f,square[1])),0,
  0,0,0,1};
 constexpr irradiance_rotation::Matrix conversion{0,0,1,0,-1,0,0,0,0,1,0,0,0,0,0,1};
 auto view=irradiance_rotation::multiply(conversion,rotation);
 const std::array<float,4> negative{-eye[0],-eye[1],-eye[2],1};
 for(unsigned row=0;row<4;++row){float v=mul(view[row],negative[0]);for(unsigned k=1;k<4;++k)v=add(v,mul(view[k*4+row],negative[k]));view[12+row]=v;}
 return view;
}

}
