// Original human input path for the browser: 24 decoded pad channels -> consumed-sample
// button history 0x321298 -> INPUT.MAP actions -> provider 0x127998 packing for the
// controller that is about to run -> per-controller command fields. One call per 60 Hz tick,
// before step_rider/animation_tick. It replaces host-side keyboard/gamepad mapping.
#include "../engine/original_input_provider.hpp"
extern "C" void ride_command(float,float,float,float,float,float,int,int,int,int);
extern "C" void request_rider_reset(int);RIDER_LOCAL extern bool browserResetFromController; // web/core.cpp

struct BrowserPadCommand {
 bool active=false;int control=0;ssx::OriginalCommandWords words;ssx::OriginalCommandFields fields;
};
RIDER_LOCAL static BrowserPadCommand padCommand;
RIDER_LOCAL static int cruiseLastControl=-1; // the controller of the last command (131608: control 0's entry clears the +0x360 jump latch)
// A placement's 11FEC8(0) (11DE60 / 11D660, reset_rider): control 0 is entered again, whatever ran before.
extern "C" void browser_cruise_enter(){cruiseLastControl=-1;}
// Control 2 on a rail (rider+0x328 set) steers the rail with RailBalance (word1 bits 6..11), not PrewindTurn (bits 0..5, the
// `steering` it passes off the rail): 12E9B8 clamps both to +-0.5 (f22 / f21) and calls 113F38(f21) at 12EC1C / 12EDF0. The
// human's INPUT.MAP packs the same stick into both; the computer-rider provider does not (R&B Allegra 700: word1 0xD).
// Without this tick's words (a host ride_command) the steering stands in.
static float rail_held_balance(float steering){return padCommand.active&&padCommand.control==2?std::clamp(padCommand.fields.railBalance,-.5f,.5f):steering;}
// A settled pad (0x321298 edge debounce edgeAge = 3, as in every PS2 savestate: the pad has been read for many frames
// before any race), so a button held from the first sample reports its press edge (tech-speedcap-groomed 338 boost).
static constexpr ssx::OriginalPadState settled_pad(){ssx::OriginalPadState s{};for(auto& b:s)b.edgeAge=3;return s;}
RIDER_LOCAL static ssx::OriginalPadState padHistory=settled_pad();
RIDER_LOCAL static float padOut[24];
static constexpr std::array<int,15> padGrabMasks={1,2,4,8,3,5,9,6,10,12,7,11,13,14,15};

// Controller state that runs next (motion owner+0xDE4 as read by getter 0x11FEE8).
static int browser_control_state(){
 if(browserNisHold)return 13; // 123640: 11FEC8(13), the NIS hold (web/core.cpp nis_hold)
 if(browserResetActive)return 9;
 if(crash.active)return 8;
 if(browserStarting)return 6;
 return gs.controlState;
}

// QA: the controller about to run (motion owner +0xDE4) and the +0x360 jump latch
extern "C" EMSCRIPTEN_KEEPALIVE float* rider_control_info(){RIDER_LOCAL static float v[3];v[0]=float(browser_control_state());v[1]=prewind.jumpGate;v[2]=float(cruiseLastControl);return v;}
extern "C" EMSCRIPTEN_KEEPALIVE void reset_pad_history(){padHistory=settled_pad();padCommand={};cruiseLastControl=-1;}
// One pad sample consumed outside the game tick (pv padCarry): cSSXApp_preUpdate (0x227F20) runs 0x321298 on the port's history once per
// app update with a pending sample, whatever runs (the front end, a card, the pause menu, the results), and the game's provider reads the
// same history. So a button pressed in a menu and still held when the ride resumes is held there without a new press edge (PS2 c0a-ret2:
// the Give Up's Yes Cross, menu samples 354..362, gives crouch target 1 and no prewind on the resumed tick 1699). The history only: no
// INPUT.MAP, no provider (no game tick runs).
extern "C" EMSCRIPTEN_KEEPALIVE void pad_history_sample(const float* values){
 ssx::OriginalPadValues sample;
 for(unsigned i=0;i<24;i++){if(!std::isfinite(values[i])||values[i]<0||values[i]>1)throw std::runtime_error("Invalid original pad channel");sample[i]=values[i];}
 ssx::originalUpdatePad(padHistory,sample);
}
// FE Controller Settings (web/fe-options.js): 0 Default = INPUT.MAP, 1 Pro = INPUT2.MAP (engine/original_input_provider.cpp).
extern "C" EMSCRIPTEN_KEEPALIVE void set_input_map(int variant){ssx::originalSetInputMap(variant);}
RIDER_LOCAL static bool rideLateSpin=false; // control5 LateSpin (word0 bit15) for the air controller's late-spin modes (animation_bridge.cpp)
static float* apply_command_words(int control,ssx::OriginalCommandWords words);
extern "C" EMSCRIPTEN_KEEPALIVE float* pad_tick(const float* values){
 ssx::OriginalPadValues sample;
 for(unsigned i=0;i<24;i++){if(!std::isfinite(values[i])||values[i]<0||values[i]>1)throw std::runtime_error("Invalid original pad channel");sample[i]=values[i];}
 const int control=browser_control_state();
 return apply_command_words(control,ssx::originalHumanInputTick(padHistory,sample,control));
}
// Per-controller command fields for the controller about to run. The human pad provider and the
// computer-rider provider 0x10A768 (web/npc_gameplay.inc) produce the same packed layouts.
static float* apply_command_words(int control,ssx::OriginalCommandWords words){
 const auto f=ssx::originalDecodeCommandFields(control,words.word0,words.word1);
 padCommand={true,control,words,f};
 float turn=0,crouch=0,brake=0,board=0,spin=0,flip=0,animTurn=0,animBrake=0,animFlip=0;int jump=0,boost=0,grab=0,apply=1,tweak=0;
 rideLateSpin=false;
 int railMask=0;float railRotate=0,railFlip=0;
 switch(control){
  case 0: //131620 cruise
   turn=f.cruiseTurn;crouch=f.cruiseCrouch;brake=f.cruiseBrake;board=f.boardPress;jump=f.jumpHeld;boost=f.boostHeld;
   // Control0 returns right after requesting control2 (1162C8 from 131794: a press, or a held jump while the latch +0x360 is 0),
   // before its cruise targets. Control-0 entry 131608 clears the latch (a crash exit or a landing with Cross still held requests
   // control 2); 1162C8's no-request return sets it. A jump held without a request (the press consumed elsewhere: a menu's, with
   // the history carried through it, core pad_history_sample) leaves 131620 at its cruise targets with 0x8000's crouch 1 and brake 0
   // (0x1317B4) and no control 2 (PS2 c0a-ret2 tick 1699: word0 0x8000, crouch target 1, control 0 then the finish's 10).
   if(cruiseLastControl!=0)prewind.jumpGate=0; // 131608 at control 0's entry (any other controller ran since its last tick)
   if(ssx::originalCrouchRequest(prewind.jumpGate,f.jumpHeld,f.jumpPressed))apply=0;
   else {jump=0;if(f.jumpHeld){crouch=1;brake=0;}}
   animTurn=turn;animBrake=brake;break;
  case 1: //12FC80 board press: the controller decodes its own words (web/boardpress_gameplay.inc).
   turn=f.cruiseTurn;board=f.boardPress;jump=f.jumpHeld;boost=f.boostHeld;animTurn=turn;break;
  case 2: //12E9B8 prewind
   turn=std::clamp(f.prewindTurn,-.5f,.5f);crouch=f.jumpHeld?1.f:0.f;spin=f.prewindSpin;flip=f.prewindFlip;jump=f.jumpHeld;boost=f.boostHeld;
   animTurn=turn;animFlip=flip;railFlip=f.prewindFlip;break;
  case 3: //12E778 soft collision
   turn=f.cruiseTurn;boost=f.boostHeld;animTurn=turn;break;
  case 4: //12F730 passive air; the controller decodes its own words below.
   turn=f.cruiseTurn;crouch=f.cruiseCrouch;grab=f.grab>=0?padGrabMasks.at(f.grab):0;animTurn=turn;break;
  case 5: //1333E0 air
   turn=f.airAdjRotLR;crouch=std::max(f.airAdjRotFB,0.f);brake=std::max(-f.airAdjRotFB,0.f);board=float(f.boardPressStep);
   spin=f.spin;flip=f.flip;boost=f.tweak;tweak=f.tweak;grab=f.grab>=0?padGrabMasks.at(f.grab):0;rideLateSpin=f.lateSpin;
   animTurn=turn;animBrake=brake;animFlip=flip;break;
  case 6: startAxis=f.gateAnticipate;startCommand=words.word0;startCommandFromPad=true;break;
  case 7: //rail control: balance is the left stick only; D-pad is RailSpin.
   turn=f.railBalance;board=f.boardPress;jump=f.jumpHeld;boost=f.boostHeld;railRotate=f.railSpin;
   railMask=f.railUberIdentity>=0?1<<f.railUberIdentity:0;animTurn=turn;break;
  case 12:
   turn=f.railBalance;boost=f.boostHeld;railMask=f.railUberIdentity>=0?1<<f.railUberIdentity:0;animTurn=turn;break;
  default:break; //8 crash reads its word directly; 9 reset has no command; 11 handplant not yet ported.
 }
 ride_command(turn,crouch,brake,board,spin,flip,jump,boost,grab,apply);
 railShoulderMask=railMask;railRotationInput=railRotate;railFlipInput=railFlip;railBalanceInput=f.railBalance; //RailBalance of controls 1/2/3/7/12 (soft control 3 on a rail reads it, not CruiseTurn)
 // Select.pressed (ResetPath, bit12) is packed by every controller except 6, 9, 10, 13.
 if(f.resetPath&&!browserResetActive){request_rider_reset(0);browserResetFromController=true;}
 padOut[0]=turn;padOut[1]=crouch;padOut[2]=brake;padOut[3]=board;padOut[4]=spin;padOut[5]=flip;padOut[6]=jump;padOut[7]=boost;
 padOut[8]=grab;padOut[9]=apply;padOut[10]=animTurn;padOut[11]=animBrake;padOut[12]=tweak;padOut[13]=animFlip;padOut[14]=control;
 padOut[15]=float(words.word0&0xffff);padOut[16]=float(words.word0>>16);padOut[17]=float(words.word1&0xffff);padOut[18]=float(words.word1>>16);
 padOut[19]=f.resetPath;padOut[20]=f.wipeoutRecover;padOut[21]=f.handplant;padOut[22]=float(int(f.attackLeft)|int(f.attackRight)<<1);padOut[23]=f.jumpPressed;
 cruiseLastControl=control;
 return padOut;
}

// 1285B8..1285D8: control8 packs WipeoutRecover (Square.pressed) at bit13; jump is not in the word.
static uint32_t crash_command_word(bool jump){return padCommand.active&&padCommand.control==8?padCommand.words.word0:(jump?0x2000u:0u);}
// 12F730 decodes its own words: CruiseTurn, CruiseCrouch, signed grab identity, handplant, attacks.
static OriginalPassiveAirCommand passive_command(float turn,int jumpHeld){
 if(padCommand.active&&padCommand.control==4)return originalDecodePassiveAirCommand(padCommand.words.word0,padCommand.words.word1);
 return {turn,jumpHeld?1.f:0.f,-1,false,false,false,false};
}
// QA: cached collision/world pose (source cm, Z up) as position xyz + quaternion xyzw per bone.
extern "C" EMSCRIPTEN_KEEPALIVE float* world_pose_bones(){RIDER_LOCAL static std::vector<float> out;out.assign(1+cachedCrashWorld.size()*7,0);out[0]=float(cachedCrashWorld.size());
 for(size_t b=0;b<cachedCrashWorld.size();b++){for(unsigned k=0;k<3;k++)out[1+b*7+k]=cachedCrashWorld[b].position[k];for(unsigned k=0;k<4;k++)out[1+b*7+3+k]=cachedCrashWorld[b].rotation[k];}return out.data();}
static void board_press_state_dump(std::vector<float>&);
static void animation_state_dump(std::vector<float>& out);
// QA (compare-ps2-capture.mjs): set physicsState words by rider offset, (offset, value) pairs, for a capture that starts from a
// state the browser cannot ride into (a CTM countdown savestate: the rider's free-ride / hold history, docs/ctm-events-in-world.md).
// The vectors and filter triplets of ground_state_dump; any other offset throws.
extern "C" EMSCRIPTEN_KEEPALIVE void ground_state_seed(const float* pairs,int count){
 auto slot=[&](OriginalGroundState& s,unsigned off)->float*{
  auto v3=[&](unsigned base,std::array<float,3>& v)->float*{return off>=base&&off<base+12&&(off-base)%4==0?&v[(off-base)/4]:nullptr;};
  auto t3=[&](unsigned base,GroundControlValue& t)->float*{return off==base?&t.current:off==base+4?&t.rate:off==base+8?&t.target:nullptr;};
  for(auto* p:{v3(0x110,s.position),v3(0x1E0,s.velocity),v3(0x1C0,s.boardUp),v3(0x1B0,s.physicalForward),v3(0x370,s.normal),v3(0x380,s.previousNormal),
    v3(0x390,s.boardNormal),v3(0x3A0,s.forward),v3(0x3B0,s.lateral),v3(0x3D0,s.surfaceVelocity),v3(0x180,s.presentationUp),
    t3(0x1F0,s.turn),t3(0x208,s.extraLean),t3(0x214,s.brake),t3(0x220,s.crouch),t3(0x2BC,s.boardAlignment),t3(0x2C8,s.presentationLift),
    t3(0x280,s.balance280),t3(0x28C,s.adjustment28C),t3(0x298,s.adjustment298),t3(0x1FC,s.animationTurn),t3(0x250,s.presentationRoll)})if(p)return p;
  if(off==0x454)return &s.distance;if(off==0x31C)return &s.boardLift;if(off==0x4CC)return &s.headingOffset;
  return nullptr;};
 for(int k=0;k<count;k++){const unsigned off=unsigned(pairs[2*k]);float* p=slot(physicsState,off);if(!p)throw std::runtime_error("ground_state_seed: unsupported rider offset");*p=pairs[2*k+1];*slot(gs,off)=pairs[2*k+1];} // gs: the animation side's copy
}
// QA: physicsState laid out as (rider offset, value) pairs for field-level PS2 comparison.
extern "C" EMSCRIPTEN_KEEPALIVE float* ground_state_dump(){
 RIDER_LOCAL static std::vector<float> out;out.clear();const auto& s=physicsState;
 auto v3=[&](unsigned off,const std::array<float,3>& v){for(unsigned k=0;k<3;k++){out.push_back(float(off+4*k));out.push_back(v[k]);}};
 auto t3=[&](unsigned off,const GroundControlValue& t){out.push_back(float(off));out.push_back(t.current);out.push_back(float(off+4));out.push_back(t.rate);out.push_back(float(off+8));out.push_back(t.target);};
 auto f1=[&](unsigned off,float x){out.push_back(float(off));out.push_back(x);};
 v3(0x110,s.position);for(unsigned k=0;k<4;k++)f1(0x120+4*k,s.quaternion[k]);v3(0x1E0,s.velocity);v3(0x1C0,s.boardUp);v3(0x1B0,s.physicalForward);
 t3(0x1F0,s.turn);t3(0x208,s.extraLean);t3(0x214,s.brake);t3(0x220,s.crouch);t3(0x2BC,s.boardAlignment);t3(0x2C8,s.presentationLift);
 t3(0x280,s.balance280);t3(0x28C,s.adjustment28C);t3(0x298,s.adjustment298);
 v3(0x370,s.normal);v3(0x380,s.previousNormal);v3(0x390,s.boardNormal);v3(0x3A0,s.forward);v3(0x3B0,s.lateral);v3(0x3D0,s.surfaceVelocity);
 v3(0x180,s.presentationUp);f1(0x454,s.distance);f1(0x2E4,physicsProfile.speedLimit);f1(0x2FC,s.boost);f1(0x300,s.timeScale);f1(0x2DC,s.manualSpin);f1(0x31C,s.boardLift);f1(0x35C,idleSeconds);v3(0x3E0,collisionHistory.previousNormal);f1(0x3F0,collisionHistory.directionChanges);f1(0x3F4,collisionHistory.secondaryCounter);
 f1(0x470,s.modeTiming);f1(0x4CC,s.headingOffset); // 13C948 reads both (+0x470 finish marker, +0x4CC route heading)
 t3(0x1FC,s.animationTurn);t3(0x250,s.presentationRoll);
 board_press_state_dump(out); //+0x268/+0x274 triplets, +0x360 latch (web/boardpress_gameplay.inc)
 animation_state_dump(out); //+0x244 handplant lean, +0x2A4/+0x2B0 prewind spin/flip (end of animation_bridge.cpp)
 out.insert(out.begin(),float(out.size()/2));return out.data();
}
