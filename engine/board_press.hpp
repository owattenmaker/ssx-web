#pragma once
// Original SSX3 (PS2 USA, SLUS_207.72) board press: control 1 (nose/tail press,
// BoardPivot rotation, R3 ollie) and the scoring it drives. Recovered instruction
// by instruction; see docs/attack-boardpress-recovery.md for the evidence and the
// oracle coverage.
//
//   entry        0x1161D0  (cruise 0x1317D8, rail 0x131E80/0x131ED8)
//   control 1    enter 0x12FC60, update 0x12FC80, exit 0x12FE98
//                airborne 0x12FFF8, phases 0x130228 / 0x1303E0 / 0x1304D0 / 0x1304E0
//                phase-1 helpers 0x1306B0 (reverse), 0x1307B8 (R3 ollie),
//                0x1308D8 (BoardPivot), 0x130DD0 (depth/FX), 0x131200 (release)
//                0x131428 (pivot finalise), 0x1313A8 (pivot spring), 0x12FEC8 (stance flip)
//                0x1162C8 jump request (+0x360 latch), 0x131348 (jump-out finalise)
//   scoring      0x1199F8 (press), 0x119AD8 + 0x119898 (pivot), 0x119A38 (end)
//   animation    kinds 13 0x1047F0, 14 0x104728, 15 0x1046B0 (-> 0x103CC8),
//                completion kind 9 0x104BD8 (-> 0x312BD0)
//
// Rider fields: +0x268 press-depth triplet (animation seek of 24/25/32/33),
// +0x274 board-depth triplet (kind-15 blend, FX), +0x280 pivot triplet
// (-1..1, x pi radians), +0x330 press style (0 none, 1 nose, 2 tail),
// +0x360 jump latch, +0x328 rail style, +0x320 reverse stance.
// Control object (motion owner +0x1D0): +0 phase, +4 time in press, +8 time at
// full depth, +0xC idle (release) time, +0x10 R3 OllieHeld latch.
//
// Arithmetic follows the recompiled original: FPU MUL.S chop (terrain_original),
// EE ADD.S/SUB.S with the guard bit (originalScalarAdd/Subtract), DIV.S nearest,
// VU dot products chop; min.s/max.s as std::min/std::max with the source operand
// order; CVT.W.S truncates.
#include "handplant.hpp"
#include <functional>
#include <string>

namespace ssx {
struct OriginalBoardPressRider {
    HandplantQuad position{0,0,0,1};   // +0x110 (effect origin)
    HandplantQuad velocity{};          // +0x1E0
    HandplantQuad forward{};           // +0x3A0 surface forward (entry / pivot direction test)
    float timeScale=1;                 // +0x300
    float manualSpin=0;                // +0x2DC
    float jumpLatch=0;                 // +0x360 (0x1162C8)
    int32_t reverseStance=0;           // +0x320
    int32_t style=0;                   // +0x328 rail style
    int32_t press=0;                   // +0x330
    int32_t surface=0;                 // +0x438
    GroundControlValue depth268,depth274,pivot280; // +0x268 / +0x274 / +0x280
};
struct OriginalBoardPressState { // motion owner +0x1D0
    int32_t phase=0;       // +0x00
    float time=0;          // +0x04 seconds in the press (curve blend, 1 s depth lock)
    float fullTime=0;      // +0x08 seconds at +0x274 == 1 (effects after 1 s)
    float idleTime=0;      // +0x0C seconds with no press input (release after 0.5 s)
    int32_t ollieLatch=0;  // +0x10 R3 OllieHeld seen
};
// 0x10EB30(rider,id,0,surface,params) parameter block built by 0x130DD0.
struct OriginalBoardPressEffect {
    int id=0;int32_t surface=0;
    HandplantQuad position{},direction{},down{},direction2{};float speed=0; // sp+0,+0x10,+0x20,+0x40 / sp+0x30
};
// External subsystems. Every callback corresponds to one original callee.
struct OriginalBoardPressAccess {
    std::function<int()> requestedSemantic;             // 0x312AA0(anim,2)
    std::function<void(int semantic)> play;             // 0x3128E8(anim,semantic,0,-1)
    std::function<void(int control)> requestControl;    // 0x11FEC8 (host runs old exit, new enter)
    std::function<void(int motion)> requestMotion;      // 0x11FE78 (host runs old exit, new enter)
    std::function<int()> motionMode;                    // 0x11FE98
    std::function<int()> controlState;                  // 0x11FEE8
    std::function<float(int style)> scorePress;         // 0x1199F8(rider+0x790,+0x330)
    std::function<float(int style)> scorePivot;         // 0x119AD8(rider+0x790,+0x330)
    std::function<float()> scoreEnd;                    // 0x119A38(rider+0x790)
    std::function<void(float)> award;                   // 0x10E098(rider,1,value)
    std::function<bool()> reverseTurn;                  // 0x114CC0(rider) (physical/control reversal + 0x115168)
    std::function<void(const HandplantQuad& rotation,int32_t mirror)> stanceRoot; // anim+0x30=(0,0,0,1), anim+0x40, anim+0x18
    std::function<void()> jumpTakeoff;                  // 0x114298(rider,1.0)
    std::function<void()> ollieEvent;                   // 0x294170(*(gp+0x410),rider)
    std::function<void()> rumble;                       // 0x2A1560(0x28B180(),rider)
    std::function<void(const OriginalBoardPressEffect&)> effect; // 0x10EB30
    std::function<void()> restoreStance;                // 0x115640(rider)
    std::function<void()> railCycle;                    // 0x1326C8(owner+0x2B0,0)
    std::function<void()> jumpStat;                     // 0x2F6AC8(owner+0xD20,1)
    // 0x12FC80 shared cruise helpers.
    std::function<bool()> statusCheck;                  // 0x116378
    std::function<bool(bool requested)> resetPath;      // 0x116120(rider,ResetPath,0)
    std::function<bool()> railAttach;                   // 0x106848
    std::function<void(bool held,bool pressed)> boost;  // 0x114130(rider,BoostHeld,BoostPressed)
    std::function<void(float crouch,float brake)> crouchBrake; // 0x113F88
    std::function<void(float turn)> turnTarget;         // 0x113E80
    std::function<void(float balance)> railTurn;        // 0x113F38
    std::function<void()> upperReactions;               // 0x115B58 then 0x115D48
};
struct OriginalBoardPressResult {
    enum class Stop {None,Status,Reset,Rail,Airborne,Jump,Phase};
    Stop stop=Stop::None;
};

namespace board_press_original {
using namespace handplant_original;
template<class F> const F& req(const F& f,const char* what){if(!f)throw std::runtime_error(std::string("Board press callback missing: ")+what);return f;}
constexpr float tick=std::bit_cast<float>(0x3c888889u);      // 1/60
constexpr float step6=std::bit_cast<float>(0x3d042108u);     // 1/31
constexpr float pi=std::bit_cast<float>(0x40490fdbu),halfPi=std::bit_cast<float>(0x3fc90fdbu);
constexpr float twoPi=std::bit_cast<float>(0x40c90fdbu),invTwoPi=std::bit_cast<float>(0x3e22f983u),invPi=std::bit_cast<float>(0x3ea2f983u);
constexpr float pivotRate=std::bit_cast<float>(0x3cccccceu);  // 0.025
constexpr float lo15=std::bit_cast<float>(0x3e19999au),hi85=std::bit_cast<float>(0x3f59999au);
// Six-bit signed command field (sll/sra) times 1/31 (MUL.S chop).
inline float field6(uint32_t word,unsigned shift){int32_t v=int32_t(word<<(26-shift))>>26;return mul(float(v),step6);}
// Runtime-initialised 4-point curves at 0x4FE920 / 0x4FE940 (bss; snapshot values).
struct Curve{std::array<float,8> p;};
constexpr Curve depthCurveA{{0.f,std::bit_cast<float>(0x3e4ccccdu),std::bit_cast<float>(0x3ecccccdu),std::bit_cast<float>(0x3ecccccdu),
    std::bit_cast<float>(0x3f19999au),std::bit_cast<float>(0x3ecccccdu),1.f,std::bit_cast<float>(0x3e4ccccdu)}};
constexpr Curve depthCurveB{{0.f,std::bit_cast<float>(0x3e99999au),std::bit_cast<float>(0x3ecccccdu),std::bit_cast<float>(0x3f99999au),
    std::bit_cast<float>(0x3f19999au),std::bit_cast<float>(0x3f99999au),1.f,std::bit_cast<float>(0x3e99999au)}};
// 0x130F48..0x130FDC: clamped piecewise-linear lookup.
inline float curve(const Curve& c,float x){
    auto seg=[&](unsigned i){float xa=c.p[2*i],ya=c.p[2*i+1],xb=c.p[2*i+2],yb=c.p[2*i+3];
        return A(ya,originalScalarDivide(mul(S(yb,ya),S(x,xa)),S(xb,xa)));};
    if(c.p[2]<x){
        if(c.p[4]<x){if(c.p[6]<x)return c.p[7];return seg(2);}
        return seg(1);
    }
    if(x<c.p[0])return c.p[1];
    return seg(0);
}
// floor(x/2pi + 0.5) wrap used by 0x1308D8: returns x - floor(...)*2pi.
inline float wrap(float x){
    float t=A(mul(x,invTwoPi),.5f);float f=float(int32_t(t));if(t<f)f=S(f,1.f);
    return S(x,mul(f,twoPi));
}
}
using board_press_original::req;

// 0x1161D0: request control 1 for a nonzero press (cruise and rail callers).
inline bool originalBoardPressEntry(OriginalBoardPressRider& r,float press,const OriginalBoardPressAccess& a){
    using namespace board_press_original;OriginalRounding rounding;
    if(press==0)return false;
    if(r.style==3||r.style==4)return false;
    if(req(a.motionMode,"motionMode")()==0&&dot4(r.velocity,r.forward)<0)return false;
    if(0.f<press){req(a.play,"play")(24);r.press=1;}else{req(a.play,"play")(32);r.press=2;}
    req(a.requestControl,"requestControl")(1);
    return true;
}
// 0x12FC60 enter.
inline void originalBoardPressEnter(OriginalBoardPressState& c,OriginalBoardPressRider& r){
    c.phase=0;r.jumpLatch=0;c.ollieLatch=0;c.time=0;c.fullTime=0;c.idleTime=0;
}
// 0x12FE98 exit: +0x330 = 0, +0x274 -> 0 at 1/60, +0x268 -> 0 at 1/30.
inline void originalBoardPressExit(OriginalBoardPressRider& r){
    r.press=0;r.depth274.rate=std::bit_cast<float>(0x3c888889u);r.depth274.target=0;
    r.depth268.rate=std::bit_cast<float>(0x3d088889u);r.depth268.target=0;
}
// 0x12FEC8 stance flip: +0x320 ^= 1, animation default root, rail style 1<->2, pivot negated.
inline void originalBoardPressFlipStance(OriginalBoardPressRider& r,const OriginalBoardPressAccess& a){
    using namespace board_press_original;OriginalRounding rounding;
    r.reverseStance^=1;
    const float angle=r.reverseStance==0?-0.f:-pi;
    auto sc=originalSinCos(mul(angle,.5f));
    req(a.stanceRoot,"stanceRoot")(HandplantQuad{mul(sc[0],0.f),mul(sc[0],0.f),mul(sc[0],1.f),sc[1]},r.reverseStance);
    if(r.style==1)r.style=2;else if(r.style==2)r.style=1;
    r.pivot280.current=-r.pivot280.current;r.pivot280.target=-r.pivot280.target;
}
// 0x1313A8: spring the pivot toward -1, 1 or 0 at 0.025 x time scale.
inline void originalBoardPressSpring(OriginalBoardPressRider& r){
    using namespace board_press_original;OriginalRounding rounding;
    if(r.pivot280.current<-.5f){r.pivot280.target=-1;r.pivot280.rate=mul(r.timeScale,pivotRate);}
    else if(.5f<r.pivot280.current){r.pivot280.target=1;r.pivot280.rate=mul(r.timeScale,pivotRate);}
    else{r.pivot280.target=0;r.pivot280.rate=mul(r.timeScale,pivotRate);}
}
// 0x131428: finish a pivot animation (37/38 -> 28 nose, 29/30 -> 36 tail), scoring
// a completed pivot, otherwise flipping the stance back.
inline void originalBoardPressFinalize(OriginalBoardPressRider& r,const OriginalBoardPressAccess& a){
    using namespace board_press_original;OriginalRounding rounding;
    const int sem=req(a.requestedSemantic,"requestedSemantic")();
    if(sem==37||sem==38){
        if(std::abs(r.pivot280.current)<=.5f){req(a.award,"award")(req(a.scorePivot,"scorePivot")(r.press));req(a.play,"play")(28);r.press=1;}
        else{originalBoardPressFlipStance(r,a);req(a.play,"play")(36);}
    }else if(sem==29||sem==30){
        if(.5f<=std::abs(r.pivot280.current)){req(a.award,"award")(req(a.scorePivot,"scorePivot")(r.press));req(a.play,"play")(36);r.press=2;}
        else{originalBoardPressFlipStance(r,a);req(a.play,"play")(28);}
    }
}
// 0x131348: control-1 jump-out (called by 0x1162C8 before requesting control 2).
inline void originalBoardPressJumpOut(OriginalBoardPressRider& r,const OriginalBoardPressAccess& a){
    originalBoardPressSpring(r);originalBoardPressFinalize(r,a);
    if(r.press==1||r.press==2)req(a.award,"award")(req(a.scoreEnd,"scoreEnd")());
}
// 0x1162C8(rider,JumpHeld,JumpPressed): pressed always requests control 2; held
// alone only while the +0x360 latch is clear. Otherwise the latch is set.
inline bool originalBoardPressJumpRequest(OriginalBoardPressRider& r,bool held,bool pressed,const OriginalBoardPressAccess& a){
    if(pressed)req(a.jumpStat,"jumpStat")();
    const bool request=r.jumpLatch==0?(held||pressed):pressed;
    if(!request){r.jumpLatch=1.f;return false;}
    if(req(a.controlState,"controlState")()==1)originalBoardPressJumpOut(r,a);
    req(a.requestControl,"requestControl")(2);
    return true;
}
// 0x12FFF8: airborne while in control 1 (motion 1).
inline bool originalBoardPressAirborne(OriginalBoardPressState& c,OriginalBoardPressRider& r,const OriginalBoardPressAccess& a){
    using namespace board_press_original;OriginalRounding rounding;
    if(req(a.motionMode,"motionMode")()!=1)return false;
    if(r.style!=0)r.style=0; //0x116930 is an empty routine
    const int sem=req(a.requestedSemantic,"requestedSemantic")();
    const bool pivot=sem==37||sem==38||sem==29||sem==30;
    if(c.phase==2){
        bool done;
        if(sem==37||sem==38){r.pivot280.target=0;r.pivot280.rate=mul(r.timeScale,pivotRate);done=std::abs(r.pivot280.current)<=lo15;}
        else if(sem==29||sem==30){r.pivot280.target=r.pivot280.current<0?-1.f:1.f;r.pivot280.rate=mul(r.timeScale,pivotRate);done=hi85<=std::abs(r.pivot280.current);}
        else return true;
        if(!done)return true;
        req(a.award,"award")(req(a.scorePivot,"scorePivot")(r.press));
        req(a.award,"award")(req(a.scoreEnd,"scoreEnd")());
        req(a.play,"play")(287);req(a.requestControl,"requestControl")(5);return true;
    }
    if(pivot){c.phase=2;return true;}
    req(a.play,"play")(r.press==1?27:35);req(a.requestControl,"requestControl")(5);return true;
}
// 0x130228 phase 0: press in (0x268 -> 1 at 2 ts/60, 0x274 -> 0.5 at ts/60).
inline void originalBoardPressPhase0(OriginalBoardPressState& c,OriginalBoardPressRider& r,uint32_t word1,const OriginalBoardPressAccess& a){
    using namespace board_press_original;OriginalRounding rounding;
    const float press=field6(word1,0),pivot=field6(word1,6);
    if(press==0&&std::abs(pivot)<.5f){
        r.depth268.target=0;r.depth268.rate=mul(A(r.timeScale,r.timeScale),tick);c.phase=3;
        req(a.play,"play")(r.press==2?33:25);return;
    }
    r.depth274.target=.5f;r.depth274.rate=mul(r.timeScale,tick);
    r.depth268.target=1;r.depth268.rate=mul(A(r.timeScale,r.timeScale),tick);
    if(r.depth268.current!=1.f)return;
    req(a.award,"award")(req(a.scorePress,"scorePress")(r.press));c.phase=1;
    if(r.press==2){req(a.play,"play")(36);r.pivot280.current=1;r.pivot280.rate=0;r.pivot280.target=1;}
    else{req(a.play,"play")(28);r.pivot280.rate=0;r.pivot280.target=0;r.pivot280.current=0;}
}
// 0x1306B0: a reverse turn swaps nose/tail through 23/31 (completion kind 9).
inline bool originalBoardPressReverse(OriginalBoardPressRider& r,const OriginalBoardPressAccess& a){
    const int sem=req(a.requestedSemantic,"requestedSemantic")();
    if(sem==23||sem==31)return true;
    if(sem==37||sem==38||sem==29||sem==30)return false;
    if(req(a.motionMode,"motionMode")()!=0)return false;
    if(r.manualSpin!=0)return false;
    if(!req(a.reverseTurn,"reverseTurn")())return false;
    if(r.press==1){req(a.play,"play")(23);r.press=2;}else{req(a.play,"play")(31);r.press=1;}
    return true;
}
// 0x1307B8: R3 OllieHeld latch; releasing it ollies (0x114298 charge 1, motion 1, control 5).
inline bool originalBoardPressOllie(OriginalBoardPressState& c,OriginalBoardPressRider& r,uint32_t word0,const OriginalBoardPressAccess& a){
    const bool held=(word0&0x20000u)!=0;
    if(held)c.ollieLatch=1;
    const bool release=c.ollieLatch!=0&&!held;
    const float magnitude=std::abs(r.pivot280.current);
    if(!release)return false;
    originalBoardPressFinalize(r,a);
    req(a.play,"play")(.5f<magnitude?35:27);
    if(r.style!=0)r.style=0; //0x116930 is an empty routine
    req(a.jumpTakeoff,"jumpTakeoff")();req(a.requestMotion,"requestMotion")(1);req(a.requestControl,"requestControl")(5);
    req(a.ollieEvent,"ollieEvent")();
    return true;
}
// 0x1308D8: BoardPivot rotation of +0x280 toward the stick angle.
inline bool originalBoardPressPivot(OriginalBoardPressState& c,OriginalBoardPressRider& r,uint32_t word1,const OriginalBoardPressAccess& a){
    using namespace board_press_original;OriginalRounding rounding;
    float pivot=field6(word1,6);const float press=field6(word1,0);
    const float magnitude=std::abs(pivot)<std::abs(press)?std::abs(press):std::abs(pivot);
    bool spring=magnitude<.1f;
    if(!spring&&(dot4(r.velocity,r.forward)<0||c.ollieLatch!=0))spring=true;
    if(spring){
        originalBoardPressSpring(r);
        const float m=std::abs(r.pivot280.current);
        if(!(m<=.2f)&&!(.8f<=m))return true;
        if(magnitude<.1f)c.idleTime=1.f;
        originalBoardPressFinalize(r,a);return false;
    }
    if(r.reverseStance!=0)pivot=-pivot;
    float target;
    if(press==0)target=pivot==0?pivot:(0.f<=pivot?halfPi:-halfPi);
    else{target=originalAtan(originalScalarDivide(pivot,press));if(press<0)target=0.f<pivot?A(target,pi):S(target,pi);}
    const float current=mul(r.pivot280.current,pi);
    const float difference=wrap(S(target,current));
    const int sem=req(a.requestedSemantic,"requestedSemantic")();
    const bool pivoting=sem==37||sem==38||sem==29||sem==30;
    if(!pivoting&&std::bit_cast<float>(0x3ef1463bu)<=std::abs(difference)){
        float kick;
        if(.5f<std::abs(r.pivot280.current))kick=0.f<difference?-hi85:hi85;
        else kick=difference<0.f?-lo15:lo15;
        r.pivot280.rate=0;r.pivot280.current=kick;r.pivot280.target=kick;
    }else{
        const float delta=mul(mul(difference,tick),A(r.timeScale,r.timeScale));
        float angle;
        if(current<0&&0.f<delta)angle=S(A(current,delta),twoPi);
        else if(0.f<current&&delta<0)angle=A(A(current,delta),twoPi);
        else angle=A(current,delta);
        const float value=mul(wrap(angle),invPi);
        r.pivot280.rate=0;r.pivot280.current=value;r.pivot280.target=value;
    }
    const float m=std::abs(r.pivot280.current);
    if(lo15<=m&&m<=hi85){
        r.depth274.target=.5f;r.depth274.rate=mul(r.timeScale,tick);
        int next;
        if(r.press==1)next=0.f<r.pivot280.current?29:30;
        else next=0.f<r.pivot280.current?37:38;
        if(sem==next)return true;
        if(!pivoting)originalBoardPressFlipStance(r,a);
        req(a.play,"play")(next);return true;
    }
    originalBoardPressFinalize(r,a);return false;
}
// 0x130DD0: press timers, +0x274 depth target/rate, rumble and spray effect.
inline bool originalBoardPressDepth(OriginalBoardPressState& c,OriginalBoardPressRider& r,uint32_t word1,const OriginalBoardPressAccess& a){
    using namespace board_press_original;OriginalRounding rounding;
    const float dt=mul(r.timeScale,tick);
    c.time=A(c.time,dt);
    if(r.depth274.current==1.f)c.fullTime=A(c.fullTime,dt);else c.fullTime=0;
    float press=field6(word1,0);if(r.press==2)press=-press;
    float target=std::max(press,0.f);
    const float time=c.time;
    if(time<=1.f)target=std::min(target,A(mul(time,.5f),.5f));
    else{
        const float pivot=std::abs(field6(word1,6));
        float larger=target;if(target<=pivot)larger=pivot;
        target=.5f<larger?1.f:0.f;
    }
    const float current=r.depth274.current;
    const bool fast=(target<=.5f&&.5f<current)||(.5f<=target&&current<.5f);
    float factor;
    if(fast)factor=1;
    else{
        const float yA=curve(depthCurveA,current),yB=curve(depthCurveB,current);
        const float w=std::min(mul(time,std::bit_cast<float>(0x3e4ccf26u)),1.f);
        factor=A(mul(yA,S(1.f,w)),mul(yB,w));
        if(.8f<r.depth274.current)req(a.rumble,"rumble")();
    }
    r.depth274.target=target;r.depth274.rate=mul(factor,dt);
    if(!(1.f<c.fullTime))return false;
    const float speed=vuSqrt(dot4(r.velocity,r.velocity));
    OriginalBoardPressEffect e;e.id=r.press==2?0x167:0x166;e.surface=r.surface;e.position=r.position;e.speed=speed;
    e.down=scale4(unitZ4,-1.f);e.direction2=e.down;
    if(std::bit_cast<float>(0x3a83126fu)<speed){e.direction=scale4(r.velocity,div(1.f,speed));e.direction2=e.direction;}
    else e.direction=unitZ4;
    req(a.effect,"effect")(e);return true;
}
// 0x131200: release test (0.5 s idle or opposite push), 0x274 -> 0 at 2 ts/60.
inline bool originalBoardPressRelease(OriginalBoardPressState& c,OriginalBoardPressRider& r,uint32_t word1){
    using namespace board_press_original;OriginalRounding rounding;
    const float press=field6(word1,0),pivot=field6(word1,6);
    if(press==0&&std::abs(pivot)<.5f)c.idleTime=A(c.idleTime,mul(r.timeScale,tick));else c.idleTime=0;
    bool release=.5f<c.idleTime;
    if(!release){if(r.press==2)release=.8f<press;else if(r.press==1)release=press<-.8f;}
    if(release){
        r.depth274.target=0;r.depth274.rate=mul(A(r.timeScale,r.timeScale),tick);
        if(r.depth274.current<.5f)return true;
    }
    return r.depth274.current==0;
}
// 0x1303E0 phase 1.
inline void originalBoardPressPhase1(OriginalBoardPressState& c,OriginalBoardPressRider& r,uint32_t word0,uint32_t word1,const OriginalBoardPressAccess& a){
    using namespace board_press_original;
    if(originalBoardPressReverse(r,a))return;
    if(originalBoardPressOllie(c,r,word0,a))return;
    if(originalBoardPressPivot(c,r,word1,a))return;
    if(originalBoardPressDepth(c,r,word1,a))return;
    if(!originalBoardPressRelease(c,r,word1))return;
    OriginalRounding rounding;
    r.depth268.target=0;r.depth268.rate=mul(A(r.timeScale,r.timeScale),tick);
    req(a.play,"play")(r.press==2?33:25);c.phase=3;
}
// 0x1304E0 phase 3: wait for 0x268 to empty (re-press returns to phase 0), then leave.
inline void originalBoardPressPhase3(OriginalBoardPressState& c,OriginalBoardPressRider& r,uint32_t word1,const OriginalBoardPressAccess& a){
    using namespace board_press_original;OriginalRounding rounding;
    const int sem=req(a.requestedSemantic,"requestedSemantic")();
    if((sem==33||sem==25)&&0.f<r.depth268.current){
        const float press=field6(word1,0);
        const bool again=(press<-.5f&&sem==33)||(.5f<press&&sem==25);
        if(!again)return;
        if(press<0){req(a.play,"play")(32);r.press=2;}else{req(a.play,"play")(24);r.press=1;}
        r.depth268.target=1;r.depth268.rate=mul(A(r.timeScale,r.timeScale),tick);c.phase=0;return;
    }
    if(r.press==1||r.press==2)req(a.award,"award")(req(a.scoreEnd,"scoreEnd")());
    if(req(a.motionMode,"motionMode")()==4){req(a.railCycle,"railCycle")();req(a.requestControl,"requestControl")(7);}
    else{req(a.restoreStance,"restoreStance")();req(a.requestControl,"requestControl")(0);}
}
// 0x12FC80 update.
inline OriginalBoardPressResult originalBoardPressUpdate(OriginalBoardPressState& c,OriginalBoardPressRider& r,uint32_t word0,uint32_t word1,const OriginalBoardPressAccess& a){
    using namespace board_press_original;using Stop=OriginalBoardPressResult::Stop;OriginalBoardPressResult out;
    if(req(a.statusCheck,"statusCheck")()){out.stop=Stop::Status;return out;}
    if(req(a.resetPath,"resetPath")((word0>>12)&1)){out.stop=Stop::Reset;return out;}
    if(req(a.railAttach,"railAttach")()){out.stop=Stop::Rail;return out;}
    if(originalBoardPressAirborne(c,r,a)){out.stop=Stop::Airborne;return out;}
    if(originalBoardPressJumpRequest(r,(word0>>15)&1,(word0>>16)&1,a)){out.stop=Stop::Jump;return out;}
    req(a.boost,"boost")((word0>>14)&1,(word0>>13)&1);
    req(a.crouchBrake,"crouchBrake")(0,0);
    {
        OriginalRounding rounding;
        if(r.style!=0){const float v=field6(word0,24);req(a.railTurn,"railTurn")(-.5f<=v?std::min(v,.5f):-.5f);}
        else{
            const float m=std::abs(r.pivot280.current);const float bound=.5f<m?S(m,.5f):S(.5f,m);
            const float t=field6(word0,18);req(a.turnTarget,"turnTarget")(-bound<=t?std::min(t,bound):-bound);
        }
    }
    req(a.upperReactions,"upperReactions")();
    out.stop=Stop::Phase;
    switch(c.phase){
    case 0:originalBoardPressPhase0(c,r,word1,a);break;
    case 1:originalBoardPressPhase1(c,r,word0,word1,a);break;
    case 2:c.phase=1;break;
    case 3:originalBoardPressPhase3(c,r,word1,a);break;
    default:break;
    }
    return out;
}

// ---------------------------------------------------------------------------
// Air controller 0x133308 (0x133590..0x133634): +0x330 from the BoardPress step
// (word1 bits 18..19) and the pre-step air adjust flip (air+0x2C).
inline int32_t originalBoardPressAirStyle(int step,float adjustFlip){
    if(step==0)return 0;
    const float direction=step==1?1.f:-1.f;
    if(adjustFlip<0)return 2;
    if(adjustFlip==0)return direction<0?2:1;
    return 1;
}
// Landing 0x13A5D4: a pending +0x330 re-enters control 1 with 26 (nose) / 34 (tail)
// instead of the landing animation, the 0x115640 stance restore and reverse turn.
inline int originalBoardPressLandingSemantic(int32_t press){return press==1?26:34;}

// ---------------------------------------------------------------------------
// Scoring (rider+0x790).
// 0x1199F8: +0x28 = +0x10 = style, +0x2C = max(+0x2C,0), 0x1176F8 (+0xA4 = -1). Returns 0.
inline float originalBoardPressScorePress(OriginalScoreBoundaryState& s,int32_t style){
    s.identity.flag28=style;s.identity.flag10=style;s.identity.time2C=std::max(s.identity.time2C,0.f);s.score.comboTimeoutA4=-1;return 0;
}
// 0x119AD8 + 0x119898: +0x28 = style; +0x34 += pi, and +0x14 trades the old for the
// new |+0x34| x 0.0159155 (clamped at 0). Returns 0.
inline float originalBoardPressScorePivot(OriginalScoreBoundaryState& s,int32_t style){
    OriginalRounding rounding;using namespace board_press_original;
    s.identity.flag28=style;
    const float x=A(s.identity.spin34,pi),k=std::bit_cast<float>(0x3c826135u);
    const float removed=mul(std::abs(s.identity.spin34),k);s.identity.spin34=x;
    const float added=mul(std::abs(x),k);
    s.score.accumulated14=std::max(A(S(s.score.accumulated14,removed),added),0.f);return 0;
}
// 0x119A38: commit(scorer,0,0,+0x20,0,+0x30>=0), 0x117838, then keep +0x30/+0x20/+0x24
// and set +0x0C = +0x20.
template<class Commit> auto originalBoardPressScoreEnd(OriginalScoreBoundaryState& s,Commit&& commit){
    const float air=s.airSeconds30,time=s.identity.time24;const int32_t style=s.identity.style20;
    auto result=commit(s,0,0,style,0,0.f<=air?1:0);
    originalResetScoreBoundary(s);
    s.airSeconds30=air;s.identity.style20=style;s.identity.time24=time;s.identity.style0C=style;return result;
}
}
