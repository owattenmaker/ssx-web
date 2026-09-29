#pragma once
#include "terrain_contact_math.hpp"
#include <functional>
namespace ssx {
struct OriginalResetControlState {bool reasonNonzero=false;float progress=0;};
struct OriginalResetControlInputs {float timeScale=1;bool eventModeActive=false;int eventVariant=0;int deviceIndex=-1;bool deviceEnabled=false;};
struct OriginalResetControlCallbacks {
 std::function<void()> refreshRoute,postPlacement,finishDeviceFade,clearScoringStance;
 std::function<terrain_original::Vector()> routeDirection;
 std::function<void(terrain_original::Vector,float,int)> place;
 std::function<float(bool)> resetScore;
 std::function<void(float)> awardBoost,setAnimationRate;
 std::function<void(int)> enterControl,enterMotion;
};
// Original12F398: the caller must implement actual placement/initialization,
// manager effects and control4/motion1 entry at the callback boundaries.
inline void originalResetControlStep(OriginalResetControlState& state,const OriginalResetControlInputs& input,const OriginalResetControlCallbacks& callbacks){
 using namespace terrain_original;Rounding rounding;const float previous=state.progress,step=mul(input.timeScale,.02500000223517418f);
 if(originalScalarAdd(1.f,step)<previous)state.progress=originalScalarSubtract(previous,step);
 else if(previous<originalScalarSubtract(1.f,step))state.progress=originalScalarAdd(previous,step);
 else state.progress=1;
 if(previous<=.5f&&state.progress>.5f){
  callbacks.refreshRoute();auto direction=callbacks.routeDirection();
  callbacks.place(direction,input.eventModeActive&&input.eventVariant==2?1000.f:200.f,287);
  callbacks.postPlacement();callbacks.awardBoost(callbacks.resetScore(!state.reasonNonzero));
 }
 if(state.progress==1){
  if(input.deviceEnabled&&input.deviceIndex>=0)callbacks.finishDeviceFade();
  callbacks.clearScoringStance();callbacks.setAnimationRate(1);callbacks.enterControl(4);callbacks.enterMotion(1);
 }
}
}
