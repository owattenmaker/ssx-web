#pragma once
#include "collision_transform.hpp"
#include <optional>
namespace ssx::ray_instance {
using terrain_original::Vector;
struct Ray {Vector origin{},delta{};};
struct Hit {float fraction=0;Vector point{},normal{};};
struct BoxHits {unsigned count=0;std::array<Hit,2> hits{};};
// Original32E688 sphere-tree ray callback is a literal zero-return stub. Sphere
// trees participate in the separate body query but supply no ray contacts.
inline constexpr unsigned sphereTreeContactCount(){return 0;}
inline Ray transform(Ray original,const collision_transform::Matrix& inverse,float inverseScale){
    terrain_original::Rounding round;
    auto p=collision_transform::apply(inverse,original.origin,1),d=collision_transform::apply(inverse,original.delta,0);
    for(unsigned i=0;i<3;i++){p[i]=terrain_original::mul(p[i],inverseScale);d[i]=terrain_original::mul(d[i],inverseScale);}
    return {p,d};
}
// Original32E288 uses the reconstructed origin+delta endpoint and inclusive
// per-axis overlap, independently of the constructor's raw endpoint bounds.
inline bool overlaps(Ray ray,Vector low,Vector high){
    using namespace terrain_original;Rounding round;
    for(unsigned i=0;i<3;i++){float end=add(ray.origin[i],ray.delta[i]);if(ray.origin[i]<low[i]&&end<low[i])return false;if(high[i]<ray.origin[i]&&high[i]<end)return false;}return true;
}
inline BoxHits box(Ray ray,Vector low,Vector high){
    using namespace terrain_original;Rounding round;BoxHits result;
    float near=-1,far=2;int nearFace=-1,farFace=-1;
    for(unsigned i=0;i<3;i++){
        if(!(std::abs(ray.delta[i])>1.000000013351432e-10f))continue;
        float a=originalScalarDivide(originalScalarSubtract(low[i],ray.origin[i]),ray.delta[i]);
        float b=originalScalarDivide(originalScalarSubtract(high[i],ray.origin[i]),ray.delta[i]);int face=int(i*2);
        if(b<a){std::swap(a,b);++face;}
        if(near<a){near=a;nearFace=face;}if(b<far){far=b;farFace=face^1;}
        if(far<near||far<0||near>1)return result;
    }
    auto emit=[&](float fraction,int face){Hit h;h.fraction=fraction;h.normal[unsigned(face)/2]=face&1?1.f:-1.f;for(unsigned i=0;i<3;i++)h.point[i]=add(ray.origin[i],mul(ray.delta[i],fraction));result.hits[result.count++]=h;};
    if(near>=0)emit(near,nearFace);if(far<1)emit(far,farFace);return result;
}
// Original32E4D0 receives the authored face normal; 32E5E8 flips it when its
// dot product with the ray is negative. Do not regenerate mesh normals here.
inline std::optional<Hit> triangle(Ray ray,Vector a,Vector b,Vector c,Vector normal,bool doubleSided=false){
    using namespace terrain_original;Rounding round;float denominator=dot(ray.delta,normal);
    if(doubleSided&&denominator<0){for(float& x:normal)x=mul(x,-1.f);denominator=dot(ray.delta,normal);}
    if(std::abs(denominator)<1.000000013351432e-10f)return {};
    float fraction=originalScalarDivide(dot(difference(a,ray.origin),normal),denominator);if(fraction<0||fraction>1)return {};
    Hit result;result.fraction=fraction;result.normal=normal;for(unsigned i=0;i<3;i++)result.point[i]=add(ray.origin[i],mul(ray.delta[i],fraction));
    if(!pointInTriangle(a,b,c,result.point))return {};return result;
}
}
