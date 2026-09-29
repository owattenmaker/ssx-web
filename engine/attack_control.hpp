#pragma once
#include <array>
#include <cstdint>
#include <functional>
namespace ssx {
// Rider fields read or written by original 0x1163B0 (attacks with no opponent).
struct OriginalAttackRider {
    bool switchStance=false;            // rider+0x320 != 0
    std::array<float,4> right{};        // rider+0x1A0 quad (w lane included)
    float timeScale=1;                  // rider+0x300
    float strength=0;                   // rider+0x350 charge strength
    std::array<float,4> facing{};       // rider+0x340 attack facing quad
};
// First playback sequence of a channel (0x311B20 -> 0x314760 list index 0).
struct OriginalAttackSequenceWeights {float weight=0,targetWeight=0,fadeRemaining=0;}; //+0x94/+0x98/+0x9C
// Animator boundary. Every call corresponds to one original callee; the caller
// applies it immediately to its animation graph.
struct OriginalAttackAccess {
    std::function<int(unsigned channel)> channelClass;             // 0x311AE8
    std::function<int(unsigned channel)> requestedSemantic;        // 0x312AA0
    std::function<bool(unsigned channel,unsigned bit)> latched;    // 0x311B20 +0xB0 via 0x1446A0
    std::function<bool(unsigned channel,unsigned bit)> raised;     // 0x311B20 +0xB8 via 0x1446B8
    std::function<void(unsigned channel,float rate)> setRate;      // first sequence +0x90
    std::function<OriginalAttackSequenceWeights(unsigned channel)> weights;
    std::function<void(unsigned channel,float weight,float target)> setWeights; // +0x94/+0x98 stores
    std::function<void(unsigned channel,float target,float fade)> fadeTo;       // 0x313A10 (also clears +0xA0)
    // 0x3128E8(animator,semantic,force,-1) with animator+0x20 = rider+0x8C0 (bone
    // mask) for the call; animator+0x20 is restored to -1 afterwards.
    std::function<void(int semantic,bool force)> play;
    std::function<void(unsigned channel,float seconds)> fade;      // 0x311E88
};
constexpr int originalAttackBlock=322,originalAttackPunchToeSide=323,originalAttackPunchHeelSide=324,
    originalAttackBlockCycle=325,originalAttackPunchToeCycle=326,originalAttackPunchHeelCycle=327;
// Complete 0x1163B0. Returns the original result: 1 while the upper channel's class
// is 3 (block) or 13 (punch). Cruise 0x131620 then clamps its turn to +-0.5
// (0x131804..0x13182C); natural air 0x12F730 returns before its targets.
bool originalAttackStep(OriginalAttackRider&,bool attackLeft,bool attackRight,const OriginalAttackAccess&);
// 0x131804..0x13182C: -0.5 <= turn ? min(turn,0.5) : -0.5 (NaN becomes -0.5).
float originalAttackClampTurn(float turn);
// 0x131C30 cruise exit (skipped when the next control is 4) and 0x12FB68 natural-air
// exit (skipped when the next control is 0): while the upper class is 3/13, fade
// channel 0 over 0.1 s and restore the channel-1 first-sequence rate to 1.
bool originalAttackControlExit(int upperClass);
}
