#pragma once
// Original per-rider trick scoring object *(rider+0x790) (0x1CC bytes, 0x5DC800 in the
// Snow Jam baseline) and its HUD message bank *(score+0x1B0) (44 slots of 0x9C bytes).
// The layout is kept byte-exact so ports are literal transcriptions of the MIPS routines and
// oracles/captures compare raw words. See docs/tricks-scoring.md for the field map.
#include "score_boundary.hpp"
#include "trick_history.hpp"
#include <array>
#include <bit>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <functional>
#include <string>
#include <vector>
#include "trick_commit.hpp"
namespace ssx {
struct OriginalHudSlot {        // 0x9C bytes; type 0x34 = free
 int32_t type=0x34;             // +00
 float maximum=-1;              // +04 duration seconds, -1 = persistent value slot
 float value=0;                 // +08 elapsed seconds (116FB8 adds 1/60), or -input for value slots
 int32_t arg=0;                 // +0C
 int32_t field10=0;             // +10
 int32_t points=0;              // +14
 std::array<char,0x84> text{};  // +18 NUL-terminated (1170A8/1171A8 write sprintf("%d",points))
};
static_assert(sizeof(OriginalHudSlot)==0x9C);
constexpr int kOriginalHudSlotCount=44;      // score+0x1B4
constexpr int kOriginalHudFree=0x34;
constexpr int kOriginalHudFixedSlots=0x22;   // types below 0x22 own slot [type]; 0x23+ are allocated by 117A58
struct OriginalScoreObject {
 static constexpr unsigned kBytes=0x1D0;     // 0x1CC used; captures record 0x1D0
 std::array<uint32_t,kBytes/4> w{};
 std::array<OriginalHudSlot,kOriginalHudSlotCount> hud{};
 bool hudPresent=true;                       // score+0x1B0 != 0 (single human rider)
 float& f(unsigned o){check(o);return *reinterpret_cast<float*>(&w[o/4]);}
 int32_t& i(unsigned o){check(o);return *reinterpret_cast<int32_t*>(&w[o/4]);}
 uint32_t& u(unsigned o){check(o);return w[o/4];}
 float f(unsigned o)const{check(o);return std::bit_cast<float>(w[o/4]);}
 int32_t i(unsigned o)const{check(o);return std::bit_cast<int32_t>(w[o/4]);}
 uint32_t u(unsigned o)const{check(o);return w[o/4];}
 uint8_t& b(unsigned o){if(o>=kBytes)throw std::runtime_error("score byte outside object");return reinterpret_cast<uint8_t*>(w.data())[o];}
 static void check(unsigned o){if(o%4||o>=kBytes)throw std::runtime_error("score word outside object");}
};

// Typed views used by the earlier verified helpers (grab/identity/commit/boundary). The
// original has one +0x60 grab list; the typed views carry it twice (score.history60 and
// identity.grabs60), both loaded from and stored to +0x60.
inline OriginalScoreBoundaryState originalScoreView(const OriginalScoreObject& o){
 OriginalScoreBoundaryState s;auto& id=s.identity;auto& g=s.score;
 id.stance00=o.i(0x00);id.field04=o.i(0x04);id.field08=o.i(0x08);id.style0C=o.i(0x0C);id.flag10=o.i(0x10);
 g.accumulated14=o.f(0x14);s.multiplier18=o.f(0x18);s.inverted1C=o.f(0x1C);id.style20=o.i(0x20);id.time24=o.f(0x24);
 id.flag28=o.i(0x28);id.time2C=o.f(0x2C);s.airSeconds30=o.f(0x30);id.spin34=o.f(0x34);id.flip38=o.f(0x38);
 g.holdIncrement3C=o.f(0x3C);g.holdSeconds40=o.f(0x40);g.totalSeconds44=o.f(0x44);g.longestSeconds48=o.f(0x48);
 g.normalCount4C=o.i(0x4C);g.tweakCount50=o.i(0x50);g.uberCount54=o.i(0x54);g.superUberCount58=o.i(0x58);g.activeUber5C=o.i(0x5C);
 for(unsigned k=0;k<3;k++)id.grabs60[k]=g.history60[k]=o.i(0x60+4*k);
 s.activeSeconds6C=o.f(0x6C);id.active70=o.i(0x70);s.field74=o.i(0x74);s.timer78=o.f(0x78);id.field7C=o.i(0x7C);s.field80=o.i(0x80);
 g.bonusPoints84=o.i(0x84);s.threshold88=o.i(0x88);g.holdThresholdIndex8C=o.i(0x8C);s.threshold90=o.i(0x90);s.threshold94=o.i(0x94);s.threshold98=o.i(0x98);
 g.comboTimeoutA4=o.f(0xA4);g.multiplier1C4=o.f(0x1C4);
 return s;
}
// Stores a typed view back. grabsFromIdentity selects which duplicate of +0x60 is authoritative
// (identity-side callers such as 11A8C8 users vs grab-score callers).
inline void originalStoreScoreView(OriginalScoreObject& o,const OriginalScoreBoundaryState& s,bool grabsFromIdentity=false){
 const auto& id=s.identity;const auto& g=s.score;
 o.i(0x00)=id.stance00;o.i(0x04)=id.field04;o.i(0x08)=id.field08;o.i(0x0C)=id.style0C;o.i(0x10)=id.flag10;
 o.f(0x14)=g.accumulated14;o.f(0x18)=s.multiplier18;o.f(0x1C)=s.inverted1C;o.i(0x20)=id.style20;o.f(0x24)=id.time24;
 o.i(0x28)=id.flag28;o.f(0x2C)=id.time2C;o.f(0x30)=s.airSeconds30;o.f(0x34)=id.spin34;o.f(0x38)=id.flip38;
 o.f(0x3C)=g.holdIncrement3C;o.f(0x40)=g.holdSeconds40;o.f(0x44)=g.totalSeconds44;o.f(0x48)=g.longestSeconds48;
 o.i(0x4C)=g.normalCount4C;o.i(0x50)=g.tweakCount50;o.i(0x54)=g.uberCount54;o.i(0x58)=g.superUberCount58;o.i(0x5C)=g.activeUber5C;
 for(unsigned k=0;k<3;k++)o.i(0x60+4*k)=grabsFromIdentity?id.grabs60[k]:g.history60[k];
 o.f(0x6C)=s.activeSeconds6C;o.i(0x70)=id.active70;o.i(0x74)=s.field74;o.f(0x78)=s.timer78;o.i(0x7C)=id.field7C;o.i(0x80)=s.field80;
 o.i(0x84)=g.bonusPoints84;o.i(0x88)=s.threshold88;o.i(0x8C)=g.holdThresholdIndex8C;o.i(0x90)=s.threshold90;o.i(0x94)=s.threshold94;o.i(0x98)=s.threshold98;
 o.f(0xA4)=g.comboTimeoutA4;o.f(0x1C4)=g.multiplier1C4;
}
inline OriginalTrickHistory originalScoreHistory(const OriginalScoreObject& o){
 OriginalTrickHistory h;for(unsigned k=0;k<10;k++)h.entries[k]={o.u(0xA8+8*k),o.u(0xAC+8*k)};h.next=o.u(0xF8);return h;
}
inline void originalStoreScoreHistory(OriginalScoreObject& o,const OriginalTrickHistory& h){
 for(unsigned k=0;k<10;k++){o.u(0xA8+8*k)=h.entries[k][0];o.u(0xAC+8*k)=h.entries[k][1];}o.u(0xF8)=h.next;
}

// ---- Original score/HUD routines on the raw object (engine/score_object.cpp) ----
struct OriginalScoreThreshold {float at=-1,points=0;};
struct OriginalScoreTables {   // 119210 tables (seconds or centimetres, points before multiplier1C4)
 std::vector<OriginalScoreThreshold> air1C,hold20,style1E,timer1F,distance1D; // 459E00 / 459E20 / 459E40 / 459F08 / 459F68
};
OriginalScoreTables originalScoreTables(); // authored values, terminated by at<0
// Live values the score routines read outside the object (rider, controllers, game mode).
struct OriginalScoreEnvironment {
 std::array<float,4> velocity{};     // rider+0x1E0 (117C28 squares all four lanes)
 float timeScale=1;                  // rider+0x300
 float up2=0;                        // rider+0x1C8 (z of +0x1C0): inverted when < 0
 int32_t motionMode=0,control=0;     // owner+0xDE0 / owner+0xDE4 (11FE98 / 11FEE8)
 int32_t owner2C0=0;float owner330=0;// control-8 (board press) substate and depth for slot 0xB
 float air18=0,air1C=0,air50=0;      // owner+0x230 +0x18/+0x1C/+0x50 (135F70/136100/136168/1360C8)
 float stick2A4=0,stick2B0=0;        // rider+0x2A4/+0x2B0 (12F118 while control 2)
 uint32_t globalTick=0;              // 1298C8
 float superTime2F0=0,meter2F8=0;int32_t tier2F4=0,drain304=0; // rider boost state (117FE0 boost widgets)
 int32_t slot19Points=0;             // 150960 (external career/progress value shown in slot 0x19)
 bool pointsToCareer=false;          // 0x535C10==4 && 0x535C11==0: 11A228/119608 call 119EF8 instead of popups
 bool careerAwards=true;             // 119EF8 gate: *(*(mgr+0x84)+0x28) is 0 or >= 10 (0 in the Snow Jam baseline)
 bool commitBlocked=false;           // 12A250(mgr+0x84+0xC) != 0: 11A228 returns 0 immediately
};
// Optional notifications for effects the score object only requests (audio/commentary/career).
struct OriginalScoreHooks {
 std::function<void(int kind,int32_t points)> career;      // 119EF8 body after its gate (1E3760/1597B0)
 std::function<void(const OriginalTrickIdentity&)> named;  // 29B7E0 named-trick commentary
 std::function<int32_t()> careerCash;                     // C+0xAC4 as 117FE0 reads it for slot 0x19: after this tick's 119EF8 awards
};
// Name builder 116950 used by 118FF8 (slot type 0 text). Tables come from the exported package.
using OriginalScoreNameBuilder=std::function<std::string(const OriginalTrickIdentity&)>;
// HUD slot API (1179E0 / 117A58 / 117048 / 1170A8 / 1171A8 / 117AE8 / 117B88 / 116FB8).
void originalHudFree(OriginalScoreObject&,int type);
int originalHudAllocate(OriginalScoreObject&,int type);
void originalHudSetValue(OriginalHudSlot&,int type,int32_t points,int32_t arg,float value); //1171A8
int originalHudPostPoints(OriginalScoreObject&,int type,int32_t points,int32_t arg,float duration); //117B88
int originalHudPostText(OriginalScoreObject&,int type,const std::string&,int32_t arg,float duration); //117AE8
void originalHudTick(OriginalHudSlot&); //116FB8
// Scoring helpers.
void originalScoreReset(OriginalScoreObject&);          //117838
void originalScoreComboReset(OriginalScoreObject&);     //1175F8
void originalScoreRunReset(OriginalScoreObject&);       //117540 (race start: stats, history, HUD)
void originalScoreComboAdd(OriginalScoreObject&,int32_t points); //117638
void originalScoreComboExpire(OriginalScoreObject&,const OriginalScoreEnvironment&,const OriginalScoreHooks&); //117718
int32_t originalScoreGrade(int32_t points);             //119310
int32_t originalScoreInvertedPoints(const OriginalScoreObject&); //117908
int32_t originalScorePendingPoints(const OriginalScoreObject&);  //117948
int32_t originalScoreMultipliedPoints(const OriginalScoreObject&); //117990
float originalScoreCareerAward(OriginalScoreObject&,int kind,int32_t amount,const OriginalScoreEnvironment&,const OriginalScoreHooks&); //119EF8
int32_t originalScoreThreshold(OriginalScoreObject&,unsigned indexOffset,const std::vector<OriginalScoreThreshold>&,int type,float value); //119210
// 11A228: complete commit. identity/named/repeat use the verified typed helpers.
struct OriginalScoreCommitResult {float meterDelta=0;int32_t points=0,repeats=0,grade=0;bool valid=false;OriginalTrickIdentity identity{};};
OriginalScoreCommitResult originalScoreCommit(OriginalScoreObject&,const OriginalTrickCommitProfile&,
 int32_t stance,int32_t alternate,int32_t style,int32_t flag,int32_t takeoff,bool riderStance,
 const OriginalScoreEnvironment&,const OriginalScoreNameBuilder&,const OriginalScoreHooks& ={});
// 117C28 per-tick update (includes 117FE0 and the HUD slot clocks). boostWidgets=false leaves
// slots 5/6/8/9/10 to the separately verified boost HUD helpers.
void originalScoreTick(OriginalScoreObject&,const OriginalScoreTables&,const OriginalScoreEnvironment&,
 const OriginalScoreHooks& ={},bool boostWidgets=true);
void originalScoreHud(OriginalScoreObject&,const OriginalScoreEnvironment&,const OriginalScoreHooks& ={},bool boostWidgets=true); //117FE0
float originalScoreCrash(OriginalScoreObject&,bool penalize,float meter); //119368 (11A7A8 lost points)
int32_t originalScoreLostPoints(OriginalScoreObject&); //11A7A8
float originalScorePickup(OriginalScoreObject&,int32_t points,const OriginalScoreEnvironment&,const OriginalScoreHooks& ={}); //119608
// 119448 multiplier pickup (stage builtin27 type 3 -> 10F1C0 -> rider vt+0x78 10E830 in the air or on a rail): +0x130++, and when
// the bank is present and the combo multiplier +0x18 is below the icon's value: HUD slot 4 = 1171A8(type 4, int(value), 0, 0)
// and +0x18 = value. Returns 0 (the 10E098 amount). R&B x2/x3/x5/x10 icons (docs/slopestyle-bigair.md).
float originalScoreMultiplierPickup(OriginalScoreObject&,float multiplier); //119448
float originalScoreBail(OriginalScoreObject&,bool attacked); //119B08 (hard crash 10EB30): returns the boost penalty
float originalScoreRecovery(OriginalScoreObject&,bool quick); //119BB0 (10F280 from crash recovery): quick get-up popup 0x21, returns +0.1 boost
// 119C98 (air control 133308, a new D-pad trick in phase 3): 11A168 records the finished rotation's
// identity (named bonus, history, name slot) without awarding points, then restarts the rotation
// fields and sets +0x08 (later repeats are not penalised). Returns the 10E098 amount (always 0).
float originalScoreTrickStart(OriginalScoreObject&,const OriginalTrickCommitProfile&,int32_t stance,bool riderStance,
 const OriginalScoreNameBuilder&,const OriginalScoreHooks& ={});
}

