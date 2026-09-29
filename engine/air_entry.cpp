#include "air_entry.hpp"
#include "original_float.hpp"
#include "terrain_contact_math.hpp"
#include "orientation_motion.hpp"
#include <bit>
#include <cmath>
#include <cfenv>
#include <stdexcept>
#pragma STDC FENV_ACCESS ON
namespace ssx {namespace {
constexpr float F(unsigned bits){return std::bit_cast<float>(bits);}
using Round=OriginalRounding;
}
bool originalAirPrewindTargets(OriginalAirPrewindState& state,float spin,float flip,const OriginalAirPrewindContext& context){
 Round round;
 if(context.animationClass==10||context.animationIndex==21){state.spin.target=state.flip.target=0;state.spin.rate=state.flip.rate=F(0x3daaaae8);return true;}
 if(context.style==0&&context.manualSpin==0&&context.reverseTurnTriggered)return false;
 float rate=context.style?F(0x40a000bd):F(0x40a00039);
 auto target=[](float x){return x<F(0xbe4ccccd)?-1.f:x>F(0x3e4ccccd)?1.f:0.f;};
 state.spin.target=target(spin);state.flip.target=target(flip);
 float a=std::abs(originalScalarSubtract(state.spin.target,state.spin.current));a=terrain_original::mul(rate,a);state.spin.rate=terrain_original::mul(a,F(0x3c888889));
 float b=std::abs(originalScalarSubtract(state.flip.target,state.flip.current));b=terrain_original::mul(rate,b);state.flip.rate=terrain_original::mul(b,F(0x3c888889));return true;
}
void originalAirPrewindApproach(OriginalAirPrewindState& state){groundControlApproach(state.spin);groundControlApproach(state.flip);}
OriginalAirControlState originalAirControlRelease(OriginalAirPrewindState& state){
 auto snapped=originalAirDirection(state.flip.current,state.spin.current,F(0x3f490fdc),F(0x3e4ccccd));state.flip.current=snapped[0];state.spin.current=snapped[1];return originalAirControlBegin(state.spin.current,state.flip.current);
}
}
namespace ssx {
bool originalAirReverseTurnRequired(float brake,std::array<float,3> velocity,std::array<float,3> forward){
 Round round;if(brake!=0)return false;float a=terrain_original::mul(velocity[0],forward[0]),b=terrain_original::mul(velocity[1],forward[1]),c=terrain_original::mul(velocity[2],forward[2]);float speed=terrain_original::add(a,b);speed=terrain_original::add(speed,c);return speed<=-F(0x42de38e4);
}
void originalAirReleaseGroundTargets(const OriginalGroundProfile& profile,OriginalGroundState& state,std::array<float,3> velocity){
 Round round;groundTurnTarget(state.turn,0,velocity,profile.surface.id);float a=terrain_original::mul(velocity[0],state.forward[0]),b=terrain_original::mul(velocity[1],state.forward[1]),c=terrain_original::mul(velocity[2],state.forward[2]);float speed=terrain_original::add(a,b);speed=terrain_original::add(speed,c);
 groundCrouchBrakeTargets(state.crouch,state.brake,0,0,speed,state.turn.current);state.controlState=5;
}
}

namespace ssx {bool originalCrouchRequest(float& gate,bool held,bool pressed){if(pressed||(gate==0&&held))return true;gate=1;return false;} }

namespace ssx {
void originalAirExitRates(OriginalAirControlState& control){
 Round round;
 float scaled=terrain_original::mul(control.spinRate,F(0x3f40027e));
 control.spinRate=scaled>F(0x4096cbe5)?F(0x40fb53d3):scaled>F(0x40490fdc)?F(0x4096cbe5):scaled>0?F(0x3fc90fdc):0;
 control.flipRate=0;if(control.progressSpin>control.targetSpin)control.spinRate=-control.spinRate;
}
void originalAirControlExit(OriginalAirControlState& control,OriginalAirPrewindState& prewind,
    OriginalAirExitState& state,std::array<float,3> pivot){
 Round round;prewind.spin={};prewind.flip={};
 state.adjustment28C.rate=state.adjustment298.rate=F(0x3c888889);
 state.adjustment28C.target=state.adjustment298.target=0;
 state.physical=originalAirPresentation(control,state.physical,pivot);
 auto rebuilt=originalRebuildOrientation(state.physical.quaternion);
 state.physical.quaternion=rebuilt.quaternion;state.right=rebuilt.right;state.forward=rebuilt.forward;state.up=rebuilt.up;
 originalAirExitRates(control);
}
}

namespace ssx {
OriginalReverseTurnResult originalReverseTurn(OriginalGroundState& state,GroundControlValue& balance){
 Round round;OriginalReverseTurnResult result;
 if(!originalAirReverseTurnRequired(state.brake.current,state.velocity,state.forward))return result;
 auto q=state.quaternion;auto axis=state.boardUp;std::array<float,4> rotated;
 for(unsigned i=0;i<3;++i){unsigned j=(i+1)%3,k=(i+2)%3;float cross=terrain_original::sub(terrain_original::mul(axis[j],q[k]),terrain_original::mul(axis[k],q[j]));float weighted=terrain_original::add(terrain_original::mul(axis[i],q[3]),terrain_original::mul(q[i],0.f));rotated[i]=terrain_original::add(weighted,cross);}
 float product=terrain_original::sub(terrain_original::mul(0.f,q[3]),terrain_original::mul(axis[0],q[0]));product=terrain_original::sub(product,terrain_original::mul(axis[1],q[1]));rotated[3]=terrain_original::sub(product,terrain_original::mul(axis[2],q[2]));
 auto rebuilt=originalRebuildOrientation(rotated);state.quaternion=rebuilt.quaternion;state.physicalForward=rebuilt.forward;state.boardUp=rebuilt.up;
 state.reverseStance=!state.reverseStance;state.state320Equals324=!state.state320Equals324;
 auto sc=originalSinCos(state.reverseStance?-F(0x3fc90fdb):-0.f);result.animationRootQuaternion={terrain_original::mul(sc[0],0.f),terrain_original::mul(sc[0],0.f),terrain_original::mul(sc[0],1.f),sc[1]};
 for(unsigned i=0;i<3;++i){state.forward[i]=terrain_original::mul(state.forward[i],-1.f);state.lateral[i]=terrain_original::mul(state.lateral[i],-1.f);}
 for(auto* value:{&state.turn,&state.brake,&state.extraLean,&state.animationTurn,&balance}){value->current=-value->current;value->target=-value->target;}
 result.reversed=true;return result;
}
}
