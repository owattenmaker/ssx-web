// Original board press for the browser: control 1 (nose/tail press, BoardPivot, R3 ollie).
// The recovered routines live in engine/board_press.hpp (docs/attack-boardpress-recovery.md);
// this adapter binds them to the browser rider, animation graph and score state.
// Included from animation_bridge.cpp after input_bridge.inc and handplant_gameplay.inc.
//
// Tick order (stage list 0x128C00): the controller runs first -- cruise 0x131620 reaches
// the entry 0x1161D0 after its jump request, control 1 runs 0x12FC80 -- then the boost
// timers 0x1200D0, the triplet approach 0x1211F8 (which also moves +0x268/+0x274/+0x280)
// and the motion update. step_rider calls board_press_step in the controller slot, before
// its cruise boost/target work, and skips the cruise controller for the tick it ran.
#include "../engine/board_press.hpp"
RIDER_LOCAL extern int (*browserBoardPressStep)();RIDER_LOCAL extern void (*browserBoardPressReset)();RIDER_LOCAL extern int (*browserBoardPressCancel)();
RIDER_LOCAL extern bool browserBoardPressFrame;RIDER_LOCAL extern double airtime,charge;RIDER_LOCAL extern bool held,airbornePrewind;
void browser_controller_takeoff(float charge);

RIDER_LOCAL static OriginalBoardPressState bpControl;
RIDER_LOCAL static GroundControlValue bp268{},bp274{}; //rider+0x268 / +0x274 triplets (1211F8 approaches them every tick)
RIDER_LOCAL static int32_t bp330=0; //rider+0x330 press style (+0x360, the jump latch, is prewind.jumpGate as for rails)
RIDER_LOCAL static bool bpActive=false;      // control 1 owns the controller
RIDER_LOCAL static bool bpAirTransition=false; // this tick control 1 requested control 5 (0x1307B8 ollie / 0x12FFF8); 0x133308 runs next tick
RIDER_LOCAL static bool bpRailTick=false; // control 1 already ran in the rail step this tick (step_rails -> board_press_rail_update)
RIDER_LOCAL static unsigned bpEntries=0,bpUpdates=0,bpOllies=0,bpLandings=0,bpEffects=0,bpRumbles=0,bpPivots=0,bpExits=0,bpAirborne=0,bpRailTicks=0,bpRailAttaches=0;
RIDER_LOCAL static int bpLastExitControl=-1,bpLastStop=0;RIDER_LOCAL static float bpLastAward=0;

static bool board_press_owned(){return bpActive;}
static int32_t board_press_style(){return bp330;} //rider+0x330 for the boost ribbon front/back offset (engine/boost_ribbon.hpp)
// Control 1 is airborne (motion 1: a passive departure or a pivot finishing in the air).
static bool board_press_air_frame(){return bpActive&&!::grounded;}
static void clear_board_press(){bpControl={};bp268={};bp274={};bp330=0;bpActive=bpAirTransition=bpRailTick=false;
 bpEntries=bpUpdates=bpOllies=bpLandings=bpEffects=bpRumbles=bpPivots=bpExits=bpAirborne=bpRailTicks=bpRailAttaches=0;bpLastExitControl=-1;bpLastStop=0;bpLastAward=0;
 graph.boardPress268=graph.boardPress274=graph.boardPress280=0;graph.boardPress330=0;}

// 11D660 (a reset placement) zeroes the +0x268 / +0x274 triplets with the other ground-control triplets (0x11DB70..0x11DB84).
static void board_press_placement_clear(){
 bp268={};
 bp274={};
 graph.boardPress268=graph.boardPress274=0;
}
static OriginalBoardPressRider bp_view(){
 OriginalBoardPressRider r;const auto& s=physicsState;
 r.position={s.position[0],s.position[1],s.position[2],1.f};r.velocity={s.velocity[0],s.velocity[1],s.velocity[2],0.f};
 r.forward={s.forward[0],s.forward[1],s.forward[2],0.f};r.timeScale=s.timeScale;r.manualSpin=s.manualSpin;r.jumpLatch=prewind.jumpGate;
 r.reverseStance=s.reverseStance;r.style=s.prewindStyle;r.press=bp330;r.surface=physicsProfile.surface.id;
 r.depth268=bp268;r.depth274=bp274;r.pivot280=s.balance280;return r;
}
static void bp_apply(const OriginalBoardPressRider& r){
 physicsState.prewindStyle=r.style;bp330=r.press;prewind.jumpGate=r.jumpLatch;bp268=r.depth268;bp274=r.depth274;physicsState.balance280=r.pivot280;
 if(bool(r.reverseStance)!=physicsState.reverseStance){physicsState.reverseStance=r.reverseStance!=0;physicsState.state320Equals324=!physicsState.state320Equals324;}
}
static void bp_publish_graph(){graph.boardPress268=bp268.current;graph.boardPress274=bp274.current;graph.boardPress280=physicsState.balance280.current;graph.boardPress330=bp330;}
// 0x1211F8 approach of the board-press triplets (the pivot +0x280 too, except where core's
// crash/reset paths or the rail step's own 0x1211F8 list already approach it).
static void board_press_filters(){
 groundControlApproach(bp268);groundControlApproach(bp274);
 if(!crash.active&&!browserResetActive&&!browserCrashActive&&!railOwned)groundControlApproach(physicsState.balance280);
 bp_publish_graph();
}
// 0x12FC60 enter (the caller already ran the old controller's exit).
static void bp_enter(OriginalBoardPressRider& r){
 originalBoardPressEnter(bpControl,r);bpActive=true;physicsState.controlState=gs.controlState=1;
}
// 0x12FE98 exit into another controller.
static void bp_leave(OriginalBoardPressRider& r,int next){
 originalBoardPressExit(r);bp_apply(r);bpActive=false;bpLastExitControl=next;++bpExits;physicsState.controlState=gs.controlState=next;
}

static OriginalBoardPressAccess bp_access(OriginalBoardPressRider& r){
 OriginalBoardPressAccess a;
 a.requestedSemantic=[](){return graph.requestedSemantics[2];};
 a.play=[](int semantic){if(!graph.enter(semantic))throw std::runtime_error("Missing original board-press animation");
  physicsState.animationIndex=gs.animationIndex=graph.requestedSemantics[2];physicsState.animationClass=gs.animationClass=graph.currentClass(2);};
 a.motionMode=[](){return browserRailActive?4: ::grounded?0:1;};
 a.controlState=[](){return physicsState.controlState;};
 a.scorePress=[](int style){auto s=capture_score();float v=originalBoardPressScorePress(s,style);apply_score(s);return v;};
 a.scorePivot=[](int style){auto s=capture_score();float v=originalBoardPressScorePivot(s,style);apply_score(s);++bpPivots;return v;};
 a.scoreEnd=[](){auto s=capture_score();auto result=originalBoardPressScoreEnd(s,commit_score_state);apply_score(s);queue_rail_score(result);grab={};lastScoreId=-1;return result.meterDelta;};
 a.award=[](float value){bpLastAward=value;award_rail_meter(value);};
 a.reverseTurn=[&r](){ //0x114CC0 + 0x115168
  bp_apply(r);auto reverse=originalReverseTurn(physicsState,physicsState.balance280);
  if(reverse.reversed){++reverseTurnSerial;reverse_roots(reverse.animationRootQuaternion);publish_motion();}
  r=bp_view();return reverse.reversed;};
 // 12FEC8 flips +0x320 in the controller (a pivot finalised by 131428, e.g. an R3 ollie out of a pivoted press): 1211F8's 120378 reads
 // the flipped stance (PS2 hl2/rail-cra3-s2b 1320: the hair request 415, the port's stale stance chose 412).
 a.stanceRoot=[](const HandplantQuad& q,int32_t mirror){graph.defaultRoot={};graph.defaultRoot.rotation=q;graph.defaultMirror=mirror!=0;browser_controller_stance(mirror!=0);};
 a.jumpTakeoff=[&r](){bp_apply(r);if(browserRailActive)rail_controller_takeoff(1.f);else browser_controller_takeoff(1.f);r=bp_view();};
 a.ollieEvent=[](){++bpOllies;};
 a.rumble=[](){++bpRumbles;audio_event(AE_WHOOH);}; /*1310C0: depth +0x274 > 0.8 -> 2A1560 Whooh speech*/
 // 0x10EB30(rider,358/359,0,+0x438,block): a full-depth press held for more than 1 s is a hard
 // crash. Its 0x11FEC8(13) runs this controller's exit before the crash control/motion.
 a.effect=[&r](const OriginalBoardPressEffect& e){++bpEffects;bp_leave(r,13);
  OriginalCollisionEvent event;for(unsigned k=0;k<3;k++){event.pointCm[k]=e.position[k];event.incomingDirection[k]=e.direction[k];event.normal[k]=e.down[k];}
  event.closingSpeedCmps=e.speed;event.surface=e.surface;event.surfaceProperty44=browser_surface_property(e.surface);
  enter_crash(e.id,event);r=bp_view();};
 a.restoreStance=[&r](){bp_apply(r);restore_stance(physicsState.prewindStyle);r=bp_view();};
 a.railCycle=[](){if(!graph.enter(originalRailAttachSemantic(physicsState.prewindStyle,false)))throw std::runtime_error("Missing original rail cycle");}; //0x1326C8(owner+0x2B0,0)
 a.jumpStat=[](){};
 a.requestMotion=[](int motion){if(motion!=1)throw std::runtime_error("Unexpected board-press motion request");};
 a.requestControl=[&r](int next){
  if(next==1){bp_enter(r);return;}
  bp_leave(r,next);
  if(next==7){auto view=rail_view();originalRailControlBegin(railControl,view);rail_apply(view);prewind.jumpGate=0;return;} //0x131D08 control-7 enter (+0x360 = 0)
  if(next==2&&browserRailActive){ //rail crouch (control 2 on motion 4), entered like control 7's upper action
   railHeldJump=true;physicsState.animationTurn.target=0;physicsState.animationTurn.rate=.03333333507180214f;
   const int semantic=originalPrewindAnimation(prewind.spin.current,prewind.flip.current,physicsState.reverseStance,physicsState.prewindStyle);
   if(semantic<0||!graph.enter(semantic))throw std::runtime_error("Missing rail crouch entry");return;}
  if(next==5){ //0x133128 air control enter; its first update runs next tick
   bpAirTransition=true;passiveMode=heldAirMode=false;passiveDeparture=0;grab={};air=originalAirControlBegin(prewind.spin.current,prewind.flip.current);
  }else if(next==2){
   // 11FEC8(2) runs control 2's entry 12E980: +0x200 = gp-0x7794 (1/30), +0x204 = 0; 1211F8 approaches +0x1FC this tick. bp_leave already
   // set control 2, so select_ground_animation's entry branch does not run (PS2 hl2/rail-dss2-s1 1422: a press into a held Cross).
   for(auto* value:{&physicsState.animationTurn,&gs.animationTurn}){
    value->target=0;
    value->rate=std::bit_cast<float>(0x3d088889u);
   }
  }else if(next!=0)throw std::runtime_error("Unexpected board-press control request");
 };
 // 0x12FC80 shared cruise helpers.
 a.statusCheck=[](){return false;}; //0x116378: +0x470 finish state is not modelled (cruise skips it too)
 a.resetPath=[](bool requested){return requested;}; //Select.pressed is routed to request_rider_reset by pad_tick
 a.railAttach=[](){return false;}; //0x106848 from control 1 (rail board slide) is not wired
 a.boost=[](bool held,bool pressed){originalBoostControl(boostState,boostProfile,held,pressed);};
 a.crouchBrake=[](float crouch,float brake){groundCrouchBrakeTargets(physicsState.crouch,physicsState.brake,crouch,brake,0,physicsState.turn.current);};
 a.turnTarget=[](float turn){groundTurnTarget(physicsState.turn,turn,physicsState.velocity,physicsProfile.surface.id);};
 a.railTurn=[](float balance){auto view=rail_view();originalRailSteerTarget(view,balance);rail_apply(view);}; //0x113F38
 a.upperReactions=[](){ //0x115B58 then 0x115D48
  ControllerDraws controllerDraws; // controller-phase draws (pass 121068)
  // 0x12FDF4: 115B58 plays a pending reaction first (a crash get-up's 314 into a press: PS2 hl-ai-9 1265, its 311710 variant draw)
  upper_request_play();
  OriginalUpperReactionContext context{graph.currentClass(1),physicsState.physicalForward,physicsState.reverseStance,animationTick+1,riderMask8C0,riderMask8D0};
  auto reaction=originalUpperReaction(idleSeconds,peers,context,[](){return rng.next();});if(reaction.semantic>=0)graph.enter(reaction.semantic,-1,reaction.mask);};
 return a;
}

// Controller slot of step_rider. Returns 1 when control 1 (or the cruise entry into it)
// was this tick's controller; step_rider then skips the cruise boost/target/animation work.
static int board_press_step(){
 if(bpRailTick){bpRailTick=false;return 1;} //control 1 ran in step_rails (the rail R3 ollie continues into motion 1 here)
 bpAirTransition=false;
 if(bpActive&&(physicsState.controlState!=1||crash.active||browserResetActive)){auto r=bp_view();originalBoardPressExit(r);bp_apply(r);bpActive=false;++bpExits;}
 if(!physicsAttached||crash.active||browserResetActive||browserStarting||browserSoftActive||railOwned||!padCommand.active)return 0;
 if(!bpActive){
  //0x131620 cruise: 0x1161D0 runs after the handplant/rail attempts and the 0x1162C8 jump request.
  if(padCommand.control!=0||physicsState.controlState!=0||!::grounded)return 0;
  const auto& f=padCommand.fields;if(f.jumpPressed||f.jumpHeld||f.resetPath)return 0;
  auto r=bp_view();auto a=bp_access(r);
  if(!originalBoardPressEntry(r,f.boardPress,a)){return 0;}
  bp_apply(r);++bpEntries;groundControllerRan=true;return 1; //cruise returned before 0x114130/0x113E80/0x115D48
 }
 if(padCommand.control!=1)return 0;
 ++bpUpdates;groundControllerRan=true;if(!::grounded)++bpAirborne;
 auto r=bp_view();auto a=bp_access(r);
 const auto result=originalBoardPressUpdate(bpControl,r,padCommand.words.word0,padCommand.words.word1,a);bpLastStop=int(result.stop);
 bp_apply(r);bp_publish_graph();return 1;
}
// ---- Control 1 on a rail (motion 4) ----
// 0x1161D0 from control 7 (0x131E80/0x131ED8; motion 4 skips the velocity test).
static void board_press_rail_begin(float press){
 bpAirTransition=false;auto r=bp_view();auto a=bp_access(r);
 if(!originalBoardPressEntry(r,press,a))throw std::runtime_error("Rail board press declined after 0x131D30 accepted it");
 bp_apply(r);bp_publish_graph();++bpEntries;
}
// 0x106D9C: the attach with a pending +0x330 already played 26/34 and set control 1; 0x12FC60.
static void board_press_rail_attach(){auto r=bp_view();originalBoardPressEnter(bpControl,r);prewind.jumpGate=r.jumpLatch;bpActive=true;++bpRailAttaches;}
// 0x12FC80 while riding the rail.
static void board_press_rail_update(){
 bpAirTransition=false;++bpUpdates;++bpRailTicks;groundControllerRan=true;
 auto r=bp_view();auto a=bp_access(r);
 const uint32_t w0=padCommand.active&&padCommand.control==1?padCommand.words.word0:0u,w1=padCommand.active&&padCommand.control==1?padCommand.words.word1:0u;
 const auto result=originalBoardPressUpdate(bpControl,r,w0,w1,a);bpLastStop=int(result.stop);bp_apply(r);bp_publish_graph();
 bpRailTick=!railOwned; //the R3 ollie released the rail: step_rider continues this tick without another controller
}
// 10EB30's 11FEC8(13) from control 1 (a collision crash during a press): the 12FE98 exit before the crash control enters.
// 11FEC8(next) from control 1 runs its exit 12FE98 (111578's table 0x456B90): a crash (13) or a reset (9, 116120).
static void board_press_control_exit(int next){
 if(!bpActive)return;
 auto r=bp_view();
 originalBoardPressExit(r);
 bp_apply(r);
 bpActive=false;
 bpLastExitControl=next;
 ++bpExits;
 bp_publish_graph();
}
static void board_press_crash_exit(){board_press_control_exit(13);}
// 10EB30 clears +0x330 with the 1F0/208/250 triplets.
static void board_press_clear_style(){bp330=0;graph.boardPress330=0;}
// ---- A soft collision during a board press (108388 from control 1) ----
// 108388 (0x1083D8..0x1083EC): in control 1 it first calls 131348 on the control-1 object (owner+0x1D0): the pivot spring
// 1313A8, the pivot finalise 131428 (which can flip the stance back, 12FEC8) and, for a nose / tail press, the end score
// 119A38 -> 10E098(rider, f, 1). Then it picks the clip (reading +0x320 after the cancel), plays it and calls 11FEC8(rider, 3),
// which runs the control-1 exit 12FE98 before control 3 enters. Before this the port ignored the collision and stayed in control 1.
static int board_press_soft_cancel(){
 if(!bpActive||physicsState.controlState!=1)return -1;
 auto r=bp_view();auto a=bp_access(r);originalBoardPressJumpOut(r,a);bp_apply(r);bp_publish_graph();
 return physicsState.reverseStance?1:0;
}
// After a reaction whose cancel ran: the soft clip and control 3 (begin_soft_control accepts control 1), then 12FE98.
static void board_press_soft_enter(int semantic,float spin){
 soft_collision_begin(semantic,spin);
 auto r=bp_view();originalBoardPressExit(r);bp_apply(r);bpActive=false;bpLastExitControl=3;++bpExits;bp_publish_graph();
}
// Landing 0x13A5D4: a pending +0x330 re-enters control 1 with 26/34 (control 1 itself keeps
// running) instead of 0x115640, the reverse turn and the landing animation. Returns true
// when the board-press branch handled the landing.
static bool board_press_landing(){
 if(bp330==0)return false;
 ++bpLandings;
 if(bpActive)return true;
 if(!graph.enter(originalBoardPressLandingSemantic(bp330)))throw std::runtime_error("Missing board-press landing animation");
 auto r=bp_view();bp_enter(r);bp_apply(r);bp_publish_graph();return true;
}
// Air controller 0x133308: +0x330 from the BoardPress step before the air step.
static void board_press_air_style(int step,float adjustFlip){bp330=originalBoardPressAirStyle(step,adjustFlip);graph.boardPress330=bp330;}

static void setup_board_press(){browserBoardPressStep=board_press_step;browserBoardPressReset=clear_board_press;browserBoardPressCancel=board_press_soft_cancel;}
// setup_board_press(): per rider context, rider_statics_animation (web/animation_bridge.cpp)

// ground_state_dump pairs (rider offset, value) for the float board-press fields.
static void board_press_state_dump(std::vector<float>& out){
 for(auto [off,v]:std::array<std::pair<unsigned,const GroundControlValue*>,2>{{{0x268,&bp268},{0x274,&bp274}}}){out.push_back(float(off));out.push_back(v->current);out.push_back(float(off+4));out.push_back(v->rate);out.push_back(float(off+8));out.push_back(v->target);}
 if(bpActive){out.push_back(float(0x360));out.push_back(prewind.jumpGate);} //cruise's own 0x1162C8 latch is not modelled
}
// QA: control-1 state and counters (source units).
extern "C" EMSCRIPTEN_KEEPALIVE float* board_press_info(){
 RIDER_LOCAL static float v[26];v[0]=bpActive;v[1]=bpControl.phase;v[2]=bpControl.time;v[3]=bpControl.fullTime;v[4]=bpControl.idleTime;v[5]=bpControl.ollieLatch;
 v[6]=bp330;v[7]=prewind.jumpGate;v[8]=bp268.current;v[9]=bp274.current;v[10]=physicsState.balance280.current;v[11]=physicsState.prewindStyle;
 v[12]=bpEntries;v[13]=bpUpdates;v[14]=bpOllies;v[15]=bpLandings;v[16]=bpEffects;v[17]=bpRumbles;v[18]=bpPivots;v[19]=bpExits;v[20]=bpLastExitControl;v[21]=bpLastStop;v[22]=bpAirborne;v[23]=bpLastAward;v[24]=bpRailTicks;v[25]=bpRailAttaches;
 return v;
}
