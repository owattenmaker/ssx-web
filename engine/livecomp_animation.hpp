#pragma once
// LiveComp: the builtin-3 animation player (object 0x6C bytes, entity vtable 0x490B10 at
// +0x3C, entity at +0x30). Plays the instance model's authored node animation (model node
// field 2, the channel curves of engine/rail_modifier.hpp) against a time parameter.
//   builtin 3 (0x2FBCB8; defaults 0x4FB498: key1 mode 1, key3/key4/key7 -1, key5 30,
//   key2/key6/key8/key9 0; key types 0x445F48) -> 0x341AA0(object, 1, 1, instance, args)
//   0x341D48 per-tick update (entity vtable +0x78, from the entity update)
//   0x361098 node matrices (entity vtable +0xD8; 0x34DC90 = 0x34E348 channels + 0x34DD18
//   compose, shared with the AnimTeeter port). The draw (0x356298) passes these node
//   world matrices with an identity instance matrix, so they include the instance transform.
// Snow Jam race users: searchlight_white_sb101..104/eb101..106 (program 44, loop 4 s: rotation
// about local Y 1 -> 76 -> 0 deg), pinlight_blue_sb/eb (programs 39/45, loop 2 s, random start),
// startgatedoorbig_1000..1011 (stage global handler 2 = program 3, run at GO: once, 10 ticks,
// rotation about local Z 0 -> 90 deg). Time units: seconds of model animation.
// Float policy as rail_modifier.hpp.
#include "rail_modifier.hpp"
#include <functional>

namespace ssx {

// Builtin-3 argument block (0x2C bytes; key k at +4k).
struct OriginalLiveCompArguments {
    std::array<uint32_t,11> words{};
    static OriginalLiveCompArguments defaults(){
        OriginalLiveCompArguments a;auto f=[&](unsigned k,float v){a.words[k]=std::bit_cast<uint32_t>(v);};
        a.words[0]=0xFFFFFFFFu;a.words[1]=1;a.words[2]=0;f(3,-1);f(4,-1);f(5,30);a.words[6]=0;f(7,-1);a.words[8]=0;a.words[9]=0;a.words[10]=0;
        return a;
    }
    float real(unsigned k)const{return std::bit_cast<float>(words.at(k));}
};

struct OriginalLiveComp {
    int16_t mode=1;        // +0x00 0 once, 1 loop, 2 ping-pong (key1)
    int32_t enabled=1;     // +0x04
    int32_t done=0;        // +0x08 once-mode end reached
    int32_t delay=0;       // +0x0C ticks to wait (<0: frozen)
    float rate=0;          // +0x10 seconds per tick (negative: reverse)
    float low=0,high=0;    // +0x14/+0x18 time range
    // +0x1C base animation component: time (+0x1C), sampleTime (+0x20), previous (+0x24),
    // unclamped (+0x28), evaluated (+0x2C), channels, node matrices, dirty (+0x42 bit 0).
    OriginalAnimTeeter anim;
    bool suppressStaticDraw=false; // 0x34D9B0: instance flags (&~2)|4 when key9 != 0 or flags bit 0
};

// 0x341AA0 (with the 0x34D9B0 base). random() = 0x317810 words (gameplay RNG 0x4FF030),
// drawn for key8 (random start in [low,high]) and key6 != 0 (rate key5 +- key6).
inline OriginalLiveComp originalLiveCompConstruct(const OriginalAnimModel& model,const RollerMatrix& instanceMatrix,float instanceScale,
        uint32_t instanceFlags,const OriginalLiveCompArguments& a,const std::function<uint32_t()>& random,int32_t fps=60){
    using terrain_original::mul;
    OriginalLiveComp s;
    s.anim=originalAnimTeeterConstruct(model,instanceMatrix,instanceScale,OriginalAnimTeeterArgs{},fps); // base part only
    s.suppressStaticDraw=a.words[9]!=0||(instanceFlags&1u);
    const float third=std::bit_cast<float>(0x3d088889u);   // gp-0x2B94..-0x2B84 1/30
    const float start=anim_original::startTime(s.anim.channels);
    auto unit=[&](float lo,float hi){  // 0x317830
        const float r=originalScalarSubtract(std::bit_cast<float>((random()&0x7FFFFFu)|0x3F800000u),1.f);
        return originalScalarAdd(lo,mul(originalScalarSubtract(hi,lo),r));
    };
    OriginalRounding rounding;
    s.mode=int16_t(a.words[1]);
    s.low=0.f<=a.real(3)?mul(a.real(3),third):start;
    s.high=a.real(4)<0.f?model.length:mul(a.real(4),third);
    float t;
    if(a.real(7)<0.f)t=s.low;
    else{t=mul(a.real(7),third);if(t<s.low)t=s.low;}
    if(s.high<t)t=s.high;
    if(a.words[8]!=0)t=unit(s.low,s.high);
    s.anim.time=t;
    if(a.real(6)==0.f)s.rate=collision_scalar::divide(mul(a.real(5),third),float(fps));
    else{
        const float center=mul(a.real(5),third),spread=mul(a.real(6),third);
        const float r=originalScalarAdd(center,mul(spread,unit(-1.f,1.f)));   // 0x317890
        s.rate=collision_scalar::divide(r,float(fps));
    }
    if(a.words[2]==1)s.rate=-s.rate;
    s.anim.dirty=true;s.enabled=1;s.anim.sampleTime=s.anim.time;s.delay=0;s.done=0;
    return s;
}

// 0x341E48 loop / 0x341EC0 ping-pong / 0x341F38 once.
namespace livecomp_original {
inline void loop(OriginalLiveComp& s){
    auto add=originalScalarAdd;auto sub=originalScalarSubtract;float& t=s.anim.time;
    if(0.f<=s.rate){t=add(t,s.rate);s.anim.unclamped=t;if(s.high<t)t=add(s.low,sub(t,s.high));}
    else{t=add(t,s.rate);s.anim.unclamped=t;if(t<s.low)t=sub(s.high,sub(s.low,t));}
}
inline void pingPong(OriginalLiveComp& s){
    auto add=originalScalarAdd;auto sub=originalScalarSubtract;float& t=s.anim.time;
    if(0.f<=s.rate){t=add(t,s.rate);s.anim.unclamped=t;if(s.high<t){const float over=sub(t,s.high);s.rate=-s.rate;t=sub(s.high,over);}}
    else{t=add(t,s.rate);s.anim.unclamped=t;if(t<s.low){const float under=sub(s.low,t);s.rate=-s.rate;t=add(s.low,under);}}
}
inline void once(OriginalLiveComp& s){
    auto add=originalScalarAdd;float& t=s.anim.time;
    if(0.f<=s.rate){if(t==s.high)s.done=1;t=add(t,s.rate);s.anim.unclamped=t;if(s.high<t)t=s.high;}
    else{if(t==s.low)s.done=1;t=add(t,s.rate);s.anim.unclamped=t;if(t<s.low)t=s.low;}
}
}

enum class OriginalLiveCompTick {Finished,Updated,Detached,Done};
// 0x341D48. instanceOwned = 0x34EBA0 (instance+0xC == this entity). Done: the entity
// callback vtable+0x110 (0x341FC8 -> 0x34FCC0) runs; the time stays at the range end.
inline OriginalLiveCompTick originalLiveCompTick(OriginalLiveComp& s,bool instanceOwned=true){
    OriginalRounding rounding;
    const float big=std::bit_cast<float>(0x501502f9u);
    s.anim.unclamped=big;s.anim.previous=big;
    if(s.done)return OriginalLiveCompTick::Finished;
    if(s.enabled&&s.delay>=0){
        if(s.delay>0)s.delay-=1;
        else{
            s.anim.previous=s.anim.time;
            if(s.mode==1)livecomp_original::loop(s);else if(s.mode==2)livecomp_original::pingPong(s);else livecomp_original::once(s);
            if(!instanceOwned)return OriginalLiveCompTick::Detached;
        }
    }
    s.anim.dirty=true;s.anim.sampleTime=s.anim.time;
    return s.done?OriginalLiveCompTick::Done:OriginalLiveCompTick::Updated;
}

// 0x361098: node world matrices (instance transform included), recomputed when dirty.
inline const std::vector<RollerMatrix>& originalLiveCompMatrices(OriginalLiveComp& s){
    if(s.anim.dirty){originalAnimEvaluateChannels(s.anim);originalAnimComposeNodes(s.anim);s.anim.dirty=false;}
    return s.anim.matrices;
}
}
