#include "reset_placement.hpp"
#include "animation_motion.hpp"
#include "ground_motion.hpp"
namespace ssx {
OriginalResetPlacement originalResetPlacement(terrain_original::Vector point,terrain_original::Vector direction,
        float clearance,const OriginalResetPlacementQuery& query){
 using namespace terrain_original;Rounding rounding;OriginalResetPlacement result;result.position=point;
 float heading;
 if(direction[0]==0)heading=direction[1]==0?direction[1]:direction[1]<0?-1.5707963705062866f:1.5707963705062866f;
 else{heading=originalAtan(originalScalarDivide(direction[1],direction[0]));if(direction[0]<0)heading=direction[1]>0?originalScalarAdd(heading,3.1415927410125732f):originalScalarSubtract(heading,3.1415927410125732f);}
 if(clearance>=0){
  // 0x11D798 / 0x11D7E8: the probe offsets are the z axis (0x4FF160) x -7000 and x 200 with the axis as fs (1.0 x on z).
  result.queried=true;auto end=point,start=point;end[2]=add(end[2],mul(1.f,-7000.f));start[2]=add(start[2],mul(1.f,200.f));
  const auto hit=query(end,start,.9791666865348816f);
  if(!hit.complete)throw OriginalAirTrajectoryUnavailable("Reset placement intersects unsupported world resources");
  if(hit.fraction>=0){result.position=hit.position;result.normal=hit.normal;result.hit=true;}
 }
 if(clearance>=0)result.position[2]=add(result.position[2],clearance);
 auto sc=originalSinCos(mul(originalScalarSubtract(heading,1.5707963705062866f),.5f));
 std::array<float,4> rotation{mul(sc[0],0.f),mul(sc[0],0.f),mul(sc[0],1.f),sc[1]};
 auto axis=cross(Vector{0,0,1},result.normal);float magnitude=terrain_original::sqrt(dot(axis,axis));
 if(magnitude>.0010000000474974513f){
  const float inverse=div(1.f,magnitude);for(auto& value:axis)value=mul(value,inverse);
  sc=originalSinCos(mul(originalAsin(std::min(magnitude,1.f)),.5f));
  std::array<float,4> tilt{mul(sc[0],axis[0]),mul(sc[0],axis[1]),mul(sc[0],axis[2]),sc[1]};
  rotation=originalAnimationCompose({{},tilt},{{},rotation}).rotation;
 }
 result.physical=originalRebuildOrientation(rotation);
 const float along=dot(result.physical.forward,result.normal);auto tangent=result.physical.forward;
 for(unsigned k=0;k<3;k++)tangent[k]=sub(tangent[k],mul(result.normal[k],along));
 const float inverse=div(1.f,terrain_original::sqrt(dot(tangent,tangent)));
 for(unsigned k=0;k<3;k++)result.forward[k]=mul(tangent[k],inverse);
 result.lateral=cross(result.normal,result.forward);return result;
}
}
