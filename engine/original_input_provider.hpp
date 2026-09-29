#pragma once
#include "input.hpp"
#include "original_input.hpp"
#include <array>
#include <cstdint>
namespace ssx {
// Original human input provider 0x127998 and the INPUT.MAP gameplay actions
// it reads. Action indices are the registration order in 0x226B60.
enum class OriginalAction : unsigned {
    CruiseTurn,CruiseCrouch,CruiseBrake,BoardPress,BoardPivot,AttackLeft,AttackRight,
    PrewindTurn,PrewindSpin,PrewindFlip,AirAdjRotFB,AirAdjRotLR,LateSpin,RailSpin,Spin,Flip,
    WipeoutRecover,JumpPressed,JumpHeld,BoostPressed,BoostHeld,Tweak,OllieHeld,RailBalance,
    HandplantBalance,Handplant,GateAnticipate,UberGrind1,UberGrind2,UberGrind3,UberGrind4,
    Trick1,Trick2,Trick3,Trick4,Trick5,Trick6,Trick7,Trick8,Trick9,Trick10,Trick11,Trick12,
    Trick13,Trick14,Trick15,ResetPath,Count
};
constexpr unsigned originalGameplayActionCount=unsigned(OriginalAction::Count); // 47
using OriginalActionValues=std::array<float,originalGameplayActionCount>;
const char* originalActionName(OriginalAction);

// Mapping VM 0x325450 result for the compiled INPUT.MAP expression, as returned
// by float getter 0x320BF0. Bare button names read the raw value; .held/.pressed
// read the history flags. Pad values must be finite (the driver never makes others).
float originalEvaluateAction(const OriginalPadState&,OriginalAction);
OriginalActionValues originalEvaluateActions(const OriginalPadState&);
// Boolean getter 0x320C48/0x321108: C.EQ.S against zero.
inline bool originalActionActive(float value){return value!=0.f;}
// 0x1276F0: first active Trick1..15 as 0..14, otherwise -1 (no grab).
int originalSelectedGrab(const OriginalActionValues&);
// 0x127848: first active UberGrind1..4 as 0..3, otherwise -1.
int originalRailUberSelection(const OriginalActionValues&);

struct OriginalCommandWords {
    uint32_t word0=0,word1=0;
    bool operator==(const OriginalCommandWords&) const=default;
};
// 0x127998 packing for the running controller state *(motionOwner+0xDE4)
// (getter 0x11FEE8). This is the only rider field the provider reads; states
// 9, 13 and anything outside 0..13 produce two zero words. Run-length bits
// (word0 & 0xFFF) are always zero.
OriginalCommandWords originalPackCommand(int controlState,const OriginalActionValues&);
OriginalCommandWords originalProvideCommand(int controlState,const OriginalPadState&);
// One consumed input sample: button history 0x321298, then provider 0x127998.
OriginalCommandWords originalHumanInputTick(OriginalPadState& history,const OriginalPadValues& sample,int controlState);
// 0 = DATA/CONFIG/INPUT.MAP (Default), 1 = INPUT2.MAP (Controller Settings "Pro"); the evaluation is per consumed sample.
void originalSetInputMap(int variant);int originalInputMap();

// Lossless decode of every provider-written field. Names are INPUT.MAP action
// names. Fields a state does not carry keep their defaults; `carried` has bit
// (1ull<<action) set for every action the state encodes. Bits the provider
// never writes for that state, run-length bits, and impossible grab/Uber/
// board-press codes throw.
struct OriginalCommandFields {
    int controlState=0;
    uint64_t carried=0;
    bool resetPath=false,handplant=false,jumpPressed=false,jumpHeld=false,boostPressed=false,boostHeld=false;
    bool attackLeft=false,attackRight=false,ollieHeld=false,tweak=false,lateSpin=false,wipeoutRecover=false;
    float cruiseTurn=0,cruiseCrouch=0,cruiseBrake=0,boardPress=0,boardPivot=0;
    float prewindTurn=0,prewindSpin=0,prewindFlip=0,airAdjRotFB=0,airAdjRotLR=0;
    float railSpin=0,spin=0,flip=0,railBalance=0,handplantBalance=0,gateAnticipate=0;
    int boardPressStep=0;    // control5 BoardPress tri-state: <-0.5 -> -1, >0.5 -> 1
    int grab=-1;             // control4/5 0x1276F0 index, -1 none
    int railUberIdentity=-1; // control7/12 0x127848 identity, -1 none
    bool carries(OriginalAction a) const {return carried>>unsigned(a)&1;}
};
OriginalCommandFields originalDecodeCommandFields(int controlState,uint32_t word0,uint32_t word1);
// RiderInput view of the same words for every state. For provider-produced
// words in states 0/2/3/4/5, every RiderInput field equals strict
// originalDecodeCommand's value whenever that decoder accepts the words; the
// provider bits it rejects fill the new fields instead of throwing. Bit12
// (ResetPath, Select.pressed) is recoverPressed. Per consumed sample:
//   auto w=originalHumanInputTick(history,originalDecodePad(packet),controlState);
//   RiderInput in=originalCommandRiderInput(controlState,w.word0,w.word1);
RiderInput originalCommandRiderInput(int controlState,uint32_t word0,uint32_t word1);
}
