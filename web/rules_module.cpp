// The rules module (docs/core-migration.md M5): a small wasm loaded at boot, before any course core, for the rules the menus and the
// career run (web/rules-module.js). It compiles the same engine/game_rules.hpp as the course core (rules_eval: web/rules_bridge.cpp)
// and holds the session's presentation generator (engine/presentation_rules.hpp). Built by web/build-rules.sh.
#include "presentation_rules.hpp"
#include <emscripten/emscripten.h>
#include <cstdint>

namespace {
ssx::rules::PresentationGenerator presentation;
}

extern "C" {
// Booted with the boot seed (once; later calls keep the first). Returns whether it was booted before.
EMSCRIPTEN_KEEPALIVE int presentation_boot(uint32_t seed) {
    const bool was = presentation.booted;
    presentation.boot(seed);
    return was ? 1 : 0;
}
EMSCRIPTEN_KEEPALIVE int presentation_booted() {
    return presentation.booted ? 1 : 0;
}
EMSCRIPTEN_KEEPALIVE uint32_t presentation_draw() {
    return presentation.draw();
}
EMSCRIPTEN_KEEPALIVE void presentation_note(double afterSeed, double perTick, double anchorTick, double ticks) {
    presentation.noteEventDraws(afterSeed, perTick, anchorTick, ticks);
}
EMSCRIPTEN_KEEPALIVE uint32_t presentation_roster_seed() {
    return presentation.rosterSeed();
}
// QA: the six words and the pending draws
EMSCRIPTEN_KEEPALIVE double* presentation_state() {
    static double out[7];
    for (unsigned k = 0; k < 6; ++k) {
        out[k] = double(presentation.words[k]);
    }
    out[6] = presentation.pendingDraws;
    return out;
}
}
