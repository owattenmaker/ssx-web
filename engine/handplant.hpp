#pragma once
// Original SSX3 (PS2 USA, SLUS_207.72) handplant: control 11 "Handplant" and
// motion 5 "Handplant". Recovered instruction by instruction; see
// docs/handplant-recovery.md for evidence and the oracle coverage.
//
//   entry        0x107578  (cruise 0x131620, natural air 0x12F730, spin 0x133308)
//   motion 5     setup 0x138BA0, enter 0x138B48, update 0x1391A8,
//                exit 0x139178, launch 0x139548 (post stage 0x139528 = 0x11E150)
//   control 11   setup 0x1329B0, enter 0x1328B0, update 0x132A30,
//                exit 0x132F98, play 0x132FB8 (semantic table 0x459FC8)
//   scoring      0x119BF0 (begin), 0x119C38 (launch commit)
//
// Source units are Z-up centimeters, cm/s, XYZW quaternions. VU vector
// arithmetic uses terrain_original helpers under OriginalRounding (chop);
// EE scalar ADD/SUB keep the guard bit, EE DIV rounds to nearest.
// Four-lane vectors keep the w lanes the original carries through lq/sq.
#include "rail_motion.hpp"
#include "score_boundary.hpp"
#include "air_alignment.hpp"
#include "collision_scalar.hpp"
#include <functional>

namespace ssx {
using HandplantQuad=std::array<float,4>;

namespace handplant_original {
using namespace terrain_original;
inline float bits(uint32_t b){return std::bit_cast<float>(b);}
inline float A(float a,float b){return originalScalarAdd(a,b);}
inline float S(float a,float b){return originalScalarSubtract(a,b);}
inline float D(float a,float b){return collision_scalar::divide(a,b);} // EE div.s
inline float minS(float a,float b){return a<b?a:b;}
inline float maxS(float a,float b){return a>b?a:b;}
// vmul.xyzw; vadday.x; vmaddaz.x (1*z); vmaddw.x (1*w)
inline float dot4(const HandplantQuad& a,const HandplantQuad& b){
    // VU0 horizontal dot: z and w go through vmaddaz.x / vmaddw.x with the 1.0 vector as fs (docs/ps2-float.md "The VU0 forms").
    float x=mul(a[0],b[0]),y=mul(a[1],b[1]),z=mul(a[2],b[2]),w=mul(a[3],b[3]);
    return add(add(add(x,y),mul(1.f,z)),mul(1.f,w));
}
inline HandplantQuad add4(HandplantQuad a,const HandplantQuad& b){for(unsigned k=0;k<4;++k)a[k]=add(a[k],b[k]);return a;}
inline HandplantQuad sub4(HandplantQuad a,const HandplantQuad& b){for(unsigned k=0;k<4;++k)a[k]=sub(a[k],b[k]);return a;}
inline HandplantQuad scale4(HandplantQuad a,float s){for(auto& x:a)x=mul(x,s);return a;}
inline float vuSqrt(float x){return terrain_original::sqrt(x);}
inline float vuReciprocal(float x){return x==0?bits(0x7F7FFFFFu):div(1.f,x);}
inline float vuRsqrt(float x){float r=vuSqrt(x);return r==0?bits(0x7F7FFFFFu):div(1.f,r);}
// vopmula.xyz/vopmsub.xyz then vsub.w: cross(a,b) with w = 0.
inline HandplantQuad cross4(const HandplantQuad& a,const HandplantQuad& b){
    return {sub(mul(a[1],b[2]),mul(b[1],a[2])),sub(mul(a[2],b[0]),mul(b[2],a[0])),sub(mul(a[0],b[1]),mul(b[0],a[1])),0.f};
}
// VU Hamilton product (a first): same sequence as rail_original::quaternionMultiply.
inline HandplantQuad quatMul(const HandplantQuad& a,const HandplantQuad& b){return rail_original::quaternionMultiply(a,b);}
inline HandplantQuad quad(const RailVector& v,float w){return {v[0],v[1],v[2],w};}
inline RailVector xyz(const HandplantQuad& q){return {q[0],q[1],q[2]};}
// Standalone 0x31BF60 sine (separate polynomial from the 0x31BE50 pair).
inline float sine(float x){
    float t=mul(x,bits(0x3f22f983));t=x<0?S(t,.5f):A(t,.5f);int quadrant=int(t);
    x=S(x,mul(float(quadrant),bits(0x3fc90fdb)));float square=mul(x,x),p;
    if(quadrant&1){p=mul(square,bits(0x37d00d03));p=A(p,bits(0xbab60b62));p=mul(p,square);p=A(p,bits(0x3d2aaaab));p=mul(p,square);p=A(p,-.5f);p=mul(p,square);p=A(p,1.f);}
    else{p=mul(square,bits(0x3638ef1f));p=A(p,bits(0xb9500d03));p=mul(p,square);p=A(p,bits(0x3c088889));p=mul(p,square);p=A(p,bits(0xbe2aaaab));p=mul(p,square);p=mul(p,x);p=A(p,x);}
    return quadrant&2?-p:p;
}
// asin(min(length,1)) rotation delta about a unit axis: 0x31C128, 0x31BE50(a*0.5).
inline HandplantQuad deltaFromSine(const HandplantQuad& axis,float length){
    float angle=originalAsin(minS(length,1.f));
    auto sc=originalSinCos(mul(angle,.5f));
    return {mul(sc[0],axis[0]),mul(sc[0],axis[1]),mul(sc[0],axis[2]),sc[1]};
}
constexpr HandplantQuad zero4{0,0,0,0},unitX4{1,0,0,0},unitZ4{0,0,1,0}; // 0x4FF120/0x4FF140/0x4FF160
}

// Semantic table 0x459FC8: [phase][side] = {semantic, reflect}. Side 1 = toe side (HPTS).
struct OriginalHandplantClip {int semantic=438;bool reflect=false;};
inline OriginalHandplantClip originalHandplantClip(int phase,int side){
    static constexpr std::array<std::array<OriginalHandplantClip,2>,7> table{{
        {{{438,false},{438,false}}},
        {{{44,false},{39,false}}},   // INTO
        {{{45,false},{40,false}}},   // BAL (kind 9 seek)
        {{{46,true},{41,true}}},     // TRANS_REFLECT (handspring)
        {{{47,false},{42,false}}},   // EXITBAL_THROUGH
        {{{48,true},{43,true}}},     // EXITBAL_REFLECT
        {{{47,false},{42,false}}},   // exit to rail
    }};
    if(phase<0||phase>6||side<0||side>1)throw std::runtime_error("Handplant clip index outside table 0x459FC8");
    return table[phase][side];
}

// Rider fields the handplant routines read or write.
struct OriginalHandplantRider {
    RailVector position{};float positionW=1;            // +0x110 (+0x11C)
    std::array<float,4> quaternion{0,0,0,1};            // +0x120
    RailVector right{1,0,0},forward{0,1,0},up{0,0,1};   // +0x1A0/+0x1B0/+0x1C0 (0x11E098 columns)
    float forwardW=0,upW=0;                             // +0x1BC/+0x1CC
    RailVector velocity{};float velocityW=0;            // +0x1E0
    float presentedUpZ=1;                               // +0x188 (0x11FA10 presentation frame up.z)
    float timeScale=1;                                  // +0x300
    int32_t reverseStance=0;                            // +0x320
    int32_t phase=0;                                    // +0x32C
    GroundControlValue turn,animationTurn,extraLean;    // +0x1F0/+0x1FC/+0x208
    GroundControlValue lean244;                         // +0x244 kind-9 seek amount (HandplantBalance)
    GroundControlValue roll250;                         // +0x250 presentation roll, reused as balance b
    RailVector handBone{};float handBoneW=1;            // posed bone rider+0x8A8 (=5) from rider+0x780->+0x2C
    int32_t motionMode=0;                               // owner+0xDE0
    int32_t channel2Class=0;                            // 0x311AE8(anim,2)
};
struct OriginalHandplantMotionState { // owner+0x110
    HandplantQuad start{},exitVelocity{},tangent{},lip{},toLip{}; // +0x00/+0x10/+0x20/+0x30/+0x40
    HandplantQuad predictorPosition{},predictorVelocity{};      // +0x50/+0x60
    HandplantQuad axis{1,0,0,0};                               // +0x70
    float rate=0,duration=0,elapsed=0,entrySpeed=0;            // +0x80/+0x84/+0x88/+0x8C
    int32_t railFlag=0,side=0;                                  // +0x90/+0x94
};
struct OriginalHandplantControlState { // owner+0x370
    float timer=0,smoothed=0;                                   // +0x00 (T, then balance timer) / +0x04
    int32_t fast=0,railFlag=0,side=0;                           // +0x08/+0x0C/+0x10
};
struct OriginalHandplantHit {
    bool found=false;
    HandplantQuad point{0,0,0,1},tangent{};  // query out+0x00 / +0x10 (tangent aligned with v by 0x107578)
    float distance=0;
    uint32_t descriptorFlags=0;bool descriptor=false; // out+0x58 -> +0x1C
    OriginalRailQueryResult query;
};

// External subsystems. Missing callbacks throw.
struct OriginalHandplantAccess {
    std::function<OriginalRailQueryResult(RailVector probe)> query;       // 0x334680(world,probe,out,2,300)
    std::function<void()> markAttempt;                                    // 0x2F6AC8(owner+0xD20,2)
    std::function<void(int)> requestControl;                              // 0x11FEC8 (host runs old exit, new enter)
    std::function<void(int)> requestMotion;                               // 0x11FE78 (host runs old exit, new enter)
    std::function<int()> motionMode;                                      // 0x11FE98
    std::function<float(int semantic)> semanticDuration;                  // 0x312790
    std::function<void(int semantic,float rate)> play;                    // anim+0x1C=rate; 0x3128E8(anim,semantic,0,-1)
    std::function<void(unsigned channel,float seconds)> fade;             // 0x311E88
    std::function<void(float radians)> rotateAnimation;                   // 0x311B48
    std::function<void()> stat;                                           // 0x29DC48(0x28B180(),rider)
    std::function<bool(bool requested)> resetPath;                        // 0x116120
    std::function<bool()> channel2Complete;                               // 0x312AE8(anim,2)
    std::function<bool(unsigned bit)> channel2Event;                      // 0x1446A0(0x311B20(anim,2)+0xB0,bit)
    std::function<std::array<float,2>()> channel2Clock;                   // seq+0x08 time, seq+0x10 duration
    std::function<void(float)> setChannel2Rate;                           // seq+0x90
    std::function<float(int kind)> scoreBegin;                            // 0x119BF0(rider+0x790,kind)
    std::function<float()> scoreLaunch;                                   // 0x119C38(rider+0x790)
    std::function<void(float)> award;                                     // 0x10E098(rider,1,value)
    std::function<bool()> railAttach;                                     // 0x106848(rider)
    std::function<void(OriginalHandplantRider&)> bakeRoot;                // 0x11FA10(rider,rider+0x110) then 0x11E098
    std::function<int()> predictorStatus;                                 // predictor+0xAC
    std::function<float()> predictorRemaining;                            // predictor+0x98 - predictor+0xA0
    std::function<void(HandplantQuad position,HandplantQuad velocity)> predictorBegin; // 0x1135B8(pred,&p,&v,3333.33)
    std::function<void(HandplantQuad& position,HandplantQuad& velocity)> predictorStep; // 0x113648(pred,&p,&v,1/60)
    std::function<RailVector()> predictorNormal,predictorHeading;         // predictor+0x20 / +0x10
};
namespace handplant_original {
template<class F> const F& need(const F& f,const char* what){if(!f)throw std::runtime_error(std::string("Handplant callback missing: ")+what);return f;}
// 0x11E098 stores the rows with vsub.w (w = 0) and copies +0x110 to +0x1D0.
inline void rebuild(OriginalHandplantRider& r){
    auto basis=originalRebuildOrientation(r.quaternion);r.quaternion=basis.quaternion;r.right=basis.right;r.forward=basis.forward;r.up=basis.up;
    r.forwardW=0;r.upW=0;
}
}

// ---------------------------------------------------------------------------
// 0x107578 acceptance test (everything before the control/motion requests).
struct OriginalHandplantEntryTest {
    enum class Reject {None,Falling,Inverted,RailCycle,NoSpline,SteepSpline,Distance,Receding};
    bool accepted=false;Reject reject=Reject::None;
    float speed=0,duration=0;HandplantQuad direction{},probe{};
    OriginalHandplantHit hit;
};
inline OriginalHandplantEntryTest originalHandplantEntryTest(const OriginalHandplantRider& r,const OriginalHandplantAccess& a){
    using namespace handplant_original;Rounding rounding;OriginalHandplantEntryTest e;using R=OriginalHandplantEntryTest::Reject;
    if(r.motionMode==1){
        if(r.velocity[2]<bits(0xc40ae38eu)){e.reject=R::Falling;return e;}
        if(r.presentedUpZ<0){e.reject=R::Inverted;return e;}
        if(uint32_t(r.channel2Class-18)<3u){e.reject=R::RailCycle;return e;}
    }
    const HandplantQuad v=quad(r.velocity,r.velocityW);
    e.speed=vuSqrt(dot4(v,v));
    e.direction=bits(0x41de38e4u)<e.speed?scale4(v,vuReciprocal(e.speed)):quad(r.forward,r.forwardW);
    const float reach=300.f;
    e.probe=add4(quad(r.position,r.positionW),scale4(e.direction,reach));
    auto q=need(a.query,"query")(xyz(e.probe));
    auto& hit=e.hit;hit.query=q;hit.found=q.found;
    if(!q.found){e.reject=R::NoSpline;return e;}
    hit.point=quad(q.point,1.f);hit.tangent=quad(q.tangent,0.f);hit.distance=q.distance;
    hit.descriptor=q.record!=nullptr;hit.descriptorFlags=q.record?q.record->flags:0;
    if(bits(0x3f666666u)<std::abs(hit.tangent[2])){e.reject=R::SteepSpline;return e;}
    const auto miss=sub4(hit.point,e.probe);
    if(reach<vuSqrt(dot4(miss,miss))){e.reject=R::Distance;return e;}
    const auto toward=sub4(hit.point,quad(r.position,r.positionW));
    if(dot4(v,toward)<0){e.reject=R::Receding;return e;}
    float span=D(bits(0x44ad9c72u),maxS(e.speed,bits(0x438ae38eu)));
    span=.5f<=span?minS(span,1.5f):.5f;
    if(dot4(hit.tangent,v)<0)hit.tangent=scale4(hit.tangent,-1.f);
    e.duration=mul(span,.5f);e.accepted=true;return e;
}

// 0x138BA0 motion-5 setup (reads the rider after the control-13 request).
inline void originalHandplantMotionSetup(OriginalHandplantMotionState& m,const OriginalHandplantRider& r,const OriginalHandplantHit& hit,float duration){
    using namespace handplant_original;Rounding rounding;
    m.start=quad(r.position,r.positionW);m.exitVelocity=zero4;m.tangent=hit.tangent;m.lip=hit.point;
    auto toLip=sub4(m.lip,quad(r.handBone,r.handBoneW));m.toLip=scale4(toLip,vuRsqrt(dot4(toLip,toLip)));
    m.duration=duration;m.elapsed=0;
    m.entrySpeed=dot4(quad(r.velocity,r.velocityW),m.tangent);
    m.railFlag=hit.descriptor?int32_t(hit.descriptorFlags&1):0;
    const float threshold=bits(0x3a83126fu);
    HandplantQuad q=r.quaternion;
    // Upright the physical frame: rotate up onto +Z.
    auto axis=cross4(quad(r.up,r.upW),unitZ4);float length=vuSqrt(dot4(axis,axis));
    if(threshold<length){axis=scale4(axis,vuReciprocal(length));q=quatMul(deltaFromSine(axis,length),q);}
    // Physical right axis (EE scalar) of the uprighted quaternion.
    float x=q[0],y=q[1],z=q[2],w=q[3];
    float f1=mul(y,y),f6=mul(z,z),f2=mul(x,z),f7=mul(w,y);f1=A(f1,f6);float f0=mul(x,y),f5=mul(w,z);
    f2=S(f2,f7);f1=A(f1,f1);f0=A(f0,f5);f2=A(f2,f2);f1=S(1.f,f1);f0=A(f0,f0);
    const HandplantQuad right{f1,f0,f2,0};
    HandplantQuad flat{m.tangent[0],m.tangent[1],0,m.tangent[3]};flat=scale4(flat,vuRsqrt(dot4(flat,flat)));
    int side=dot4(flat,right)<0?1:0;
    if(side)flat=scale4(flat,-1.f);
    if(r.reverseStance)side^=1;
    m.side=side;
    // Turn the right axis onto the (signed) horizontal tangent.
    auto turn=cross4(right,flat);float turnLength=vuSqrt(dot4(turn,turn));
    HandplantQuad target=q;
    if(threshold<turnLength){turn=scale4(turn,vuReciprocal(turnLength));target=quatMul(deltaFromSine(turn,turnLength),q);}
    const HandplantQuad conjugate{-r.quaternion[0],-r.quaternion[1],-r.quaternion[2],r.quaternion[3]};
    const auto relative=quatMul(target,conjugate);
    float angle,half;
    if(0<relative[3]){angle=originalAsin(minS(relative[3],1.f));half=.5f;}
    else {angle=originalAsin(-maxS(relative[3],-1.f));half=-.5f;}
    angle=S(bits(0x3fc90fdbu),angle);angle=A(angle,angle);m.rate=angle;
    if(bits(0x3727c5acu)<=angle){
        float s=sine(mul(angle,half));
        m.axis=scale4({relative[0],relative[1],relative[2],0.f},vuReciprocal(s));
    } else m.axis=unitX4;
    m.rate=D(m.rate,m.duration);
}
// 0x138B48 motion-5 enter: v = (lip - position) / T (VU reciprocal).
inline void originalHandplantMotionEnter(const OriginalHandplantMotionState& m,OriginalHandplantRider& r){
    using namespace handplant_original;Rounding rounding;
    auto v=scale4(sub4(m.lip,quad(r.position,r.positionW)),vuReciprocal(m.duration));
    r.velocity=xyz(v);r.velocityW=v[3];
}
// 0x139548: phase-dependent launch velocity, computed once into +0x10.
inline bool originalHandplantLaunch(OriginalHandplantMotionState& m,int phase){
    using namespace handplant_original;Rounding rounding;
    const auto& e=m.exitVelocity;
    if(!(e[0]==0&&e[1]==0&&e[2]==0&&e[3]==0))return true;
    const float unit=bits(0x41de38e4u); // 1 km/h in cm/s
    if(phase==3){
        auto side=cross4(unitZ4,m.tangent);side=scale4(side,vuRsqrt(dot4(side,side)));
        float along=mul(m.entrySpeed,bits(0x3f333333u));
        float push=m.railFlag?15.f:2.f;float lateral=mul(push,unit);
        if(dot4(m.toLip,side)<0)lateral=-lateral;
        m.exitVelocity=sub4(scale4(m.tangent,along),scale4(side,lateral));
        m.exitVelocity[2]=mul(m.railFlag?10.f:0.f,unit);
        return true;
    }
    if(phase==4||phase==5){
        auto side=cross4(m.tangent,unitZ4);side=scale4(side,vuRsqrt(dot4(side,side)));
        float d=dot4(side,m.toLip);
        if(phase==4?d<0:0<d)side=scale4(side,-1.f);
        float push=m.railFlag?15.f:5.f;
        m.exitVelocity=scale4(side,mul(push,unit));
        m.exitVelocity[2]=mul(m.railFlag?5.f:0.f,unit);
        return true;
    }
    if(phase==6){
        m.exitVelocity=scale4(m.tangent,bits(0x44505556u));
        m.exitVelocity[2]=A(m.exitVelocity[2],bits(0x43d05556u));
        return true;
    }
    return false;
}
// 0x139178 motion-5 exit.
inline void originalHandplantMotionExit(OriginalHandplantMotionState& m,OriginalHandplantRider& r){
    originalHandplantLaunch(m,r.phase);r.velocity=handplant_original::xyz(m.exitVelocity);r.velocityW=m.exitVelocity[3];
}
struct OriginalHandplantMotionResult {bool pinned=false,launched=false,aligned=false;unsigned predictorSteps=0;};
// 0x1391A8 motion-5 update.
inline OriginalHandplantMotionResult originalHandplantMotionUpdate(OriginalHandplantMotionState& m,OriginalHandplantRider& r,const OriginalHandplantAccess& a){
    using namespace handplant_original;Rounding rounding;OriginalHandplantMotionResult out;
    const float step=mul(r.timeScale,bits(0x3c888889u));
    const float elapsed=A(m.elapsed,step);m.elapsed=elapsed;
    if(elapsed<=m.duration){
        float t=D(elapsed,m.duration);float s=mul(t,S(t,2.f));
        auto p=add4(scale4(sub4(m.start,m.lip),s),m.start);
        r.position=xyz(p);r.positionW=1.f;
        float angle=mul(mul(m.rate,step),.5f);auto sc=originalSinCos(angle);
        HandplantQuad delta{mul(sc[0],m.axis[0]),mul(sc[0],m.axis[1]),mul(sc[0],m.axis[2]),sc[1]};
        r.quaternion=quatMul(delta,r.quaternion);
    } else {
        out.pinned=true;
        r.position=xyz(m.lip);r.positionW=m.lip[3];r.velocity={};r.velocityW=0;
        const auto& e=m.exitVelocity;
        if(e[0]==0&&e[1]==0&&e[2]==0&&e[3]==0){
            if(originalHandplantLaunch(m,r.phase)){
                out.launched=true;
                m.predictorPosition=add4(m.lip,scale4(unitZ4,5.f));m.predictorVelocity=m.exitVelocity;
                need(a.predictorBegin,"predictorBegin")(m.predictorPosition,m.predictorVelocity);
                auto& stepPredictor=need(a.predictorStep,"predictorStep");auto& status=need(a.predictorStatus,"predictorStatus");
                do{stepPredictor(m.predictorPosition,m.predictorVelocity);++out.predictorSteps;}while(out.predictorSteps<8);
                for(;;){int s=status();if(s==1||s==3)break;stepPredictor(m.predictorPosition,m.predictorVelocity);++out.predictorSteps;}
            }
        } else {
            int s=need(a.predictorStatus,"predictorStatus")();
            if(s==1||s==3){
                auto normal=need(a.predictorNormal,"predictorNormal")();
                if(0<dot4(quad(normal,0.f),m.exitVelocity)){
                    auto heading=need(a.predictorHeading,"predictorHeading")();
                    r.quaternion=originalAirAlignment(r.quaternion,normal,heading,A(r.timeScale,r.timeScale),mul(r.timeScale,bits(0x40d43b68u))).quaternion;
                    out.aligned=true;
                }
            } else {need(a.predictorStep,"predictorStep")(m.predictorPosition,m.predictorVelocity);++out.predictorSteps;}
        }
    }
    rebuild(r);return out;
}

// ---------------------------------------------------------------------------
// Control 11.
// 0x1329B0 setup.
inline void originalHandplantControlSetup(OriginalHandplantControlState& c,const OriginalHandplantRider& r,const OriginalHandplantHit& hit,int side,float duration){
    using namespace handplant_original;Rounding rounding;
    c.timer=duration;c.side=side!=0;
    const auto v=quad(r.velocity,r.velocityW);
    c.fast=bits(0x450ae38eu)<vuSqrt(dot4(v,v));
    c.railFlag=hit.descriptor?int32_t(hit.descriptorFlags&1):0;
}
// 0x132FB8 play; the reflect variants turn the rider 180 degrees about up first.
inline void originalHandplantPlay(OriginalHandplantRider& r,int phase,int side,float rate,const OriginalHandplantAccess& a){
    using namespace handplant_original;Rounding rounding;
    const auto clip=originalHandplantClip(phase,side);
    if(clip.reflect){
        r.quaternion=quatMul({r.up[0],r.up[1],r.up[2],0.f},r.quaternion);rebuild(r);
        need(a.rotateAnimation,"rotateAnimation")(bits(0x40490fdbu));
    }
    need(a.play,"play")(clip.semantic,rate);
}
// 0x1328B0 enter.
inline void originalHandplantControlEnter(OriginalHandplantControlState& c,OriginalHandplantRider& r,const OriginalHandplantAccess& a){
    using namespace handplant_original;Rounding rounding;
    const float rate=bits(0x3d4ccccdu+1u); // 0x3D4CCCCE
    r.animationTurn.rate=rate;r.animationTurn.target=0;r.turn.rate=rate;r.turn.target=0;
    r.phase=1;
    need(a.stat,"stat")();
    float duration=need(a.semanticDuration,"semanticDuration")(originalHandplantClip(1,c.side).semantic);
    originalHandplantPlay(r,1,c.side,D(duration,c.timer),a);
    c.smoothed=0;
    auto& fade=need(a.fade,"fade");const float seconds=bits(0x3ea8f5c3u);fade(1,seconds);fade(0,seconds);
}
// 0x132F98 exit.
inline void originalHandplantControlExit(OriginalHandplantRider& r){
    r.phase=0;r.lean244.rate=std::bit_cast<float>(0x3d4ccccdu+1u);r.lean244.target=0;
}
struct OriginalHandplantControlResult {
    enum class Stop {None,Reset,Waiting,Scored,Balance,Exit,Launched,Attached,Ground,Air,Clip};
    Stop stop=Stop::None;int scoreKind=0;
};
// 0x132A30 update. word0 is the control-11 command word (bit12 ResetPath,
// bit13 Handplant held, bits14..19 HandplantBalance).
inline OriginalHandplantControlResult originalHandplantControlUpdate(OriginalHandplantControlState& c,OriginalHandplantRider& r,uint32_t word0,const OriginalHandplantAccess& a){
    using namespace handplant_original;Rounding rounding;OriginalHandplantControlResult out;using St=OriginalHandplantControlResult::Stop;
    if(need(a.resetPath,"resetPath")((word0>>12)&1)){out.stop=St::Reset;return out;}
    const bool held=word0&0x2000;
    if(r.phase==1){
        if(!need(a.channel2Complete,"channel2Complete")()){out.stop=St::Waiting;return out;}
        int kind;
        if(held&&c.fast==0){r.phase=2;kind=1;}else{r.phase=3;kind=2;}
        float award=need(a.scoreBegin,"scoreBegin")(kind);need(a.award,"award")(award);
        originalHandplantPlay(r,r.phase,c.side,1.f,a);
        c.timer=0;out.stop=St::Scored;out.scoreKind=kind;return out;
    }
    if(r.phase==2){
        const float dt=mul(r.timeScale,bits(0x3c888889u));
        c.timer=A(c.timer,dt);
        if(held&&std::abs(r.roll250.current)<1.f){
            float input=mul(float(int32_t(word0<<12)>>26),bits(0x3d042108u));
            if(r.reverseStance)input=-input;
            if(c.side==1)input=-input;
            const float lean=mul(dt,5.f),step=mul(dt,3.f);
            r.lean244.target=input;r.lean244.rate=lean;
            float smoothed;const float last=c.smoothed;
            if(A(input,step)<last)smoothed=S(last,step);
            else {float low=S(input,step);smoothed=last<low?A(last,step):input;}
            c.smoothed=smoothed;
            float target=smoothed;const float timer=c.timer;
            if(smoothed<0)target=-1.f;
            else if(0<smoothed)target=1.f;
            else {
                const float b=r.roll250.current;
                if(bits(0x3f0057b7u)<timer)target=b<0?-1.f:1.f;
                else if(!(b==0))target=b<0?-1.f:1.f;
            }
            float k=minS(mul(timer,bits(0x3e4cca86u)),1.f);
            const float magnitude=std::abs(r.roll250.current);
            float rest=S(1.f,k),outward=mul(k,5.5f);
            r.roll250.target=target;
            float inward=mul(k,4.f),restOutward=mul(rest,1.5f),centre=S(1.f,magnitude);
            inward=A(inward,rest);outward=A(outward,restOutward);inward=mul(inward,centre);outward=mul(outward,magnitude);
            r.roll250.rate=mul(A(outward,inward),dt);
            out.stop=St::Balance;return out;
        }
        int next;
        if(c.railFlag&&std::abs(r.roll250.current)<.25f)next=6;
        else next=r.roll250.current<bits(0xbd4ccccdu)?4:5;
        r.phase=next;
        need(a.bakeRoot,"bakeRoot")(r);
        r.turn={};r.extraLean={};r.roll250={};
        originalHandplantPlay(r,r.phase,c.side,1.f,a);
        out.stop=St::Exit;return out;
    }
    auto& mode=need(a.motionMode,"motionMode");
    if(mode()==5){
        float award=need(a.scoreLaunch,"scoreLaunch")();need(a.award,"award")(award);
        need(a.requestMotion,"requestMotion")(1);out.stop=St::Launched;
    }
    auto& play=need(a.play,"play");auto& control=need(a.requestControl,"requestControl");
    if(need(a.channel2Event,"channel2Event")(1)){
        if(need(a.railAttach,"railAttach")()){out.stop=St::Attached;return out;}
        if(mode()==0){play(61,1.f);control(0);out.stop=St::Ground;return out;}
    } else if(mode()==1){
        int s=need(a.predictorStatus,"predictorStatus")();
        if(s==1||s==3){
            float remaining=need(a.predictorRemaining,"predictorRemaining")();
            if(0<remaining){
                auto clock=need(a.channel2Clock,"channel2Clock")();
                float rate=D(S(clock[1],clock[0]),remaining);
                rate=.5f<=rate?minS(rate,2.f):.5f;
                need(a.setChannel2Rate,"setChannel2Rate")(rate);
            }
        }
    }
    if(!need(a.channel2Complete,"channel2Complete")())return out;
    if(mode()==1){play(287,1.f);control(4);out.stop=St::Air;}
    else {play(61,1.f);control(0);out.stop=St::Ground;}
    return out;
}

// ---------------------------------------------------------------------------
// 0x107578 complete entry: test, control 13, motion/control setup, control 11,
// motion 5. The host's requestControl/requestMotion run the original exits and
// enters (0x1328B0 via originalHandplantControlEnter, 0x138B48 via
// originalHandplantMotionEnter) and may modify the rider (control exits bake).
inline OriginalHandplantEntryTest originalHandplantEnter(OriginalHandplantRider& r,OriginalHandplantMotionState& m,
        OriginalHandplantControlState& c,const OriginalHandplantAccess& a){
    using handplant_original::need;
    if(a.markAttempt)a.markAttempt();
    auto test=originalHandplantEntryTest(r,a);
    if(!test.accepted)return test;
    need(a.requestControl,"requestControl")(13);
    originalHandplantMotionSetup(m,r,test.hit,test.duration);
    originalHandplantControlSetup(c,r,test.hit,m.side,test.duration);
    a.requestControl(11);
    need(a.requestMotion,"requestMotion")(5);
    return test;
}

// ---------------------------------------------------------------------------
// Scoring. 0x119BF0: +0x7C kind, +0x30 = -1, +0x78 = 0, +0x14 += 0.05f-ulp,
// 0x1176F8 (+0xA4 = -1). Returns the 0x10E098 award (always 0).
inline float originalHandplantScoreBegin(OriginalScoreBoundaryState& s,int kind){
    OriginalRounding rounding;
    s.identity.field7C=kind;s.airSeconds30=-1;s.timer78=0;
    s.score.accumulated14=originalScalarAdd(s.score.accumulated14,std::bit_cast<float>(0x3d4cccccu));
    s.score.comboTimeoutA4=-1;return 0;
}
// 0x119C38: +0x80++, 0x11A228(scorer,0,0,0,0,1), 0x117838, +0x30 = 0.
template<class Commit> auto originalHandplantScoreLaunch(OriginalScoreBoundaryState& s,Commit&& commit){
    s.field80=std::bit_cast<int32_t>(uint32_t(s.field80)+1u);
    auto result=commit(s,0,0,0,0,1);
    originalResetScoreBoundary(s);s.airSeconds30=0;return result;
}
}
