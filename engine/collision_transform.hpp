#pragma once
#include "body_collision.hpp"
namespace ssx::collision_transform {
using Matrix=std::array<float,16>;
using terrain_original::Vector;
inline constexpr Matrix identity{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
inline Vector apply(const Matrix& m,Vector p,float w=1) {
    using namespace terrain_original;Vector result;
    for(unsigned k=0;k<3;++k)result[k]=add(add(add(mul(m[k],p[0]),mul(m[4+k],p[1])),mul(m[8+k],p[2])),mul(m[12+k],w));
    return result;
}
inline Matrix scaledNode(const Matrix& local,const Matrix& parent,float scale) {
    using namespace terrain_original;Rounding rounding;auto node=local;Matrix result;
    for(unsigned k=0;k<3;++k)node[12+k]=mul(node[12+k],scale);
    for(unsigned i=0;i<4;++i)for(unsigned j=0;j<4;++j)
        result[i*4+j]=add(add(add(mul(parent[j],node[i*4]),mul(parent[4+j],node[i*4+1])),mul(parent[8+j],node[i*4+2])),mul(parent[12+j],node[i*4+3]));
    return result;
}
// Original 0x334D98..0x334E58 transposes the rotation and transforms negative
// translation. Uniform scale is applied separately to query centers/radii.
inline Matrix inverseRigid(const Matrix& m) {
    using namespace terrain_original;Rounding rounding;Matrix result=m;
    for(unsigned i=0;i<3;++i)for(unsigned j=0;j<3;++j)result[i*4+j]=m[j*4+i];
    auto t=apply(result,{-m[12],-m[13],-m[14]},0);
    for(unsigned k=0;k<3;++k)result[12+k]=t[k];result[15]=1;return result;
}
inline BodyCollisionVolume toLocal(const BodyCollisionVolume& source,const Matrix& inverse,float scale) {
    using namespace terrain_original;Rounding rounding;auto result=source;float reciprocal=collision_scalar::divide(1,scale);
    auto point=[&](Vector p){auto q=apply(inverse,p);for(auto& x:q)x=mul(x,reciprocal);return q;};
    result.broadCenterCm=point(source.broadCenterCm);result.broadRadiusCm=mul(source.broadRadiusCm,reciprocal);
    for(unsigned i=0;i<source.count;++i){result.spheres[i].centerCm=point(source.spheres[i].centerCm);result.spheres[i].radiusCm=mul(source.spheres[i].radiusCm,reciprocal);}
    return result;
}
inline Vector toWorldPoint(Vector p,const Matrix& matrix,float scale) {
    using namespace terrain_original;Rounding rounding;for(auto& x:p)x=mul(x,scale);return apply(matrix,p);
}
}
