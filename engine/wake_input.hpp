#pragma once
#include "terrain_contact_math.hpp"
namespace ssx {
struct OriginalWakeFrameInput {
 terrain_original::Vector normal{},lateral{},velocity{},scaledAxis0{},unitAxis0{},unitAxis2{},boardPosition{};
 float turn1F0=0,lean208=0,brake214=0,surfaceAlpha48=0,surfaceScale4C=0;
 bool reverse320=false,surfaceFlag84=false;
};
struct OriginalWakeTargets {
 terrain_original::Vector normal{},side{},direction{},point{};
 float amplitude=0,alpha=0,growth=0,signedTurn=0,turnAmount=0,speed=0;
};
//2DD2A4..2DD6F0: live targets before motion/eligibility gates or filtering.
OriginalWakeTargets originalWakeTargets(const OriginalWakeFrameInput&);
}
