#include "../web/pose_space.hpp"
#include "../engine/rider_pose_motion.hpp"
#include "../engine/orientation_motion.hpp"
#include <random>
#include <iostream>
#include <cassert>
using namespace ssx;
int main(){
 std::mt19937 rng(731);std::uniform_real_distribution<float> random(-1,1);
 float maximumPositionError=0,maximumRotationError=0;
 std::vector<AnimationBone> bones(24);for(int i=1;i<22;i++)bones[i].parent=0;bones[22].part=bones[23].part=1;bones[23].parent=22;
 std::vector<AnimationTransform> local(24);local[22].position={14,-21,-70};local[23].position={3,0,2};
 for(int trial=0;trial<5000;++trial){
  auto q=originalRebuildOrientation({random(rng),random(rng),random(rng),random(rng)}).quaternion;
  AnimationTransform physical{{random(rng)*1000,random(rng)*1000,random(rng)*1000},q};
  RiderRootPresentation source;source.turn=random(rng);source.brake=random(rng)*.25f;
  source.extraLean=random(rng);source.roll=random(rng);source.liftCm=random(rng)*20;source.controlState=trial%14;
  source.lateral=originalOrientationBasis(q).right;
  RiderPoseContact contact;contact.normal=originalOrientationBasis(originalRebuildOrientation({random(rng),random(rng),random(rng),random(rng)}).quaternion).up;
  contact.boardDirection=contact.normal;contact.boardAlignment=(random(rng)+1)*.5f;contact.boardLiftCm=random(rng)*8;
  AnimationTransform board;auto body=originalRiderRootPresentation(physical,local[22].position,{1,1,1},source,&board);
  auto expected=originalAnimationWorldPose(bones,local,body,{1,1,1},{body,board});
  originalRiderPoseContact(expected,local,{1,1,1},contact);
  // Browser poses are relative to the presented root, including during flips.
  source.lateral=browserPoseDirection(q,source.lateral);
  contact.normal=browserPoseDirection(q,contact.normal);contact.boardDirection=browserPoseDirection(q,contact.boardDirection);
  body=originalRiderRootPresentation({},local[22].position,{1,1,1},source,&board);
  auto relative=originalAnimationWorldPose(bones,local,body,{1,1,1},{body,board});
  originalRiderPoseContact(relative,local,{1,1,1},contact);
  for(unsigned i=0;i<relative.size();++i){
   auto actual=originalAnimationCompose(physical,relative[i]);
   for(unsigned k=0;k<3;k++)maximumPositionError=std::max(maximumPositionError,std::abs(actual.position[k]-expected[i].position[k]));
   for(unsigned k=0;k<4;k++)maximumRotationError=std::max(maximumRotationError,std::abs(actual.rotation[k]-expected[i].rotation[k]));
  }
 }
 std::cout<<"5000 world/renderer pose comparisons: max position error "<<maximumPositionError<<" cm, quaternion component error "<<maximumRotationError<<"\n";
 assert(maximumPositionError<.03f);assert(maximumRotationError<.0001f);
}
