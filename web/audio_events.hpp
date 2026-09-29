#pragma once
#include "rider_local.hpp"
// Audio observers (docs/audio-logic.md section 5). Gameplay sites that call the original SSXAudioSystem
// (28B180 -> 29xxxx dispatchers) post one compact event here instead; web/sfx.js drains them after every
// tick (audio_events / audio_events_clear, web/audio_events.inc). Observers only: nothing here may change
// rider state or draw from the game RNG.
enum AudioEventType {
 AE_TAKEOFF=1,        // 294170 (13F178 ground leave, 12E9B8 charged jump, 1307B8 ollie, 13BFA8 rail leave): a = speed 0x75C, b = predictor valid (+0xAC 1/3), c = predicted air T (+0x98)
 AE_LANDING=2,        // 2948D0 from 10E910 (air landing, rail entry) / 12D848 get-up: a = trick value (119D40 meter delta), b = impact 0x770, c = 1 rail entry, 2 get-up
 AE_CRASH=3,          // 10EB30: 296310 + 29F660(kind 0) + 2961F0(penalty): a = 119B08 penalty, b = surface 0x438
 AE_CRASH_LOOP=4,     // 12CD20 / 12D4E8 ground phase: a = channel-2 class (0x19/0x1D -> 296868 else 296E80), b = 1 from the air phase (12D4E8: 296310 too)
 AE_CRASH_AIR=5,      // 12D160 leaves the slide phase 2 (-> 3 get-up or -> 1 air): 296E20 + 297438 stop the slide loops (0.75 s)
 AE_CRASH_GRUNT=6,    // 12D160 class 0x1C after 10F280: 29F660(rider, 1, 0)
 AE_CRASH_EXIT=7,     // 12E690 (control 8 exit): wipeout speech 2A02D8
 AE_CRASH_METER=8,    // 12CB68: a = 1 reached (29E970 snd 0x60) / 0 (29E590 snd 0x5F), b = meter +0x70
 AE_BOOST=9,          // 114130: a = 1 start (298D90, b = pressed a2), 2 denied (299368), 3 release (2992D8)
 AE_FILL_RESET=10,    // 10E098 -> 29AB08: boost-fill tick pitch back to 0x1000
 AE_GRAB=11,          // 1352A8 -> 29A530 (snd 0x77)
 AE_UBER_DENIED=12,   // 132620 -> 299B70 (snd 0x6C)
 AE_HANDPLANT=13,     // 1328B0 -> 29DC48 (snd 0x61)
 AE_COMBO=14,         // 11A228 -> 29B430: a = +0x54 (this commit's Ubers), b = +0x114 (run Uber count before this commit)
 AE_NAMED_TRICK=15,   // 11A168 -> 29B7E0 (arcade speech 0x2133 variant 8)
 AE_TRICK_SPEECH=16,  // 115B58 reaction kinds 1/2 -> 29FF80(rider, a = 0 easy / 1 difficult)
 AE_WHOOH=17,         // 130DD0 -> 2A1560
 AE_SOFT=18,          // 108388 -> 2A0E70 (speech 0x2077 + grunt)
 AE_OBSTACLE=19,      // 13F488 -> 2989A8 (snd 0x36)
 AE_CONTACT=20,       // 105398 -> 296088: a = instance sound id (watrig), b = |v| cm/s, c = instance id
 AE_RESET=21,         // 116120 -> 29A220 (snd 0x7B), 28F108 intensity reset
 AE_PICKUP=22,        // 29CED8: a = kind 0..5, b = f12
 AE_ANIM_EVENT=23,    // 103AA0: a sequence event newly latched this tick (latched & raised): a = primary clip id, b = event bit, c = channel;
                      //   the JS keeps the clip event ids (0x8050/0x8051/0x8052 -> 104CC8 -> 289B18 sounds 0x51/0x50/0x52)
 AE_PAIR=24,          // 10E228/10E2E8/10E3A8/10E468 rider pairs: a = kind 0..3, b = relative speed, c = attacker, d = other rider slot
 AE_TIME_UP=25,       // 125228 -> 2A3C00 (arcade prompt 2)
 AE_FINISH=26,        // 125108 -> 286EA0
 AE_OVERTAKE=27,      // 10F998 -> 299E28: a = rider slot, b = old place * 8 + new place, c = rider behind (-1), d = rider ahead (-1)
 AE_SCRIPT_SOUND=28,  // stage builtins 30 (2FFF00 -> 2974A0 one-shot), 31 (3000A8 -> 297950 loop), 73 (300260 -> 297EB8 stop): a = 0/1/2, b = sound id, c = instance resource, d = key-2 word
 AE_ARCADE_BONUS=29,  // 1194C0 -> 2A3CE8(audio, rider, 1): Arcade_Bonus speech 0x20A9 for an accepted checkpoint bonus (slope style 0x535C12 == 1); a = value
 AE_RUMBLE_IMPACT=30, // crash controller 12D28C / 12D6F8 / 12D8A0 (ragdoll impacts): rider vtable+0x88 -> owner 1278D0, owner+0xDFC (v0) = max(v0, a); a = |impact velocity| (ctrl+0x60)
 AE_RUMBLE_SLIDE=31,  // crash slide 12D23C / get-up 12D8E8: rider vtable+0x90 -> owner 1278E0, owner+0xE00 (v1) = a; a = 0.5 x 12E528 playback base (every phase-2 tick) or 2 x (1 - clip progress) (phase-3 ground get-up)
};
void audio_event(int type,float a=0,float b=0,float c=0,float d=0);
RIDER_LOCAL extern float audioSpeed75C,audioTurn754,audioBrake750,audioCompression758,audioImpact770;
#include <array>
// 13F178 end (13F3AC..13F404): 0x75C = min(|v| / 3333.33, 1) from the velocity before the 13F358 clamp, 0x754 = |+0x1F0|, 0x750 = |+0x214|.
void audio_ground_speed(const std::array<float,3>& velocity);
// Stage-script sound builtins (web/stage_world.inc, web/stage_script_gameplay.inc): kind 0 play, 1 loop, 2 stop.
void audio_stage_sound(int kind,const std::array<uint32_t,3>& keys,uint32_t currentInstance);
// Every 114130 call site (core.cpp and the controller .inc files) goes through the observer: this header is
// included after the engine headers, so the macro only rewrites calls, not the declaration.
#include "../engine/boost_control.hpp"
ssx::OriginalBoostEffects audio_boost_observe(ssx::OriginalBoostEffects);
#define originalBoostControl(...) audio_boost_observe(::ssx::originalBoostControl(__VA_ARGS__))
