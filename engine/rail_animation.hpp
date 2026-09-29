#pragma once
// Original SSX3 (SLUS_207.72) rail/grind animation drivers, transcribed from
// the PS2 executable. See RAIL_ANIMATION_RECOVERY.md for the address map.
//
//   kind 5 driver          0x104238 -> 0x103CC8   rail balance cycle (semantics 18/19/20)
//   attach semantic        0x1326C8              18/19/20 grounded, 68/69/70 airborne
//   kind 9/10 drivers      0x104660 / 0x1042E0 -> 0x103BE0  seek-by-magnitude pairs (half-pipe / uber BAL_L-BAL_R)
//   rail uber (control 12) 0x136268 enter, 0x136508 update, records 0x45A038
//   completion kinds       0x104C38 (6 -> 18/19/20, sequence kept), 0x104BB8 (8 -> semantic 19)
//
// Header-only, C++20. Unrecovered paths throw std::runtime_error explicitly.
#include "animation_sequence.hpp"
#include "terrain_contact_math.hpp"
#include "original_float.hpp"
#include <array>
#include <bit>
#include <cfenv>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <string_view>
#include <vector>
#pragma STDC FENV_ACCESS ON
namespace ssx {
namespace rail_animation_detail {
using Round=OriginalRounding;
// 0x313C50: assign a clip to slot `index`, publish its duration, initialise the
// slot only when it was disabled (+0x18 == 0) and mark the sequence seek (+0xC4).
inline void assignSlot(OriginalAnimationSequence&s,unsigned index,uint32_t clip,float duration){
    if(duration<=0)throw std::runtime_error("Invalid original rail clip duration");
    while(s.slots.size()<=index)s.slots.push_back({});
    auto&slot=s.slots[index];slot.clip=clip;slot.duration=duration;
    if(!slot.enabled){slot.time=0;slot.rate=1;slot.weight=1;slot.enabled=true;slot.loop=false;}
    s.seekPending=true;
}
inline float tickSeconds(float timeScale){return terrain_original::mul(timeScale,std::bit_cast<float>(0x3c888889u));} // gp-0x7F6C / gp-0x7F70
}

// ---------------------------------------------------------------------------
// Lookup leaves (0x311710 table, packed clip at *(gp+0xD8C)+0x1030+leaf*4)
// ---------------------------------------------------------------------------
struct OriginalRailLeafClip{uint32_t leaf;uint32_t bankIndex;std::string_view name;};
// Verified against the loaded lookup table of the Snow Jam glide savestate.
inline constexpr std::array<OriginalRailLeafClip,60> originalRailLeafClips{{
    {63,210,"RS_FWD_CYC"},{64,211,"RS_BAL_L_CYC"},{65,212,"RS_BAL_R_CYC"},
    {66,213,"RSFS_FWD_CYC"},{67,214,"RSFS_BAL_L_CYC"},{68,215,"RSFS_BAL_R_CYC"},
    {69,216,"RSBS_FWD_CYC"},{70,217,"RSBS_BAL_L_CYC"},{71,218,"RSBS_BAL_R_CYC"},
    {72,219,"RSREG_INTO_FS"},{73,220,"RSREG_INTO_BS"},{74,221,"RSFS_INTO_REG"},
    {75,222,"RSFAKIE_INTO_FS"},{76,223,"RSFAKIE_INTO_BS"},{77,224,"RSBS_INTO_REG"},
    {90,157,"L_RS_NORMAL"},{91,158,"L_RSBS_NORMAL"},{92,159,"L_RSFS_NORMAL"},
    {346,384,"HPTS_BAL_THROUGH"},{347,385,"HPTS_BAL_REFLECT"},{352,390,"HPHS_BAL_THROUGH"},{353,391,"HPHS_BAL_REFLECT"},
    {357,347,"RSBS_INTO_FS_GRIND1"},{358,348,"RSREG_INTO_FS_GRIND1"},{359,349,"RSFAKIE_INTO_FS_GRIND1"},{360,350,"RSFS_INTO_FS_GRIND1"},
    {361,351,"RSFS_GRIND1_CYC"},{362,352,"RSFS_GRIND1_BAL_L"},{363,353,"RSFS_GRIND1_BAL_R"},{364,355,"RSFS_GRIND1_LAND"},{365,354,"RSFS_OUTOF_GRIND1"},
    {366,356,"RSBS_INTO_FS_GRIND2"},{367,357,"RSREG_INTO_FS_GRIND2"},{368,358,"RSFAKIE_INTO_FS_GRIND2"},{369,359,"RSFS_INTO_FS_GRIND2"},
    {370,360,"RSFS_GRIND2_CYC"},{371,361,"RSFS_GRIND2_BAL_L"},{372,362,"RSFS_GRIND2_BAL_R"},{373,364,"RSFS_GRIND2_LAND"},{374,363,"RSFS_OUTOF_GRIND2"},
    {375,365,"RSBS_INTO_FS_GRIND3"},{376,366,"RSREG_INTO_FS_GRIND3"},{377,367,"RSFAKIE_INTO_FS_GRIND3"},{378,368,"RSFS_INTO_FS_GRIND3"},
    {379,369,"RSFS_GRIND3_CYC"},{380,370,"RSFS_GRIND3_BAL_L"},{381,371,"RSFS_GRIND3_BAL_R"},{382,373,"RSFS_GRIND3_LAND"},{383,372,"RSFS_OUTOF_GRIND3"},
    {384,374,"RSBS_INTO_FS_GRIND4"},{385,375,"RSREG_INTO_FS_GRIND4"},{386,376,"RSFAKIE_INTO_FS_GRIND4"},{387,377,"RSFS_INTO_FS_GRIND4"},
    {388,378,"RSFS_GRIND4_CYC"},{389,379,"RSFS_GRIND4_BAL_L"},{390,380,"RSFS_GRIND4_BAL_R"},{391,382,"RSFS_GRIND4_LAND"},{392,381,"RSFS_OUTOF_GRIND4"},
}};
inline const OriginalRailLeafClip&originalRailLeafClip(uint32_t leaf){
    for(const auto&entry:originalRailLeafClips)if(entry.leaf==leaf)return entry;
    throw std::runtime_error("Unrecovered original rail lookup leaf");
}
inline uint32_t originalRailLeafClipId(uint32_t leaf){return originalRailLeafClip(leaf).bankIndex<<8;} // packed id, bank 0

// ---------------------------------------------------------------------------
// Kind 5: rail balance cycle (0x104238). Leaf order is the 0x103CC8 argument
// order: {negative side (a3), centre (t0), positive side (t1)}.
// ---------------------------------------------------------------------------
struct OriginalRailCycleLeaves{uint32_t negative,center,positive;};
inline OriginalRailCycleLeaves originalRailCycleLeaves(int semantic){
    if(semantic==18)return{65,63,64}; // RS_BAL_R_CYC, RS_FWD_CYC, RS_BAL_L_CYC
    if(semantic==19)return{68,66,67}; // RSFS_BAL_R_CYC, RSFS_FWD_CYC, RSFS_BAL_L_CYC
    if(semantic==20)return{71,69,70}; // RSBS_BAL_R_CYC, RSBS_FWD_CYC, RSBS_BAL_L_CYC
    throw std::runtime_error("Original kind-5 driver only serves rail semantics 18/19/20");
}
// Clip ids in the same {negative,center,positive} order (bank 0 packed ids).
inline std::array<uint32_t,3> originalRailCycleClips(int semantic){
    auto leaves=originalRailCycleLeaves(semantic);
    return {originalRailLeafClipId(leaves.negative),originalRailLeafClipId(leaves.center),originalRailLeafClipId(leaves.positive)};
}
// 0x104238 + 0x103CC8 per tick. `balance238` is the rider+0x238 balance
// current; `switch320` is rider+0x320 (negates the amount). `clips`/`durations`
// are {negative,center,positive}. Slot 0 is the centre cycle (looped, advanced
// by 0x3135B0), slot 1 the side clip whose clock follows slot 0's phase.
// Weights: centre = 1 - |amount|, side = |amount|. Returns true when 0x313800
// finished a stop fade and 0x3145F8 removed the sequence.
inline bool originalRailCycleStep(OriginalAnimationSequence&s,const std::array<uint32_t,3>&clips,const std::array<float,3>&durations,float balance238,bool switch320,float timeScale){
    using namespace rail_animation_detail;Round round;
    float amount=balance238;if(switch320)amount=-amount; // 0x104250 neg.s
    assignSlot(s,0,clips[1],durations[1]);
    unsigned side;float magnitude;
    if(0.f<amount){side=2;magnitude=amount;}else{side=0;magnitude=-amount;} // 0x103D20 c.lt.s $f0(0),$f20
    assignSlot(s,1,clips[side],durations[side]);
    s.slots[0].weight=originalScalarSubtract(1.f,magnitude);s.slots[1].weight=magnitude;
    s.slots[0].loop=true; // 0x313D40(seq,0,1)
    float dt=tickSeconds(timeScale);
    s.completed=originalAnimationSlotStep(s.slots[0],s.rate,dt); // 0x3135B0(seq,0,dt)
    float phase=originalScalarDivide(s.slots[0].time,s.slots[0].duration);
    s.slots[1].time=phase*s.slots[1].duration;s.seekPending=true; // 0x313CF0(seq,1,·)
    return originalAnimationFadeStep(s,dt); // 0x313800 -> 0x3145F8
}

// ---------------------------------------------------------------------------
// Attach/cycle semantic (0x1326C8): style 1/2 -> 18, 3 -> 19, 4 -> 20 when
// grounded; airborne (controls 4/5) -> 68 L_RS_NORMAL, 70 L_RSFS_NORMAL,
// 69 L_RSBS_NORMAL. Played with blend -1 (state-table blend) and flags 0.
// ---------------------------------------------------------------------------
inline int originalRailAttachSemantic(int style,bool airborne){
    if(style==4)return airborne?69:20;
    if(style==3)return airborne?70:19;
    return airborne?68:18;
}
// Completion callbacks reached by rail clips (jump table 0x456990, indexed by the
// state's completion kind): kind 6 (rail spins 49..54, air entries 68..70) ->
// 0x104C38 -> 0x312BD0, which keeps the finished sequence (fading it out) and
// plays 20 for 69, 19 for 70, otherwise 18; kind 8 (RSFS_OUTOF_GRINDn) ->
// 0x104BB8 -> 0x312B18(...,19), which replaces the sequence.
inline int originalRailCompletionReplacement(uint32_t completionKind,int semantic){
    if(completionKind==6)return semantic==69?20:(semantic==70?19:18); // 0x104C38
    if(completionKind==8)return 19;  // 0x104BB8 -> 0x312B18(...,19)
    throw std::runtime_error("Completion kind is not a rail replacement");
}

// ---------------------------------------------------------------------------
// Two-way seek-by-magnitude driver 0x103BE0 (kinds 9 and 10). Leaves are
// {negative (t0), nonNegative (a3)}: amount < 0 -> negative clip at
// -amount*duration, otherwise nonNegative clip at amount*duration. No clock
// advance; only the sequence fade steps.
//   kind 9  0x104660: semantic 45 -> HPHS 352/353, else HPTS 346/347 (through,
//           reflect); amount = rider+0x244 (control 11 half-pipe balance).
//   kind 10 0x1042E0: rail uber BAL states 218/226/234/242 -> GRINDn BAL_R (t0)
//           / BAL_L (a3); amount = rider+0x238 balance current, not negated
//           by rider+0x320.
// ---------------------------------------------------------------------------
struct OriginalTwoWayLeaves{uint32_t negative,nonNegative;};
inline OriginalTwoWayLeaves originalHalfpipeBalanceLeaves(int semantic){return semantic==45?OriginalTwoWayLeaves{352,353}:OriginalTwoWayLeaves{346,347};} // kind 9: through, reflect
inline OriginalTwoWayLeaves originalRailUberBalanceLeaves(int semantic){ // kind 10: BAL_R, BAL_L
    if(semantic==218)return{363,362};
    if(semantic==226)return{372,371};
    if(semantic==234)return{381,380};
    return{390,389}; // 0x1042E0 falls through to GRIND4 for any other semantic
}
inline bool originalTwoWayBalanceStep(OriginalAnimationSequence&s,uint32_t negativeClip,float negativeDuration,uint32_t nonNegativeClip,float nonNegativeDuration,float amount,float timeScale){
    using namespace rail_animation_detail;Round round;
    if(amount<0.f){assignSlot(s,0,negativeClip,negativeDuration);float t=-amount;s.slots[0].time=terrain_original::mul(t,s.slots[0].duration);}
    else{assignSlot(s,0,nonNegativeClip,nonNegativeDuration);s.slots[0].time=terrain_original::mul(amount,s.slots[0].duration);}
    s.seekPending=true;
    return originalAnimationFadeStep(s,tickSeconds(timeScale));
}
// Kind-10 convenience: clips resolved from the leaf table, amount = rider+0x238.
inline bool originalRailUberBalanceStep(OriginalAnimationSequence&s,int semantic,float balanceRightDuration,float balanceLeftDuration,float balance238,float timeScale){
    auto leaves=originalRailUberBalanceLeaves(semantic);
    return originalTwoWayBalanceStep(s,originalRailLeafClipId(leaves.negative),balanceRightDuration,originalRailLeafClipId(leaves.nonNegative),balanceLeftDuration,balance238,timeScale);
}

// ---------------------------------------------------------------------------
// Rail uber (control 12). Record table 0x45A038, 0x24 bytes, identity 0..3:
//   +0x00 RSBS_INTO_FS_GRINDn   (style 4)   +0x10 RSFS_GRINDn_CYC   (kind 2)
//   +0x04 RSFS_INTO_FS_GRINDn   (style 3)   +0x14 RSFS_GRINDn_BAL_L (kind 10)
//   +0x08 RSFAKIE_INTO_FS_GRINDn (style 2)  +0x18 RSFS_GRINDn_LAND  (kind 1)
//   +0x0C RSREG_INTO_FS_GRINDn  (style 1)   +0x1C RSFS_OUTOF_GRINDn (completion 8)
//   +0x20 trick tier n
// ---------------------------------------------------------------------------
struct OriginalRailUberRecord{std::array<int,4> into;int cycle,balance,land,outOf,tier;};
inline OriginalRailUberRecord originalRailUberRecord(int identity){
    if(identity<0||identity>3)throw std::runtime_error("Original rail uber identity outside records 0..3");
    int base=213+identity*8;
    return {{base,base+1,base+2,base+3},base+4,base+5,base+6,base+7,identity+1};
}
// Balance-triplet rate used by every control-12 write (gp-0x72D4/-0x72D0/-0x72CC/-0x72C4).
inline constexpr float originalRailUberBalanceRate=std::bit_cast<float>(0x3d6eeef0u); // 0.0583333
inline constexpr float originalRailUberBalanceThreshold=std::bit_cast<float>(0x3dcccccdu); // gp-0x72C8, 0.1
inline constexpr float originalRailUberRootHalfAngle=std::bit_cast<float>(0x3f490fdbu); // gp-0x72E8/-0x72E4/-0x72E0, pi/4
inline constexpr float originalRailUberChannelFade=std::bit_cast<float>(0x3ea8f5c3u); // gp-0x72DC, 0.33

// 0x136268 enter. The caller applies the fields in order: optional root
// (anim+0x30 = 0x4FF130 translation, anim+0x40 = (0,0,sin,cos) of the half
// angle about 0x4FF160 Z, exactly the 0x132060 helper), optional switch clear
// (rider+0x320 = 0 and anim+0x18 = 0), play `semantic` with blend -1 flags 0,
// scoring 0x119938(tier, style) -> 0x10E098(rider,1,·), rider+0x328 = 3,
// control phase 0, 0x116930(rider), then fade channels 1 and 0 over 0.33 s.
struct OriginalRailUberEntry{int semantic;bool setRoot;float rootHalfAngle;bool clearSwitch;int scoreStyle;int tier;int newStyle=3;float channelFade=originalRailUberChannelFade;};
inline OriginalRailUberEntry originalRailUberEnter(int identity,int style){
    auto record=originalRailUberRecord(identity);
    if(style==4)return{record.into[0],true,originalRailUberRootHalfAngle,false,style,record.tier};
    if(style==3)return{record.into[1],false,0,false,style,record.tier};
    if(style==2)return{record.into[2],true,originalRailUberRootHalfAngle,true,style,record.tier};
    return{record.into[3],true,originalRailUberRootHalfAngle,false,style,record.tier};
}

// 0x136508 update, minus the caller-owned prologue (0x116120 recovery,
// 0x114130 boost, 0x113F38 turn, 0x113F88) which runs only on the not-attached
// path. Requests are emitted in original order and must be applied immediately
// (a Play replaces the channel-2 primary: current semantic, class 21, not
// completed, no raised flags).
struct OriginalRailUberState{int phase=0;int identity=-1;};
struct OriginalRailUberInputs{
    bool attached=false;        // 0x106848 returned nonzero this tick
    int commandIdentity=-1;     // control12 command bits15..22, signed
    int motionMode=0;           // 0x11FE98
    int currentSemantic=438;    // 0x312AA0(anim,2)
    bool primaryCompleted=false;// 0x312AE8(anim,2): first channel-2 sequence +0xC0
    int primaryClass=0;         // 0x311AE8
    bool primaryRaisedBit0=false;// 0x1446A0(seq+0xB0,0)
    float balance238=0;         // rider+0x238
    float railBalance=0;        // motion owner+0xC8 = rail motion +0x18
};
struct OriginalRailUberRequest{
    enum Kind{Play,PrimaryRate,BalanceTarget,ReturnToRail,RestoreStance,RequestControl,Score,UberCounter} kind;
    int value=0;float amount=0,rate=0;
};
inline void originalRailUberUpdate(OriginalRailUberState&state,const OriginalRailUberInputs&in,int style,std::vector<OriginalRailUberRequest>&out){
    auto record=originalRailUberRecord(state.identity);
    int current=in.currentSemantic;bool completed=in.primaryCompleted,raised=in.primaryRaisedBit0;int primaryClass=in.primaryClass;
    auto play=[&](int semantic){out.push_back({OriginalRailUberRequest::Play,semantic});current=semantic;completed=false;raised=false;primaryClass=21;};
    auto balance=[&](float target){out.push_back({OriginalRailUberRequest::BalanceTarget,0,target,originalRailUberBalanceRate});};
    if(in.attached){if(state.phase==1)play(record.land);return;}
    if(state.phase==0){
        balance(0);
        if(completed){play(record.cycle);out.push_back({OriginalRailUberRequest::PrimaryRate,0,1});state.phase=1;}
    }
    if(state.phase==1){
        if(in.commandIdentity!=state.identity||in.motionMode==0){
            play(record.outOf);out.push_back({OriginalRailUberRequest::PrimaryRate,0,1});state.phase=2;
        }else if(current!=record.land||completed){
            if(in.motionMode==4)balance(in.railBalance);else balance(0);
            int wanted=std::fabs(in.balance238)<originalRailUberBalanceThreshold?record.cycle:record.balance;
            if(wanted!=current)play(wanted);
        }
    }
    if(state.phase!=2)return;
    balance(0);
    if(primaryClass==21&&!completed&&!raised)return;
    if(in.motionMode==4){out.push_back({OriginalRailUberRequest::ReturnToRail,originalRailAttachSemantic(style,false)});out.push_back({OriginalRailUberRequest::RequestControl,7});}
    else{out.push_back({OriginalRailUberRequest::RestoreStance});out.push_back({OriginalRailUberRequest::RequestControl,in.motionMode==1?5:0});}
    out.push_back({OriginalRailUberRequest::Score,style});
    out.push_back({OriginalRailUberRequest::UberCounter}); // rider+0x2F4: if < 10 then ++, and when it becomes 10 rider+0x2F0 = 60.0 (0x42700000); >= 10 leaves both
}
// Control 7 -> control 12 hand-off (0x132620): identity != -1, rider+0x2F0 > 0
// and rider+0xB2C bit 1 set -> owner+0x394 = identity, request control 12.
inline bool originalRailUberAvailable(int identity,float uberMeter2F0,uint32_t flagsB2C){return identity!=-1&&0.f<uberMeter2F0&&((flagsB2C>>1)&1u);}
}
