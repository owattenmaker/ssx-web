#include "original_input_provider.hpp"
#include "software_float.hpp"
#include <algorithm>
#include <bit>
#include <cfenv>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>
#pragma STDC FENV_ACCESS ON
namespace ssx {
namespace {
// The oracle runs the recompiled EE code under FE_TOWARDZERO. WebAssembly
// ignores fesetround, so SUB.S/MUL.S use explicit toward-zero helpers there;
// SSX_ORIGINAL_INPUT_SOFTWARE_FLOAT exercises that path in native oracles.
#if defined(__EMSCRIPTEN__) || defined(SSX_ORIGINAL_INPUT_SOFTWARE_FLOAT)
struct Round {int old=std::fegetround();Round(){if(std::fesetround(FE_TONEAREST))throw std::runtime_error("Provider rounding");}~Round(){std::fesetround(old);}};
float rzSub(float a,float b){return software_float::sub(a,b);}
float rzMul(float a,float b){return software_float::mul(a,b);}
#else
struct Round {int old=std::fegetround();Round(){if(std::fesetround(FE_TOWARDZERO))throw std::runtime_error("Provider rounding");}~Round(){std::fesetround(old);}};
float rzSub(float a,float b){volatile float x=a,y=b;return x-y;}
float rzMul(float a,float b){volatile float x=a,y=b;return x*y;}
#endif
using A=OriginalAction;
enum Button : unsigned {Select,Start,L3,R3,DPadR,DPadL,DPadU,DPadD,Triangle,Circle,Cross,Square,
    L1,R1,L2,R2,RStickL,RStickR,RStickU,RStickD,LStickL,LStickR,LStickU,LStickD};
constexpr float reciprocal31=std::bit_cast<float>(0x3d042108u);
constexpr std::array<uint8_t,15> trickMasks={1,2,4,8,3,5,9,6,10,12,7,11,13,14,15}; // L1=1,L2=2,R1=4,R2=8
constexpr std::array<const char*,originalGameplayActionCount> names={
    "CruiseTurn","CruiseCrouch","CruiseBrake","BoardPress","BoardPivot","AttackLeft","AttackRight",
    "PrewindTurn","PrewindSpin","PrewindFlip","AirAdjRotFB","AirAdjRotLR","LateSpin","RailSpin","Spin","Flip",
    "WipeoutRecover","JumpPressed","JumpHeld","BoostPressed","BoostHeld","Tweak","OllieHeld","RailBalance",
    "HandplantBalance","Handplant","GateAnticipate","UberGrind1","UberGrind2","UberGrind3","UberGrind4",
    "Trick1","Trick2","Trick3","Trick4","Trick5","Trick6","Trick7","Trick8","Trick9","Trick10","Trick11",
    "Trick12","Trick13","Trick14","Trick15","ResetPath"};
// 0x325344..0x325398: uint32 flag -> CVT.S.W (unsigned fixup doubles a halved
// value). Toward zero this is truncation to 24 significant bits.
float flag(uint32_t value){
    if(!value)return 0.f;
    int top=31-std::countl_zero(value);
    if(top>23)value&=~((1u<<(top-23))-1u);
    return float(value);
}
// Opcode16 (0x3259C8): C.LT.S right<left ? left : right.
float vmMax(float left,float right){return right<left?left:right;}
// Opcodes 31 (!) and 5 (&&) produce 1.0/0.0 from C.EQ.S zero tests.
bool truthy(float value){return value!=0.f;}
unsigned shoulderMask(const OriginalPadState& p){
    return (truthy(flag(p[L1].held))?1u:0u)|(truthy(flag(p[L2].held))?2u:0u)|
           (truthy(flag(p[R1].held))?4u:0u)|(truthy(flag(p[R2].held))?8u:0u);
}
// Controller Settings "Pro" (profile controller preset, FE 22control): DATA/CONFIG/INPUT2.MAP differs from
// INPUT.MAP only in these gameplay expressions (the rest, incl. the Trick/UberGrind chords, is identical):
//   PrewindTurn=0; PrewindSpin=Turn; PrewindFlip=Tilt; RailBalance=!Cross?Turn:0; HandplantBalance=Turn;
//   LateSpin=Cross; RailSpin=ABS(Tilt)<0.5f||ABS(Turn)>0.2f?0:Tilt; Spin=0; Flip=0; AirAdjRotLR=Turn;
//   AirAdjRotFB=Tilt; Handplant=Triangle  (Turn/Tilt = MAX(DPad,LStick) differences as in INPUT.MAP).
constinit thread_local int inputMapVariant=0; // per rider context (web/rider_local.hpp)
bool proExpression(const OriginalPadState& p,A action,float& out){
    auto v=[&](Button b){return p[b].value;};
    auto turn=[&](){return rzSub(vmMax(v(DPadR),v(LStickR)),vmMax(v(DPadL),v(LStickL)));};
    auto tilt=[&](){return rzSub(vmMax(v(DPadU),v(LStickU)),vmMax(v(DPadD),v(LStickD)));};
    switch(action){
    case A::PrewindTurn:case A::Spin:case A::Flip:out=0.f;return true;
    case A::PrewindSpin:case A::HandplantBalance:case A::AirAdjRotLR:out=turn();return true;
    case A::PrewindFlip:case A::AirAdjRotFB:out=tilt();return true;
    case A::RailBalance:out=truthy(v(Cross))?0.f:turn();return true;
    case A::LateSpin:out=v(Cross);return true;
    case A::RailSpin:{float t=tilt(),r=turn();out=(std::fabs(t)<0.5f||std::fabs(r)>std::bit_cast<float>(0x3e4ccccdu))?0.f:t;return true;}
    case A::Handplant:out=v(Triangle);return true;
    default:return false;
    }
}
float evaluate(const OriginalPadState& p,A action){
    auto v=[&](Button b){return p[b].value;};
    if(inputMapVariant==1){float pro;if(proExpression(p,action,pro))return pro;}
    switch(action){
    case A::CruiseTurn:return rzSub(vmMax(v(DPadR),v(LStickR)),vmMax(v(DPadL),v(LStickL)));
    case A::CruiseCrouch:return vmMax(v(DPadU),v(LStickU));
    case A::CruiseBrake:return vmMax(v(DPadD),v(LStickD));
    case A::BoardPress:return rzSub(v(RStickU),v(RStickD));
    case A::BoardPivot:return rzSub(v(RStickR),v(RStickL));
    case A::AttackLeft:return vmMax(v(L1),v(L2));
    case A::AttackRight:return vmMax(v(R1),v(R2));
    case A::PrewindTurn:case A::AirAdjRotLR:case A::RailBalance:case A::HandplantBalance:
        return rzSub(v(LStickR),v(LStickL));
    case A::PrewindSpin:case A::RailSpin:case A::Spin:return rzSub(v(DPadR),v(DPadL));
    case A::PrewindFlip:case A::Flip:return rzSub(v(DPadU),v(DPadD));
    case A::AirAdjRotFB:return rzSub(v(LStickU),v(LStickD));
    case A::LateSpin:return 0.f; // compiled literal 0x00000000
    case A::WipeoutRecover:case A::BoostPressed:return flag(p[Square].pressed);
    case A::JumpPressed:return flag(p[Cross].pressed);
    case A::JumpHeld:return v(Cross);
    case A::BoostHeld:case A::Tweak:return v(Square);
    case A::OllieHeld:return v(R3);
    case A::Handplant:return v(Circle);
    case A::GateAnticipate:return rzSub(vmMax(v(DPadU),v(LStickU)),vmMax(v(DPadD),v(LStickD)));
    case A::ResetPath:return flag(p[Select].pressed);
    default:break;
    }
    unsigned index=unsigned(action);
    // UberGrind1..4 and Trick1..15 are exclusive held-flag && / ! chords.
    if(index>=unsigned(A::UberGrind1)&&index<=unsigned(A::UberGrind4))
        return shoulderMask(p)==trickMasks[index-unsigned(A::UberGrind1)]?1.f:0.f;
    if(index>=unsigned(A::Trick1)&&index<=unsigned(A::Trick15))
        return shoulderMask(p)==trickMasks[index-unsigned(A::Trick1)]?1.f:0.f;
    throw std::runtime_error("Unknown original gameplay action");
}
void requireFinite(const OriginalPadState& p){
    for(const auto& b:p)if(!std::isfinite(b.value))throw std::runtime_error("Nonfinite original pad value");
}
// MUL.S by 31.0 (0x41F8) then CVT.W.S (truncation, saturating) and six-bit field.
uint32_t six(float value){
    float scaled=rzMul(value,31.f);int32_t whole;
    if(scaled>=2147483648.f)whole=std::numeric_limits<int32_t>::max();
    else if(scaled<-2147483648.f)whole=std::numeric_limits<int32_t>::min();
    else whole=int32_t(scaled);
    return uint32_t(whole)&63u;
}
float axis(uint32_t word,unsigned shift){int value=int((word>>shift)&63);if(value>=32)value-=64;return rzMul(float(value),reciprocal31);}
void require(bool valid,const char* what){if(!valid)throw std::runtime_error(what);}
uint64_t bits(std::initializer_list<A> actions){uint64_t r=0;for(auto a:actions)r|=1ull<<unsigned(a);return r;}
}
const char* originalActionName(OriginalAction action){
    if(unsigned(action)>=originalGameplayActionCount)throw std::runtime_error("Unknown original gameplay action");
    return names[unsigned(action)];
}
float originalEvaluateAction(const OriginalPadState& pad,OriginalAction action){
    requireFinite(pad);Round round;return evaluate(pad,action);
}
OriginalActionValues originalEvaluateActions(const OriginalPadState& pad){
    requireFinite(pad);Round round;OriginalActionValues out{};
    for(unsigned i=0;i<originalGameplayActionCount;i++)out[i]=evaluate(pad,A(i));
    return out;
}
int originalSelectedGrab(const OriginalActionValues& a){
    for(unsigned i=0;i<15;i++)if(originalActionActive(a[unsigned(A::Trick1)+i]))return int(i);
    return -1;
}
int originalRailUberSelection(const OriginalActionValues& a){
    for(unsigned i=0;i<4;i++)if(originalActionActive(a[unsigned(A::UberGrind1)+i]))return int(i);
    return -1;
}
OriginalCommandWords originalPackCommand(int controlState,const OriginalActionValues& a){
    for(float value:a)if(!std::isfinite(value))throw std::runtime_error("Nonfinite original action value");
    Round round;OriginalCommandWords w;
    auto on=[&](A action,unsigned bit){return originalActionActive(a[unsigned(action)])?1u<<bit:0u;};
    auto q=[&](A action,unsigned shift){return six(a[unsigned(action)])<<shift;};
    auto byte=[](int value,unsigned shift){return (uint32_t(value)&255u)<<shift;};
    switch(controlState){
    case 0: // 0x1279F4 cruise (update 0x131620)
        w.word0=on(A::ResetPath,12)|on(A::Handplant,13)|on(A::JumpPressed,14)|on(A::JumpHeld,15)|
            on(A::BoostPressed,16)|on(A::BoostHeld,17)|on(A::AttackLeft,18)|on(A::AttackRight,19)|
            q(A::CruiseTurn,20)|q(A::CruiseCrouch,26);
        w.word1=q(A::CruiseBrake,0)|q(A::BoardPress,6);break;
    case 1: // 0x127C68 (update 0x12FC80)
        w.word0=on(A::ResetPath,12)|on(A::BoostPressed,13)|on(A::BoostHeld,14)|on(A::JumpHeld,15)|
            on(A::JumpPressed,16)|on(A::OllieHeld,17)|q(A::CruiseTurn,18)|q(A::RailBalance,24);
        w.word1=q(A::BoardPress,0)|q(A::BoardPivot,6);break;
    case 2: // 0x12803C prewind (update 0x12E9B8)
        w.word0=on(A::ResetPath,12)|on(A::JumpHeld,13)|on(A::BoostHeld,14)|q(A::PrewindSpin,15)|q(A::PrewindFlip,21);
        w.word1=q(A::PrewindTurn,0)|q(A::RailBalance,6);break;
    case 3: // 0x128194 soft collision (update 0x12E778)
        w.word0=on(A::ResetPath,12)|on(A::BoostPressed,13)|on(A::BoostHeld,14)|q(A::CruiseTurn,15)|q(A::RailBalance,21);break;
    case 4: // 0x128280 passive air (update 0x12F730); SB of 0x1276F0 at byte2
        w.word0=on(A::ResetPath,12)|on(A::Handplant,13)|on(A::AttackLeft,14)|on(A::AttackRight,15)|
            byte(originalSelectedGrab(a),16)|q(A::CruiseTurn,24);
        w.word1=q(A::CruiseCrouch,0);break;
    case 5: { // 0x1283A4 air (update 0x133308)
        float press=a[unsigned(A::BoardPress)];
        int step=!(-.5f<=press)?-1:(.5f<press?1:0);
        w.word0=on(A::ResetPath,12)|on(A::Handplant,13)|on(A::Tweak,14)|on(A::LateSpin,15)|
            byte(originalSelectedGrab(a),16)|q(A::Spin,24);
        w.word1=q(A::Flip,0)|q(A::AirAdjRotFB,6)|q(A::AirAdjRotLR,12)|((uint32_t(step)&3u)<<18);break;
    }
    case 6: // 0x127DFC race start (update 0x12BF68); no ResetPath bit
        w.word0=q(A::GateAnticipate,12);break;
    case 7: // 0x127E08 rail (update 0x131D30)
        w.word0=on(A::ResetPath,12)|on(A::JumpPressed,13)|on(A::JumpHeld,14)|on(A::BoostPressed,15)|
            on(A::BoostHeld,16)|byte(originalRailUberSelection(a),17)|q(A::RailBalance,25);
        w.word1=q(A::RailSpin,0)|q(A::BoardPress,6);break;
    case 8: // 0x128598 crash (update 0x12CB68)
        w.word0=on(A::ResetPath,12)|on(A::WipeoutRecover,13);break;
    case 10: // 0x1285EC (update 0x12C678)
        w.word0=q(A::CruiseTurn,12);break;
    case 11: // 0x127BDC handplant (update 0x132A30)
        w.word0=on(A::ResetPath,12)|on(A::Handplant,13)|q(A::HandplantBalance,14);break;
    case 12: // 0x127F60 rail Uber (update 0x136508)
        w.word0=on(A::ResetPath,12)|on(A::BoostPressed,13)|on(A::BoostHeld,14)|
            byte(originalRailUberSelection(a),15)|q(A::RailBalance,23);break;
    default:break; // 9 reset, 13 transition and >=14 unsigned: 0x128630 with zero words
    }
    return w;
}
OriginalCommandWords originalProvideCommand(int controlState,const OriginalPadState& pad){
    return originalPackCommand(controlState,originalEvaluateActions(pad));
}
void originalSetInputMap(int variant){if(variant<0||variant>1)throw std::runtime_error("Unknown original input map");inputMapVariant=variant;}
int originalInputMap(){return inputMapVariant;}
OriginalCommandWords originalHumanInputTick(OriginalPadState& history,const OriginalPadValues& sample,int controlState){
    originalUpdatePad(history,sample);return originalProvideCommand(controlState,history);
}
OriginalCommandFields originalDecodeCommandFields(int state,uint32_t a,uint32_t b){
    Round round;OriginalCommandFields f;f.controlState=state;
    require(!(a&0xfff),"Original command still contains run-length bits");
    auto flagBit=[&](uint32_t word,unsigned bit){return bool(word>>bit&1);};
    auto signedByte=[&](uint32_t word,unsigned shift,int low,int high,const char* what){
        int value=int8_t((word>>shift)&255);require(value>=low&&value<=high,what);return value;};
    auto only=[&](uint32_t used0,uint32_t used1){require(!(a&~used0)&&!(b&~used1),"Original command bits outside controller layout");};
    constexpr uint32_t f6=63;
    switch(state){
    case 0:
        only(0xfffff000u,0xfffu);
        f.carried=bits({A::ResetPath,A::Handplant,A::JumpPressed,A::JumpHeld,A::BoostPressed,A::BoostHeld,A::AttackLeft,A::AttackRight,A::CruiseTurn,A::CruiseCrouch,A::CruiseBrake,A::BoardPress});
        f.resetPath=flagBit(a,12);f.handplant=flagBit(a,13);f.jumpPressed=flagBit(a,14);f.jumpHeld=flagBit(a,15);
        f.boostPressed=flagBit(a,16);f.boostHeld=flagBit(a,17);f.attackLeft=flagBit(a,18);f.attackRight=flagBit(a,19);
        f.cruiseTurn=axis(a,20);f.cruiseCrouch=axis(a,26);f.cruiseBrake=axis(b,0);f.boardPress=axis(b,6);break;
    case 1:
        only(0x3ffff000u,0xfffu);
        f.carried=bits({A::ResetPath,A::BoostPressed,A::BoostHeld,A::JumpHeld,A::JumpPressed,A::OllieHeld,A::CruiseTurn,A::RailBalance,A::BoardPress,A::BoardPivot});
        f.resetPath=flagBit(a,12);f.boostPressed=flagBit(a,13);f.boostHeld=flagBit(a,14);f.jumpHeld=flagBit(a,15);
        f.jumpPressed=flagBit(a,16);f.ollieHeld=flagBit(a,17);f.cruiseTurn=axis(a,18);f.railBalance=axis(a,24);
        f.boardPress=axis(b,0);f.boardPivot=axis(b,6);break;
    case 2:
        only(0x07fff000u,0xfffu);
        f.carried=bits({A::ResetPath,A::JumpHeld,A::BoostHeld,A::PrewindSpin,A::PrewindFlip,A::PrewindTurn,A::RailBalance});
        f.resetPath=flagBit(a,12);f.jumpHeld=flagBit(a,13);f.boostHeld=flagBit(a,14);
        f.prewindSpin=axis(a,15);f.prewindFlip=axis(a,21);f.prewindTurn=axis(b,0);f.railBalance=axis(b,6);break;
    case 3:
        only(0x07fff000u,0);
        f.carried=bits({A::ResetPath,A::BoostPressed,A::BoostHeld,A::CruiseTurn,A::RailBalance});
        f.resetPath=flagBit(a,12);f.boostPressed=flagBit(a,13);f.boostHeld=flagBit(a,14);
        f.cruiseTurn=axis(a,15);f.railBalance=axis(a,21);break;
    case 4:
        only(0x3ffff000u,f6);
        f.carried=bits({A::ResetPath,A::Handplant,A::AttackLeft,A::AttackRight,A::CruiseTurn,A::CruiseCrouch})|(0x7fffull<<unsigned(A::Trick1));
        f.resetPath=flagBit(a,12);f.handplant=flagBit(a,13);f.attackLeft=flagBit(a,14);f.attackRight=flagBit(a,15);
        f.grab=signedByte(a,16,-1,14,"Invalid original grab index");f.cruiseTurn=axis(a,24);f.cruiseCrouch=axis(b,0);break;
    case 5:
        only(0x3ffff000u,0xfffffu);
        f.carried=bits({A::ResetPath,A::Handplant,A::Tweak,A::LateSpin,A::Spin,A::Flip,A::AirAdjRotFB,A::AirAdjRotLR,A::BoardPress})|(0x7fffull<<unsigned(A::Trick1));
        f.resetPath=flagBit(a,12);f.handplant=flagBit(a,13);f.tweak=flagBit(a,14);f.lateSpin=flagBit(a,15);
        f.grab=signedByte(a,16,-1,14,"Invalid original grab index");f.spin=axis(a,24);
        f.flip=axis(b,0);f.airAdjRotFB=axis(b,6);f.airAdjRotLR=axis(b,12);
        f.boardPressStep=int((b>>18)&3);require(f.boardPressStep!=2,"Invalid original airborne board press");
        if(f.boardPressStep==3)f.boardPressStep=-1;
        f.boardPress=float(f.boardPressStep);break;
    case 6:
        only(0x0003f000u,0);f.carried=bits({A::GateAnticipate});f.gateAnticipate=axis(a,12);break;
    case 7:
        only(0x7ffff000u,0xfffu);
        f.carried=bits({A::ResetPath,A::JumpPressed,A::JumpHeld,A::BoostPressed,A::BoostHeld,A::UberGrind1,A::UberGrind2,A::UberGrind3,A::UberGrind4,A::RailBalance,A::RailSpin,A::BoardPress});
        f.resetPath=flagBit(a,12);f.jumpPressed=flagBit(a,13);f.jumpHeld=flagBit(a,14);f.boostPressed=flagBit(a,15);f.boostHeld=flagBit(a,16);
        f.railUberIdentity=signedByte(a,17,-1,3,"Invalid original Uber identity");
        f.railBalance=axis(a,25);f.railSpin=axis(b,0);f.boardPress=axis(b,6);break;
    case 8:
        only(0x3000u,0);f.carried=bits({A::ResetPath,A::WipeoutRecover});
        f.resetPath=flagBit(a,12);f.wipeoutRecover=flagBit(a,13);break;
    case 10:
        only(0x0003f000u,0);f.carried=bits({A::CruiseTurn});f.cruiseTurn=axis(a,12);break;
    case 11:
        only(0x000ff000u,0);f.carried=bits({A::ResetPath,A::Handplant,A::HandplantBalance});
        f.resetPath=flagBit(a,12);f.handplant=flagBit(a,13);f.handplantBalance=axis(a,14);break;
    case 12:
        only(0x1ffff000u,0);
        f.carried=bits({A::ResetPath,A::BoostPressed,A::BoostHeld,A::UberGrind1,A::UberGrind2,A::UberGrind3,A::UberGrind4,A::RailBalance});
        f.resetPath=flagBit(a,12);f.boostPressed=flagBit(a,13);f.boostHeld=flagBit(a,14);
        f.railUberIdentity=signedByte(a,15,-1,3,"Invalid original Uber identity");f.railBalance=axis(a,23);break;
    default:
        only(0,0);break; // 9, 13 and states outside the table encode nothing
    }
    return f;
}
RiderInput originalCommandRiderInput(int state,uint32_t a,uint32_t b){
    auto f=originalDecodeCommandFields(state,a,b);RiderInput r;
    r.recoverPressed=f.resetPath;r.handplant=f.handplant;
    r.jumpPressed=f.jumpPressed;r.jumpHeld=f.jumpHeld;r.boostPressed=f.boostPressed;r.boostHeld=f.boostHeld;
    r.attackLeft=f.attackLeft;r.attackRight=f.attackRight;r.ollieHeld=f.ollieHeld;r.tweak=f.tweak;r.lateSpin=f.lateSpin;
    r.wipeoutRecover=f.wipeoutRecover;r.railSpin=f.railSpin;r.railBalance=f.railBalance;
    r.handplantBalance=f.handplantBalance;r.gateAnticipate=f.gateAnticipate;r.railUberIdentity=f.railUberIdentity;
    switch(state){
    case 0:r.turn=f.cruiseTurn;r.crouch=f.cruiseCrouch;r.brake=f.cruiseBrake;r.boardPress=f.boardPress;break;
    case 1:r.turn=f.cruiseTurn;r.boardPress=f.boardPress;r.boardPivot=f.boardPivot;break;
    case 2: // 12E9B8 consumer conventions retained from originalDecodeCommand.
        r.prewindTurn=f.prewindTurn;r.turn=std::clamp(f.prewindTurn,-.5f,.5f);
        r.spin=f.prewindSpin;r.flip=f.prewindFlip;r.crouch=f.jumpHeld?1.f:0.f;break;
    case 3: // Existing soft-collision view names bits21..26 boardPress; they carry RailBalance.
        r.turn=f.cruiseTurn;r.boardPress=f.railBalance;break;
    case 4:r.turn=f.cruiseTurn;r.crouch=f.cruiseCrouch;r.passiveInputCode=f.grab;
        r.passiveUpper14=f.attackLeft;r.passiveUpper15=f.attackRight;break;
    case 5:r.boostHeld=f.tweak; // existing view: bit14 (Tweak = Square) as boostHeld
        r.spin=f.spin;r.flip=f.flip;r.airAdjustFB=f.airAdjRotFB;r.airAdjustLR=f.airAdjRotLR;
        r.boardPress=f.boardPress;r.grabMask=f.grab<0?0:trickMasks[unsigned(f.grab)];break;
    case 7:r.boardPress=f.boardPress;break;
    case 10:r.turn=f.cruiseTurn;break;
    default:break;
    }
    return r;
}
}
