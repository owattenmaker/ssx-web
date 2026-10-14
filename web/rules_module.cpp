// The rules module (docs/core-migration.md M5): a small wasm loaded at boot, before any course core, for the rules the menus and the
// career run (web/rules-module.js). It compiles the same engine/game_rules.hpp as the course core (rules_eval: web/rules_bridge.cpp)
// and holds the session's presentation generator (engine/presentation_rules.hpp) and the career kernel (engine/career_cascade.hpp: the
// award cascade, the messages and the lodge, M5d / M5e). Built by web/build-rules.sh.
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

// ---- M5d / M5e (pv coreCareerCascade, pv coreCareerLodge; docs/core-migration.md): the career kernel (engine/career_cascade.hpp) ----
#include "career_cascade.hpp"
#include <string>

namespace {
ssx::career::Data careerData;
std::string careerOut;
}

extern "C" {
// The session's static data: {rules, courses, rewards, shop, messages, challengeRows} (any field absent keeps the one given before).
// Returns 0, or -1 when the text does not parse.
EMSCRIPTEN_KEEPALIVE int career_data(const char* text) {
    try {
        const auto d = ssx::career::json::parse(text);
        for (const char* key : {"rules", "courses", "rewards", "shop", "messages", "challengeRows"}) {
            if (!d.contains(key)) {
                continue;
            }
            ssx::career::json& slot = std::string(key) == "rules" ? careerData.rules
                                      : std::string(key) == "courses" ? careerData.courses
                                      : std::string(key) == "rewards" ? careerData.rewards
                                      : std::string(key) == "shop" ? careerData.shop
                                      : std::string(key) == "messages" ? careerData.messages
                                      : careerData.challengeRows;
            slot = d[key];
        }
        return 0;
    } catch (...) {
        return -1;
    }
}
// One call (engine/career_cascade.hpp kernel): the output JSON text, or null when the call failed. A failed call leaves the session's
// presentation generator as it was before it, so the page's JS version runs from the same words.
EMSCRIPTEN_KEEPALIVE const char* career_kernel(const char* text) {
    const ssx::rules::PresentationGenerator before = presentation;
    try {
        careerOut = ssx::career::kernel(careerData, presentation, ssx::career::json::parse(text)).dump();
        return careerOut.c_str();
    } catch (...) {
        presentation = before;
        careerOut.clear();
        return nullptr;
    }
}
EMSCRIPTEN_KEEPALIVE unsigned career_out_size() {
    return unsigned(careerOut.size());
}
}
