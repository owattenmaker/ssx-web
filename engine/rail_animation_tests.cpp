// Assert-based checks for the recovered rail animation drivers.
// clang++ -std=c++20 -frounding-math -ffp-contract=off -UNDEBUG -Iengine \
//   engine/rail_animation_tests.cpp engine/animation_sequence.cpp engine/animation_cycle.cpp
#include "rail_animation.hpp"
#include "animation_cycle.hpp"
#include <bit>
#include <cassert>
#include <cfenv>
#include <cstdio>
#include <random>
namespace {
uint32_t bits(float value){return std::bit_cast<uint32_t>(value);}
float f(uint32_t value){return std::bit_cast<float>(value);}
ssx::OriginalAnimationSequence railSequence(int semantic,const std::array<uint32_t,3>&clips,const std::array<float,3>&durations){
    ssx::OriginalAnimationSequence s;s.semantic=semantic;s.channel=2;
    s.slots={{clips[1],0,durations[1],1,1,true,true},{clips[0],0,durations[0],1,0,false,true}};
    return s;
}
bool threw(auto&&fn){try{fn();}catch(const std::runtime_error&){return true;}return false;}
}
int main(){
    using namespace ssx;
    // Leaf tables (0x104238) and their loaded clips.
    assert(originalRailCycleLeaves(18).negative==65&&originalRailCycleLeaves(18).center==63&&originalRailCycleLeaves(18).positive==64);
    assert(originalRailCycleLeaves(19).negative==68&&originalRailCycleLeaves(19).center==66&&originalRailCycleLeaves(19).positive==67);
    assert(originalRailCycleLeaves(20).negative==71&&originalRailCycleLeaves(20).center==69&&originalRailCycleLeaves(20).positive==70);
    assert(threw([]{originalRailCycleLeaves(21);}));
    assert(originalRailLeafClip(63).name=="RS_FWD_CYC"&&originalRailLeafClipId(63)==210u<<8);
    assert(originalRailLeafClip(67).name=="RSFS_BAL_L_CYC"&&originalRailLeafClipId(71)==218u<<8);
    assert(originalRailLeafClip(90).name=="L_RS_NORMAL"&&originalRailLeafClip(392).name=="RSFS_OUTOF_GRIND4");
    assert(threw([]{originalRailLeafClip(62);}));
    assert((originalRailCycleClips(18)==std::array<uint32_t,3>{212u<<8,210u<<8,211u<<8}));
    // 0x1326C8 semantic selection.
    assert(originalRailAttachSemantic(1,false)==18&&originalRailAttachSemantic(2,false)==18&&originalRailAttachSemantic(3,false)==19&&originalRailAttachSemantic(4,false)==20);
    assert(originalRailAttachSemantic(1,true)==68&&originalRailAttachSemantic(3,true)==70&&originalRailAttachSemantic(4,true)==69);
    assert(originalRailCompletionReplacement(6,68)==18&&originalRailCompletionReplacement(6,69)==20&&originalRailCompletionReplacement(6,70)==19&&originalRailCompletionReplacement(6,52)==18&&originalRailCompletionReplacement(8,220)==19&&threw([]{originalRailCompletionReplacement(1,0);}));

    // Kind 5 driver against the already verified three-way transcription (0x103CC8).
    const std::array<uint32_t,3> clips=originalRailCycleClips(19);const std::array<float,3> durations{f(0x3fd33334),f(0x40533334),f(0x3fb33334)};
    std::mt19937 rng(0x104238);std::uniform_real_distribution<float> v(-1,1);
    for(unsigned t=0;t<20000;++t){
        auto s=railSequence(19,clips,durations);s.slots[0].time=durations[1]*(v(rng)+1)*.5f;s.slots[0].rate=v(rng)*2;s.rate=(v(rng)+1)*1.5f;
        float balance=t<3?float(int(t)-1):v(rng);bool switched=t&1;float timeScale=1+v(rng)*.5f;
        OriginalAnimationCyclePair pair;pair.clips={s.slots[0].clip,s.slots[1].clip};pair.times={s.slots[0].time,0};pair.durations={durations[1],durations[0]};pair.rates={s.slots[0].rate,1};pair.sequenceRate=s.rate;
        originalThreeWayAnimationStep(pair,clips,durations,switched?-balance:balance,timeScale);
        bool removed=originalRailCycleStep(s,clips,durations,balance,switched,timeScale);
        assert(!removed&&s.completed==pair.completed&&s.seekPending&&s.slots[0].loop&&!s.slots[1].loop);
        for(unsigned i=0;i<2;++i){assert(s.slots[i].clip==pair.clips[i]);assert(bits(s.slots[i].time)==bits(pair.times[i]));assert(bits(s.slots[i].weight)==bits(pair.weights[i]));assert(bits(s.slots[i].duration)==bits(pair.durations[i]));}
    }
    {   // Switch negation selects the opposite side clip; the centre weight is 1-|amount|.
        auto s=railSequence(18,originalRailCycleClips(18),durations);
        originalRailCycleStep(s,originalRailCycleClips(18),durations,.25f,false,1);
        assert(s.slots[1].clip==211u<<8&&bits(s.slots[1].weight)==0x3e800000u&&bits(s.slots[0].weight)==0x3f400000u);
        s=railSequence(18,originalRailCycleClips(18),durations);
        originalRailCycleStep(s,originalRailCycleClips(18),durations,.25f,true,1);
        assert(s.slots[1].clip==212u<<8&&bits(s.slots[1].weight)==0x3e800000u);
        // Zero balance is the negative side; neg.s makes the side weight -0.0 (c.lt.s 0 < amount is false).
        s=railSequence(18,originalRailCycleClips(18),durations);
        originalRailCycleStep(s,originalRailCycleClips(18),durations,0,false,1);
        assert(s.slots[1].clip==212u<<8&&bits(s.slots[1].weight)==0x80000000u&&bits(s.slots[0].weight)==0x3f800000u);
        // A disabled side slot is initialised by 0x313C50; the side clock follows the centre phase.
        s=railSequence(20,originalRailCycleClips(20),durations);s.slots[1].enabled=false;s.slots[1].time=99;s.slots[0].time=durations[1]*.5f;
        originalRailCycleStep(s,originalRailCycleClips(20),durations,-.5f,false,1);
        assert(s.slots[1].enabled&&s.slots[1].clip==218u<<8&&s.slots[1].time<durations[0]&&s.slots[1].time>0);
        // A stop fade that ends this tick reports removal (0x313800 -> 0x3145F8).
        s=railSequence(18,originalRailCycleClips(18),durations);originalAnimationFadeOut(s,f(0x3c888889));
        assert(originalRailCycleStep(s,originalRailCycleClips(18),durations,0,false,1));
        assert(threw([&]{auto bad=railSequence(18,originalRailCycleClips(18),durations);originalRailCycleStep(bad,originalRailCycleClips(18),{0,0,0},0,false,1);}));
    }
    // Kinds 9/10 (0x103BE0): seek by magnitude, no clock advance.
    {
        assert(originalHalfpipeBalanceLeaves(45).negative==352&&originalHalfpipeBalanceLeaves(40).negative==346&&originalHalfpipeBalanceLeaves(40).nonNegative==347);
        assert(originalRailUberBalanceLeaves(218).negative==363&&originalRailUberBalanceLeaves(218).nonNegative==362&&originalRailUberBalanceLeaves(226).negative==372&&originalRailUberBalanceLeaves(234).nonNegative==380&&originalRailUberBalanceLeaves(242).negative==390&&originalRailUberBalanceLeaves(999).nonNegative==389);
        OriginalAnimationSequence s;s.semantic=218;s.slots={{0,0,1,1,1,false,true}};
        bool removed=originalTwoWayBalanceStep(s,384u<<8,2.f,385u<<8,4.f,-.25f,1);
        assert(!removed&&s.slots[0].clip==384u<<8&&bits(s.slots[0].time)==0x3f000000u&&s.slots[0].duration==2.f&&s.seekPending);
        removed=originalTwoWayBalanceStep(s,384u<<8,2.f,385u<<8,4.f,.25f,1);
        assert(!removed&&s.slots[0].clip==385u<<8&&bits(s.slots[0].time)==0x3f800000u);
        removed=originalTwoWayBalanceStep(s,384u<<8,2.f,385u<<8,4.f,0,1);
        assert(s.slots[0].clip==385u<<8&&bits(s.slots[0].time)==0);
        originalAnimationFadeOut(s,f(0x3c888889));assert(originalTwoWayBalanceStep(s,384u<<8,2.f,385u<<8,4.f,0,1));
        // Kind 10 resolves GRINDn BAL_R (negative) / BAL_L (non-negative) from the leaf table.
        s=OriginalAnimationSequence{};s.semantic=234;s.slots={{0,0,1,1,1,false,false}};
        originalRailUberBalanceStep(s,234,3.f,5.f,-.5f,1);assert(s.slots[0].clip==371u<<8&&s.slots[0].enabled&&bits(s.slots[0].time)==0x3fc00000u);
        originalRailUberBalanceStep(s,234,3.f,5.f,.5f,1);assert(s.slots[0].clip==370u<<8&&bits(s.slots[0].time)==0x40200000u);
    }
    // Rail uber records (0x45A038) and entry (0x136268).
    {
        auto r=originalRailUberRecord(0);assert(r.into[0]==213&&r.into[1]==214&&r.into[2]==215&&r.into[3]==216&&r.cycle==217&&r.balance==218&&r.land==219&&r.outOf==220&&r.tier==1);
        r=originalRailUberRecord(3);assert(r.into[0]==237&&r.cycle==241&&r.outOf==244&&r.tier==4);
        assert(threw([]{originalRailUberRecord(4);})&&threw([]{originalRailUberRecord(-1);}));
        auto e=originalRailUberEnter(1,4);assert(e.semantic==221&&e.setRoot&&bits(e.rootHalfAngle)==0x3f490fdbu&&!e.clearSwitch&&e.newStyle==3&&e.tier==2);
        e=originalRailUberEnter(1,3);assert(e.semantic==222&&!e.setRoot&&!e.clearSwitch);
        e=originalRailUberEnter(1,2);assert(e.semantic==223&&e.setRoot&&e.clearSwitch);
        e=originalRailUberEnter(1,1);assert(e.semantic==224&&e.setRoot&&!e.clearSwitch&&bits(e.channelFade)==0x3ea8f5c3u);
        assert(originalRailUberAvailable(2,1.f,2)&&!originalRailUberAvailable(-1,1.f,2)&&!originalRailUberAvailable(2,0.f,2)&&!originalRailUberAvailable(2,1.f,1));
    }
    // Rail uber update (0x136508) scenario: INTO -> CYC -> BAL -> OUTOF -> back to rail.
    {
        using R=OriginalRailUberRequest;OriginalRailUberState st{0,0};std::vector<R> out;
        OriginalRailUberInputs in;in.commandIdentity=0;in.motionMode=4;in.currentSemantic=216;in.primaryClass=21;in.railBalance=.3f;
        originalRailUberUpdate(st,in,3,out);
        assert(st.phase==0&&out.size()==1&&out[0].kind==R::BalanceTarget&&out[0].amount==0&&bits(out[0].rate)==0x3d6eeef0u);
        out.clear();in.primaryCompleted=true;originalRailUberUpdate(st,in,3,out);
        // Same tick: phase 0 plays CYC, then phase 1 keeps CYC (balance below 0.1).
        assert(st.phase==1&&out.size()==4&&out[1].kind==R::Play&&out[1].value==217&&out[2].kind==R::PrimaryRate&&out[3].kind==R::BalanceTarget&&bits(out[3].amount)==bits(.3f));
        out.clear();in.primaryCompleted=false;in.currentSemantic=217;in.balance238=.2f;originalRailUberUpdate(st,in,3,out);
        assert(out.size()==2&&out[1].kind==R::Play&&out[1].value==218);
        out.clear();in.currentSemantic=218;in.balance238=-.05f;in.motionMode=1;originalRailUberUpdate(st,in,3,out);
        assert(out.size()==2&&out[0].kind==R::BalanceTarget&&out[0].amount==0&&out[1].kind==R::Play&&out[1].value==217);
        // Landing on a rail during phase 1 plays LAND and returns; LAND holds until complete.
        out.clear();in.attached=true;originalRailUberUpdate(st,in,3,out);assert(out.size()==1&&out[0].kind==R::Play&&out[0].value==219);
        out.clear();in.attached=false;in.currentSemantic=219;in.motionMode=4;originalRailUberUpdate(st,in,3,out);assert(out.empty()&&st.phase==1);
        out.clear();in.primaryCompleted=true;originalRailUberUpdate(st,in,3,out);assert(out.size()==2&&out[1].kind==R::Play&&out[1].value==217);
        // Releasing the identity plays OUTOF; phase 2 waits for class-21 completion or event bit 0.
        out.clear();in.primaryCompleted=false;in.currentSemantic=217;in.commandIdentity=-1;originalRailUberUpdate(st,in,3,out);
        assert(st.phase==2&&out.size()==3&&out[0].kind==R::Play&&out[0].value==220&&out[1].kind==R::PrimaryRate&&out[2].kind==R::BalanceTarget);
        out.clear();in.currentSemantic=220;originalRailUberUpdate(st,in,3,out);assert(out.size()==1&&out[0].kind==R::BalanceTarget);
        out.clear();in.primaryRaisedBit0=true;originalRailUberUpdate(st,in,3,out);
        assert(out.size()==5&&out[1].kind==R::ReturnToRail&&out[1].value==19&&out[2].kind==R::RequestControl&&out[2].value==7&&out[3].kind==R::Score&&out[4].kind==R::UberCounter);
        // Off the rail (motion 0) the OUTOF is played from phase 1 and the exit restores stance.
        st={1,2};out.clear();in=OriginalRailUberInputs{};in.commandIdentity=2;in.motionMode=0;in.currentSemantic=233;in.primaryClass=21;
        originalRailUberUpdate(st,in,3,out);assert(st.phase==2&&out[0].kind==R::Play&&out[0].value==236);
        out.clear();in.primaryClass=15;originalRailUberUpdate(st,in,3,out);
        assert(out.size()==5&&out[1].kind==R::RestoreStance&&out[2].kind==R::RequestControl&&out[2].value==0);
        st={2,2};out.clear();in.motionMode=1;originalRailUberUpdate(st,in,3,out);assert(out[2].value==5);
        // Attached in phase 0 or 2 returns without requests.
        st={0,1};out.clear();in.attached=true;originalRailUberUpdate(st,in,3,out);assert(out.empty()&&st.phase==0);
        st={0,-1};assert(threw([&]{originalRailUberUpdate(st,in,3,out);}));
    }
    assert(std::fegetround()==FE_TONEAREST);
    puts("rail animation: 20,000 kind-5 ticks match the three-way transcription; tables, kind-10 seek, uber entry/update scenarios pass");
}
