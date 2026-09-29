#pragma once
#include "air_trajectory_world.hpp"
#include "orientation_motion.hpp"
namespace ssx {
struct OriginalResetPlacement {
 terrain_original::Vector position{},normal{0,0,1},forward{},lateral{};
 OriginalPhysicalOrientation physical;
 bool queried=false,hit=false;
};
using OriginalResetPlacementQuery=std::function<OriginalWorldSegmentHit(terrain_original::Vector,terrain_original::Vector,float)>;
// Original11D660..11DACC placement/physical-basis stage. Input clearance is
// vertical height, not search radius. The caller owns cache clearing and the
// following control/animation/body/camera initialization callbacks.
OriginalResetPlacement originalResetPlacement(terrain_original::Vector point,terrain_original::Vector direction,
 float clearanceCm,const OriginalResetPlacementQuery&);
}
