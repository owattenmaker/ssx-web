// Original control 10 (0x12C678, the finish stop) for every rider (human and computer riders).
// Controllers 0/1/2/3 start with 0x116378: once the rider's finish marker +0x470 is >= 0 they request
// control 10 (0x11FEC8) and end their tick. Control 10, each tick (object: +0 phase, +4 rider):
//   +0x200/+0x204 = 1/30, +0x1F0 (animation-turn rate/target follow the turn);
//   phase 0: with +0x2DC == 0, 0x114CC0 (riding backwards: 180-degree turn-around, semantic 21);
//     otherwise, on the ground only: turn target +-0.1 (sign of +0x1F0),
//     boost off (0x114130), +0x2DC = 0, semantic 11; normal.z < 0.8: crouch/brake targets 0;
//     else velocity *= 0.97, brake target 1 until |brake| == 1 or speed < 277.78 -> phase 1;
//   phase 1: targets 0, velocity *= 0.93, when |brake| < 0.2 and turn == 0 -> phase 2 with the
//     10E028 reaction (+0x100 == 1 ? 1 : 4) and semantic 4;
//   phase 2: velocity 0, 0x115B58 plays the pending reaction at rate 0.75; when channel 1 has no
//     class: +0x470 = max(+0x470, 10) -> phase 3; phase 3: velocity 0.
// The chosen main semantic is entered when it differs from the current one (0x3128E8).
extern "C" float browser_finish_elapsed();extern "C" void browser_finish_elapsed_min(float); // web/race_bridge.cpp
RIDER_LOCAL extern int (*browserFinishStep)(); // web/core.cpp
RIDER_LOCAL extern uint32_t reverseTurnSerial; // web/core.cpp
static float score_reset(bool penalize); // web/score_gameplay.inc (119368)
namespace {
struct FinishControl {bool active=false;int phase=0;};
RIDER_LOCAL FinishControl finishControl;
RIDER_LOCAL bool finishCelebrate=true; // rider +0x100: at the finish (125108 -> 238358) the race results handler clears it when the human's 0-based place 0x536730[slot] >= 3 (off the podium), the freestyle handler (0x23A05C) when it is > 0; set by the host (main.js / compare-ps2-capture.mjs)
int finish_step(){
 using namespace terrain_original;
 if(!finishControl.active){
  // The controller slot is gs.controlState (as the pad decode, input_bridge.inc): physicsState.controlState stays 0
  // through passive (4/5) flights, and a pipe finish is crossed in the air (pipe-finish 4404: control 5, 10 after landing).
  const int control=gs.controlState;
  if(browserResetActive||crash.active||browserStarting||!(control==0||control==1||control==2||control==3))return 0;
  if(!(browser_finish_elapsed()>=0))return 0;
  // Control 10 entry 0x12C5C8: 119368(score, 1) (the reset count +0x120 and the combo; its meter delta is
  // dropped), then phase 0 unless the rider is slow (|v| <= gp+0x1B9C 277.78), has no manual spin
  // (+0x2DC) and stands on flat ground (+0x378 normal.z >= gp-0x77FC 0.8): phase 1 (pipe-run-event 2213).
  score_reset(true);
  int phase=0;{OriginalRounding rounding;const auto& v=physicsState.velocity;const float speed=terrain_original::sqrt(terrain_original::add(terrain_original::add(terrain_original::mul(v[0],v[0]),terrain_original::mul(v[1],v[1])),terrain_original::mul(v[2],v[2])));
   if(!(277.77777099609375f<speed)&&physicsState.manualSpin==0&&!(physicsState.normal[2]<0.800000011920929f))phase=1;}
  finishControl={true,phase};physicsState.controlState=gs.controlState=10;heldAirMode=false;groundControllerRan=true;return 1; //0x116378 -> requestControl(10), controller ends
 }
 // The animation graph is absent in physics-only harnesses (graph.rig null).
 OriginalRounding rounding;const int current=graph.requestedSemantics[2];int semantic=current;
 physicsState.animationTurn.rate=0.03333333507180214f;physicsState.animationTurn.target=physicsState.turn.current;
 auto scale=[](float k){for(auto& v:physicsState.velocity)v=mul(v,k);};
 auto speed=[](){const auto& v=physicsState.velocity;return terrain_original::sqrt(add(add(mul(v[0],v[0]),mul(v[1],v[1])),mul(v[2],v[2])));};
 const float forward=dot(physicsState.velocity,physicsState.forward);
 switch(finishControl.phase){
  case 0:{
   if(current==21)break;
   // 0x12C714: with no manual spin, 0x114CC0 turns a rider moving backwards (velocity . +3A0 below the gp-0x7B98
   // limit, brake 0) around by 180 degrees (+0x115168 stance flip) and plays 21; this tick ends there.
   if(physicsState.manualSpin==0){auto reverse=originalReverseTurn(physicsState,physicsState.balance280);
    if(reverse.reversed){++reverseTurnSerial;if(graph.rig)reverse_roots(reverse.animationRootQuaternion);semantic=21;physicsState.animationIndex=21;physicsState.animationClass=7;break;}}
   if(!::grounded)break; // 0x12C744: the rest of phase 0 runs on the ground only (11FE98 == 0)
   groundTurnTarget(physicsState.turn,0<physicsState.turn.current?0.1f:-0.1f,physicsState.velocity,physicsProfile.surface.id);semantic=11;
   originalBoostControl(boostState,boostProfile,false,false);physicsState.manualSpin=0;
   if(physicsState.normal[2]<0.800000011920929f){groundCrouchBrakeTargets(physicsState.crouch,physicsState.brake,0,0,forward,physicsState.turn.current);break;}
   scale(0.9700000286102295f);
   if(std::abs(physicsState.brake.current)==1||speed()<277.77777099609375f){finishControl.phase=1;break;}
   groundCrouchBrakeTargets(physicsState.crouch,physicsState.brake,0,1,forward,physicsState.turn.current);break;
  }
  case 1:
   groundCrouchBrakeTargets(physicsState.crouch,physicsState.brake,0,0,forward,physicsState.turn.current);
   groundTurnTarget(physicsState.turn,0,physicsState.velocity,physicsProfile.surface.id);scale(0.9300000071525574f);
   if(std::abs(physicsState.brake.current)<0.20000000298023224f&&physicsState.turn.current==0){
    finishControl.phase=2;semantic=4;upperRequest358=finishCelebrate?1:4;upperRequestTick354=int32_t(controllerGround.logicTick);}
   break;
  case 2:{
   physicsState.velocity={};
   if(upperRequest358&&graph.rig){const int mainClass=graph.currentClass(2); //12C918 writes animator +0x1C = 0.75, 0x115B58 plays, 12C938 writes 1 again
    if(mainClass!=5&&graph.currentClass(1)==0&&mainClass!=10){const int32_t age=std::bit_cast<int32_t>(controllerGround.logicTick-uint32_t(upperRequestTick354));
     if(age>=0){if(age<180){const int kind=upperRequest358;if(kind==1||kind==2)audio_event(AE_TRICK_SPEECH,float(kind-1));const int s=kind>=1&&kind<=3?315:kind==4?314:-1;if(s>=0&&![&]{graph.nextRate=0.75f;const bool ok=graph.enter(s,-1,riderMask8C8);graph.nextRate=1;return ok;}())throw std::runtime_error("Missing finish reaction animation");}
      upperRequest358=0;upperRequestTick354=-1;}}}
   if(!graph.rig||graph.currentClass(1)==0){browser_finish_elapsed_min(10.f);finishControl.phase=3;}
   break;
  }
  default:physicsState.velocity={};break;
 }
 if(graph.rig&&semantic!=current&&!graph.enter(semantic))throw std::runtime_error("Missing finish control animation "+std::to_string(semantic));
 if(graph.rig){physicsState.animationIndex=graph.requestedSemantics[2];physicsState.animationClass=graph.currentClass(2);} //13D818's lift (13E144) and board-alignment targets read the class the controller left (pipe-finish 4500)
 gs.animationTurn=physicsState.animationTurn; //+0x1FC/+0x200/+0x204 belong to the controller (the animation stage copies gs back)
 physicsState.controlState=gs.controlState=10;groundControllerRan=true;commit_rider_physics();browser_controller_takeoff_wind(physicsState.velocity); /*12C678 scales +0x1E0 before 1211F8 -> 120378 (pipe-run-event 2214)*/return 1;
}
}
static void finish_reset(){finishControl={};}
namespace {
// browserFinishStep=finish_step: per rider context, rider_statics_animation (web/animation_bridge.cpp)
}
extern "C" EMSCRIPTEN_KEEPALIVE float* finish_control_info(){RIDER_LOCAL static float v[3];v[0]=finishControl.active;v[1]=finishControl.phase;v[2]=finishCelebrate;return v;}
extern "C" EMSCRIPTEN_KEEPALIVE void finish_celebrate(int celebrate){finishCelebrate=celebrate!=0;}
// The single-player freestyle finish (125108 -> 238358, handler 0 of every Single Event and career heat: vtable 0x47CFA0, results
// 0x239230): 238B70 ranks the round's values 0x536640, then +0x100 = place 0x536730 < 3 (0x239548 slti 3; a second ranking with one
// more slot at 0x23955C decides the same way), and 0x239854 sets the human's boost meter +0x2F8 by place (1st gp-0x515C = 0.7, 2nd
// gp-0x5158 = 0.35, else 0). The winner-only clear 0x23A05C is the multiplayer freestyle handler 9 (vtable 0x47CE68, results
// 0x239EC8; 0x238160 maps modes 1-3 to it only when 0x535C11 == 2). The host ranks the run with the posted scores (web/career.js
// finishPlace) and hands the place in on the finish tick (PS2 crows-invert 2079, perpendiculous 3996: +0x100 and the meter together).
extern "C" EMSCRIPTEN_KEEPALIVE void finish_standing(int place){finishCelebrate=place<3;boostState.meter=place==0?std::bit_cast<float>(0x3f333333u):place==1?std::bit_cast<float>(0x3eb33333u):0.f;}
