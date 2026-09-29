#pragma once
#include "irradiance_rim.hpp"
#include "irradiance_rotation.hpp"
#include "irradiance_view.hpp"
namespace ssx {
//Complete389CB8 mathematics after the host provides the renderer's view matrix.
//Constants: shape constant,Y²,Z²,X,global rim scale (read from original globals).
inline void originalIrradianceViewRim(OriginalIrradianceCoefficients& destination,
 const std::array<float,16>& view,const std::array<float,4>& point,float scale,const std::array<float,5>& constants){
 auto direction=originalIrradianceViewDirection(view,point);
 auto angles=originalIrradianceAngles(direction.direction,direction.horizontalLength);
 auto rotation=originalIrradianceRimRotation(angles.pitch,angles.yaw,direction.eye);
 auto shape=originalIrradianceRimShape(constants[0],constants[1],constants[2],constants[3]);
 originalIrradianceRimCompose(destination,shape,rotation,scale,constants[4]);
}
}
