// Original attacks with no opponent for the browser: 0x1163B0 (engine/attack_control.hpp,
// docs/attack-boardpress-recovery.md). Included from animation_bridge.cpp after
// input_bridge.inc (padCommand) and handplant_gameplay.inc.
//
// Callers: cruise 0x131620 at 0x1317E8..0x1317FC (command bits 18/19, after the handplant,
// rail, jump and board-press attempts; a held attack clamps the turn to +-0.5 at
// 0x131804) and natural air 0x12F730 at 0x12F778..0x12F7C4 (bits 14/15 through the
// +0x8 latch; a held attack returns before handplant, rail attach and the targets).
// Exits 0x131C30 (cruise, unless the next control is 4) and 0x12FB68 (natural air,
// unless the next control is 0) fade channel 0 and restore the channel-1 rate.
#include "../engine/attack_control.hpp"
RIDER_LOCAL extern bool (*browserAttackAir)();RIDER_LOCAL extern void (*browserAttackCruise)(float&);RIDER_LOCAL extern void (*browserAttackReset)();

RIDER_LOCAL static ssx::OriginalAttackRider attackRider;
RIDER_LOCAL static int attackPreviousControl=-1;RIDER_LOCAL static uint32_t attackAirTick=0xffffffffu;RIDER_LOCAL static bool attackAirResult=false,attackAirLatchUsed=false;
RIDER_LOCAL static unsigned attackCalls=0,attackHolds=0,attackExits=0;
static void clear_attack(){attackRider={};attackPreviousControl=-1;attackAirTick=0xffffffffu;attackAirResult=attackAirLatchUsed=false;attackCalls=attackHolds=attackExits=0;}

static OriginalAnimationSequence* attack_first_sequence(unsigned channel){
 for(auto& s:graph.sequences)if(s.channel==int(channel))return &s;return nullptr;
}
static ssx::OriginalAttackAccess attack_access(){
 ssx::OriginalAttackAccess a;
 a.channelClass=[](unsigned ch){return graph.currentClass(int(ch));};
 a.requestedSemantic=[](unsigned ch){return graph.requestedSemantics.at(ch);};
 a.latched=[](unsigned ch,unsigned bit){auto*s=attack_first_sequence(ch);return s&&((s->flags>>bit)&1);};
 a.raised=[](unsigned ch,unsigned bit){auto*s=attack_first_sequence(ch);return s&&((s->raisedFlags>>bit)&1);};
 a.setRate=[](unsigned ch,float rate){graph.setRate(ch,rate);};
 a.weights=[](unsigned ch){auto*s=attack_first_sequence(ch);if(!s)throw std::runtime_error("Attack channel has no playback sequence");return ssx::OriginalAttackSequenceWeights{s->weight,s->targetWeight,s->fadeRemaining};};
 a.setWeights=[](unsigned ch,float weight,float target){auto*s=attack_first_sequence(ch);if(!s)throw std::runtime_error("Attack channel has no playback sequence");s->weight=weight;s->targetWeight=target;};
 a.fadeTo=[](unsigned ch,float target,float fade){auto*s=attack_first_sequence(ch);if(!s)throw std::runtime_error("Attack channel has no playback sequence");s->targetWeight=target;s->fadeRemaining=fade;s->stopWhenFaded=false;};
 a.play=[](int semantic,bool force){if(!graph.enter(semantic,-1,riderMask8C0 /*rider+0x8C0, also 115D48's mask*/,force))throw std::runtime_error("Missing original attack animation");};
 a.fade=[](unsigned ch,float seconds){graph.fade(ch,seconds);};
 return a;
}
static bool attack_step(bool left,bool right){
 attackRider.switchStance=physicsState.reverseStance; //rider+0x320
 const auto basis=originalOrientationBasis(physicsState.quaternion);attackRider.right={basis.right[0],basis.right[1],basis.right[2],0.f};
 attackRider.timeScale=physicsState.timeScale;++attackCalls;
 const bool held=ssx::originalAttackStep(attackRider,left,right,attack_access());attackHolds+=held;return held;
}
// Cruise: step_rider's grounded controller calls this after handplant/rail/jump and
// before the turn target. Only control 0 on the ground reaches 0x1163B0.
static void attack_cruise(float& steering){
 if(!padCommand.active||padCommand.control!=0||!::grounded||physicsState.controlState!=0)return;
 const auto& f=padCommand.fields;if(f.jumpPressed||f.jumpHeld)return; //0x1162C8 requested control 2 first
 if(attack_step(f.attackLeft,f.attackRight))steering=ssx::originalAttackClampTurn(steering);
}
// Natural air: evaluated in the controller stage (before 0x107578 / 0x106848). The
// passive step in animation_tick consumes the stored result through access.upper.
static bool attack_air(){
 if(!padCommand.active||padCommand.control!=4||!passiveMode||passiveDeparture!=0||::grounded||railOwned||browserSoftActive||browserStarting)return false;
 const auto& f=padCommand.fields;attackAirLatchUsed=passive.upperLatch!=0;
 attackAirResult=attackAirLatchUsed?attack_step(f.attackLeft,f.attackRight):(attack_step(false,false),false);
 attackAirTick=animationTick+1;return attackAirResult; //animation_tick increments animationTick first
}
// OriginalPassiveAirAccess::upper for 0x12F730.
static bool attack_passive_upper(bool left,bool right){
 if(attackAirTick==animationTick)return attackAirResult; //already run this tick (the latch is unchanged)
 return attack_step(left,right);
}
// 0x131C30 / 0x12FB68, applied in the controller stage of the tick whose controller changed.
static void attack_control_changes(int now){
 const int before=attackPreviousControl;attackPreviousControl=now;
 if(before==now||(before!=0&&before!=4))return;
 if((before==0&&now==4)||(before==4&&now==0))return;
 if(ssx::originalAttackControlExit(graph.currentClass(1))){graph.fade(0,std::bit_cast<float>(0x3dcccccdu));graph.setRate(1,1);++attackExits;}
}
static void setup_attack(){browserAttackAir=attack_air;browserAttackCruise=attack_cruise;browserAttackReset=clear_attack;}
// setup_attack(): per rider context, rider_statics_animation (web/animation_bridge.cpp)
extern "C" EMSCRIPTEN_KEEPALIVE float* attack_info(){
 RIDER_LOCAL static float v[16];v[0]=attackCalls;v[1]=attackHolds;v[2]=attackExits;v[3]=attackRider.strength;for(unsigned k=0;k<4;k++)v[4+k]=attackRider.facing[k];
 v[8]=graph.requestedSemantics[0];v[9]=graph.requestedSemantics[1];v[10]=graph.currentClass(1);v[11]=attackAirResult;v[12]=attackPreviousControl;return v;
}
