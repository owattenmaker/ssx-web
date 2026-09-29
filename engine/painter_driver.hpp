#pragma once
#include "terrain_contact_math.hpp"
#include <functional>
#include <optional>
namespace ssx {
struct OriginalPainterDriverState {float distance=-99999.f,lastX=0,lastY=0;};
struct OriginalPainterSample {uint32_t type=0;float rate=0;};
struct OriginalPainterDriverAccess {
 std::function<std::optional<OriginalPainterSample>(float,float)> sample;
 std::function<bool()> matches;
 std::function<void(float)> blend;
 std::function<void()> reset;
};
//2C0778, after the host resolves the current region and painter section.
//MissingSection is distinguished from MissingRegion: initial missing-section
//dispatch calls reset twice in the original (2C0840 followed by2C09E8).
enum class OriginalPainterAvailability { Ready,MissingRegion,MissingSection };
inline void originalPainterDriverStep(OriginalPainterDriverState& s,float x,float y,uint32_t type,
 OriginalPainterAvailability availability,float weight,const OriginalPainterDriverAccess& access){
 terrain_original::Rounding rounding;const bool initial=s.distance==-99999.f;
 if(!initial){float dx=originalScalarSubtract(s.lastX,x),dy=originalScalarSubtract(s.lastY,y);
 s.distance=originalScalarAdd(s.distance,originalScalarSqrt(originalScalarAdd(terrain_original::mul(dx,dx),terrain_original::mul(dy,dy))));}
 s.lastX=x;s.lastY=y;
 if(availability!=OriginalPainterAvailability::Ready){if(initial&&availability==OriginalPainterAvailability::MissingSection)access.reset();access.reset();return;}
 const auto sample=access.sample(x,y);if(!sample){access.reset();return;}
 if(sample->type!=type)return;
 if(access.matches())s.distance=0;
 if(initial){access.blend(-1.f);s.distance=0;return;}
 if(weight!=-99999.f){access.blend(weight);return;}
 if(sample->rate>=0&&sample->rate<=s.distance){access.blend(1.f);s.distance=0;}
 else access.blend(-sample->rate);
}
}
