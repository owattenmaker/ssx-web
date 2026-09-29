#include "rider_pose_motion.hpp"
#include "orientation_motion.hpp"
#include "original_float.hpp"
#include "terrain_contact_math.hpp"
#include <algorithm>
#include <bit>
#include <cfenv>
#include <cmath>
#include <stdexcept>
#pragma STDC FENV_ACCESS ON
namespace ssx {namespace {
using Round=OriginalRounding;
using V=AnimationVector;
float dot(V a,V b){float x=terrain_original::mul(a[0],b[0]),y=terrain_original::mul(a[1],b[1]),z=terrain_original::mul(a[2],b[2]);return terrain_original::add((terrain_original::add(x,y)),z);}
V cross(V a,V b){return {terrain_original::sub(terrain_original::mul(a[1],b[2]),terrain_original::mul(b[1],a[2])),terrain_original::sub(terrain_original::mul(a[2],b[0]),terrain_original::mul(b[2],a[0])),terrain_original::sub(terrain_original::mul(a[0],b[1]),terrain_original::mul(b[0],a[1]))};}
V subtract(V a,V b){for(unsigned k=0;k<3;++k)a[k]=terrain_original::sub(a[k],b[k]);return a;}
V scaled(V a,float s){for(float&x:a)x=terrain_original::mul(x,s);return a;}
V normalized(V a){return scaled(a,terrain_original::div(1.f,terrain_original::sqrt(dot(a,a))));}
AnimationQuaternion product(AnimationQuaternion a,AnimationQuaternion b){return originalAnimationCompose({{},a},{{},b}).rotation;}
AnimationQuaternion axisQuaternion(V axis,float halfAngle){auto sc=originalSinCos(halfAngle);return {terrain_original::mul(axis[0],sc[0]),terrain_original::mul(axis[1],sc[0]),terrain_original::mul(axis[2],sc[0]),sc[1]};}
// Original31BF60, not the31BE50 sine/cosine pair: odd quadrants use
// the authored cosine polynomial instead of sqrt(1-sin²).
float legBlendSin(float x){
    auto f=[](uint32_t bits){return std::bit_cast<float>(bits);};
    float reduced=terrain_original::mul(x,f(0x3f22f983));reduced=x<0?originalScalarSubtract(reduced,.5f):originalScalarAdd(reduced,.5f);
    int quadrant=int(reduced);x=originalScalarSubtract(x,terrain_original::mul(float(quadrant),f(0x3fc90fdb)));float square=terrain_original::mul(x,x),result;
    if(quadrant&1){result=originalScalarAdd(terrain_original::mul(square,f(0x37d00d03)),f(0xbab60b62));result=originalScalarAdd(terrain_original::mul(result,square),f(0x3d2aaaab));result=originalScalarAdd(terrain_original::mul(result,square),-.5f);result=originalScalarAdd(terrain_original::mul(result,square),1);}
    else{result=originalScalarAdd(terrain_original::mul(square,f(0x3638ef1f)),f(0xb9500d03));result=originalScalarAdd(terrain_original::mul(result,square),f(0x3c088889));result=originalScalarAdd(terrain_original::mul(result,square),f(0xbe2aaaab));result=terrain_original::mul(result,square);result=originalScalarAdd(terrain_original::mul(result,x),x);}
    return quadrant&2?-result:result;
}
// Original31BCB0 shortest-hemisphere quaternion interpolation. No normalization.
AnimationQuaternion legBlendRotation(AnimationQuaternion current,AnimationQuaternion target,float weight){
    float cosine=originalScalarAdd(terrain_original::mul(current[0],target[0]),terrain_original::mul(current[1],target[1]));cosine=originalScalarAdd(cosine,terrain_original::mul(current[2],target[2]));cosine=originalScalarAdd(cosine,terrain_original::mul(current[3],target[3]));
    if(cosine<0){for(auto&x:target)x=-x;cosine=-cosine;}
    float a,b;
    if(.0010000000474974513f<originalScalarSubtract(1,cosine)){
        float angle=originalScalarSubtract(1.5707963705062866f,originalAsin(cosine));
        float sine=legBlendSin(angle);a=originalScalarDivide(legBlendSin(terrain_original::mul(originalScalarSubtract(1,weight),angle)),sine);b=originalScalarDivide(legBlendSin(terrain_original::mul(weight,angle)),sine);
    }else{a=originalScalarSubtract(1,weight);b=weight;}
    AnimationQuaternion result;for(unsigned k=0;k<4;++k)result[k]=originalScalarAdd(terrain_original::mul(a,current[k]),terrain_original::mul(b,target[k]));return result;
}
}
void originalRiderPoseContact(std::vector<AnimationTransform>& world,const std::vector<AnimationTransform>& local,V scale,const RiderPoseContact&p){
    Round round;auto&board=world.at(p.boardRoot);
    if(p.boardAlignment!=0){const auto&q=board.rotation;float yy=terrain_original::mul(q[1],q[1]),zz=terrain_original::mul(q[2],q[2]),xz=terrain_original::mul(q[0],q[2]),wy=terrain_original::mul(q[3],q[1]),xy=terrain_original::mul(q[0],q[1]),wz=terrain_original::mul(q[3],q[2]);float yz=originalScalarAdd(yy,zz);yz=originalScalarAdd(yz,yz);float b=originalScalarAdd(xy,wz);b=originalScalarAdd(b,b);float c=originalScalarSubtract(xz,wy);c=originalScalarAdd(c,c);V direction{originalScalarSubtract(1.f,yz),b,c};
        V axis=cross(p.boardDirection,direction);float magnitude=terrain_original::sqrt(dot(axis,axis));
        if(magnitude>1e-8f){axis=scaled(axis,terrain_original::div(1.f,magnitude));float angle=originalAsin(std::min(magnitude,1.f));if(dot(p.boardDirection,direction)<0)angle=originalScalarSubtract(angle,1.5707963705062866f);else angle=originalScalarSubtract(1.5707963705062866f,angle);angle=terrain_original::mul(angle,p.boardAlignment);angle=terrain_original::mul(angle,.5f);board.rotation=product(axisQuaternion(axis,angle),board.rotation);}
        if(p.boardLiftCm>0)for(unsigned k=0;k<3;++k){float lift=terrain_original::mul(p.normal[k],p.boardLiftCm);lift=terrain_original::mul(lift,p.boardAlignment);board.position[k]=terrain_original::add(board.position[k],lift);}
        world.at(p.boardChild)=originalAnimationCompose(board,local.at(p.boardChild),scale);
    }
    if(p.legWeight==0)return;
    for(const auto&leg:p.legs){auto target=originalAnimationCompose(world.at(p.boardChild),leg.boardLocalFoot);originalRiderLegContact(world,local,scale,leg,target,p.legWeight);}
}
void originalRiderLegContact(std::vector<AnimationTransform>& world,const std::vector<AnimationTransform>& local,V scale,const RiderLegBinding&leg,AnimationTransform target,float weight){
    Round round;auto&foot=world.at(leg.foot);
    if(weight!=1){for(unsigned k=0;k<3;++k){float delta=terrain_original::sub(target.position[k],foot.position[k]);delta=terrain_original::mul(delta,weight);target.position[k]=terrain_original::add(foot.position[k],delta);}target.rotation=legBlendRotation(foot.rotation,target.rotation,weight);}
    foot=target;auto&upper=world.at(leg.thigh);auto&mid=world.at(leg.shin);
        float a=terrain_original::mul(local.at(leg.shin).position[0],scale[0]),b=terrain_original::mul(local.at(leg.foot).position[0],scale[0]);V delta=subtract(target.position,upper.position);float aa=terrain_original::mul(a,a),bb=terrain_original::mul(b,b),ab=terrain_original::mul(a,b);float cosine=dot(delta,delta);cosine=originalScalarSubtract(cosine,aa);cosine=originalScalarSubtract(cosine,bb);cosine=terrain_original::mul(cosine,.5f);cosine=originalScalarDivide(cosine,ab);cosine=std::clamp(cosine,-1.f,1.f);
        float bend=originalScalarSubtract(originalAsin(cosine),1.5707963705062866f);bend=terrain_original::mul(bend,.5f);auto kneeQ=axisQuaternion({0,0,1},bend);
        AnimationTransform kneeLocal{{a,0,0},kneeQ};auto predictedKnee=originalAnimationCompose(upper,kneeLocal);auto predictedFoot=originalAnimationCompose(predictedKnee,{{b,0,0},{0,0,0,1}});
        auto first=normalized(subtract(predictedFoot.position,upper.position)),second=normalized(delta);auto axis=cross(first,second);float magnitude=terrain_original::sqrt(dot(axis,axis));
        if(magnitude>1e-8f){axis=scaled(axis,terrain_original::div(1.f,magnitude));float angle=terrain_original::mul(originalAsin(magnitude),.5f);upper.rotation=product(axisQuaternion(axis,angle),upper.rotation);}
        mid=originalAnimationCompose(upper,kneeLocal);
}
}
namespace ssx {namespace {
V rotated(AnimationQuaternion q,V p){V axis{q[0],q[1],q[2]};auto a=cross(axis,p),b=cross(axis,a);V result;for(unsigned k=0;k<3;++k){float term=terrain_original::mul(a[k],q[3]),r=terrain_original::add(p[k],term);r=terrain_original::add(r,term);r=terrain_original::add(r,b[k]);result[k]=terrain_original::add(r,b[k]);}return result;}
V summed(V a,V b){for(unsigned k=0;k<3;++k)a[k]=terrain_original::add(a[k],b[k]);return a;}
}
AnimationTransform originalRiderRootPresentation(AnimationTransform root,V boardLocalPosition,V scale,const RiderRootPresentation&p,AnimationTransform* secondaryRoot){
    Round round;
    float twiceBrake=originalScalarAdd(std::abs(p.brake),std::abs(p.brake));
    float lean=originalScalarSubtract(std::abs(p.extraLean),twiceBrake);if(lean<0)lean=0;else if(p.extraLean<0)lean=-lean;
    float turn=originalScalarSubtract(std::abs(p.turn),twiceBrake);if(turn<0)turn=0;else if(p.turn<0)turn=-turn;
    if(turn!=0||lean!=0){
        V pivot;for(unsigned k=0;k<3;++k)pivot[k]=terrain_original::mul(boardLocalPosition[k],scale[k]);
        root.position=summed(root.position,rotated(root.rotation,pivot));
        float angle=terrain_original::mul(lean,0.523598849773407f);angle=terrain_original::mul(angle,.5f);root.rotation=product(root.rotation,axisQuaternion({1,0,0},angle));
        angle=terrain_original::mul(turn,0.7853982448577881f);angle=terrain_original::mul(angle,.5f);root.rotation=product(root.rotation,axisQuaternion({0,1,0},angle));
        root.position=summed(root.position,rotated(root.rotation,scaled(pivot,-1.f)));
    }
    if(p.roll!=0){float angle=terrain_original::mul(p.roll,0.7853982448577881f);angle=terrain_original::mul(angle,.5f);root.rotation=product(root.rotation,axisQuaternion({1,0,0},angle));}
    float shift=terrain_original::mul(p.turn,45.f);shift=terrain_original::mul(scale[0],shift);root.position=summed(root.position,scaled(p.lateral,shift));
    if(secondaryRoot)*secondaryRoot=root;
    if(p.controlState!=13&&p.liftCm!=0)root.position=summed(root.position,rotated(root.rotation,scaled(V{0,0,1},p.liftCm)));
    return root;
}
}

namespace ssx {
OriginalCollisionFrame originalRiderCollisionFrame(const AnimationTransform&root){
    Round round;const auto&q=root.rotation;float dx=terrain_original::add(q[0],q[0]),dy=terrain_original::add(q[1],q[1]),dz=terrain_original::add(q[2],q[2]);float xx=terrain_original::mul(dx,q[0]),yy=terrain_original::mul(dy,q[1]),zz=terrain_original::mul(dz,q[2]),wx=terrain_original::mul(dx,q[3]),wy=terrain_original::mul(dy,q[3]),wz=terrain_original::mul(dz,q[3]);float yz=terrain_original::mul(dy,q[2]),zx=terrain_original::mul(dz,q[0]),xy=terrain_original::mul(dx,q[1]);yz=terrain_original::add(0.f,yz);zx=terrain_original::add(0.f,zx);xy=terrain_original::add(0.f,xy);
    OriginalCollisionFrame frame;float diagonal=terrain_original::sub(1.f,yy);frame.right={terrain_original::sub(diagonal,zz),terrain_original::add(xy,wz),terrain_original::sub(zx,wy)};diagonal=terrain_original::sub(1.f,zz);frame.forward={terrain_original::sub(xy,wz),terrain_original::sub(diagonal,xx),terrain_original::add(yz,wx)};diagonal=terrain_original::sub(1.f,xx);frame.up={terrain_original::add(zx,wy),terrain_original::sub(yz,wx),terrain_original::sub(diagonal,yy)};frame.origin=root.position;return frame;
}
}
