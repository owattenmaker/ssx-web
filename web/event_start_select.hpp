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

// Snow Jam's human seed (the reference the per-course character seeds are mapped from, web/animation_bridge.cpp).
inline ssx::OriginalGroundState browser_event_snow_jam_ground_state() {
#if SSX_EXACT_EVENT_SEEDS && SSX_EXACT_SNOW_JAM_SEED
    if (ssx::software_float::exactArithmetic) {
        return browser_start_exact_ARA1::browserEventGroundState();
    }
#endif
    return browser_start_ARA1::browserEventGroundState();
}
