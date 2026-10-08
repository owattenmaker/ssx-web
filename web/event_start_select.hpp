#pragma once
// The Single Event start seeds the core uses. A seed is a recorded result (the PS2 countdown anchor's rider block), not the
// PS2's computation: the PS2 gets it from the race load (1297C8 -> 11D390's event branch) and the motion-3 hold up to the anchor.
// Its bits depend on the arithmetic that ran them, so an SSX_PS2_EXACT_FPU build with the console model on (exactArithmetic) takes
// the seeds of the exact-derived anchors (tools/export_exact_event_starts.py, tools/generate_event_seed.py --exact ->
// generated/event_start_seed_exact.hpp) where one exists, and the mode-1 seeds otherwise.
#include "generated/event_start_seed.hpp"
#if SSX_PS2_EXACT_FPU && __has_include("generated/event_start_seed_exact.hpp")
#include "generated/event_start_seed_exact.hpp"
#include "../engine/software_float.hpp"
#define SSX_EXACT_EVENT_SEEDS 1
#endif

inline bool browser_event_exact_seed() {
#if SSX_EXACT_EVENT_SEEDS
    return ssx::software_float::exactArithmetic && browserEventExactSeeded();
#else
    return false;
#endif
}

inline ssx::OriginalGroundState browser_event_ground_state() {
#if SSX_EXACT_EVENT_SEEDS
    if (browser_event_exact_seed()) {
        return browserEventGroundStateExact();
    }
#endif
    return browserEventGroundState();
}

inline ssx::OriginalGroundProfile browser_event_ground_profile() {
#if SSX_EXACT_EVENT_SEEDS
    if (browser_event_exact_seed()) {
        return browserEventGroundProfileExact();
    }
#endif
    return browserEventGroundProfile();
}

inline ssx::OriginalRaceParticipant browser_event_participant() {
#if SSX_EXACT_EVENT_SEEDS
    if (browser_event_exact_seed()) {
        return browserEventParticipantExact();
    }
#endif
    return browserEventParticipant();
}

// The countdown anchor's camera words (web/core.cpp's event camera seed): the exact anchor's own block where the exact seed is in
// use and the generator wrote one (SSX_EXACT_EVENT_CAMERA_SEEDS), the mode-1 block (event_instance_seed.hpp) otherwise.
inline const std::array<uint32_t, 271>* browser_event_camera() {
#if SSX_EXACT_EVENT_SEEDS && SSX_EXACT_EVENT_CAMERA_SEEDS
    if (browser_event_exact_seed() && browserEventCameraExact()) {
        return browserEventCameraExact();
    }
#endif
    return browserEventCamera;
}

inline uint32_t browser_event_camera_anchor_tick() {
#if SSX_EXACT_EVENT_SEEDS && SSX_EXACT_EVENT_CAMERA_SEEDS
    if (browser_event_exact_seed() && browserEventCameraExact()) {
        return browserEventCameraAnchorTickExact();
    }
#endif
    return browserEventCameraAnchorTick;
}

// Snow Jam's human seed (the reference the per-course character seeds are mapped from, web/animation_bridge.cpp).
inline ssx::OriginalGroundState browser_event_snow_jam_ground_state() {
#if SSX_EXACT_EVENT_SEEDS && SSX_EXACT_SNOW_JAM_SEED
    if (ssx::software_float::exactArithmetic) {
        return browser_start_exact_ARA1::browserEventGroundState();
    }
#endif
    return browser_start_ARA1::browserEventGroundState();
}

#if SSX_EXACT_EVENT_SEEDS && SSX_EXACT_CHARACTER_SEEDS
#include <string>
// The human rider's id (local/reference-exact/characters/<id>, set by the comparers through human_event_character, web/core.cpp):
// an exact core then seeds that rider's own exact grid state on this course where tools/export_exact_event_starts.py exported one,
// in place of the mode-1 seed it was handed (settings original_event_start, lineups.json humanGridState). init_animation clears it.
// A function-local static (per rider context, outside the snapshot registry: a comparer setting, set before the race starts).
inline std::string& browser_human_event_character() {
    RIDER_LOCAL static std::string id;
    return id;
}
#endif

// That rider's exact grid state on this course: false (keep the mode-1 seed) outside the console model, without a character id or
// without an export for this course and character.
inline bool browser_event_character_ground_state(ssx::OriginalGroundState& out) {
#if SSX_EXACT_EVENT_SEEDS && SSX_EXACT_CHARACTER_SEEDS
    if (ssx::software_float::exactArithmetic && !browser_human_event_character().empty()) {
        return browserEventCharacterGroundStateExact(browserEventLocation, browser_human_event_character(), out);
    }
#endif
    (void)out;
    return false;
}
