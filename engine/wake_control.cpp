#include "wake_control.hpp"
namespace ssx {
void originalWakeControl(OriginalWakeControlState& state,OriginalWakeTargets target,int motionMode,float roll,const OriginalWakeControlCallbacks& callbacks){
 using namespace terrain_original;Rounding rounding;
 bool permitted=motionMode==0&&std::abs(roll)<=.15000000596046448f&&target.amplitude>=416.66668701171875f&&target.turnAmount>=.15000000596046448f&&target.alpha>=.05000000074505806f;
 if(!permitted)target.amplitude=0;
 if(!state.active3C){
  if(!permitted)return;
  state.amplitude90=0;state.normal60=target.normal;state.side70=target.side;state.direction80=target.direction;state.point50=target.point;state.counter40=0;state.positiveSideB0=target.signedTurn>0;state.alpha94=0;
  callbacks.advanceRow();callbacks.createRow(target.point,0);
  state.alpha94=target.alpha;callbacks.advanceRow();callbacks.createRow(target.point,0);state.active3C=true;return;
 }
 if((state.positiveSideB0&&target.signedTurn<0)||(!state.positiveSideB0&&target.signedTurn>0)){target.amplitude=0;state.amplitude90=0;}
 if(motionMode!=0){state.alpha94=0;state.active3C=false;callbacks.createRow(target.point,state.alpha94);return;}
 for(unsigned k=0;k<3;k++){
  state.normal60[k]=add(mul(state.normal60[k],.800000011920929f),mul(target.normal[k],.19999998807907104f));
  state.side70[k]=add(mul(state.side70[k],.800000011920929f),mul(target.side[k],.19999998807907104f));
  state.direction80[k]=add(mul(state.direction80[k],.800000011920929f),mul(target.direction[k],.19999998807907104f));
 }
 state.alpha94=originalScalarAdd(mul(state.alpha94,.699999988079071f),mul(target.alpha,.30000001192092896f));
 state.amplitude90=originalScalarAdd(mul(state.amplitude90,.800000011920929f),mul(target.amplitude,.19999998807907104f));
 if(state.alpha94>.05000000074505806f&&state.amplitude90>55.55555725097656f){
  auto delta=difference(target.point,state.point50);
  if(terrain_original::sqrt(dot(delta,delta))>50.f){callbacks.advanceRow();state.point50=target.point;}
  callbacks.createRow(target.point,target.growth);
 }else{state.alpha94=0;state.active3C=false;callbacks.createRow(target.point,state.alpha94);}
}
}
