#pragma once
#include "wake_input.hpp"
#include <functional>
namespace ssx {
struct OriginalWakeControlState {
 terrain_original::Vector normal60{},side70{},direction80{},point50{};
 float amplitude90=0,alpha94=0;bool active3C=false,positiveSideB0=false;int counter40=0;
 bool operator==(const OriginalWakeControlState&)const=default;
};
struct OriginalWakeControlCallbacks {
 std::function<void()> advanceRow;
 std::function<void(terrain_original::Vector,float)> createRow;
};
//2DD6F4..2DDA88: eligibility, side changes, filtering and exact row requests.
void originalWakeControl(OriginalWakeControlState&,OriginalWakeTargets,int motionMode,float roll274,const OriginalWakeControlCallbacks&);
}
