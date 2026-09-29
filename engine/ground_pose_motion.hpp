#pragma once
#include "ground_motion.hpp"
namespace ssx {
// Original13F068..13F118, source centimeters/second and controller+0 phase.
void originalGroundBoardLift(OriginalGroundState&,const std::array<float,3>& relativeVelocity);
void originalGroundPresentationTarget(const OriginalGroundProfile&,OriginalGroundState&,const OriginalGroundDiagnostics&);
void originalGroundBoardNormal(OriginalGroundState&); //13F2E4..13F354, after body query.
void originalGroundVisualTargets(const OriginalGroundProfile&,OriginalGroundState&,float stepTime,const std::array<float,3>& relativeVelocity); //13EEA0..13F064.
float originalCosine(float radians); //0x31C040, independent cosine polynomial.
}
